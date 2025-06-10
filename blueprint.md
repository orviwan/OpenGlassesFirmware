# OpenGlass Firmware Blueprint

This document outlines the architecture, functionality, and interaction model for the OpenGlass ESP32-S3 firmware.

## Core Functionality

1.  **Audio Streaming:** Captures audio from the onboard PDM microphone, processes it (PCM 16kHz, 16-bit mono, with gain), and streams it over BLE.
1.  **Audio Streaming:** Captures audio from the onboard PDM microphone, processes it (PCM 8kHz, 16-bit mono, with gain), and streams it over BLE.
2.  **Photo Capture & Transfer:** Captures JPEG images (SVGA resolution) from the camera, and transfers them in chunks over BLE. Photo capture can be initiated by the client (single shot or interval) or can run on a default interval.
3.  **BLE Services:**
    *   **Custom "Friend" Service:** For audio data, photo data, photo control, and audio codec information.
    *   **Standard Device Information Service:** Provides manufacturer, model, firmware, and hardware revision.
    *   **Standard Battery Service:** Provides battery level.
4.  **Device Management:** Initializes hardware (BLE, microphone, camera), manages memory for buffers, and handles the main application loop.

## System Architecture & Components

The firmware is modular, with distinct components handling specific tasks. These components reside in the `src/` directory.

```
firmware/
├── firmware.ino         // Main entry point (setup, loop)
├── camera_pins.h        // Hardware pin definitions for the camera
└── src/
    ├── config.h           // Global constants and configuration
    ├── ble_handler.h/cpp  // BLE services, characteristics, advertising, connection handling
    ├── audio_handler.h/cpp// Microphone setup, shared buffer, audio dispatch
    ├── audio_pcm.h/cpp    // PCM audio capture, PCM BLE streaming
    ├── audio_opus.h/cpp   // Opus encoding, Opus BLE streaming
    ├── camera_handler.h/cpp// Camera setup, image capture (JPEG)
    ├── photo_manager.h/cpp// Photo capture control logic, interval timing, image chunking & upload
    └── battery_handler.h/cpp// Battery level monitoring and reporting
```

## Component Breakdown & Interactions

### 1. `firmware.ino` (Main Application Logic)
*   **`setup()`:**
    *   Initializes `Serial` communication.
    *   **Crucially, checks for PSRAM availability (`psramFound()`). Halts if not found.**
    *   Calls initialization functions for each module:
        *   `configure_ble()`
        *   `configure_microphone()`
        *   `configure_camera()`
        *   `initialize_photo_manager()` (sets up photo chunk buffer and default capture interval)
        *   `initialize_battery_handler()` (passes the BLE battery characteristic)
    *   Performs an initial `update_battery_level()`.
*   **`loop()`:**
    *   If a BLE client is connected (`g_is_ble_connected`):
        *   Calls `process_and_send_audio()` to capture and stream audio.
        *   Calls `process_photo_capture_and_upload()` to manage photo taking and sending.
        *   Periodically calls `update_battery_level()` based on `BATTERY_UPDATE_INTERVAL_MS`.
    *   Includes a `delay(LOOP_DELAY_MS)` to yield processing time.

### 2. `src/config.h` (Configuration)
*   Defines `CAMERA_MODEL_XIAO_ESP32S3`.
*   Contains constants for:
    *   PCM Audio (frame size for output packet, I2S sample rate set to 8kHz, bits, gain, header length).
    *   BLE UUIDs for all services and characteristics.
    *   Audio Codec ID (e.g., `AUDIO_CODEC_ID_PCM_8KHZ_16BIT`).
    *   Photo Chunking (max payload, header length, buffer size).
    *   Timings (battery update interval, main loop delay).

### 3. `src/camera_pins.h` (Hardware Pins)
*   Includes `config.h` to get the `CAMERA_MODEL_XIAO_ESP32S3` definition.
*   Defines the specific GPIO pin numbers used by the camera module based on the selected model. This is critical for `camera_handler.cpp` to correctly interface with the camera hardware.

