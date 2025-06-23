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
#include <esp_gatts_api.h> // For esp_ble_gatts_cb_param_t
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Define global BLE variables here
BLECharacteristic *g_photo_data_characteristic = nullptr;
BLECharacteristic *g_photo_control_characteristic = nullptr;
BLECharacteristic *g_audio_data_characteristic = nullptr;
BLECharacteristic *g_battery_level_characteristic = nullptr; // This will be passed to battery_handler

volatile bool g_is_ble_connected = false;

// The photo streaming task handle and implementation are now moved to photo_manager.cpp
int g_photo_chunk_payload_size = 20; // Default to a safe size for 23-byte MTU

// --- ServerHandler Class Implementation ---
void ServerHandler::onConnect(BLEServer *server)
{
    g_is_ble_connected = true;
    logger_printf("[BLE] Client connected.\n");
    set_led_status(LED_STATUS_CONNECTED); // Set LED to green

    // On connection, the chunk size is the default until MTU is negotiated.
    logger_printf("[BLE] Using default chunk size: %d\n", g_photo_chunk_payload_size);

    // Tasks are now managed by their own logic (e.g., subscription status or commands)
    // and should not be blindly resumed on connection.
    /*
    if (photo_streaming_task_handle != nullptr) {
        vTaskResume(photo_streaming_task_handle);
        logger_printf("[TASK] Resumed photo streaming task.\n");
    }
    */
}

void ServerHandler::onDisconnect(BLEServer *server)
{
    g_is_ble_connected = false;
    logger_printf("[BLE] Client disconnected. Restarting advertising.\n");
    set_led_status(LED_STATUS_DISCONNECTED); // Set LED to orange

    // The photo manager state is now reset by its own subscription callback (stop_photo_streaming_task)
    // when the client unsubscribes or disconnects. Removing it from here prevents confusing logs.
    // reset_photo_manager_state();

    BLEDevice::startAdvertising(); // Restart advertising
}

void ServerHandler::onMtuChanged(BLEServer *pServer, void *param) {
    esp_ble_gatts_cb_param_t* p = (esp_ble_gatts_cb_param_t*)param;
    logger_printf("[BLE] MTU changed to: %d\n", p->mtu.mtu);
    // The payload size is the MTU minus 3 bytes for the ATT header and 2 bytes for our chunk header.
    g_photo_chunk_payload_size = p->mtu.mtu - 3 - PHOTO_CHUNK_HEADER_LEN;
    logger_printf("[BLE] Payload chunk size updated to: %d bytes\n", g_photo_chunk_payload_size);
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
    // BLEDevice::setMTU(247); // Let's use the default MTU for now to test compatibility
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new ServerHandler());

    // Create the main custom service
    BLEService *service = server->createService(SERVICE_UUID);

    // Photo Data Characteristic
    g_photo_data_characteristic = service->createCharacteristic(
        PHOTO_DATA_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    
    BLE2902* pPhoto2902 = new BLE2902();
    pPhoto2902->setCallbacks(new PhotoDescriptorCallback());
    g_photo_data_characteristic->addDescriptor(pPhoto2902);

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

    BLE2902* pAudio2902 = new BLE2902();
    pAudio2902->setCallbacks(new AudioDescriptorCallback());
    g_audio_data_characteristic->addDescriptor(pAudio2902);

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
    // The value is in units of 0.625ms. 0x0A0 = 160 -> 160 * 0.625ms = 100ms.
    advertising->setMinInterval(0x0A0); // 100ms
    advertising->setMaxInterval(0x0A0); // 100ms
    // Set preferred connection parameters for lower latency
    advertising->setMinPreferred(0x06);  // 7.5ms
    advertising->setMaxPreferred(0x10);  // 20ms
    BLEDevice::startAdvertising();

    logger_printf("[BLE] Initialized and advertising started (100ms interval).\n");
}

// --- PhotoDescriptorCallback Class Implementation ---
void PhotoDescriptorCallback::onWrite(BLEDescriptor* pDescriptor) {
    uint8_t* pValue = pDescriptor->getValue();
    if (pDescriptor->getLength() == 2) {
        uint16_t ccc_value = (pValue[1] << 8) | pValue[0];
        if (ccc_value == 0x0001) {
            // Notifications enabled
            logger_printf("[BLE] Photo notifications ENABLED. Starting photo stream.\n");
            start_photo_streaming_task();
        } else if (ccc_value == 0x0000) {
            // Notifications disabled
            logger_printf("[BLE] Photo notifications DISABLED. Stopping photo stream.\n");
            stop_photo_streaming_task();
        }
    }
}

// --- AudioDescriptorCallback Class Implementation ---
void AudioDescriptorCallback::onWrite(BLEDescriptor* pDescriptor) {
    uint8_t* pValue = pDescriptor->getValue();
    if (pDescriptor->getLength() == 2) {
        uint16_t ccc_value = (pValue[1] << 8) | pValue[0];
        if (ccc_value == 0x0001) {
            // Notifications enabled
            logger_printf("[BLE] Audio notifications ENABLED. Starting audio stream.\n");
            start_ulaw_streaming_task();
        } else if (ccc_value == 0x0000) {
            // Notifications disabled
            logger_printf("[BLE] Audio notifications DISABLED. Stopping audio stream.\n");
            stop_ulaw_streaming_task();
        }
    }
}

// The photo_streaming_task and its start function have been moved to photo_manager.cpp
// to consolidate all photo-related logic.

