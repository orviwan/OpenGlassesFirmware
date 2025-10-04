# OpenGlass Firmware v2.0 Implementation Plan

This document tracks the progress of implementing the Firmware v2.0 blueprint.

## Phase 0: Project Setup & Refactoring
- [x] Create this progress tracking file.
- [x] Analyze existing codebase against the new blueprint.
- [x] Created a new state machine module (`state_machine.h`/`.cpp`).
- [x] Updated `config.h` with new BLE UUIDs from the v2.0 blueprint.
- [x] Refactored `ble_handler.h` and `ble_handler.cpp` to implement the new BLE service structure and security model.
- [x] Refactored the main `OpenGlassesFirmware.ino` file to be driven by the new state machine.

## Phase 1: BLE Implementation
- [x] Implemented the full v2.0 GATT service structure, including the custom OpenGlass Service and all standard services.
- [x] Enforced mandatory BLE Bonding for all custom characteristics, enhancing security.
- [x] Implemented the **Device Control** characteristic with a command parser for:
    - [x] Single Photo Capture (`0x01`)
    - [x] Interval Photo Capture (`0x02`, `0x03`)
    - [x] BLE Audio Streaming (`0x10`, `0x11`)
    - [x] Device Reboot (`0xFE`)
- [x] Implemented the **Device Status** characteristic, which now reports firmware version, battery level, and device state, with notifications on change.
- [x] Implemented the **Error Notification** characteristic and added initial error-handling logic.
- [x] Refactored and integrated the existing audio and photo handlers to work with the new state machine and command structure.
- [x] The **Wi-Fi Endpoint** characteristic is in place as a placeholder for Phase 2.

## Phase 2: Wi-Fi Aware & WebSocket Streaming
- [x] Implemented the Wi-Fi activation logic triggered by the BLE commands (`0x20`, `0x21`).
- [x] Created a new `wifi_handler` module to encapsulate Wi-Fi functionality.
- [x] Added a basic implementation for Wi-Fi Aware (NAN) service publishing using ESP-IDF APIs.
- [ ] **Action Required:** The project now mixes Arduino and ESP-IDF APIs. The build environment needs to be configured to correctly compile and link the ESP-IDF Wi-Fi components. This will likely involve custom build flags or a transition to the ESP-IDF build system (e.g., via PlatformIO).
- [ ] Handle Wi-Fi Direct datapath establishment in the NAN event handler.
- [ ] Implement the WebSocket server on the established datapath.
- [ ] Populate and expose the Wi-Fi Endpoint characteristic.
- [ ] Implement the binary streaming protocol over WebSockets for A/V.
- [ ] Implement graceful Wi-Fi teardown.

## Phase 3: State Machine & Integration
- [ ] Implement the formal state machine as defined in the blueprint (IDLE, CONNECTED_BLE, etc.).
- [ ] Integrate all handlers (BLE, Wi-Fi, Camera, Audio) into the state machine.
- [ ] Ensure all state transitions are handled correctly and invalid ones are rejected.
- [ ] Implement power management logic (peripheral gating, Wi-Fi power control).

## Phase 4: Testing & Refinement
- [ ] Update or create new client test scripts for all new functionalities.
- [ ] Perform comprehensive testing of BLE services and bonding.
- [ ] Test Wi-Fi activation, connection, and streaming.
- [ ] Test state transitions and error handling.
- [ ] Profile and optimize power consumption.
- [ ] Bug fixing and final code cleanup.
