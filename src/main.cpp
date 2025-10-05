#include <Arduino.h>
#include <esp_pm.h>

// Include all custom module headers
#include "config.h"
#include "state_handler.h"
#include "ble_handler.h"
#include "battery_handler.h"
#include "led_handler.h"
#include "logger.h"
#include "camera_handler.h"
#include "photo_handler.h"
#include "audio_streamer.h"

// Helper to print pretty uptime (hh:mm:ss)
String prettyUptime(unsigned long ms)
{
    unsigned long seconds = ms / 1000;
    unsigned int hours = seconds / 3600;
    unsigned int minutes = (seconds % 3600) / 60;
    unsigned int secs = seconds % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hours, minutes, secs);
    return String(buf);
}

void setup()
{
    Serial.begin(115200);
    initialize_logger();
    logger_printf(" \n");

    if (!psramFound()) {
        logger_printf("[PSRAM] ERROR: PSRAM not found! Halting.\n");
        while (1);
    } else {
        logger_printf("[PSRAM] Total: %u bytes, Free: %u bytes\n", ESP.getPsramSize(), ESP.getFreePsram());
    }

    logger_printf("[SETUP] System starting...\n");

    initialize_state_machine();
    initialize_led();
    initialize_camera_mutex_and_task();
    configure_camera(); // Explicitly configure camera before warmup
    warm_up_camera();   // Warm up the camera sensor
    initialize_photo_handler();
    configure_ble();
    initialize_battery_handler(g_battery_level_characteristic);

    logger_printf("[SETUP] Complete. Entering main loop.\n");
}

void loop()
{
    // Handle LED patterns
    handle_led();

    // Update battery level periodically
    if (millis() - g_last_battery_update_ms >= BATTERY_UPDATE_INTERVAL_MS) {
        update_battery_level();
    }

    // Update device status characteristic if state has changed
    update_device_status();

    // Main loop delay
    vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
}
