#include "ble_handler.h"
#include "config.h" // For UUIDs
#include "photo_manager.h" // For handle_photo_control
#include "audio_handler.h" // For configure_microphone, deinit_microphone
#include "camera_handler.h" // For configure_camera, deinit_camera
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

static TaskHandle_t photo_streaming_task_handle = nullptr;
static TaskHandle_t ulaw_streaming_task_handle = nullptr;

// --- ServerHandler Class Implementation ---
void ServerHandler::onConnect(BLEServer *server)
{
    g_is_ble_connected = true;
    Serial.println("[BLE] Client connected.");
    // Initialize peripherals on connect
    Serial.println("[PERIPH] Initializing camera and microphone...");
    configure_camera();
    configure_microphone();
    Serial.println("[PERIPH] Camera and microphone initialized.");
}

void ServerHandler::onDisconnect(BLEServer *server)
{
    g_is_ble_connected = false;
    Serial.println("[BLE] Client disconnected. Restarting advertising.");
    // De-initialize peripherals on disconnect
    Serial.println("[PERIPH] De-initializing camera and microphone...");
    deinit_camera();
    deinit_microphone();
    Serial.println("[PERIPH] Camera and microphone de-initialized.");
    BLEDevice::startAdvertising(); // Restart advertising
}

// --- PhotoControlCallback Class Implementation ---
void PhotoControlCallback::onWrite(BLECharacteristic *characteristic)
{
    size_t len = characteristic->getLength();
    Serial.printf("[BLE] PhotoControl received %zu bytes of data.\n", len);
    if (len == 1)
    {
        int8_t received_value = characteristic->getData()[0];
        Serial.printf("[BLE] PhotoControl single byte value: %d\n", received_value);
        handle_photo_control(received_value); // Call function from photo_manager
    } else if (len > 0) {
        Serial.print("[BLE] PhotoControl received data (HEX): ");
        uint8_t* data = characteristic->getData();
        for (size_t i = 0; i < len; i++) {
            Serial.printf("%02X ", data[i]);
        }
        Serial.println();
        Serial.println("[BLE] PhotoControl expected a single byte. Command ignored.");
    } else {
        Serial.println("[BLE] PhotoControl received empty data. Command ignored.");
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
    g_audio_data_characteristic = service->createCharacteristic(
        AUDIO_CODEC_ULAW_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    g_audio_data_characteristic->addDescriptor(new BLE2902());

    // Device Information Service
    BLEService *device_info_service = server->createService(DEVICE_INFORMATION_SERVICE_UUID);
    BLECharacteristic *manufacturer = device_info_service->createCharacteristic(MANUFACTURER_NAME_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    manufacturer->setValue(DEVICE_MANUFACTURER_NAME);
    BLECharacteristic *model = device_info_service->createCharacteristic(MODEL_NUMBER_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    model->setValue(DEVICE_MODEL_NUMBER);
    BLECharacteristic *firmware = device_info_service->createCharacteristic(FIRMWARE_REVISION_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    firmware->setValue(DEVICE_FIRMWARE_REVISION);
    BLECharacteristic *hardware = device_info_service->createCharacteristic(HARDWARE_REVISION_STRING_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    hardware->setValue(DEVICE_HARDWARE_REVISION);

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

void photo_streaming_task(void *pvParameters) {
    while (true) {
        if (g_is_ble_connected && g_photo_data_characteristic) {
            BLE2902* desc = (BLE2902*)g_photo_data_characteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
            bool notifications_enabled = (desc && desc->getNotifications());
            // Log status less frequently to avoid spamming, e.g., only on change or periodically
            static bool last_notifications_enabled_status = false;
            if (notifications_enabled != last_notifications_enabled_status) {
                Serial.printf("[BLE] Photo data notifications status: %s\n", notifications_enabled ? "ENABLED" : "DISABLED");
                last_notifications_enabled_status = notifications_enabled;
            }

            if (notifications_enabled) {
                process_photo_capture_and_upload(millis());
            } else {
                // If notifications are not enabled, reset photo capture mode to stop
                // This prevents the photo manager from trying to capture if client disconnects/disables notifications
                // without sending a stop command.
                // handle_photo_control(0); // 0 is stop command
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Task runs approx every 10ms + processing time
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

