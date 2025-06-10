#include "audio_opus.h"
#include "audio_handler.h"
#include "config.h"
#include <Arduino.h>
#include "opus.h"

extern uint8_t *s_i2s_recording_buffer;
extern BLECharacteristic* g_opus_audio_characteristic;
OpusEncoder* opus_encoder = nullptr;

static uint8_t opus_encoded_buffer[256]; // Adjust size as needed

size_t read_microphone_data(uint8_t *buffer, size_t buffer_size);

void process_and_send_opus_audio() {
    // Fill s_i2s_recording_buffer with PCM data
    size_t bytes_recorded = read_microphone_data(s_i2s_recording_buffer, FRAME_SIZE * (SAMPLE_BITS / 8));
    if (bytes_recorded > 0) {
        int opus_bytes = opus_encode(
            opus_encoder,
            (const opus_int16*)s_i2s_recording_buffer,
            FRAME_SIZE, // Number of samples per channel
            opus_encoded_buffer,
            sizeof(opus_encoded_buffer)
        );
        if (opus_bytes > 0 && g_opus_audio_characteristic) {
            g_opus_audio_characteristic->setValue(opus_encoded_buffer, opus_bytes);
            g_opus_audio_characteristic->notify();
        } else if (opus_bytes <= 0) {
            Serial.println("[AUDIO] Opus encoding failed");
        }
    }
}
