#include "audio_handler.h"
#include "config.h"
#include "logger.h"
#include <Arduino.h>
#include <I2S.h> // Use the Arduino I2S library

// Buffer for raw microphone data
size_t i2s_recording_buffer_size = FRAME_SIZE * (SAMPLE_BITS / 8);
uint8_t *s_i2s_recording_buffer = nullptr;

// Flag to track if the driver is active
static bool i2s_driver_installed = false;

void configure_microphone()
{
    logger_printf("\n[MIC] Configuring microphone using Arduino I2S library...\n");

    // Allocate the recording buffer if it hasn't been already
    if (s_i2s_recording_buffer == nullptr) {
        s_i2s_recording_buffer = (uint8_t *)ps_calloc(i2s_recording_buffer_size, sizeof(uint8_t));
        if (!s_i2s_recording_buffer) {
            logger_printf("[MIC] ERROR: Failed to allocate recording buffer!\n");
            return;
        }
        logger_printf("[MIC] Recording buffer allocated (%zu bytes).\n", i2s_recording_buffer_size);
    }

    // Use the simpler Arduino I2S configuration from the old code
    // Note: For PDM, the pin mapping can be tricky.
    // The old code used I2S.setAllPins(-1, 42, 41, -1, -1);
    // This seems to map SCK to 42 and SD to 41. We will use the pins from config.h
    I2S.setAllPins(-1, I2S_SCK_PIN, I2S_SD_PIN, -1, -1);

    if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, SAMPLE_BITS))
    {
        logger_printf("[MIC] ERROR: Failed to initialize I2S!\n");
        return;
    }

    i2s_driver_installed = true;
    logger_printf("[MIC] Arduino I2S library configured successfully.\n");
}

size_t read_microphone_data(uint8_t *buffer, size_t buffer_size)
{
    if (buffer && i2s_driver_installed)
    {
        // Use I2S.read() to get data
        return I2S.read(buffer, buffer_size);
    }
    return 0;
}

void deinit_microphone()
{
    if (i2s_driver_installed)
    {
        I2S.end();
        logger_printf("[MIC] I2S driver uninstalled successfully.\n");
        i2s_driver_installed = false;

        // Free the recording buffer
        if (s_i2s_recording_buffer != nullptr) {
            free(s_i2s_recording_buffer);
            s_i2s_recording_buffer = nullptr;
            logger_printf("[MIC] Recording buffer freed.\n");
        }
    }
    else
    {
        logger_printf("[MIC] I2S driver was not installed, skipping uninstall.\n");
    }
}

bool is_microphone_initialized()
{
    return i2s_driver_installed;
}