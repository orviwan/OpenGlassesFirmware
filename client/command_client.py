import asyncio
import argparse
from bleak import BleakClient, BleakScanner
import sys

# --- Configuration ---
# UUIDs must match the firmware
COMMAND_SERVICE_UUID = "d27157e8-318c-4320-b359-5b8a625c727a"
COMMAND_CHAR_UUID = "ab473a0a-4531-4963-87a9-05e7b315a8e5"

# --- Command Mapping ---
COMMANDS = {
    "take photo": "0x01",
    "start audio": "0x10",
    "stop audio": "0x11",
    "start wifi": "0x20",
    "stop wifi": "0x21",
}

def print_commands():
    """Prints the available commands."""
    print("\nAvailable commands:")
    for cmd in COMMANDS:
        print(f"  - {cmd}")
    print("  - help (to see this list again)")
    print("  - exit (to quit)")
    print()

async def main():
    """Main function to connect and send commands."""
    print("Scanning for OpenGlasses device...")
    device = await BleakScanner.find_device_by_name("OpenGlasses")
    if not device:
        print("Error: Could not find a device named 'OpenGlasses'.")
        return

    print(f"Found OpenGlasses: {device.address}")

    async with BleakClient(device) as client:
        if not client.is_connected:
            print("Error: Failed to connect to the device.")
            return
        
        print("Successfully connected to OpenGlasses.")
        print_commands()

        while True:
            try:
                user_input = await asyncio.to_thread(sys.stdin.readline)
                cmd_text = user_input.strip().lower()

                if not cmd_text:
                    continue

                if cmd_text == "exit":
                    print("Disconnecting...")
                    break
                
                if cmd_text == "help":
                    print_commands()
                    continue

                if cmd_text in COMMANDS:
                    hex_cmd = COMMANDS[cmd_text]
                    byte_cmd = int(hex_cmd, 16).to_bytes(1, 'little')
                    
                    print(f"Sending command: '{cmd_text}' ({hex_cmd})")
                    await client.write_gatt_char(COMMAND_CHAR_UUID, byte_cmd)
                    print("Command sent successfully.")
                else:
                    print(f"Unknown command: '{cmd_text}'. Type 'help' for a list of commands.")

            except KeyboardInterrupt:
                print("\nDisconnecting...")
                break
            except Exception as e:
                print(f"An error occurred: {e}")
                break

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nClient stopped.")
