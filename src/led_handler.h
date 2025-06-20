#ifndef LED_HANDLER_H
#define LED_HANDLER_H

// #include <Adafruit_NeoPixel.h> // No longer used for single-color LED

enum led_status_t {
    LED_STATUS_DISCONNECTED,
    LED_STATUS_CONNECTED,
    LED_STATUS_AUDIO_STREAMING,
    LED_STATUS_PHOTO_CAPTURING,
    LED_STATUS_LOW_POWER, // For indicating CPU is in low-power mode (80 MHz)
    LED_STATUS_OFF
};

void initialize_led();
void set_led_status(led_status_t status);
void handle_led(); // Must be called in the main loop to handle blinking patterns

#endif // LED_HANDLER_H
