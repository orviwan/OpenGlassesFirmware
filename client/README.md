# OpenGlass Python BLE Clients

This directory contains Python scripts for testing the photo and audio functionalities of the OpenGlass firmware over Bluetooth Low Energy (BLE).

## Clients

-   `ble_photo_client.py`: Connects to the device, requests a single photo, saves it as a timestamped JPEG file, and reports transfer performance.
-   `ble_audio_client.py`: Connects to the device, records 20 seconds of audio, saves it as a timestamped WAV file, and reports performance metrics.

## Requirements

-   Python 3.7+
-   `bleak` library

## Installation

1.  **Install Python:** Make sure you have Python 3.7 or newer installed. You can check your version with `python3 --version`.

2.  **Install Dependencies:** The only external dependency is the `bleak` library. An up-to-date version is required for features like MTU negotiation to work correctly.

    If you have an older version of `bleak` installed, upgrade it:
    ```bash
    pip3 install --upgrade bleak
    ```

    If you do not have it installed, run:
    ```bash
    pip3 install bleak
    ```

## Usage

First, ensure your OpenGlass device is powered on and advertising.

### Photo Client

Run the photo client from your terminal:

```bash
python3 client/ble_photo_client.py
```

The script will automatically scan for the device, connect, request a photo, receive the data, validate it, and save the resulting image (e.g., `photo_20250621_143000.jpg`). It will then print a summary of the transfer performance.

### Audio Client

Run the audio client from your terminal:

```bash
python3 client/ble_audio_client.py
```

The script will connect, record 20 seconds of audio, convert it from µ-law to PCM, and save it as a WAV file (e.g., `audio_20250621_150000.wav`). It will then print a summary of the session.