### 4. `src/ble_handler.h/.cpp` (Bluetooth Low Energy)
*   **Global Variables:**
    *   `g_photo_data_characteristic`, `g_photo_control_characteristic`, `g_audio_data_characteristic`, `g_opus_audio_characteristic`, `g_battery_level_characteristic`: Pointers to the BLE characteristic objects.
    *   `g_is_ble_connected`, `g_opus_streaming_enabled`: Boolean flags for BLE connection and Opus streaming state.
*   **`OpusAudioNotifyCallback` Class:** Handles subscribe/unsubscribe for Opus notifications, sets `g_opus_streaming_enabled`.
*   **`configure_ble()`:**
    *   Initializes `BLEDevice` with the name "OpenGlass".
    *   Creates the BLE server and sets callbacks.
    *   **Main Custom Service (`SERVICE_UUID`):**
        *   Audio Data Characteristic (`AUDIO_DATA_UUID`): Read, Notify. For streaming audio packets.
        *   Audio Codec Characteristic (`AUDIO_CODEC_UUID`): Read. Value set to `AUDIO_CODEC_ID_PCM_16KHZ_16BIT`.
        *   Audio Codec Characteristic (`AUDIO_CODEC_UUID`): Read. Value set to reflect 8kHz PCM (e.g., `AUDIO_CODEC_ID_PCM_8KHZ_16BIT`).
        *   Photo Data Characteristic (`PHOTO_DATA_UUID`): Read, Notify. For sending photo chunks.
        *   Photo Control Characteristic (`PHOTO_CONTROL_UUID`): Write. For client commands to control photo capture.
        *   Opus Audio Characteristic (`AUDIO_CODEC_OPUS_UUID`): Read, Notify. For streaming Opus audio packets.
    *   **Device Information Service (Standard):** Provides manufacturer, model, firmware ("1.0.2"), hardware.
    *   **Battery Service (Standard):** Provides battery level via `g_battery_level_characteristic`.
    *   Adds `BLE2902` descriptors to notifiable characteristics to allow clients to subscribe.
    *   Starts all services and BLE advertising.

### 5. `src/audio_handler.h/.cpp`, `audio_pcm.h/.cpp`, `audio_opus.h/.cpp` (Audio Processing)
*   **Shared:**
    *   Microphone setup, buffer allocation, and `read_microphone_data()` in `audio_handler`.
*   **PCM:**
    *   `process_and_send_audio()` in `audio_pcm.cpp` captures PCM and streams over BLE.
*   **Opus:**
    *   `process_and_send_opus_audio()` in `audio_opus.cpp` encodes PCM to Opus and streams over BLE.
    *   `handle_opus_streaming()` in `ble_handler.cpp` is called from the main loop to stream Opus audio when enabled.
### 6. `src/camera_handler.h/.cpp` (Camera Interface)
*   **Global Variable:**
    *   `fb`: Pointer to `camera_fb_t` (frame buffer structure), stores the most recently captured image.
*   **`configure_camera()`:**
    *   Sets up the `camera_config_t` structure using pin definitions from `camera_pins.h`.
    *   Configures camera for `FRAMESIZE_SVGA` (800x600), `PIXFORMAT_JPEG`, `jpeg_quality = 10`.
    *   **Crucially, sets `fb_location = CAMERA_FB_IN_PSRAM` (frame buffer in PSRAM).**
    *   Initializes the camera using `esp_camera_init()`. **Halts if initialization fails.**
*   **`take_photo()`:**
    *   Calls `release_photo_buffer()` to free any previous frame.
    *   Captures a new image using `esp_camera_fb_get()`.
    *   Returns `true` on success, `false` on failure (and logs an error).
*   **`release_photo_buffer()`:** If `fb` is not null, calls `esp_camera_fb_return(fb)` to free the camera frame buffer and sets `fb = nullptr`.

### 7. `src/photo_manager.h/.cpp` (Photo Logic & Upload)
*   **Global Variables:**
    *   `g_is_capturing_photos`, `g_capture_interval_ms`, `g_last_capture_time_ms`: State for photo capture timing and mode.
    *   `g_sent_photo_bytes`, `g_sent_photo_frames`: Track progress of current photo upload.
    *   `g_is_photo_uploading`: Flag indicating if an upload is in progress.
    *   `s_photo_chunk_buffer`: Buffer allocated in PSRAM for assembling photo chunks to be sent over BLE.
