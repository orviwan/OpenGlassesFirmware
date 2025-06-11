#include "ble_handler.h"
#include "config.h" // For UUIDs
#include "photo_manager.h" // For handle_photo_control
#include "audio_handler.h"
#include "audio_ulaw.h" // For μ-law streaming support
#include <Arduino.h> // For Serial
#include <BLEDevice.h>
#include <BLE2902.h> // For BLE2902 descriptor
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Define global BLE variables here
BLECharacteristic *g_photo_data_characteristic = nullptr;
BLECharacteristic *g_photo_control_characteristic = nullptr;
BLECharacteristic *g_audio_data_characteristic = nullptr;
BLECharacteristic *g_battery_level_characteristic = nullptr; // This will be passed to battery_handler

bool g_is_ble_connected = false;

static TaskHandle_t pcm_streaming_task_handle = nullptr;
static TaskHandle_t photo_streaming_task_handle = nullptr;
static TaskHandle_t ulaw_streaming_task_handle = nullptr;

// --- ServerHandler Class Implementation ---
void ServerHandler::onConnect(BLEServer *server)
{
    g_is_ble_connected = true;
    Serial.println("[BLE] Client connected.");
}

void ServerHandler::onDisconnect(BLEServer *server)
{
    g_is_ble_connected = false;
    Serial.println("[BLE] Client disconnected. Restarting advertising.");
    BLEDevice::startAdvertising(); // Restart advertising
}

// --- PhotoControlCallback Class Implementation ---
void PhotoControlCallback::onWrite(BLECharacteristic *characteristic)
{
    if (characteristic->getLength() == 1)
    {
        int8_t received_value = characteristic->getData()[0];
        Serial.printf("[BLE] PhotoControl received: %d\n", received_value);
        handle_photo_control(received_value); // Call function from photo_manager
    }
}

void configure_ble()
{
    Serial.println("[BLE] Initializing...");
    BLEDevice::init("OpenGlass"); // Device name
    BLEDevice::setMTU(247); // Increase MTU for higher throughput
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new ServerHandler());

    // Create the main custom service
    BLEService *service = server->createService(SERVICE_UUID);

    // Audio Data Characteristic
    g_audio_data_characteristic = service->createCharacteristic(
        AUDIO_DATA_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    g_audio_data_characteristic->addDescriptor(new BLE2902());

    // Audio Codec Characteristic
    BLECharacteristic *audioCodecCharacteristic = service->createCharacteristic(
        AUDIO_CODEC_UUID,
        BLECharacteristic::PROPERTY_READ);
    uint8_t current_audio_codec_id = AUDIO_CODEC_ID_PCM_8KHZ_16BIT; // Use the new 8kHz ID
    audioCodecCharacteristic->setValue(&current_audio_codec_id, 1);

    // Photo Data Characteristic
    g_photo_data_characteristic = service->createCharacteristic(
        PHOTO_DATA_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    g_photo_data_characteristic->addDescriptor(new BLE2902());

    // Photo Control Characteristic
    g_photo_control_characteristic = service->createCharacteristic(
        PHOTO_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    g_photo_control_characteristic->setCallbacks(new PhotoControlCallback());
    uint8_t initial_control_value = 0; // Default to stop
    g_photo_control_characteristic->setValue(&initial_control_value, 1);

    // μ-law Audio Characteristic
    BLECharacteristic *ulaw_audio_characteristic = service->createCharacteristic(
        AUDIO_CODEC_ULAW_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    ulaw_audio_characteristic->addDescriptor(new BLE2902());

    // Device Information Service
    BLEService *device_info_service = server->createService(DEVICE_INFORMATION_SERVICE_UUID);
    BLECharacteristic *manufacturer = device_info_service->createCharacteristic(MANUFACTURER_NAME_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    manufacturer->setValue("Based Hardware");
    BLECharacteristic *model = device_info_service->createCharacteristic(MODEL_NUMBER_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    model->setValue("OpenGlass");
    BLECharacteristic *firmware = device_info_service->createCharacteristic(FIRMWARE_REVISION_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    firmware->setValue("1.0.2"); // Example version
    BLECharacteristic *hardware = device_info_service->createCharacteristic(HARDWARE_REVISION_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    hardware->setValue("Seeed Xiao ESP32S3 Sense");

    // Battery Service
    BLEService *battery_service = server->createService(BATTERY_SERVICE_UUID);
    g_battery_level_characteristic = battery_service->createCharacteristic(
        BATTERY_LEVEL_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    g_battery_level_characteristic->addDescriptor(new BLE2902());
    // Initial battery value is set by battery_handler via initialize_battery_handler

    // Start services
    service->start();
    device_info_service->start();
    battery_service->start();

    // Start advertising
    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->addServiceUUID(DEVICE_INFORMATION_SERVICE_UUID);
    advertising->addServiceUUID(BATTERY_SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06); // 7.5ms
    advertising->setMaxPreferred(0x12); // 25ms
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Initialized and advertising started.");
}

void pcm_streaming_task(void *pvParameters) {
    while (true) {
        if (g_is_ble_connected && g_audio_data_characteristic) {
            // Only send PCM if notifications are enabled
            BLE2902* desc = (BLE2902*)g_audio_data_characteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
            if (desc && desc->getNotifications()) {
                process_and_send_audio(g_audio_data_characteristic);
            } else {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void start_pcm_streaming_task() {
    if (pcm_streaming_task_handle == nullptr) {
        xTaskCreatePinnedToCore(
            pcm_streaming_task, "PCMStreamTask", 4096, NULL, 1, &pcm_streaming_task_handle, 1);
    }
}

void photo_streaming_task(void *pvParameters) {
    while (true) {
        if (g_is_ble_connected && g_photo_data_characteristic) {
            BLE2902* desc = (BLE2902*)g_photo_data_characteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
            if (desc && desc->getNotifications()) {
                process_photo_capture_and_upload(millis());
            } else {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void start_photo_streaming_task() {
    if (photo_streaming_task_handle == nullptr) {
        xTaskCreatePinnedToCore(
            photo_streaming_task, "PhotoStreamTask", 4096, NULL, 1, &photo_streaming_task_handle, 1);
    }
}

void ulaw_streaming_task(void *pvParameters) {
    while (true) {
        if (g_is_ble_connected && g_audio_data_characteristic) {
            BLE2902* desc = (BLE2902*)g_audio_data_characteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
            if (desc && desc->getNotifications()) {
                process_and_send_ulaw_audio(g_audio_data_characteristic);
            } else {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void start_ulaw_streaming_task() {
    if (ulaw_streaming_task_handle == nullptr) {
        xTaskCreatePinnedToCore(
            ulaw_streaming_task, "ULawStreamTask", 4096, NULL, 1, &ulaw_streaming_task_handle, 1);
    }
}

void initialize_ble_and_tasks() {
    configure_ble();
    start_pcm_streaming_task();
    start_ulaw_streaming_task();
    start_photo_streaming_task();
}