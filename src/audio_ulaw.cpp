#include "config.h"
#include "audio_ulaw.h"
#include "audio_handler.h"
#include "ble_handler.h" // For g_audio_data_characteristic
#include "logger.h"
#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <BLE2902.h> // For BLE2902 descriptor

// Task handle for the audio streaming task
static TaskHandle_t ulaw_streaming_task_handle = nullptr;

// Forward declaration
void process_and_send_ulaw_audio(BLECharacteristic *audio_characteristic);

// The actual FreeRTOS task for streaming audio
void ulaw_streaming_task(void *pvParameters) {
    logger_printf("[TASK] Audio streaming task is running.\n");

    // Initialize the microphone when the task starts running
    configure_microphone();

    while (true) {
        // This task will be suspended when no client is subscribed, so we just
        // need to read and send data when it's running.
        if (g_is_ble_connected && g_audio_data_characteristic) {
            process_and_send_ulaw_audio(g_audio_data_characteristic);
        } else {
            // If not connected, delay to prevent busy-waiting. The task will be
            // suspended on disconnect anyway, but this is a safeguard.
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        // Delay to control the streaming rate
        vTaskDelay(pdMS_TO_TICKS(ULAW_TASK_DELAY_MS));
    }
}

// --- u-law encoding implementation (G.711 standard) ---
// This implementation is based on the G.711 standard for clarity and correctness.
static uint8_t linear_to_ulaw(int16_t pcm_val) {
    const int16_t ULAW_MAX = 8159; // Max value for u-law (after bias)
    const int16_t ULAW_BIAS = 33;
    uint8_t sign, exponent, mantissa, ulaw_byte;

    // Get the sign and magnitude of the pcm value
    if (pcm_val < 0) {
        pcm_val = -pcm_val;
        sign = 0x80;
    } else {
        sign = 0x00;
    }

    // Add the bias
    pcm_val += ULAW_BIAS;

    // Clip the value
    if (pcm_val > ULAW_MAX) {
        pcm_val = ULAW_MAX;
    }

    // Find the segment (exponent)
    if (pcm_val >= 4096) exponent = 7;
    else if (pcm_val >= 2048) exponent = 6;
    else if (pcm_val >= 1024) exponent = 5;
    else if (pcm_val >= 512) exponent = 4;
    else if (pcm_val >= 256) exponent = 3;
    else if (pcm_val >= 128) exponent = 2;
    else if (pcm_val >= 64) exponent = 1;
    else exponent = 0;

    // The mantissa is the 4 bits after the most significant bit
    mantissa = (pcm_val >> (exponent + 3)) & 0x0F;

    // Assemble the u-law byte
    ulaw_byte = sign | (exponent << 4) | mantissa;

    // Invert all bits as per the standard
    return ~ulaw_byte;
}

// This function reads raw PCM data, encodes it to u-law, and sends it over BLE.
void process_and_send_ulaw_audio(BLECharacteristic *audio_characteristic) {
    extern uint8_t *s_i2s_recording_buffer; // Buffer for PCM data
    const size_t pcm_samples_to_read = FRAME_SIZE;
    const size_t pcm_buffer_size = pcm_samples_to_read * sizeof(int16_t);

    // Read a chunk of PCM data from the I2S microphone
    size_t bytes_recorded = read_microphone_data(s_i2s_recording_buffer, pcm_buffer_size);

    if (bytes_recorded > 0 && audio_characteristic) {
        // The u-law data will be half the size of the PCM data (8-bit vs 16-bit).
        size_t ulaw_buffer_size = bytes_recorded / 2;
        uint8_t* ulaw_buffer = (uint8_t*)malloc(ulaw_buffer_size);
        if (!ulaw_buffer) {
            logger_printf("[ERROR] Failed to allocate u-law buffer!\n");
            return;
        }

        // Cast the PCM buffer to int16_t* for easier access
        int16_t* pcm_samples = (int16_t*)s_i2s_recording_buffer;
        size_t num_samples = bytes_recorded / sizeof(int16_t);

        // Encode each PCM sample to u-law
        for (size_t i = 0; i < num_samples; i++) {
            ulaw_buffer[i] = linear_to_ulaw(pcm_samples[i]);
        }

        // --- Chunk the data before sending to respect MTU size ---
        size_t bytes_sent = 0;
        while (bytes_sent < ulaw_buffer_size) {
            size_t chunk_size = ulaw_buffer_size - bytes_sent;
            if (chunk_size > AUDIO_BLE_PACKET_SIZE) {
                chunk_size = AUDIO_BLE_PACKET_SIZE;
            }

            audio_characteristic->setValue(ulaw_buffer + bytes_sent, chunk_size);
            audio_characteristic->notify();

            bytes_sent += chunk_size;
            
            // A small delay between chunks can improve reliability for some clients
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        // Free the temporary u-law buffer
        free(ulaw_buffer);

    } else if (bytes_recorded == 0) {
        // This can be noisy, so it's commented out.
        // logger_printf("[AUDIO] WARN: read_microphone_data() returned 0 bytes.\n");
    }
}

void start_ulaw_streaming_task() {
    if (ulaw_streaming_task_handle == nullptr) {
        logger_printf("[TASK] Creating audio streaming task.\n");
        xTaskCreatePinnedToCore(
            ulaw_streaming_task,          // Task function
            "uLawStreamer",               // Name of the task
            4096,                         // Stack size in words
            NULL,                         // Task input parameter
            2,                            // Priority of the task
            &ulaw_streaming_task_handle,  // Task handle
            1                             // Core where the task should run
        );
    } else {
        logger_printf("[TASK] Resuming audio streaming task.\n");
        // Re-initialize the microphone in case it was de-initialized
        configure_microphone();
        vTaskResume(ulaw_streaming_task_handle);
    }
}

void stop_ulaw_streaming_task() {
    if (ulaw_streaming_task_handle != nullptr) {
        logger_printf("[TASK] Suspending audio streaming task.\n");
        vTaskSuspend(ulaw_streaming_task_handle);
        // De-initialize the microphone to save power immediately
        deinit_microphone();
    }
}
