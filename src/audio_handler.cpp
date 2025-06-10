#include "audio_handler.h"
#include "config.h"
#include <Arduino.h>
#include <I2S.h>

size_t i2s_recording_buffer_size = FRAME_SIZE * (SAMPLE_BITS / 8);
size_t audio_packet_buffer_size = (FRAME_SIZE * (SAMPLE_BITS / 8)) + AUDIO_FRAME_HEADER_LEN;

uint8_t *s_i2s_recording_buffer = nullptr;
uint8_t *s_audio_packet_buffer = nullptr;
uint16_t g_audio_frame_count = 0;

void configure_microphone()
{
    I2S.setAllPins(-1, 42, 41, -1, -1);
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