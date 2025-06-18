#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include <stdint.h>

void initialize_led();
void set_led_color(uint8_t r, uint8_t g, uint8_t b);
void indicate_photo_capture();
void set_led_state_connected();
void set_led_state_disconnected();
void set_led_state_audio(bool is_streaming);

#endif // LED_HANDLER_H
