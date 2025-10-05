#include "led_handler.h"
#include "logger.h"
#include "config.h"
#include "state_handler.h" // For firmware_state_t
#include <Arduino.h>

void initialize_led() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    logger_printf("[LED] Initialized on pin %d.\n", PIN_LED);
}

void handle_led() {
    static unsigned long last_blink_time = 0;
    static bool led_state = false;
    unsigned long current_time = millis();
    firmware_state_t current_state = get_current_state();

    switch (current_state) {
        case STATE_IDLE: // Slow blink
            if (current_time - last_blink_time >= 500) {
                last_blink_time = current_time;
                led_state = !led_state;
                digitalWrite(PIN_LED, led_state);
            }
            break;

        case STATE_CONNECTED_BLE: // Solid ON
            digitalWrite(PIN_LED, HIGH);
            break;

        case STATE_STREAMING_AUDIO_BLE:
        case STATE_TAKING_PHOTO:
        case STATE_SENDING_PHOTO:
        case STATE_STREAMING_AV_WIFI: // Fast blink
            if (current_time - last_blink_time >= 250) {
                last_blink_time = current_time;
                led_state = !led_state;
                digitalWrite(PIN_LED, led_state);
            }
            break;

        case STATE_WIFI_ACTIVATING: // Rapid pulse
            if (current_time - last_blink_time >= 100) {
                last_blink_time = current_time;
                led_state = !led_state;
                digitalWrite(PIN_LED, led_state);
            }
            break;

        case STATE_ERROR: // Very fast blink
            if (current_time - last_blink_time >= 50) {
                last_blink_time = current_time;
                led_state = !led_state;
                digitalWrite(PIN_LED, led_state);
            }
            break;

        default:
            digitalWrite(PIN_LED, LOW); // Off for any other state
            break;
    }
}
