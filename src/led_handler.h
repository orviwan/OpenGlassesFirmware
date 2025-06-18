#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include <Adafruit_NeoPixel.h>

enum led_status_t {
    LED_STATUS_DISCONNECTED,
    LED_STATUS_CONNECTED,
    LED_STATUS_AUDIO_STREAMING,
    LED_STATUS_PHOTO_CAPTURING,
    LED_STATUS_OFF
};

void initialize_led();
void set_led_status(led_status_t status);

#endif // LED_HANDLER_H
