#include "audio_pcm.h"
#include "audio_handler.h"
#include "config.h"
#include <Arduino.h>
#include <I2S.h>

extern uint8_t *s_i2s_recording_buffer;
extern uint8_t *s_audio_packet_buffer;
extern uint16_t g_audio_frame_count;

size_t read_microphone_data(uint8_t *buffer, size_t buffer_size);

void process_and_send_audio(BLECharacteristic *audio_characteristic) {
    if (!s_i2s_recording_buffer || !s_audio_packet_buffer || !audio_characteristic) return;

    size_t bytes_recorded = read_microphone_data(s_i2s_recording_buffer, FRAME_SIZE * (SAMPLE_BITS / 8));
    if (bytes_recorded > 0) {
        size_t samples_to_process = bytes_recorded / (SAMPLE_BITS / 8);
        int16_t* input_samples = (int16_t*)s_i2s_recording_buffer;
        for (size_t i = 0; i < samples_to_process; i++) {
            int16_t sample = input_samples[i];
            sample = sample << VOLUME_GAIN;
            s_audio_packet_buffer[i * 2 + AUDIO_FRAME_HEADER_LEN] = sample & 0xFF;
            s_audio_packet_buffer[i * 2 + AUDIO_FRAME_HEADER_LEN + 1] = (sample >> 8) & 0xFF;
        }
        s_audio_packet_buffer[0] = g_audio_frame_count & 0xFF;
        s_audio_packet_buffer[1] = (g_audio_frame_count >> 8) & 0xFF;
        s_audio_packet_buffer[2] = 0x00;
        audio_characteristic->setValue(s_audio_packet_buffer, (samples_to_process * (SAMPLE_BITS / 8)) + AUDIO_FRAME_HEADER_LEN);
        audio_characteristic->notify();
        g_audio_frame_count++;
    }
}
