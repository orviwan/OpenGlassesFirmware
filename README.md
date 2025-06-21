# OpenGlass Firmware

This repository contains the firmware for the OpenGlass project, a wearable device based on the ESP32. The firmware provides BLE (Bluetooth Low Energy) services for streaming audio and photos.

## Features

- **BLE Services:**
  - Photo streaming (single-shot and interval-based)
  - Real-time audio streaming using μ-law (G.711) codec.
- **Power Management:**
  - Automatic deep sleep and light sleep modes to conserve battery.
  - Dynamic CPU frequency scaling.
- **Task Management:**
  - FreeRTOS tasks for handling photo and audio streaming, ensuring smooth operation.
- **LED Status Indicator:**
  - Onboard LED provides visual feedback for connection status, streaming activity, and power modes.

## BLE Protocol

The firmware uses a custom BLE service for its core functionalities.

- **Service UUID:** `19B10000-E8F2-537E-4F6C-D104768A1214`

### Photo Streaming

- **Photo Data Characteristic:** `19B10005-E8F2-537E-4F6C-D104768A1214` (Notify)
  - Streams photo data in chunks.
  - Each chunk is prefixed with a 2-byte frame number (little-endian).
  - The end of a photo is marked by a special frame number `0xFFFF`.
- **Photo Control Characteristic:** `19B10006-E8F2-537E-4F6C-D104768A1214` (Write)
  - `-1` (or `0xFF`): Request a single photo.
  - `0`: Stop any ongoing interval capture.
  - `5-127`: Set the interval for photo capture in seconds.

### Audio Streaming

- **Audio Data Characteristic:** `19B10001-E8F2-537E-4F6C-D104768A1214` (Notify)
  - Streams audio data encoded with μ-law (G.711).
  - Sample rate: 8kHz.

## Client Implementation

A Python client script is provided in the `client` directory (`ble_photo_client.py`) that demonstrates how to connect to the device, request a photo, and receive the data.

### Dependencies

- `bleak`: Asynchronous BLE client library for Python.

### Usage

1.  Run the script:
    ```sh
    python client/ble_photo_client.py
    ```
2.  The script will scan for the "OpenGlass" device, connect to it, and request a photo.
3.  The received photo will be saved as `received_photo.jpg` in the same directory.

## Building the Firmware

This project is built using the Arduino framework for the ESP32.

### Dependencies

- ESP32 Arduino Core

### Setup

1.  Open the `OpenGlassesFirmware.ino` file in the Arduino IDE.
2.  Select the correct ESP32 board from the Tools menu.
3.  Install the required libraries.
4.  Compile and upload the firmware to your device.

