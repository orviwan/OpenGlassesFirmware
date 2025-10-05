#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <NimBLEDevice.h>

// Extern declarations for global BLE variables
extern BLECharacteristic *g_device_status_characteristic;
extern BLECharacteristic *g_battery_level_characteristic;
extern BLECharacteristic *g_photo_data_characteristic;
extern volatile bool g_is_ble_connected;
extern SemaphoreHandle_t g_photo_ack_semaphore;

void configure_ble();
void update_device_status();
bool is_ble_connected();
void notify_photo_data(const uint8_t* data, size_t length);
void notify_audio_data(const uint8_t* data, size_t length);

#endif // BLE_HANDLER_H