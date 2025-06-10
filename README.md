# OpenGlass Firmware

Everything in here is a modified version the awesome [Omi OpenGlass](https://github.com/BasedHardware/omi/tree/main/omiGlass) project.

This repository contains the firmware for the OpenGlass project, an ESP32-S3 based wearable device designed for capturing and transmitting audio and photo data over Bluetooth Low Energy (BLE). It's specifically tailored for boards like the Seeed Studio XIAO ESP32S3 Sense.

## Key Features

*   **Audio Streaming:**
    *   **PCM:** Captures audio from the onboard PDM microphone and streams it as PCM 16kHz, 16-bit mono data over BLE.
    *   **Opus:** Streams Opus-encoded audio (16-bit, 16kHz mono) over a dedicated BLE characteristic for efficient, compressed audio (requires client support).
*   **Photo Capture & Transfer:** Captures SVGA (800x600) resolution JPEG images and transfers them in chunks over BLE.
    *   Supports single-shot photo requests.
    *   Supports client-defined interval photo capture.
    *   Defaults to a 30-second interval capture if no client command is given.
*   **Bluetooth Low Energy (BLE) Services:**
    *   **Custom "Friend" Service:**
        *   Audio Data Characteristic (for streaming PCM audio)
        *   **Opus Audio Characteristic (for streaming Opus-encoded audio)**
        *   Audio Codec Characteristic (reports PCM 16kHz, 16-bit)
        *   Photo Data Characteristic (for streaming photo chunks)
        *   Photo Control Characteristic (to command photo capture)
    *   **Standard Device Information Service:** Provides manufacturer, model, firmware revision, and hardware revision.
    *   **Standard Battery Service:** Provides battery level (currently static, see TODOs).
*   **Modular Design:** Code is organized into logical modules (BLE, audio, camera, photo management, battery) within the `src/` directory for better maintainability.
*   **PSRAM Dependent:** Utilizes PSRAM for camera frame buffers and audio buffers. **The firmware will halt if PSRAM is not detected.**

## Hardware Requirements

*   An ESP32-S3 based board with an integrated camera and PDM microphone.
*   Currently configured for **Seeed Studio XIAO ESP32S3 Sense** (see `src/config.h` and `src/camera_pins.h`).

## Project Structure

```
firmware/
├── firmware.ino         // Main Arduino sketch file (setup, loop)
├── README.md            // This file
├── blueprint.md         // Detailed application blueprint
├── camera_pins.h        // Hardware pin definitions for the camera
└── src/                   // Source code modules
    ├── config.h           // Global constants and configuration
    ├── ble_handler.h/cpp  // BLE logic
    ├── audio_handler.h/cpp// Audio handler (shared buffer, mic config, dispatch)
    ├── audio_pcm.h/cpp    // PCM audio capture and BLE streaming
    ├── audio_opus.h/cpp   // Opus encoding and BLE streaming
    ├── camera_handler.h/cpp// Camera interface
    ├── photo_manager.h/cpp// Photo capture and upload logic
    └── battery_handler.h/cpp// Battery level reporting
```

## Building the Firmware (Arduino IDE)

1.  **Board Selection:**
    *   Open the `firmware.ino` file in the Arduino IDE.
    *   Select the correct board from "Tools" > "Board" (e.g., "XIAO_ESP32S3").
    *   **Crucially, ensure PSRAM is enabled in your board settings if there's an option (e.g., Tools > PSRAM > OPI PSRAM).** The firmware will halt if PSRAM is not found.
2.  **Libraries:**
    *   The necessary libraries (ESP32 BLE, I2S, ESP32 Camera Driver) are typically included with the ESP32 board support package for the Arduino IDE.
3.  **Compilation & Upload:**
    *   Verify/Compile and Upload the sketch to your ESP32-S3 device.

## Connecting and Interacting

A client application (e.g., mobile app) can interact with the OpenGlass device as follows:

1.  **Scan & Connect:** Scan for a BLE device named "OpenGlass" and establish a connection.
2.  **Discover Services & Characteristics:** Refer to `blueprint.md` for detailed UUIDs.
3.  **Subscribe to Notifications:**
    *   **Audio Data Characteristic:** To receive streaming PCM audio packets.
    *   **Opus Audio Characteristic:** To receive Opus-encoded audio packets (client must decode Opus).
    *   **Photo Data Characteristic:** To receive photo image chunks.
    *   **Battery Level Characteristic:** To receive battery level updates.
4.  **Read Audio Codec:** Read the Audio Codec characteristic to confirm audio format (PCM 16kHz, 16-bit).
5.  **Control Photos:** Write to the Photo Control characteristic:
    *   `-1` (0xFF): Request a single photo.
    *   `0` (0x00): Stop photo capture.
    *   `1-127`: Start interval capture (value is interval in seconds).
6.  **Opus Audio Streaming:**
    *   Subscribe to the Opus Audio Characteristic to receive a continuous stream of Opus-encoded audio packets.
    *   Unsubscribe (disable notifications) to stop the stream.
7.  **Reassemble Photos:** The client must reassemble received photo chunks (using the 2-byte sequence number header) until an end-of-photo marker (`0xFFFF` sequence number) is received.

For a detailed guide on services, characteristics, and data formats, please see `blueprint.md`.

## Important Notes & TODOs

*   **PSRAM is Critical:** The firmware will not function without PSRAM. Ensure your board has it and it's enabled.
*   **Battery Level:** The battery level reporting is currently static (defaults to 100%). Actual battery reading logic needs to be implemented in `src/battery_handler.cpp`.
*   **Error Handling:** While basic error handling and halts on critical failures are present, further robustness could be added.

## Contributing

Contributions and improvements are welcome! Please feel free to fork the repository, make changes, and submit a pull request.
