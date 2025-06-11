#ifndef AUDIO_ULAW_H
#define AUDIO_ULAW_H
#include <stdint.h>
#include <stddef.h>
#include "ble_handler.h"

void process_and_send_ulaw_audio(BLECharacteristic *audio_characteristic);

#endif // AUDIO_ULAW_H
