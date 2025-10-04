#include "state_handler.h"
#include "logger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static firmware_state_t g_current_state = STATE_IDLE;
static const char* state_strings[] = {
    "IDLE",
    "CONNECTED_BLE",
    "STREAMING_AUDIO_BLE",
    "TRANSFERRING_PHOTO_BLE",
    "WIFI_ACTIVATING",
    "STREAMING_AV_WIFI",
    "ERROR"
};

void initialize_state_machine() {
    g_current_state = STATE_IDLE;
    logger_printf("[STATE] State machine initialized. Current state: %s\n", get_state_string(g_current_state));
}

firmware_state_t get_current_state() {
    return g_current_state;
}

void set_current_state(firmware_state_t new_state) {
    if (g_current_state != new_state) {
        logger_printf("[STATE] Transitioning from %s to %s\n", get_state_string(g_current_state), get_state_string(new_state));
        g_current_state = new_state;
        // Here you would also notify the client via the Device Status characteristic
    }
}

const char* get_state_string(firmware_state_t state) {
    if (state >= STATE_IDLE && state <= STATE_ERROR) {
        return state_strings[state];
    }
    return "UNKNOWN";
}
