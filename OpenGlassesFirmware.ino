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
    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 240, // Maximum CPU frequency
        .min_freq_mhz = 80,  // Minimum CPU frequency for power saving
        .light_sleep_enable = true // Enable automatic light sleep
    };
    esp_pm_configure(&pm_config);
    logger_printf("[POWER] Dynamic Frequency Scaling and Light Sleep enabled.\n");

    // Initialize modules
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
    unsigned long current_time_ms = millis();
    static unsigned long last_debug_log_ms = 0;

    if (g_is_ble_connected)
    {
        // Photo streaming is handled by its dedicated FreeRTOS task (photo_streaming_task)

        // Handle battery updates
        if (current_time_ms - g_last_battery_update_ms >= BATTERY_UPDATE_INTERVAL_MS) {
            update_battery_level();
        }
    }

    // Periodic debug log every 10 seconds
    if (current_time_ms - last_debug_log_ms >= 10000) {
        last_debug_log_ms = current_time_ms;
        float heap_mb = ESP.getFreeHeap() / (1024.0 * 1024.0);
        float psram_mb = ESP.getFreePsram() / (1024.0 * 1024.0);
        int cpu_freq = getCpuFrequencyMhz();
        logger_printf("[DEBUG][LOOP] Uptime: %s, BLE: %s, Camera: %s, Microphone: %s, CPU: %d MHz, FreeHeap: %.2f MB, FreePSRAM: %.2f MB", 
            prettyUptime(current_time_ms).c_str(), bleState(g_is_ble_connected), onOff(is_camera_initialized()), onOff(is_microphone_initialized()), cpu_freq, heap_mb, psram_mb);
    }

    // When disconnected, the device just advertises.
    // When connected, other tasks handle streaming.
    // This non-blocking delay allows the idle task to run, which will trigger
    // light sleep if enabled and conditions are met, saving power.
    vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
}
