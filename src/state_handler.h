#ifndef STATE_HANDLER_H
#define STATE_HANDLER_H

#include <stdint.h>

// Firmware states as defined in the blueprint
typedef enum {
    STATE_IDLE = 0,
    STATE_CONNECTED_BLE,
    STATE_STREAMING_AUDIO_BLE,
    STATE_TRANSFERRING_PHOTO_BLE,
    STATE_WIFI_ACTIVATING,
    STATE_STREAMING_AV_WIFI,
    STATE_ERROR
} firmware_state_t;

void initialize_state_machine();
firmware_state_t get_current_state();
void set_current_state(firmware_state_t new_state);
const char* get_state_string(firmware_state_t state);

#endif // STATE_HANDLER_H
