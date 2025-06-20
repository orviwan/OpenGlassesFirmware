// Main sketch file for OpenGlass firmware

#include <Arduino.h>
#include <esp_pm.h>

// Include all custom module headers
#include "src/config.h"         // For global constants and configurations
#include "src/ble_handler.h"    // For BLE setup and handling
#include "src/audio_handler.h"  // For microphone and audio processing
#include "src/camera_handler.h" // For camera operations
#include "src/photo_manager.h"  // For photo capture logic and uploading
#include "src/battery_handler.h"// For battery level monitoring
#include "src/led_handler.h"      // For onboard LED control
#include "src/logger.h"         // For thread-safe logging

// Helper to print pretty uptime (hh:mm:ss)
String prettyUptime(unsigned long ms) {
    unsigned long seconds = ms / 1000;
    unsigned int hours = seconds / 3600;
    unsigned int minutes = (seconds % 3600) / 60;
    unsigned int secs = seconds % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hours, minutes, secs);
    return String(buf);
}

// Helper to print ON/OFF for bool
const char* onOff(bool state) {
    return state ? "ON" : "OFF";
}

// Helper to print ACTIVE/IDLE for BLE connection
const char* bleState(bool connected) {
    return connected ? "ACTIVE" : "IDLE";
}

unsigned long g_last_disconnect_time = 0;

/**
 * @brief Arduino setup function. Initializes hardware and software components.
 */
void setup()
{
    Serial.begin(921600);
    initialize_logger(); // Must be called after Serial.begin()
    logger_printf(" \n");
    if (!psramFound()) {
        logger_printf("[PSRAM] ERROR: PSRAM not found! Halting early.\n");
        while(1);
    } else {
        logger_printf("[PSRAM] Total PSRAM: %u bytes\n", ESP.getPsramSize());
        logger_printf("[PSRAM] Free PSRAM before any custom allocations: %u bytes\n", ESP.getFreePsram());
    }

    logger_printf("[SETUP] System starting...\n");

    // Configure and enable power management by default for optimal battery life.
    // This includes Dynamic Frequency Scaling (DFS) and automatic light sleep.
    esp_pm_config_esp32s3_t pm_config = {
        .max_freq_mhz = 240, // Maximum CPU frequency
        .min_freq_mhz = 80,  // Minimum CPU frequency for power saving
        .light_sleep_enable = true // Enable automatic light sleep
    };
    esp_pm_configure(&pm_config);
    logger_printf("[POWER] Dynamic Frequency Scaling and Light Sleep enabled.\n");

    // Initialize modules
    initialize_camera_mutex(); // Initialize the camera mutex
    initialize_led();
    configure_ble();
    initialize_photo_manager(); // Initializes photo buffer and default interval
    initialize_battery_handler(g_battery_level_characteristic); // Pass the BLE characteristic
    start_ulaw_streaming_task(); // Start μ-law audio streaming task
    start_photo_streaming_task(); // Start photo streaming task
    // Set initial battery level after BLE characteristic is available
    update_battery_level(); 
    
    logger_printf("[SETUP] Complete. Entering main loop.\n");
}

/**
 * @brief Arduino main loop function. Handles audio, photo, and battery updates.
 */
void loop()
{
    static led_status_t last_led_status = LED_STATUS_OFF;

    // Must be called on every loop to drive the LED blink patterns
    handle_led();

    if (g_is_ble_connected)
    {
        // When connected, ensure LED shows connected status.
        // This overrides any other state like LOW_POWER.
        if (last_led_status != LED_STATUS_CONNECTED) {
            set_led_status(LED_STATUS_CONNECTED);
            last_led_status = LED_STATUS_CONNECTED;
            g_last_disconnect_time = 0; // Reset disconnect timer
        }

        // Handle battery updates
        if (millis() - g_last_battery_update_ms >= BATTERY_UPDATE_INTERVAL_MS) {
            update_battery_level();
        }
    }
    else // When disconnected
    {
        if (g_last_disconnect_time == 0) {
            g_last_disconnect_time = millis();
        }

        // Check if it's time to go to deep sleep
        if (millis() - g_last_disconnect_time >= 10000) { // 10 seconds
            logger_printf("[POWER] No connection. Entering deep sleep for 10 seconds.\n");
            esp_sleep_enable_timer_wakeup(10 * 1000000); // 10 seconds
            esp_deep_sleep_start();
        }

        // Check CPU frequency and update LED to provide visual feedback on power state.
        led_status_t new_status = (getCpuFrequencyMhz() <= 80) ? LED_STATUS_LOW_POWER : LED_STATUS_DISCONNECTED;
        if (last_led_status != new_status) {
            set_led_status(new_status);
            last_led_status = new_status;
        }
    }

    // The periodic debug log remains disabled to prevent serial comms from blocking light sleep.

    // This non-blocking delay allows the idle task to run, which will trigger
    // light sleep if enabled and conditions are met. The delay is longer when
    // disconnected to further encourage power saving.
    if (g_is_ble_connected) {
        vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS)); // Active delay
    } else {
        vTaskDelay(pdMS_TO_TICKS(500)); // Idle delay
    }
}
