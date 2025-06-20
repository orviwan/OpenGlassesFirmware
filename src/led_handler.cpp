#include "led_handler.h"
#include "logger.h"
#include "config.h"
#include "ble_handler.h" // For g_is_ble_connected
#include <Arduino.h>     // For pinMode, digitalWrite, millis

static led_status_t g_current_led_status = LED_STATUS_OFF;

void initialize_led() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW); // Start with LED off
    set_led_status(LED_STATUS_DISCONNECTED); // Set initial logical state
    logger_printf("[LED] Single-color LED handler initialized on pin %d.\n", PIN_LED);
}

void set_led_status(led_status_t status) {
    // Photo capturing is a momentary flash, handled in handle_led directly
    if (status == LED_STATUS_PHOTO_CAPTURING) {
        // Immediately flash the LED
        digitalWrite(PIN_LED, HIGH);
        delay(50); // Brief blocking delay for a visible flash
        digitalWrite(PIN_LED, LOW);
        // The main handle_led loop will then resume the previous state's pattern
        return;
    }
    g_current_led_status = status;
}

void handle_led() {
    static unsigned long last_blink_time = 0;
    static bool led_state = false;
    unsigned long current_time = millis();

    switch (g_current_led_status) {
        case LED_STATUS_CONNECTED:
            digitalWrite(PIN_LED, HIGH); // Solid ON
            break;

        case LED_STATUS_DISCONNECTED: // Slow blink (1s period)
            if (current_time - last_blink_time >= 500) {
                last_blink_time = current_time;
                led_state = !led_state;
                digitalWrite(PIN_LED, led_state);
            }
            break;

        case LED_STATUS_AUDIO_STREAMING: // Fast blink (0.5s period)
            if (current_time - last_blink_time >= 250) {
                last_blink_time = current_time;
                led_state = !led_state;
                digitalWrite(PIN_LED, led_state);
            }
            break;

        case LED_STATUS_LOW_POWER: // Slow "heartbeat" pulse (50ms pulse every 2s)
            if (!led_state && (current_time - last_blink_time >= 2000)) {
                last_blink_time = current_time;
                led_state = true;
                digitalWrite(PIN_LED, HIGH);
            } else if (led_state && (current_time - last_blink_time >= 50)) {
                led_state = false;
                digitalWrite(PIN_LED, LOW);
            }
            break;

        case LED_STATUS_OFF:
            digitalWrite(PIN_LED, LOW); // Solid OFF
            break;

        default:
            break; // Should not happen
    }
}
