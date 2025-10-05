#include "ble_handler.h"
#include "config.h"
#include "state_handler.h"
#include "logger.h"
#include "command_handler.h"
#include "photo_handler.h"
#include <Arduino.h>
#include <NimBLEDevice.h>


// Global BLE variables
BLECharacteristic *g_device_status_characteristic = nullptr;
BLECharacteristic *g_battery_level_characteristic = nullptr;
BLECharacteristic *g_audio_data_characteristic = nullptr;
BLECharacteristic *g_photo_data_characteristic = nullptr;
BLECharacteristic *g_photo_control_characteristic = nullptr;

// Add this line
SemaphoreHandle_t g_photo_ack_semaphore = NULL;

volatile bool g_is_ble_connected = false;
uint32_t g_conn_handle = 0;

// --- Security & Bonding Callbacks ---
class ServerSecurityCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
        g_is_ble_connected = true;
        g_conn_handle = desc->conn_handle;
        logger_printf("[BLE] Client connected. Handle: %d\n", g_conn_handle);
        
        // Check if the device is bonded
        if (NimBLEDevice::isBonded(desc->peer_ota_addr)) {
            logger_printf("[BLE] Client is bonded.\n");
            set_current_state(STATE_CONNECTED_BLE);
        } else {
            logger_printf("[BLE] New client. Requesting pairing...\n");
            // This will initiate the pairing process
            pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
        }
    }

    void onDisconnect(NimBLEServer* pServer) {
        g_is_ble_connected = false;
        logger_printf("[BLE] Client disconnected.\n");
        set_current_state(STATE_IDLE);
        // Restart advertising to allow new connections
        NimBLEDevice::getAdvertising()->start();
    }

    void onAuthenticationComplete(ble_gap_conn_desc* desc) {
        if (NimBLEDevice::isBonded(desc->peer_ota_addr)) {
            logger_printf("[BLE] Pairing successful. Client is now bonded.\n");
            set_current_state(STATE_CONNECTED_BLE);
        } else {
            logger_printf("[BLE] Pairing failed.\n");
        }
    }
};

// --- Characteristic Callbacks ---
class CommandControlCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        handle_command_control(value);
    }
};

class PhotoControlCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            // Check for ACK (2 bytes)
            if (value.length() == 2) {
                if (g_photo_ack_semaphore != NULL) {
                    xSemaphoreGive(g_photo_ack_semaphore);
                }
                return;
            }

            uint8_t command = value[0];
            if (command == 0x01) { // Single photo request command
                logger_printf("[BLE] Photo request received.\n");
                start_photo_transfer_task();
            } else {
                logger_printf("[BLE] Unknown photo command: 0x%02X\n", command);
            }
        }
    }
};

void configure_ble() {
    logger_printf("[BLE] Initializing...\n");

    NimBLEDevice::init(DEVICE_MODEL_NUMBER);

    // Setup security
    NimBLEDevice::setSecurityAuth(true, true, true); // Bonding, MITM, Secure Connections
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Set power to max

    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerSecurityCallbacks());

    // --- OpenGlass Service ---
    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    // Command Control Characteristic
    NimBLECharacteristic *pCommandControlChar = pService->createCharacteristic(
        COMMAND_CONTROL_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC
    );
    pCommandControlChar->setCallbacks(new CommandControlCallback());

    // Device Status Characteristic
    g_device_status_characteristic = pService->createCharacteristic(
        COMMAND_STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC
    );

    // Error Notification Characteristic
    NimBLECharacteristic *pErrorChar = pService->createCharacteristic(
        ERROR_NOTIFICATION_UUID,
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC
    );

    // Audio Data Characteristic
    g_audio_data_characteristic = pService->createCharacteristic(
        AUDIO_DATA_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // Photo Data Characteristic (for sending image data)
    g_photo_data_characteristic = pService->createCharacteristic(
        PHOTO_DATA_UUID,
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC
    );

    // Photo Control Characteristic (for initiating photo capture)
    g_photo_control_characteristic = pService->createCharacteristic(
        PHOTO_CONTROL_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC
    );
    g_photo_control_characteristic->setCallbacks(new PhotoControlCallback());


    // --- Standard Services ---
    // Device Information Service
    NimBLEService *pDeviceInfoService = pServer->createService(DEVICE_INFORMATION_SERVICE_UUID);
    pDeviceInfoService->createCharacteristic(MANUFACTURER_NAME_STRING_CHAR_UUID, NIMBLE_PROPERTY::READ)->setValue(DEVICE_MANUFACTURER_NAME);
    pDeviceInfoService->createCharacteristic(MODEL_NUMBER_STRING_CHAR_UUID, NIMBLE_PROPERTY::READ)->setValue(DEVICE_MODEL_NUMBER);

    // Battery Service
    NimBLEService *pBatteryService = pServer->createService(BATTERY_SERVICE_UUID);
    g_battery_level_characteristic = pBatteryService->createCharacteristic(
        BATTERY_LEVEL_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Start services
    pService->start();
    pDeviceInfoService->start();
    pBatteryService->start();

    // --- Advertising ---
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    logger_printf("[BLE] Advertising started with name: %s\n", DEVICE_MODEL_NUMBER);
}

void update_device_status() {
    static firmware_state_t last_notified_state = (firmware_state_t)-1;
    firmware_state_t current_state = get_current_state();

    if (g_is_ble_connected && g_device_status_characteristic && current_state != last_notified_state) {
        uint8_t status_payload[6] = {0};
        
        // Bytes 0-1: Firmware Version
        sscanf(DEVICE_FIRMWARE_REVISION, "%hhu.%hhu", &status_payload[0], &status_payload[1]);

        // Byte 2: Battery Level
        status_payload[2] = 100; // Placeholder for now, will integrate with battery_handler

        // Byte 3: State ID
        status_payload[3] = (uint8_t)current_state;

        // Bytes 4-5: BLE Connection Interval (Placeholder)
        uint16_t conn_interval_ms = 0; // Placeholder
        memcpy(&status_payload[4], &conn_interval_ms, sizeof(conn_interval_ms));

        g_device_status_characteristic->setValue(status_payload, sizeof(status_payload));
        g_device_status_characteristic->notify();
        last_notified_state = current_state;
        logger_printf("[STATE] Notified client of new state: %s\n", get_state_string(current_state));
    }
}

bool is_ble_connected() {
    return g_is_ble_connected;
}

void notify_photo_data(const uint8_t* data, size_t length) {
    if (g_is_ble_connected && g_photo_data_characteristic) {
        g_photo_data_characteristic->setValue(data, length);
        g_photo_data_characteristic->notify();
    }
}

void notify_audio_data(const uint8_t* data, size_t length) {
    if (g_is_ble_connected && g_audio_data_characteristic) {
        g_audio_data_characteristic->setValue(data, length);
        g_audio_data_characteristic->notify();
    }
}
