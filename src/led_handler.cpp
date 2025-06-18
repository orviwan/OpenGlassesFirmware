#include "led_handler.h"
#include "config.h"

Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

void initialize_led() {
    pixels.begin();
    pixels.setBrightness(20);
    set_led_status(LED_STATUS_DISCONNECTED); // Initial state
}

void set_led_status(led_status_t status) {
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
            // Revert to connected status, assuming photo can only be taken when connected
            set_led_status(LED_STATUS_CONNECTED); 
            return; // Avoid extra show at the end
        case LED_STATUS_OFF:
            pixels.clear();
            break;
    }
    pixels.show();
}
