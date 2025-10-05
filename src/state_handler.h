#ifndef STATE_HANDLER_H
#define STATE_HANDLER_H

#include <stdint.h>
#include <Arduino.h>

// Define a macro to wrap the state setting function
// This will automatically pass the file and line number
#define set_current_state(newState) _set_current_state(newState, __FILE__, __LINE__)

typedef enum {
    STATE_UNKNOWN = 0,
    STATE_IDLE,
    STATE_BOOTING,
    STATE_CONNECTED_BLE,
    STATE_STREAMING_AUDIO_BLE,
    STATE_TAKING_PHOTO,
    STATE_TRANSFERRING_PHOTO_BLE,
    STATE_WIFI_MODE,
    STATE_STREAMING_AV_WIFI,
    STATE_SLEEPING,
    STATE_ERROR
} firmware_state_t;

void initialize_state_handler();
void _set_current_state(firmware_state_t new_state, const char* file, int line);
firmware_state_t get_current_state();
const char* get_state_string(firmware_state_t state);

#endif // STATE_HANDLER_H
