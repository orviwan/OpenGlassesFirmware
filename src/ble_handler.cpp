#include "ble_handler.h"
#include "config.h" // For UUIDs
#include "photo_manager.h" // For handle_photo_control
#include "audio_handler.h"
#include <Arduino.h> // For Serial
#include <BLEDevice.h>
#include <BLE2902.h> // For BLE2902 descriptor

// Define global BLE variables here
BLECharacteristic *g_photo_data_characteristic = nullptr;
BLECharacteristic *g_photo_control_characteristic = nullptr;
BLECharacteristic *g_audio_data_characteristic = nullptr;
BLECharacteristic *g_battery_level_characteristic = nullptr; // This will be passed to battery_handler
BLECharacteristic *g_opus_audio_characteristic = nullptr;

bool g_is_ble_connected = false;
bool g_opus_streaming_enabled = false;

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

// Helper to check if Opus notifications are enabled
bool is_opus_notify_enabled() {
    if (!g_opus_audio_characteristic) return false;
    BLE2902* desc = (BLE2902*)g_opus_audio_characteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    return desc && desc->getNotifications();
}

void configure_ble()
{
    Serial.println("[BLE] Initializing...");
    BLEDevice::init("OpenGlass"); // Device name
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

    // Opus Audio Characteristic
    g_opus_audio_characteristic = service->createCharacteristic(
        AUDIO_CODEC_OPUS_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    g_opus_audio_characteristic->addDescriptor(new BLE2902());

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
    // Min/Max interval for connection parameters (can be adjusted)
    // advertising->setMinPreferred(0x06); // e.g. 12.5ms
    // advertising->setMaxPreferred(0x12); // e.g. 25ms
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Initialized and advertising started.");
}

// Add this function to be called from your main loop:
void handle_opus_streaming() {
    if (is_opus_notify_enabled()) {
        g_opus_streaming_enabled = true;
        process_and_send_opus_audio();
    } else {
        g_opus_streaming_enabled = false;
    }
}