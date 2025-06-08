#include "audio_handler.h"
#include "config.h" // For audio constants like SAMPLE_RATE, etc.
#include <Arduino.h> // For Serial
#include <I2S.h>     // For I2S functionality

// Static (file-scope) buffers and their sizes
static size_t i2s_recording_buffer_size = FRAME_SIZE * (SAMPLE_BITS / 8);
static size_t audio_packet_buffer_size = (FRAME_SIZE * (SAMPLE_BITS / 8) / 2) + AUDIO_FRAME_HEADER_LEN;

uint8_t *s_i2s_recording_buffer = nullptr;
uint8_t *s_audio_packet_buffer = nullptr;

// Global audio frame count, defined here
uint16_t g_audio_frame_count = 0;

void configure_microphone()
{
    // start I2S at SAMPLE_RATE with SAMPLE_BITS per sample
    I2S.setAllPins(-1, 42, 41, -1, -1); // Pins for XIAO ESP32S3 Sense microphone
    if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, SAMPLE_BITS))
    {
        Serial.println("[AUDIO] ERROR: Failed to initialize I2S! Halting.");
        while (1); 
    }

    Serial.printf("[MEM] Free PSRAM before I2S recording buffer alloc: %u bytes\n", ESP.getFreePsram());
    s_i2s_recording_buffer = (uint8_t *)ps_calloc(i2s_recording_buffer_size, sizeof(uint8_t));
    if (!s_i2s_recording_buffer)
    {
        Serial.println("[MEM] ERROR: Failed to allocate I2S recording buffer! Halting.");
        while (1);
    }

    Serial.printf("[MEM] Free PSRAM before audio packet buffer alloc: %u bytes\n", ESP.getFreePsram());
    s_audio_packet_buffer = (uint8_t *)ps_calloc(audio_packet_buffer_size, sizeof(uint8_t));
    if (!s_audio_packet_buffer)
    {
        Serial.println("[MEM] ERROR: Failed to allocate audio packet buffer! Halting.");
        while (1);
    }
    Serial.println("[AUDIO] Microphone configured and buffers allocated.");
}

size_t read_microphone_data(uint8_t *buffer, size_t buffer_size)
{
    size_t bytes_recorded = 0;
    if (buffer) {
        esp_i2s::i2s_read(esp_i2s::I2S_NUM_0, buffer, buffer_size, &bytes_recorded, portMAX_DELAY);
    }
    return bytes_recorded;
}

void process_and_send_audio(BLECharacteristic *audio_characteristic) {
    if (!s_i2s_recording_buffer || !s_audio_packet_buffer || !audio_characteristic) return;

    size_t bytes_recorded = read_microphone_data(s_i2s_recording_buffer, i2s_recording_buffer_size);
    if (bytes_recorded > 0) {
        size_t samples_to_process = (bytes_recorded / (SAMPLE_BITS / 8)) / 2; // Downsample by 2
        for (size_t i = 0; i < samples_to_process; i++) {
            int16_t sample = ((int16_t *)s_i2s_recording_buffer)[i * 2];
            sample = sample << VOLUME_GAIN;
            s_audio_packet_buffer[i * 2 + AUDIO_FRAME_HEADER_LEN] = sample & 0xFF;
            s_audio_packet_buffer[i * 2 + AUDIO_FRAME_HEADER_LEN + 1] = (sample >> 8) & 0xFF;
        }
        s_audio_packet_buffer[0] = g_audio_frame_count & 0xFF;
        s_audio_packet_buffer[1] = (g_audio_frame_count >> 8) & 0xFF;
        s_audio_packet_buffer[2] = 0; // Reserved
        audio_characteristic->setValue(s_audio_packet_buffer, (samples_to_process * 2) + AUDIO_FRAME_HEADER_LEN);
        audio_characteristic->notify();
        g_audio_frame_count++;
    }
}