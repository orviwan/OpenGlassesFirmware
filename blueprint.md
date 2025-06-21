# OpenGlass Firmware Blueprint

## Overview

This document outlines the technical architecture of the OpenGlass firmware. It is intended for developers who want to understand the design principles, component interactions, and overall structure of the system.

## Core Architecture

The firmware is built on the ESP32 Arduino Core and leverages FreeRTOS for multitasking. The architecture is modular, with distinct handlers for different hardware components and functionalities.

- **`ble_handler`**: Manages all BLE services, characteristics, and connection events.
- **`photo_manager`**: Handles the logic for photo capture, including single-shot and interval modes.
- **`camera_handler`**: Interfaces with the camera module for initialization and image capture.
- **`audio_handler`**: Manages the microphone and audio buffer.
- **`audio_ulaw`**: Implements the μ-law (G.711) audio encoding and streaming.
- **`led_handler`**: Controls the onboard LED for status indication.
- **`logger`**: Provides a thread-safe logging mechanism using a FreeRTOS mutex.

## Task Management

To ensure non-blocking operation and responsiveness, the firmware uses dedicated FreeRTOS tasks for data streaming:

- **Photo Streaming Task**: Manages the process of capturing a photo and sending it over BLE. This task is created on startup and suspended until a photo is requested.
- **Audio Streaming Task**: Handles real-time audio capture, encoding, and streaming. This task is also suspended until a client subscribes to audio notifications.

This task-based approach allows for concurrent photo and audio streaming, although bandwidth is shared.

## BLE Protocol

The communication protocol is designed for simplicity and efficiency.

- **MTU Negotiation**: The firmware supports MTU negotiation to allow for larger data packets, significantly improving photo transfer speed. The client is expected to request a larger MTU (e.g., 247 bytes) upon connection.
- **Photo Transfer**: Photos are sent in chunks, with each chunk prefixed by a 2-byte frame number. The transfer is terminated by a special `0xFFFF` marker. There is no CRC check; data integrity is handled by the BLE link layer.
- **Control Commands**: Simple, single-byte commands are used to control features like photo capture.

## Power Management

Power efficiency is a key consideration. The firmware employs several strategies to conserve battery life:

- **Peripheral Management**: The camera and microphone are only initialized when needed and are de-initialized when the client disconnects or stops using them.
- **Sleep Modes**: The ESP32's light sleep and deep sleep modes are used to reduce power consumption during idle periods.
- **Optimized BLE Parameters**: Advertising and connection intervals are configured to balance responsiveness and power usage.

## Client-Side Considerations

A client interacting with the firmware should:

1.  Scan for the "OpenGlass" device.
2.  Connect and negotiate a higher MTU for better performance.
3.  Subscribe to notifications for the desired data characteristics (photo or audio).
4.  Use the control characteristics to initiate actions.
5.  Be prepared to handle data chunks and reassemble them (e.g., for photos).