*   **`initialize_photo_manager()`:**
    *   **Allocates `s_photo_chunk_buffer` using `ps_calloc`. Halts if allocation fails.**
    *   Sets default photo capture behavior: `g_is_capturing_photos = true`, `g_capture_interval_ms = 30000` (30 seconds).
*   **`handle_photo_control()`:**
    *   Called by `PhotoControlCallback` in `ble_handler.cpp`.
    *   Interprets client commands:
        *   `-1`: Single photo request (sets `g_capture_interval_ms = 0`).
        *   `0`: Stop photo capture.
        *   `1-127`: Start interval capture, value is interval in seconds.
    *   Updates `g_is_capturing_photos` and `g_capture_interval_ms` accordingly.
*   **`process_photo_capture_and_upload()`:**
    *   Called from the main `loop()`.
    *   **Capture Logic:**
        *   Checks if conditions are met to take a photo (BLE connected, capture enabled, not already uploading, interval elapsed or single shot requested).
        *   If a single shot, sets `g_is_capturing_photos = false` after triggering.
        *   Calls `take_photo()` from `camera_handler.cpp`.
        *   If successful, sets `g_is_photo_uploading = true` and resets upload counters.
    *   **Upload Logic:**
        *   If `g_is_photo_uploading` is true and a valid frame buffer (`fb`) exists:
            *   Calculates remaining bytes.
            *   Constructs a photo chunk in `s_photo_chunk_buffer` with a 2-byte header (chunk sequence number) followed by up to `MAX_PHOTO_CHUNK_PAYLOAD_SIZE` of image data.
            *   Sends the chunk via `g_photo_data_characteristic->setValue()` and `notify()`.
            *   Updates `g_sent_photo_bytes` and `g_sent_photo_frames`.
        *   If all data is sent, sends an end-of-photo marker (sequence number `0xFFFF`).
        *   Sets `g_is_photo_uploading = false` and calls `release_photo_buffer()`.

### 8. `src/battery_handler.h/.cpp` (Battery Monitoring)
*   **Global Variables:**
    *   `g_battery_level_percent`: Current battery level (static 100% for now).
    *   `g_last_battery_update_ms`: Timestamp of last update.
    *   `g_battery_level_characteristic_ptr`: Pointer to the BLE battery characteristic.
*   **`initialize_battery_handler()`:** Stores the passed `g_battery_level_characteristic`.
*   **`update_battery_level()`:**
    *   Sets the value of the BLE battery characteristic (currently static `g_battery_level_percent`).
    *   Sends a notification.
    *   Updates `g_last_battery_update_ms`.
    *   **Contains a `// TODO:` to implement actual battery reading.**

## Data Flow Examples

### Audio Streaming
1.  `loop()` calls `process_and_send_audio()` for PCM and/or `handle_opus_streaming()` for Opus.
2.  PCM: Captures and streams raw PCM audio packets over BLE.
3.  Opus: Captures PCM, encodes to Opus, and streams Opus packets over BLE when notifications are enabled.

### Photo Capture & Transfer (Interval)
1.  `loop()` calls `process_photo_capture_and_upload()`.
2.  Conditions are met (BLE connected, interval elapsed).
3.  `take_photo()` is called, `fb` gets populated.
4.  `g_is_photo_uploading` becomes true.
5.  On subsequent `loop()` calls, `process_photo_capture_and_upload()` enters the upload logic.
6.  Image data from `fb->buf` is copied into `s_photo_chunk_buffer` with a header.
7.  `g_photo_data_characteristic` is updated and a notification sent for each chunk.
8.  Client (subscribed to notifications) receives photo chunks.
9.  End-of-photo marker is sent.
10. `release_photo_buffer()` is called.

## How to Connect and Interact with the Device (Client Perspective)

A client application (e.g., mobile app, desktop app, web app using Web Bluetooth) would typically perform the following:

1.  **Scan for BLE Devices:**
    Scan for devices advertising the "OpenGlass" name or specifically the `SERVICE_UUID` (`19B10000-E8F2-537E-4F6C-D104768A1214`).

2.  **Connect to the Device:**
    Establish a BLE connection.

