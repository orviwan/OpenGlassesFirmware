#include "ble_handler.h"
#include "config.h" // For UUIDs
#include "photo_manager.h" // For handle_photo_control
#include "audio_handler.h" // For configure_microphone, deinit_microphone
#include "camera_handler.h" // For configure_camera, deinit_camera
#include "audio_ulaw.h" // For μ-law streaming support
#include "led_handler.h"    // For LED status indicators
#include "logger.h"         // For thread-safe logging
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLE2902.h> // For BLE2902 descriptor
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Define global BLE variables here
BLECharacteristic *g_photo_data_characteristic = nullptr;
BLECharacteristic *g_photo_control_characteristic = nullptr;
BLECharacteristic *g_audio_data_characteristic = nullptr;
BLECharacteristic *g_battery_level_characteristic = nullptr; // This will be passed to battery_handler

volatile bool g_is_ble_connected = false;

static TaskHandle_t photo_streaming_task_handle = nullptr;
static TaskHandle_t ulaw_streaming_task_handle = nullptr;
int g_photo_chunk_payload_size = 200; // Default, will be set dynamically

// --- ServerHandler Class Implementation ---
void ServerHandler::onConnect(BLEServer *server)
{
    g_is_ble_connected = true;
    logger_printf("[BLE] Client connected.\n");
    set_led_status(LED_STATUS_CONNECTED); // Set LED to green
    // Dynamically set photo chunk size based on negotiated MTU
    int mtu = BLEDevice::getMTU();
    g_photo_chunk_payload_size = mtu > 23 ? (mtu - 3) : 20;
    logger_printf("[BLE] Negotiated MTU: %d, photo chunk payload size set to: %d\n", mtu, g_photo_chunk_payload_size);
    
    // Resume tasks for active connection
    if (photo_streaming_task_handle != nullptr) {
        vTaskResume(photo_streaming_task_handle);
        logger_printf("[TASK] Resumed photo streaming task.\n");
    }
    if (ulaw_streaming_task_handle != nullptr) {
        vTaskResume(ulaw_streaming_task_handle);
        logger_printf("[TASK] Resumed u-law streaming task.\n");
    }
}

void ServerHandler::onDisconnect(BLEServer *server)
{
    g_is_ble_connected = false;
    logger_printf("[BLE] Client disconnected. Restarting advertising.\n");
    set_led_status(LED_STATUS_DISCONNECTED); // Set LED to orange

    // Suspend tasks to save power
    if (photo_streaming_task_handle != nullptr) {
        vTaskSuspend(photo_streaming_task_handle);
        logger_printf("[TASK] Suspended photo streaming task.\n");
    }
    if (ulaw_streaming_task_handle != nullptr) {
        vTaskSuspend(ulaw_streaming_task_handle);
        logger_printf("[TASK] Suspended u-law streaming task.\n");
    }

    // Reset the photo manager state to stop any ongoing processes
    reset_photo_manager_state();

    // De-initialize peripherals on disconnect
    logger_printf("[PERIPH] De-initializing microphone...\n");
    // deinit_camera(); // DEBUG: Do not de-init camera to test stability
    deinit_microphone();
    logger_printf("[PERIPH] Microphone de-initialized.\n");
    BLEDevice::startAdvertising(); // Restart advertising
}

// --- PhotoControlCallback Class Implementation ---
void PhotoControlCallback::onWrite(BLECharacteristic *characteristic)
{
    size_t len = characteristic->getLength();
    logger_printf("[BLE] PhotoControl received %zu bytes of data.\n", len);
    if (len == 1)
    {
        int8_t received_value = characteristic->getData()[0];
        logger_printf("[BLE] PhotoControl single byte value: %d\n", received_value);
        handle_photo_control(received_value); // Call function from photo_manager
    } else if (len > 0) {
        logger_printf("[BLE] PhotoControl received data (HEX): ");
        uint8_t* data = characteristic->getData();
        for (size_t i = 0; i < len; i++) {
            logger_printf("%02X ", data[i]);
        }
        logger_printf("\n");
        logger_printf("[BLE] PhotoControl expected a single byte. Command ignored.\n");
    } else {
        logger_printf("[BLE] PhotoControl received empty data. Command ignored.\n");
    }
}

