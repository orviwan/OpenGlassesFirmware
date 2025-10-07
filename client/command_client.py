import asyncio
from bleak import BleakClient, BleakScanner
import sys
import tty
import termios
from datetime import datetime
import logging
from tqdm import tqdm

def get_single_char():
    """Gets a single character from standard input without requiring Enter."""
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(sys.stdin.fileno())
        ch = sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    return ch


# --- Configuration ---
DEVICE_NAME = "OpenGlass"
COMMAND_SERVICE_UUID = "19b10000-e8f2-537e-4f6c-d104768a1214"
COMMAND_CHAR_UUID = "19b10001-e8f2-537e-4f6c-d104768a1214"
PHOTO_CONTROL_UUID = "19b10006-e8f2-537e-4f6c-d104768a1214"
PHOTO_DATA_UUID = "19b10005-e8f2-537e-4f6c-d104768a1214"

# Setup basic logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# --- Global State for Photo Transfer ---
photo_data = bytearray()
transfer_complete = asyncio.Event()
last_sequence_number = -1
pbar = None # Progress bar
client_ble = None # Global client for ACK handler

# --- Command Mapping ---
COMMANDS = [
    {"name": "take photo", "hex": "0x01", "handler": "photo"},
    {"name": "start interval photo", "hex": "0x04", "handler": "command"},
    {"name": "stop interval photo", "hex": "0x05", "handler": "command"},
    {"name": "start audio", "hex": "0x10", "handler": "command"},
    {"name": "stop audio", "hex": "0x11", "handler": "command"},
    {"name": "start wifi", "hex": "0x20", "handler": "command"},
    {"name": "stop wifi", "hex": "0x21", "handler": "command"},
    {"name": "reboot", "hex": "0xFE", "handler": "command"},
]

def print_commands():
    """Prints the available commands."""
    print("\nAvailable commands:")
    for i, cmd_info in enumerate(COMMANDS):
        print(f"  {i+1}: {cmd_info['name']}")
    print("  - h (to see this list again)")
    print("  - q (to quit)")
    print()

def photo_notification_handler(sender, data: bytearray):
    """Handles incoming data from the photo data characteristic."""
    global photo_data, last_sequence_number, pbar

    # End-of-transfer marker
    if len(data) == 2 and data == b'\xff\xff':
        logger.debug("End-of-transfer marker received.")
        if pbar:
            # Make sure the bar shows 100% on completion
            pbar.n = pbar.total
            pbar.refresh()
            pbar.close()
        transfer_complete.set()
        return

    # Start-of-transfer marker with file size
    if len(data) == 6 and data[:2] == b'\xfe\xff':
        photo_data.clear() # Clear previous photo data
        total_size = int.from_bytes(data[2:], 'little')
        logger.debug(f"Start-of-transfer marker received. Total size: {total_size} bytes.")
        pbar = tqdm(total=total_size, unit='B', unit_scale=True, desc="Receiving Photo")
        return

    if len(data) < 2:
        logger.warning(f"Received a runt packet of size {len(data)}. Ignoring.")
        return
    
    sequence_number = int.from_bytes(data[:2], 'little')
    payload = data[2:]

    if last_sequence_number != -1 and sequence_number != last_sequence_number + 1:
        logger.warning(f"Missed a packet! Expected sequence {last_sequence_number + 1}, but got {sequence_number}.")

    last_sequence_number = sequence_number
    photo_data.extend(payload)
    if pbar:
        pbar.update(len(payload))
    logger.debug(f"Received chunk {sequence_number}, total size: {len(photo_data)}")