3.  **Discover Services and Characteristics:**
    *   Discover the main custom service (`SERVICE_UUID`).
    *   Within this service, discover:
        *   Audio Data Characteristic (`AUDIO_DATA_UUID`)
        *   Audio Codec Characteristic (`AUDIO_CODEC_UUID`)
        *   Photo Data Characteristic (`PHOTO_DATA_UUID`)
        *   Photo Control Characteristic (`PHOTO_CONTROL_UUID`)
        *   Opus Audio Characteristic (`AUDIO_CODEC_OPUS_UUID`)
    *   Discover the Device Information Service and its characteristics.
    *   Discover the Battery Service and its Battery Level characteristic.

4.  **Read Initial Information (Optional but Recommended):**
    *   Read the Audio Codec Characteristic: Value will be `0x01` (indicating PCM 16kHz 16-bit).
    *   Read the Audio Codec Characteristic: Value will now indicate 8kHz PCM (e.g., `0x02`).
    *   Read Device Information characteristics (Manufacturer, Model, Firmware, Hardware).
    *   Read the initial Battery Level.

5.  **Subscribe to Notifications:**
    *   **Crucial for Audio:** Enable notifications on the Audio Data Characteristic. Audio packets will start streaming.
    *   **Crucial for Photos:** Enable notifications on the Photo Data Characteristic. Photo chunks will be sent here.
    *   **Recommended for Battery:** Enable notifications on the Battery Level Characteristic to receive updates.
    *   **Crucial for Opus:** Enable notifications on the Opus Audio Characteristic to receive continuous Opus audio stream.

6.  **Receive and Process Audio Data:**
    *   When an audio notification is received:
        *   The first 2 bytes are the `audio_frame_count` (little-endian).
        *   The 3rd byte is reserved (currently 0).
        *   The subsequent bytes are the PCM 16-bit mono audio samples (e.g., 80 samples, 160 bytes for 8kHz 10ms packets).
        *   The client can then play back this audio or process it further.

7.  **Control Photo Capture (via Photo Control Characteristic - `PHOTO_CONTROL_UUID`):**
    *   **Write a single byte:**
        *   `-1` (or `0xFF` as signed `int8_t`): Request a single photo.
        *   `0` (or `0x00`): Stop any ongoing interval photo capture.
        *   `1` to `127` (or `0x01` to `0x7F`): Start interval photo capture. The value represents the interval in seconds.
    *   The device will start capturing photos based on this command (or continue its default 30s interval if no command is sent after connection).

8.  **Receive and Reassemble Photo Data:**
    *   When a photo data notification is received:
        *   The first 2 bytes are the `chunk_sequence_number` (little-endian).
        *   If `chunk_sequence_number` is `0xFFFF` (65535), it's the end-of-photo marker. The current photo is complete.
        *   Otherwise, the subsequent bytes (up to `MAX_PHOTO_CHUNK_PAYLOAD_SIZE`) are part of the JPEG image data.
        *   The client needs to buffer these chunks in order (using the sequence number) and append them to reconstruct the full JPEG image.
        *   Once the end-of-photo marker is received, the assembled buffer contains the complete JPEG, which can then be displayed or saved.

9.  **Receive Opus Audio Stream:**
    *   When an Opus audio notification is received:
        *   The client receives a continuous stream of Opus-encoded audio packets.
        *   Unsubscribing from the Opus Audio Characteristic stops the stream.
        *   The client must decode Opus frames for playback or processing.

10.  **Handle Disconnection:**
    Be prepared for disconnections and implement reconnection logic if needed.

## Critical Considerations for Client Implementation

*   **PSRAM Dependency:** The firmware heavily relies on PSRAM. If the device reports "PSRAM not found," the firmware will halt early, and no services will be fully functional.
*   **Buffer Sizes:** The client should be aware of the audio packet structure and photo chunking mechanism to correctly parse incoming data.
*   **Error Handling:** Implement robust error handling for BLE operations (connection, discovery, reads, writes, notifications).
*   **Data Reassembly:** For photos, the client *must* correctly reassemble chunks in order.
*   **Battery Level:** The current implementation sends a static battery level. A real implementation would require the ESP32 to read an ADC or a fuel gauge IC.
*   **Opus Streaming:** The client can enable/disable Opus streaming by subscribing/unsubscribing to the Opus Audio Characteristic. Continuous streaming provides real-time audio but requires handling of Opus decoding.
