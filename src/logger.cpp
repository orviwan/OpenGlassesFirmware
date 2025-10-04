#include "logger.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Mutex to protect serial port access
static SemaphoreHandle_t s_log_mutex = NULL;

// Flag to avoid printing from multiple tasks at once
static volatile bool s_is_logging = false;

void initialize_logger() {
    if (s_log_mutex == NULL) {
        s_log_mutex = xSemaphoreCreateMutex();
    }
}

void logger_printf(const char *format, ...) {
    if (s_log_mutex == NULL) return; // Not initialized

    // Attempt to take the mutex
    if (xSemaphoreTake(s_log_mutex, portMAX_DELAY) == pdTRUE) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        // Format the string
        int len = vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        // Ensure null termination
        if (len >= 0 && len < sizeof(buffer)) {
            // Print the formatted string
            Serial.println(buffer);
        } else {
            // Handle potential overflow or encoding error
            Serial.println("[LOG] Error: Log message too long or invalid format.");
        }

        // Release the mutex
        xSemaphoreGive(s_log_mutex);
    }
}

void log_message(const char * format, ...) {
    logger_printf(format);
}
