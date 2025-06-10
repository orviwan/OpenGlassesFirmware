#ifndef AUDIO_PCM_H
#define AUDIO_PCM_H
#include <stdint.h>
#include <stddef.h>
#include "ble_handler.h"

void process_and_send_audio(BLECharacteristic *audio_characteristic);

#endif // AUDIO_PCM_H
