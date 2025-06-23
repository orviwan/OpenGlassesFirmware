#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEDescriptor.h>

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
    void onMtuChanged(BLEServer *pServer, void *param); // Use void* to avoid header dependency
};

// BLE Characteristic Event Callbacks for Photo Control
class PhotoControlCallback : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic) override;
};

// Callback for photo data descriptor (for subscription management)
class PhotoDescriptorCallback : public BLEDescriptorCallbacks {
    void onWrite(BLEDescriptor* pDescriptor) override;
};

// Callback for audio data descriptor (for subscription management)
class AudioDescriptorCallback : public BLEDescriptorCallbacks {
    void onWrite(BLEDescriptor* pDescriptor) override;
};

void configure_ble();

// Forward declaration from photo_manager.h, used by PhotoControlCallback
void handle_photo_control(int8_t control_value);

void start_photo_streaming_task();

#endif // BLE_HANDLER_H