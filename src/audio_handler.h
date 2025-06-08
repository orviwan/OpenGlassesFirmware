#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <stddef.h> // For size_t
#include <stdint.h> // For uint8_t, uint16_t

// Forward declaration for BLECharacteristic to avoid including full BLE headers here
// if they are not strictly needed by the interface.
// However, process_and_send_audio needs it.
#include <BLECharacteristic.h>

// External declaration for global audio frame count
extern uint16_t g_audio_frame_count;

// Buffers used by audio processing - can be extern if needed by other modules,
// or kept static within audio_handler.cpp if not. For now, keep them internal.
extern uint8_t *s_i2s_recording_buffer; 
extern uint8_t *s_audio_packet_buffer;

void configure_microphone();
size_t read_microphone_data(uint8_t *buffer, size_t buffer_size); // More generic read
void process_and_send_audio(BLECharacteristic *audio_characteristic);

#endif // AUDIO_HANDLER_H