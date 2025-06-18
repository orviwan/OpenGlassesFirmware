#include "led_handler.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

// Define the GPIO pin for the onboard LED
#define ONBOARD_LED_PIN 48

// Create a NeoPixel object
Adafruit_NeoPixel onboard_led(1, ONBOARD_LED_PIN, NEO_GRB + NEO_KHZ800);

// Variable to store the last color before a temporary indication
static uint32_t last_color = onboard_led.Color(255, 165, 0); // Default to orange
static bool is_audio_streaming = false;

void initialize_led() {
    onboard_led.begin();
    onboard_led.setBrightness(20); // Set a low brightness (0-255)
    set_led_state_disconnected(); // Initial state is disconnected (orange)
}

void set_led_color(uint8_t r, uint8_t g, uint8_t b) {
    last_color = onboard_led.Color(r, g, b);
    onboard_led.setPixelColor(0, last_color);
    onboard_led.show();
}

void indicate_photo_capture() {
    onboard_led.setPixelColor(0, onboard_led.Color(255, 0, 0)); // Red
    onboard_led.show();
    vTaskDelay(pdMS_TO_TICKS(100)); // Keep it red for 100ms
    onboard_led.setPixelColor(0, last_color); // Revert to the last color
    onboard_led.show();
}

void set_led_state_connected() {
    if (!is_audio_streaming) {
        set_led_color(0, 255, 0); // Green
    }
}

void set_led_state_disconnected() {
    is_audio_streaming = false;
    set_led_color(255, 165, 0); // Orange
}

void set_led_state_audio(bool is_streaming) {
    is_audio_streaming = is_streaming;
    if (is_streaming) {
        set_led_color(0, 0, 255); // Blue
    } else {
        set_led_state_connected(); // Revert to green if connected
    }
}
