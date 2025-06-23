#ifndef AUDIO_ULAW_H
#define AUDIO_ULAW_H

#include <stdint.h>
#include <stddef.h>
#include "ble_handler.h"

// Only μ-law streaming is supported now

// Function to process a buffer of u-law audio data and send it as μ-law encoded packets
void process_and_send_ulaw_audio(BLECharacteristic *audio_characteristic);

// Function to start the dedicated μ-law audio streaming task
void start_ulaw_streaming_task();

// Function to stop the dedicated μ-law audio streaming task
void stop_ulaw_streaming_task();

#endif // AUDIO_ULAW_H
