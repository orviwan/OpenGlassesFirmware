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

Python client scripts are provided in the `client/` directory to demonstrate how to interact with the device's BLE services.

-   **`ble_photo_client.py`**: A client to request and receive a single photo. It saves the image as a timestamped JPEG file and prints transfer statistics.
-   **`ble_audio_client.py`**: A client to record 20 seconds of audio. It saves the stream as a timestamped WAV file and prints session statistics.

### Dependencies

The only external dependency for the clients is the `bleak` library, which can be installed via pip:

```bash
pip3 install bleak
```

### Usage

First, ensure your OpenGlass device is powered on and advertising.

**To test photo capture:**

```bash
python3 client/ble_photo_client.py
```

The script will connect, request a photo, and save it in the `client/` directory.

**To test audio recording:**

```bash
python3 client/ble_audio_client.py
```

The script will connect, record 20 seconds of audio, and save it as a WAV file in the `client/` directory.

## Building the Firmware

This project is built using the Arduino framework for the ESP32.

### Dependencies

- ESP32 Arduino Core

### Setup

1.  Open the `OpenGlassesFirmware.ino` file in the Arduino IDE.
2.  Select the correct ESP32 board from the Tools menu.
3.  Install the required libraries.
4.  Compile and upload the firmware to your device.

