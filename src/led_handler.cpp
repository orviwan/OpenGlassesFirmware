#include "led_handler.h"
#include "logger.h"
#include "config.h"

Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

static led_status_t last_status = LED_STATUS_OFF;

void initialize_led() {
    pixels.begin();
    pixels.setBrightness(20);
    set_led_status(LED_STATUS_DISCONNECTED); // Initial state
}

void set_led_status(led_status_t status) {
    if (status == last_status && status != LED_STATUS_PHOTO_CAPTURING) {
        // Avoid redundant updates except for photo blink
        return;
    }
    last_status = status;
    switch (status) {
        case LED_STATUS_DISCONNECTED:
            pixels.setPixelColor(0, pixels.Color(255, 165, 0)); // Orange
            break;
        case LED_STATUS_CONNECTED:
            pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
            break;
        case LED_STATUS_AUDIO_STREAMING:
            pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
            break;
        case LED_STATUS_PHOTO_CAPTURING:
            // Quick red blink
            pixels.setPixelColor(0, pixels.Color(255, 0, 0));
            pixels.show();
            delay(100);
            set_led_status(LED_STATUS_CONNECTED); 
            return;
        case LED_STATUS_OFF:
            pixels.clear();
            break;
    }
    logger_printf("[LED] Set LED status to %x\n", status);
    pixels.show();
}