void configure_ble()
{
    logger_printf("\n");
    logger_printf("[BLE] Initializing...\n");
    BLEDevice::init(DEVICE_MODEL_NUMBER); // Device name
    BLEDevice::setMTU(247); // Increase MTU for higher throughput
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new ServerHandler());

    // Create the main custom service
    BLEService *service = server->createService(SERVICE_UUID);

    // Photo Data Characteristic
    g_photo_data_characteristic = service->createCharacteristic(
        PHOTO_DATA_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    g_photo_data_characteristic->addDescriptor(new BLE2902()); // For notifications
    BLEDescriptor *pPhotoDataDesc = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    pPhotoDataDesc->setValue(PHOTO_DATA_USER_DESCRIPTION);
    g_photo_data_characteristic->addDescriptor(pPhotoDataDesc);

    // Photo Control Characteristic
    g_photo_control_characteristic = service->createCharacteristic(
        PHOTO_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    g_photo_control_characteristic->setCallbacks(new PhotoControlCallback());
    BLEDescriptor *pPhotoControlDesc = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    pPhotoControlDesc->setValue(PHOTO_CONTROL_USER_DESCRIPTION);
    g_photo_control_characteristic->addDescriptor(pPhotoControlDesc);
    uint8_t initial_control_value = 0; // Default to stop
    g_photo_control_characteristic->setValue(&initial_control_value, 1);

    // μ-law Audio Characteristic
    g_audio_data_characteristic = service->createCharacteristic(
        AUDIO_CODEC_ULAW_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    g_audio_data_characteristic->addDescriptor(new BLE2902()); // For notifications
    BLEDescriptor *pAudioUlawDesc = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    pAudioUlawDesc->setValue(AUDIO_ULAW_USER_DESCRIPTION);
    g_audio_data_characteristic->addDescriptor(pAudioUlawDesc);

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
    g_battery_level_characteristic->addDescriptor(new BLE2902()); // For notifications
    BLEDescriptor *pBatteryLevelDesc = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    pBatteryLevelDesc->setValue(BATTERY_LEVEL_USER_DESCRIPTION);
    g_battery_level_characteristic->addDescriptor(pBatteryLevelDesc);
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
    // Set advertising interval for power efficiency. A slower interval saves significant
    // power when the device is idle and waiting for a connection.
    // The value is in units of 0.625ms. 0x640 = 1600 -> 1600 * 0.625ms = 1000ms.
    advertising->setMinInterval(0x640); // 1000ms
    advertising->setMaxInterval(0x640); // 1000ms
    BLEDevice::startAdvertising();

    logger_printf("[BLE] Initialized and advertising started (1s interval).\n");
}

void photo_streaming_task(void *pvParameters) {
    unsigned long last_log_time = 0;
    while (true) {
        if (g_is_ble_connected && g_photo_data_characteristic) {
            BLE2902* desc = (BLE2902*)g_photo_data_characteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
            
            // Update the global flag based on the descriptor's notification status.
            g_photo_notifications_enabled = (desc && desc->getNotifications());
            
            static bool last_notifications_enabled_status = false;
            if (g_photo_notifications_enabled != last_notifications_enabled_status) {
                logger_printf("[BLE] Photo data notifications status: %s\n", g_photo_notifications_enabled ? "ENABLED" : "DISABLED");
                last_notifications_enabled_status = g_photo_notifications_enabled;
            }

            unsigned long now = millis();
            if (now - last_log_time > 5000) {
                logger_printf("[PHOTO_MGR] Uptime: %lus, Mode: %d, Uploading: %d, Ready: %d, Pending: %d\n",
                              now / 1000, g_capture_mode, g_is_photo_uploading, g_is_photo_ready, g_single_shot_pending);
                last_log_time = now;
            }
            
            process_photo_capture_and_upload(now);

        } else {
            // If not connected, delay to prevent busy-waiting
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        // Add a delay here to prevent starving other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void start_photo_streaming_task() {
    if (photo_streaming_task_handle == nullptr) {
        xTaskCreatePinnedToCore(
            photo_streaming_task, "PhotoStreamTask", 8192, NULL, 1, &photo_streaming_task_handle, 1);
        vTaskSuspend(photo_streaming_task_handle); // Suspend task immediately after creation
        logger_printf("[TASK] Created and suspended photo streaming task.\n");
    }
}

void ulaw_streaming_task(void *pvParameters) {
    static bool mic_active = false;
    while (true) {
        if (g_is_ble_connected && g_audio_data_characteristic) {
            BLE2902* desc = (BLE2902*)g_audio_data_characteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
            if (desc && desc->getNotifications()) {
                if (!mic_active) {
                    configure_microphone();
                    mic_active = true;
                }
                set_led_status(LED_STATUS_AUDIO_STREAMING); // Set LED to blue
                process_and_send_ulaw_audio(g_audio_data_characteristic);
                vTaskDelay(pdMS_TO_TICKS(5)); // Active streaming delay
            } else {
                if (mic_active) {
                    deinit_microphone();
                    mic_active = false;
                }
                set_led_status(LED_STATUS_CONNECTED); // Revert LED state
                vTaskDelay(pdMS_TO_TICKS(100)); // Connected but notifications disabled
            }
        } else {
            if (mic_active) {
                deinit_microphone();
                mic_active = false;
            }
            set_led_status(LED_STATUS_DISCONNECTED); // Revert LED state
            vTaskDelay(pdMS_TO_TICKS(500)); // Disconnected, sleep longer
        }
    }
}

void start_ulaw_streaming_task() {
    if (ulaw_streaming_task_handle == nullptr) {
        xTaskCreatePinnedToCore(
            ulaw_streaming_task, "ULawStreamTask", 4096, NULL, 1, &ulaw_streaming_task_handle, 1);
        vTaskSuspend(ulaw_streaming_task_handle); // Suspend task immediately after creation
        logger_printf("[TASK] Created and suspended u-law streaming task.\n");
    }
}

