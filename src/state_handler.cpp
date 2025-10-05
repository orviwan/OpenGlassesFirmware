#include "state_handler.h"
#include "logger.h"
#include "ble_handler.h"

static firmware_state_t g_current_state = STATE_UNKNOWN;

void initialize_state_handler() {
    _set_current_state(STATE_IDLE, __FILE__, __LINE__);
}

void _set_current_state(firmware_state_t new_state, const char* file, int line) {
    if (g_current_state == new_state) {
        return; // No change
    }

    logger_printf("[STATE] Transitioning from %s to %s (called from %s:%d)", 
                  get_state_string(g_current_state), 
                  get_state_string(new_state),
                  file,
                  line);

    g_current_state = new_state;
    update_device_status(); // Notify BLE clients
}

firmware_state_t get_current_state() {
    return g_current_state;
}

const char* get_state_string(firmware_state_t state) {
    switch (state) {
        case STATE_UNKNOWN: return "UNKNOWN";
        case STATE_IDLE: return "IDLE";
        case STATE_BOOTING: return "BOOTING";
        case STATE_CONNECTED_BLE: return "CONNECTED_BLE";
        case STATE_STREAMING_AUDIO_BLE: return "STREAMING_AUDIO_BLE";
        case STATE_TAKING_PHOTO: return "TAKING_PHOTO";
        case STATE_TRANSFERRING_PHOTO_BLE: return "TRANSFERRING_PHOTO_BLE";
        case STATE_WIFI_MODE: return "WIFI_MODE";
        case STATE_STREAMING_AV_WIFI: return "STREAMING_AV_WIFI";
        case STATE_SLEEPING: return "SLEEPING";
        case STATE_ERROR: return "ERROR";
        default: return "UNDEFINED";
    }
}
