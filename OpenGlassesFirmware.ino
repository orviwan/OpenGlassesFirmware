// Main sketch file for OpenGlass firmware

#include <Arduino.h>

// Include all custom module headers
#include "src/config.h"         // For global constants and configurations
#include "src/ble_handler.h"    // For BLE setup and handling
#include "src/audio_handler.h"  // For microphone and audio processing
#include "src/camera_handler.h" // For camera operations
#include "src/photo_manager.h"  // For photo capture logic and uploading
#include "src/battery_handler.h"// For battery level monitoring

/**
 * @brief Arduino setup function. Initializes hardware and software components.
 */
void setup()
{
    Serial.begin(921600);
    Serial.println(" ");
    if (!psramFound()) {
        Serial.println("[PSRAM] ERROR: PSRAM not found! Halting early.");
        while(1);
    } else {
        Serial.printf("\r\n[PSRAM] Total PSRAM: %u bytes\n", ESP.getPsramSize());
        Serial.printf("\r\n[PSRAM] Free PSRAM before any custom allocations: %u bytes\n", ESP.getFreePsram());
    }

    Serial.println("[SETUP] System starting...");

    // Initialize modules
    configure_ble();
    initialize_photo_manager(); // Initializes photo buffer and default interval
    initialize_battery_handler(g_battery_level_characteristic); // Pass the BLE characteristic
    start_ulaw_streaming_task(); // Start μ-law audio streaming task
    start_photo_streaming_task(); // Start photo streaming task
    // Set initial battery level after BLE characteristic is available
    update_battery_level(); 
    
    Serial.println("[SETUP] Complete. Entering main loop.");
}

/**
 * @brief Arduino main loop function. Handles audio, photo, and battery updates.
 */
void loop()
{
    unsigned long current_time_ms = millis();

    if (g_is_ble_connected)
    {
        // Photo streaming is handled by its dedicated FreeRTOS task (photo_streaming_task)

        // Handle battery updates
        if (current_time_ms - g_last_battery_update_ms >= BATTERY_UPDATE_INTERVAL_MS) {
            update_battery_level();
        }
    }
    delay(LOOP_DELAY_MS); // Small delay to yield to other tasks
}
