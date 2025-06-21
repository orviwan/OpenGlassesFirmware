#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// Extern declarations for global BLE variables
extern BLECharacteristic *g_photo_data_characteristic;
extern BLECharacteristic *g_photo_control_characteristic;
extern BLECharacteristic *g_audio_data_characteristic;
extern BLECharacteristic *g_battery_level_characteristic;

extern volatile bool g_is_ble_connected;
extern int g_photo_chunk_payload_size;

// BLE Server Event Callbacks
class ServerHandler : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) override;
    void onDisconnect(BLEServer *pServer) override;
    void onMtuChanged(BLEServer *pServer, esp_ble_gatts_cb_param_t *param);
};

// BLE Characteristic Event Callbacks for Photo Control
class PhotoControlCallback : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic) override;
};

void configure_ble();

// Forward declaration from photo_manager.h, used by PhotoControlCallback
void handle_photo_control(int8_t control_value);

void start_ulaw_streaming_task();
void start_photo_streaming_task();

#endif // BLE_HANDLER_H