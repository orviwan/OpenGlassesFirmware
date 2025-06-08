#ifndef PHOTO_MANAGER_H
#define PHOTO_MANAGER_H

#include <stdint.h> // For uint8_t, int8_t, uint16_t
#include <stddef.h> // For size_t

// Extern declarations for global photo state variables
extern bool g_is_capturing_photos;
extern int g_capture_interval_ms;
extern unsigned long g_last_capture_time_ms;
extern size_t g_sent_photo_bytes;
extern uint16_t g_sent_photo_frames; // Changed to uint16_t
extern bool g_is_photo_uploading;

// Buffer for photo chunks - can be extern if needed by other modules,
// or kept static within photo_manager.cpp. For now, keep it internal.
extern uint8_t *s_photo_chunk_buffer;


void initialize_photo_manager();
void handle_photo_control(int8_t control_value);
void process_photo_capture_and_upload(unsigned long current_time_ms);


#endif // PHOTO_MANAGER_H