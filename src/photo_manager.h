#ifndef PHOTO_MANAGER_H
#define PHOTO_MANAGER_H

#include <stdint.h> // For uint8_t, int8_t, uint16_t
#include <stddef.h> // For size_t

#include "ble_handler.h"    // For g_photo_data_characteristic and g_is_ble_connected
#include <Arduino.h> // For Serial, millis(), memcpy()

enum PhotoCaptureMode {
    MODE_STOP,
    MODE_SINGLE,
    MODE_INTERVAL
};

// Extern declarations for global photo state variables
extern PhotoCaptureMode g_capture_mode;
extern int g_capture_interval_ms;
extern unsigned long g_last_capture_time_ms;
extern size_t g_sent_photo_bytes;
extern uint16_t g_sent_photo_frames;
extern bool g_is_photo_uploading;
extern bool g_is_processing_capture_request; // Flag to indicate if a capture request is being processed
extern bool g_single_shot_pending; // Flag for single photo request pending

extern uint8_t *s_photo_chunk_buffer;


void initialize_photo_manager();
void handle_photo_control(int8_t control_value);
void process_photo_capture_and_upload(unsigned long current_time_ms);
void reset_photo_manager_state();


#endif // PHOTO_MANAGER_H