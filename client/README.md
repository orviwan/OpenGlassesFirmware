# OpenGlass Python BLE Client

This directory contains a Python script (`ble_photo_client.py`) for testing the photo capture functionality of the OpenGlass firmware over Bluetooth Low Energy (BLE).

## Features
- Connects to the "OpenGlass" BLE device.
- Requests a single photo capture.
- Receives the JPEG image data in chunks.
- Performs basic validation of the received data to ensure it starts and ends with JPEG markers.
- Saves the final image with a timestamp (e.g., `photo_20250621_143000.jpg`).
- Displays detailed performance metrics after each transfer (scan time, connect time, download time, speed).

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

The script will automatically scan for the device, connect, request a photo, receive the data, validate it, and save the resulting image. It will then print a summary of the transfer performance.
