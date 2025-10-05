# OpenGlasses Python Clients

This directory contains Python scripts to interact with the OpenGlasses firmware. These clients allow you to send commands, receive photos, and stream live audio and video.

## Requirements

- Python 3.7+
- Bluetooth Low Energy (BLE) adapter on your computer.

## Setup

1.  Navigate to this `client` directory in your terminal.
2.  Install the necessary Python libraries using the `requirements.txt` file:

    ```sh
    python3 -m venv venv
    source venv/bin/activate
    pip3 install -r requirements.txt
    ```

## Available Clients

### 1. Interactive Command Client (`command_client.py`)

This is the primary, all-in-one client for interacting with the glasses. It provides a user-friendly interactive prompt for sending all commands, including taking photos.

**Usage:**

Make sure your OpenGlasses device is powered on and advertising.

```sh
python command_client.py
```

The script will automatically find and connect to the "OpenGlass" device. On the first connection, you may be prompted by your operating system to pair with the device; please accept the request.

Once connected, you can type commands (or their corresponding number) and press Enter.

**Commands:**

-   `take photo`: Triggers the camera, transfers the image over BLE, and saves it locally as `photo_YYYYMMDD_HHMMSS.jpg`.
-   `start interval photo`: (Not yet implemented)
-   `stop interval photo`: (Not yet implemented)
-   `start audio`: Starts streaming audio over BLE.
-   `stop audio`: Stops streaming audio over BLE.
-   `start wifi`: Activates the Wi-Fi hotspot and A/V streaming endpoints.
-   `stop wifi`: Deactivates the Wi-Fi hotspot.
-   `reboot`: Reboots the device.
-   `help`: Shows the list of available commands.
-   `exit`: Disconnects from the device and closes the client.

### 2. Wi-Fi A/V Stream Client (`wifi_stream_client.py`)

This client connects to the glasses' Wi-Fi hotspot to display the live video and play the live audio.

**Usage:**

1.  Use the `command_client.py` to send the `start wifi` command.
2.  Connect your computer's Wi-Fi to the network named "OpenGlasses" (password: "openglasses").
3.  Run the stream client:

    ```sh
    python wifi_stream_client.py
    ```

A window will open showing the live video feed, and audio will play through your computer's speakers. Press 'q' in the video window to quit.

### 3. Legacy Clients

The `ble_photo_client.py` and `ble_audio_client.py` are now considered legacy. Their functionality has been merged into the main `command_client.py`. They are kept for reference but are no longer needed for standard operation.

