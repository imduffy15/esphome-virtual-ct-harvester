#!/usr/bin/env python3
"""Generate unique build credentials and a private setup card for one device."""
import argparse
import base64
import os
from pathlib import Path
import secrets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', type=int, help='Built-in serial number for this device, from 1 to 16777215. The owner can change it later on the device web page. Default: a random serial number.')
    args = parser.parse_args()
    serial = args.serial if args.serial is not None else secrets.randbelow(0xFFFFFF) + 1
    if not 1 <= serial <= 0xFFFFFF:
        parser.error('--serial must be between 1 and 16777215')
    root = Path(__file__).resolve().parent
    config = root / 'secrets.yaml'
    card = root / 'device-card.txt'
    if config.exists() or card.exists():
        parser.error('secrets.yaml or device-card.txt already exists. Keep the existing credentials, or prepare this device in a new directory.')
    api_key = base64.b64encode(secrets.token_bytes(32)).decode()
    setup_password = secrets.token_hex(8)
    name = f'virtual-ct-harvester-{serial:06x}'
    ssid = f'Virtual CT Harvester {serial:06x}'
    yaml = f'''# Private credentials for this device only. Keep them for future updates.
device_name: "{name}"
setup_ap_ssid: "{ssid}"
setup_password: "{setup_password}"
harvi_radio_api_key: "{api_key}"
virtual_harvi_serial: {serial}
'''
    instructions = f'''Virtual CT Harvester — private device setup card

Keep this card private. It contains the credentials for this device.

Virtual serial: {serial}
Setup Wi-Fi: {ssid}
Setup Wi-Fi password: {setup_password}
Wi-Fi setup page: http://192.168.4.1

After you join your home Wi-Fi network:
Device address: http://{name}.local (or the IP address shown on the display)
Web page username: admin
Web page password: {setup_password}
Encryption key (Home Assistant and firmware updates): {api_key}

Set up the device:
1. Open the device address in a browser and sign in.
2. Follow the setup guide on the page. It works with or without Home
   Assistant.
3. To use Home Assistant power readings, add the device in the Home Assistant
   ESPHome integration with the encryption key above.
For details, see README.md.

Reset saved settings: on the web page, click Reset to factory settings, or
after the device starts normally, hold BOOT for 10 seconds. This erases saved settings, including Wi-Fi, and returns the device
to setup. Don't hold BOOT while you power on the device.
'''
    os.umask(0o077)
    for path, value in ((config, yaml), (card, instructions)):
        with path.open('x') as output:
            output.write(value)
    print('Created secrets.yaml and device-card.txt. Keep both private, and give the setup card to the device owner.')


if __name__ == '__main__':
    main()
