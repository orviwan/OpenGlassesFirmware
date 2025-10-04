# OpenGlasses Python Clients

This directory contains Python scripts to interact with the OpenGlasses firmware. These clients allow you to send commands, receive photos, and stream live audio and video.

## Requirements

- Python 3.7+
- Bluetooth Low Energy (BLE) adapter on your computer.

## Setup

1.  Navigate to this `client` directory in your terminal.
2.  Install the necessary Python libraries using the `requirements.txt` file:

    ```sh
    pip install -r requirements.txt
    ```

## Available Clients

### 1. Interactive Command Client

This is the recommended client for sending commands to the glasses. It provides a user-friendly interactive prompt.

**Usage:**

```sh
python command_client.py
```

The script will automatically find and connect to the "OpenGlasses" device. Once connected, you can type commands and press Enter.

**Commands:**

-   `take photo`: Triggers the camera to take a picture.
-   `start audio`: Starts streaming audio over BLE (for use with `ble_audio_client.py` in streaming mode).
-   `stop audio`: Stops streaming audio over BLE.
-   `start wifi`: Activates the Wi-Fi hotspot and A/V streaming endpoints.
-   `stop wifi`: Deactivates the Wi-Fi hotspot.
-   `help`: Shows the list of available commands.
-   `exit`: Disconnects from the device and closes the client.

### 2. Wi-Fi A/V Stream Client

This client connects to the glasses' Wi-Fi hotspot to display the live video and play the live audio.

**Usage:**

1.  Use the `command_client.py` to send the `start wifi` command.
2.  Connect your computer's Wi-Fi to the network named "OpenGlasses" (password: "openglasses").
3.  Run the stream client:

    ```sh
    python wifi_stream_client.py
    ```

A window will open showing the live video feed, and audio will play through your computer's speakers. Press 'q' in the video window to quit.

### 3. BLE Photo Client

This client connects to the glasses and waits to receive a photo over BLE.

**Usage:**

1.  Run the photo client:

    ```sh
    python ble_photo_client.py
    ```

2.  While the client is running, use the `command_client.py` to send the `take photo` command.
3.  The photo will be transferred and saved as `received_photo.jpg` in the `client` directory.

### 4. BLE Audio Client (Legacy)

This client can be used to stream audio over BLE or to send a single command. The interactive `command_client.py` is recommended for sending commands.

**Usage (to send a single command):**

```sh
# Example: Send the 'start wifi' command (0x20)
python ble_audio_client.py --command 0x20
```