async def handle_photo_command(client: BleakClient, cmd_info: dict):
    """Handles the entire photo taking and receiving process."""
    global photo_data, transfer_complete, last_sequence_number, pbar
    # Reset state for the new transfer
    photo_data.clear()
    transfer_complete.clear()
    last_sequence_number = -1
    if pbar:
        pbar.close()
    pbar = None

    hex_cmd = cmd_info['hex']
    cmd_name = cmd_info['name']
    byte_cmd = int(hex_cmd, 16).to_bytes(1, 'little')
    
    logger.info(f"Subscribing to photo data notifications on {PHOTO_DATA_UUID}...")
    await client.start_notify(PHOTO_DATA_UUID, photo_notification_handler)
    logger.info("Subscription successful.")

    await asyncio.sleep(1)

    # Silence the bleak logger during transfer to avoid spam
    logging.getLogger("bleak").setLevel(logging.WARNING)

    logger.info(f"Sending '{cmd_name}' command ({hex_cmd}) to {PHOTO_CONTROL_UUID}...")
    await client.write_gatt_char(PHOTO_CONTROL_UUID, byte_cmd, response=True)
    logger.info("Command sent.")
    await asyncio.sleep(0.1) # Yield to event loop to process initial ACK

    try:
        logger.info("Waiting for photo data transfer to complete...")
        await asyncio.wait_for(transfer_complete.wait(), timeout=30.0)
        # The progress bar handles the "complete" status message
    except asyncio.TimeoutError:
        logger.error("Timeout occurred while waiting for photo data.")
    finally:
        # Restore the bleak logger's level
        logging.getLogger("bleak").setLevel(logging.INFO)
        try:
            await client.stop_notify(PHOTO_DATA_UUID)
        except Exception as e:
            logger.warning(f"An error occurred while unsubscribing: {e}")
        
        if pbar:
            pbar.close() # Ensure pbar is closed on error or timeout

    if photo_data:
        filename = f"photo_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
        with open(filename, "wb") as f:
            f.write(photo_data)
        logger.info(f"Photo saved as {filename} ({len(photo_data)} bytes)")
    else:
        logger.warning("No photo data was received.")

async def handle_generic_command(client: BleakClient, cmd_info: dict):
    """Handles sending a standard command."""
    hex_cmd = cmd_info['hex']
    cmd_name = cmd_info['name']
    byte_cmd = int(hex_cmd, 16).to_bytes(1, 'little')
    
    logger.info(f"Sending command: '{cmd_name}' ({hex_cmd})")
    await client.write_gatt_char(COMMAND_CHAR_UUID, byte_cmd, response=True)
    logger.info("Command sent successfully.")

async def main():
    """Main function to connect and send commands."""
    global client_ble
    logger.info("Scanning for OpenGlass device...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME)
    if not device:
        logger.error(f"Error: Could not find a device named '{DEVICE_NAME}'.")
        return

    logger.info(f"Found OpenGlass: {device.address}")

    async with BleakClient(device, timeout=20.0) as client:
        client_ble = client
        if not client.is_connected:
            logger.error("Error: Failed to connect to the device.")
            return
        
        logger.info(f"Successfully connected to OpenGlasses. Negotiated MTU: {client.mtu_size} bytes.")
        
        logger.info("Attempting to pair...")
        try:
            paired = await client.pair()
            if paired:
                logger.info("Pairing successful.")
            else:
                logger.warning("Pairing not successful. May already be paired.")
        except Exception as e:
            logger.warning(f"Pairing error: {e}. Continuing...")

        print_commands()

        while True:
            try:
                print("\rWaiting for command... ", end="", flush=True)
                char_input = await asyncio.to_thread(get_single_char)
                print(f"'{char_input}'") # Echo the character

                # Exit on 'q' or Ctrl+C (which is '\x03' in raw mode)
                if char_input.lower() == 'q' or char_input == '\x03':
                    print("Exiting...")
                    break
                
                if char_input.lower() == 'h':
                    print_commands()
                    continue

                cmd_to_run = None
                if char_input.isdigit() and 1 <= int(char_input) <= len(COMMANDS):
                    cmd_to_run = COMMANDS[int(char_input) - 1]
                
                if cmd_to_run:
                    print(f"Executing: {cmd_to_run['name']}")
                    if cmd_to_run["handler"] == "photo":
                        await handle_photo_command(client, cmd_to_run)
                    else:
                        await handle_generic_command(client, cmd_to_run)
                    print_commands() # Show commands again after action
                else:
                    logger.warning(f"Unknown command key: '{char_input}'. Press 'h' for help.")

            except Exception as e:
                logger.error(f"An error occurred in the main loop: {e}")
                break
    
    logger.info("Disconnected.")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nClient stopped.")

