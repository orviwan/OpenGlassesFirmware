# OpenGlass Python BLE Client

This directory contains a Python script (`ble_photo_client.py`) for testing the photo capture functionality of the OpenGlass firmware over Bluetooth Low Energy (BLE).

## Features
- Connects to the "OpenGlass" BLE device.
- Requests a single photo capture.
- Receives the JPEG image data in chunks.
- Verifies the image integrity using a CRC32 checksum that is compatible with the ESP32's hardware implementation.
- Saves the final image as `received_photo.jpg`.

## Requirements
- Python 3.7+
- `bleak` library

## Installation

1.  **Install Python:** Make sure you have Python 3.7 or newer installed. You can check your version with `python3 --version`.

2.  **Install Dependencies:** Install the required `bleak` library using `pip3`. An up-to-date version is required for MTU negotiation to work correctly.

    If you have an older version of `bleak` installed, upgrade it to the latest version:
    ```bash
    pip3 install --upgrade bleak
    ```

    If you do not have it installed, run:
    ```bash
    pip3 install bleak
    ```

## Usage

1.  **Power On Device:** Ensure your OpenGlass device is powered on and advertising.

2.  **Run the Script:** Execute the client script from your terminal.

    ```bash
    python3 ble_photo_client.py
    ```

The script will automatically scan for the device, connect, request a photo, receive the data, verify the checksum, and save the resulting image in the same directory.
