# OpenGlasses Firmware Blueprint v2.0

## Overview

This document outlines the technical architecture of the OpenGlasses firmware, version 2.0. The firmware is built using the PlatformIO ecosystem for the Seeed Studio XIAO ESP32-S3. It features a modular, state-driven design to manage complex operations like BLE control, photo capture, and simultaneous audio/video streaming over Wi-Fi.

## Core Architecture: State-Driven and Modular

The firmware's foundation is a **state machine** managed by the `state_handler`. This central component dictates the device's overall behavior by transitioning between defined states (e.g., `STATE_IDLE`, `STATE_TAKING_PHOTO`, `STATE_STREAMING_AV_WIFI`). This approach ensures that operations are performed in a controlled and predictable manner.

Functionality is encapsulated within distinct, single-responsibility handlers:

-   **`main.cpp`**: The entry point. It initializes all handlers and runs the main loop, which acts as the state machine's driver.
-   **`state_handler`**: Defines and manages the global `firmware_state_t` enum and the current state variable.
-   **`ble_handler`**: Manages all BLE (NimBLE) services, characteristics, advertising, and connection events. It acts as the primary interface for receiving commands.
-   **`command_handler`**: Decouples command processing from the BLE layer. It receives command bytes and triggers actions by calling other handlers (e.g., `photo_handler`, `wifi_handler`).
-   **`wifi_handler`**: Manages the Wi-Fi subsystem. It can create a Soft AP (hotspot) and runs an `ESPAsyncWebServer` to serve live A/V streams.
-   **`camera_handler`**: A low-level interface for initializing the camera hardware and capturing frames.
-   **`photo_handler`**: Orchestrates the process of taking a photo and transferring the resulting JPEG data over a BLE notification stream.
-   **`audio_streamer`**: Manages the I2S microphone. It runs a dedicated FreeRTOS task to continuously capture audio data and route it to the appropriate handler (BLE or Wi-Fi) based on the current firmware state.
-   **`led_handler`**: Controls the onboard LED to provide visual feedback for different states.
-   **`battery_handler`**: A simple handler for monitoring the battery voltage.
-   **`logger`**: Provides a thread-safe logging mechanism for debugging.

## Communication Protocols

### 1. BLE Protocol

The firmware uses NimBLE for its efficiency and uses custom BLE services for command and data transfer.

-   **Command Service:** `d27157e8-318c-4320-b359-5b8a625c727a`
    -   **Command Characteristic:** `ab473a0a-4531-4963-87a9-05e7b315a8e5` (Write)
        -   Accepts single-byte commands to change the device's state.
        -   `0x01`: Take Photo
        -   `0x10`: Start Audio Stream (over BLE)
        -   `0x11`: Stop Audio Stream (over BLE)
        -   `0x20`: Start Wi-Fi Hotspot & A/V Streams
        -   `0x21`: Stop Wi-Fi Hotspot

-   **Photo Service:** A dedicated service for transferring JPEG data to a connected client via notifications.

### 2. Wi-Fi Streaming Protocol

When activated via the `0x20` command, the device creates a Wi-Fi hotspot.

-   **SSID:** `OpenGlasses`
-   **Password:** `openglasses`
-   **IP Address:** `192.168.4.1`

The integrated web server provides two streaming endpoints:

-   **Video Stream:** `http://192.168.4.1/stream`
    -   Serves a `multipart/x-mixed-replace` stream (MJPEG) directly from the camera, allowing for low-latency video playback in a web browser or client like OpenCV.
-   **Audio Stream:** `ws://192.168.4.1/audio`
    -   A WebSocket endpoint that streams raw PCM audio data (16000 Hz, 16-bit, mono). This provides a real-time, low-overhead audio feed.

## Data Flow Example: Wi-Fi Streaming

1.  A client connects via BLE and writes `0x20` to the Command Characteristic.
2.  `ble_handler` receives the byte and passes it to `command_handler`.
3.  `command_handler` calls `wifi_handler::start_wifi_hotspot()`.
4.  `wifi_handler` starts the Soft AP and the asynchronous web server with the `/stream` and `/audio` endpoints.
5.  `command_handler` sets the global state to `STATE_STREAMING_AV_WIFI`.
6.  A client (e.g., `wifi_stream_client.py`) connects to the "OpenGlasses" Wi-Fi network.
7.  The client makes an HTTP request to `/stream`. The `wifi_handler` responds by sending camera frames as a continuous MJPEG stream.
8.  The client opens a WebSocket connection to `/audio`.
9.  The `audio_streamer` task, seeing the state is `STATE_STREAMING_AV_WIFI`, captures audio from the I2S microphone and sends the raw data to `wifi_handler`.
10. `wifi_handler` broadcasts the received audio data to all connected WebSocket clients.

## Power Management

Power efficiency is managed through the state machine. In `STATE_IDLE`, the device can be programmed to enter light or deep sleep. Peripherals like the camera and Wi-Fi radio are only powered on when their respective states are active, and are shut down when returning to idle, conserving significant power.
