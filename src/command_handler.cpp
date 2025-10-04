#include "command_handler.h"
#include "audio_streamer.h"
#include "photo_handler.h"
#include "logger.h"
#include "state_handler.h"

void handle_device_command(const std::string& value) {
    if (value.length() == 0) {
        logger_printf("[CMD] Error: Received empty command.\n");
        return;
    }

    uint8_t command_id = value[0];
    logger_printf("[CMD] Received Command ID: 0x%02X\n", command_id);

    switch (command_id) {
        case 0x01: // Take Photo
            logger_printf("[CMD] Command: Take Photo received\n");
            take_and_send_photo();
            break;
        case 0x02: // Start Interval Photo
            logger_printf("[CMD] Command: Start Interval Photo received\n");
            if (value.length() > 1) {
                uint32_t interval_ms = (uint32_t)value[1] * 1000;
                start_interval_photo(interval_ms);
            } else {
                logger_printf("[CMD] Error: Interval not provided for interval photo command.\n");
            }
            break;
        case 0x03: // Stop Interval Photo
            logger_printf("[CMD] Command: Stop Interval Photo received\n");
            stop_interval_photo();
            break;
        case 0x10: // Start Audio Stream
            logger_printf("[CMD] Command: Start Audio Stream received\n");
            start_audio_stream();
            break;
        case 0x11: // Stop Audio Stream
            logger_printf("[CMD] Command: Stop Audio Stream received\n");
            stop_audio_stream();
            break;
        case 0x20: // Get Wi-Fi Status
            logger_printf("[CMD] Command: Get Wi-Fi Status received\n");
            set_current_state(STATE_WIFI_ACTIVATING);
            break;
        case 0x21: // Shutdown Wi-Fi Mode
            logger_printf("[CMD] Command: Shutdown Wi-Fi Mode\n");
            if (get_current_state() == STATE_STREAMING_AV_WIFI || get_current_state() == STATE_WIFI_ACTIVATING) {
                set_current_state(STATE_CONNECTED_BLE);
            }
            break;
        case 0xFE: // Reboot Device
            logger_printf("[CMD] Command: Reboot Device\n");
            // ESP.restart(); // This will be enabled later
            break;
        default:
            logger_printf("[CMD] Error: Unknown Command ID 0x%02X\n", command_id);
            break;
    }
}
