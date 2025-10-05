#include "command_handler.h"
#include "audio_streamer.h"
#include "photo_handler.h"
#include "wifi_handler.h"
#include "logger.h"
#include "state_handler.h"

void handle_command_control(const std::string& value) {
    if (value.length() == 0) {
        logger_printf("[CMD] Error: Received empty command.\n");
        return;
    }

    uint8_t command_id = value[0];
    logger_printf("[CMD] Received Command ID: 0x%02X\n", command_id);

    switch (command_id) {
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
            start_wifi_hotspot();
            break;
        case 0x21: // Shutdown Wi-Fi Mode
            logger_printf("[CMD] Command: Shutdown Wi-Fi Mode\n");
            stop_wifi_hotspot();
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
