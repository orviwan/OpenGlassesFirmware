# OpenGlasses Firmware

This repository contains the PlatformIO-based firmware for the OpenGlasses project, a wearable device built on the Seeed Studio XIAO ESP32-S3. The firmware provides a rich set of features including BLE command and control, photo capture, and live audio/video streaming over Wi-Fi.

The architecture is modular and state-driven, ensuring robust handling of different operational modes.

## Features

-   **State-Driven Architecture:** A central state handler manages the device's mode, from idle and sleeping to active streaming.
-   **BLE Command and Control:** Uses NimBLE for efficient Bluetooth Low Energy communication to receive commands and send notifications.
-   **Photo Capture:** On-demand photo capture (JPEG) triggered via BLE, with the image transferred over the BLE connection.
-   **Wi-Fi Hotspot:** Can create its own Wi-Fi network for direct connection without needing an external router.
-   **Live A/V Streaming:**
    -   **Video:** Streams a live MJPEG video feed over HTTP.
    -   **Audio:** Streams live, raw 16-bit audio over a WebSocket.
-   **Power Management:** Utilizes ESP32's sleep modes to conserve battery when idle.
-   **LED Status Indicator:** Provides visual feedback for different states like idle, connecting, and streaming.

## Communication Protocols

### 1. BLE Protocol

The firmware uses custom BLE services for commands and data transfer.

-   **Command Service:** `d27157e8-318c-4320-b359-5b8a625c727a`
    -   **Command Characteristic:** `ab473a0a-4531-4963-87a9-05e7b315a8e5` (Write)
        -   Accepts single-byte commands to control the device's functions.
        -   `0x10`: Start Audio Stream (BLE)
        -   `0x11`: Stop Audio Stream (BLE)
        -   `0x20`: Start Wi-Fi Hotspot & A/V Streams
        -   `0x21`: Stop Wi-Fi Hotspot

    -   **Photo Control Characteristic:** (Write)
        -   `0x01`: Take Photo

-   **Photo Service:**
    -   Transfers JPEG data to a connected client.

### 2. Wi-Fi Protocol

When activated, the device creates a Wi-Fi hotspot with the following credentials:
-   **SSID:** `OpenGlasses`
-   **Password:** `openglasses`

Once connected, clients can access the following endpoints on `http://192.168.4.1`:

-   **Video Stream:** `http://192.168.4.1/stream`
    -   An HTTP endpoint that serves a `multipart/x-mixed-replace` stream containing JPEG frames (MJPEG).
-   **Audio Stream:** `ws://192.168.4.1/audio`
    -   A WebSocket endpoint that streams raw PCM audio data (16000 Hz, 16-bit, mono).

## Building the Firmware

This project is built using **PlatformIO**.

### Dependencies

All required libraries are listed in the `platformio.ini` file and will be downloaded automatically by PlatformIO.
-   `nimble-cpp` (for BLE)
-   `ESPAsyncWebServer` & `AsyncTCP` (for Wi-Fi server)

### Build & Upload

1.  [Install PlatformIO IDE for VSCode](https://platformio.org/install/ide?install=vscode).
2.  Open the `OpenGlassesFirmware` folder in VSCode.
3.  To build the project, use the PlatformIO "Build" task.
4.  To upload the firmware to the device, use the PlatformIO "Upload" task.

## Client Applications

Python clients for interacting with the firmware are located in the `client/` directory. These scripts allow you to send commands, view the live video stream, and save photos.

For detailed instructions on setting up and using the clients, please see the **[Client README](./client/README.md)**.


