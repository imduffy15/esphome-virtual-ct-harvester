# Harvi Radio for ESPHome

Independent community project; not affiliated with or endorsed by myenergi.
References to harvi and zappi identify the devices and protocol targeted by this
experimental implementation. They are not this project’s manufacturer or sponsor.

A configurable Home Assistant radio gateway for the **LILYGO T3 LoRa32 V1.6.1
(classic ESP32, SX1276, 868 MHz, 0.96-inch OLED)**. Works with power sensors from
any Home Assistant integration, with no vendor-specific dependencies.

**Experimental commissioning build:** supports Wi-Fi setup, power input display,
radio reception and manually started pairing advertisements. **Usable virtual CT
measurements are not implemented.** Phase timing and calibration remain unfinished,
and physical hardware/RF testing is pending. This is not yet a finished product
or a replacement for your existing Grid CT.

## Set up a preflashed device

You need the device's private setup card, your 2.4 GHz Wi-Fi password and Home
Assistant. Attach the 868 MHz antenna, then power the board with a USB cable.

1. On your phone, join **Harvi Setup …** using the password on the setup card.
   The setup network appears when the device has no working Wi-Fi connection.
   If your phone says it has no internet, stay connected. Open **http://192.168.4.1**
   if the Wi-Fi setup page does not open automatically. Select your home network
   and enter its password. No firmware editing or USB connection to a computer
   is needed for this step.
2. Rejoin your home Wi-Fi. Open the address on the OLED or the `.local` address
   on the card. Log in as **admin**, with the card's setup password. The settings
   page is served by the device and works without an internet connection.
3. In Home Assistant, open **Settings → Devices & services** and configure the
   discovered ESPHome device. If it is not discovered, add the ESPHome integration
   and enter the device's IP. Use the **API encryption key** on the card when asked.
4. On the device's settings page, enter up to three **Power 1/2/3 Entity** IDs
   from Home Assistant, such as `sensor.grid_power`. Find an entity ID in that
   entity's settings in HA. Select each source's **W** or **kW** unit. Leave unused
   inputs blank. Press **Apply Input Settings and Restart**, then refresh after
   the device reconnects. You can also edit these settings through HA.
5. Check **Power Input 1/2/3** against the originals. kW inputs are converted to W;
   positive and negative signs are preserved. Input numbers do not assign CT roles
   on Zappi. **Power Inputs Recently Received** indicates that all configured
   inputs delivered finite values within **Input Receipt Timeout** (120 seconds
   by default). This measures receipt age, not source measurement age. HA may not
   resend unchanged readings, so constant inputs can time out.

The builder supplies the setup card. Each device has its own credentials and
stable virtual serial; retain the card for future setup and recovery.

## Radio reception and pairing test

If you have a genuine Harvi, enter its serial under **Reference Harvi Serial**
and press **Find Genuine Harvi Channel**. The scan takes up to 96 seconds and
saves a frequency after two distinct packets from that Harvi. Alternatively,
choose **Receive Frequency** manually. Candidate numbers are not verified Zappi
channel labels. Your real Harvi needs enough CT current to power its transmitter.

Open **Linked Devices → Pairing** on Zappi. Press **Start Pairing** on the device's
page or in HA. You can also hold and release BOOT for 0.5–3 seconds after startup.
Select the virtual serial shown on the OLED/card and confirm it on Zappi.
Advertisements stop after two minutes, on transmit failure, or when you press
**Stop Pairing**. A sent counter alone does not confirm Zappi acceptance.

This build advertises **no CT readings**. Keep its CT roles unused and retain your
existing Grid measurement. Successful pairing does not enable measurement TX.

## Recovery

- **Wi-Fi changed or unavailable:** reconnect to the setup network and enter
  the new Wi-Fi details. Its password is on the original card.
- **Reset saved settings:** after normal startup, hold BOOT for 10 seconds.
  This erases saved Wi-Fi, input entities, units and frequency settings and returns
  to setup. The device's built-in identity and card credentials stay unchanged.
  Do not hold BOOT while powering on: that enters the ESP32 USB bootloader.
- **No power inputs:** check the HA integration connection, entity IDs and units;
  press Apply Input Settings and Restart after editing. Missing inputs do not
  prevent a pairing test.
- **No packets / Radio failed:** check the board variant, antenna and frequency.
  A blank OLED can be an I²C/display issue; USB logs work independently.

Allow a minute for ordinary setting changes to be saved before unplugging.
Apply Input Settings and Restart explicitly saves pending settings first.

## Build and flash your own

The complete image has been build-checked with **ESPHome 2026.9.0**.
Use Python **3.12–3.14** and the pinned ESPHome version. Download or clone this
repository into its own directory, then:

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python prepare_device.py
esphome compile harvi-radio.yaml
esphome upload harvi-radio.yaml --device /dev/ttyUSB0
```

Replace the port with your board's port. The first build downloads the ESP32
build toolchain and web interface assets. The font and radio component are bundled.
`prepare_device.py` creates unique `secrets.yaml` and a private `device-card.txt`;
it refuses to overwrite existing credentials. An optional `--serial NUMBER`
selects a specific nonzero 24-bit identity distinct from your other devices.
Never flash multiple units with the same generated secrets: prepare a separate
working directory and setup card per unit. Preserve each unit's secrets for updates.

For ESPHome Device Builder, copy the YAML, packages, components, fonts and that
unit's secrets into its configuration directory, retaining existing unrelated
secrets. Choose **Install** and USB for the first flash. A separate USB installer
needs the **factory** image; later OTA updates use that unit's existing credentials.
Do not distribute credential-bearing binaries publicly. Example secrets are
only for build validation and must not be used for real devices.

| Hardware connection | GPIO |
| --- | --- |
| SPI clock / MISO / MOSI | 5 / 19 / 27 |
| Radio CS / reset / DIO0 | 18 / 23 / 26 |
| OLED SDA / SCL, address 0x3c | 21 / 22 |
| BOOT button | 0 |

Use the classic ESP32/SX1276 868 MHz board above. SX1262, ESP32-S3 and 433 MHz
variants need different support. The radio uses **FSK**, not LoRa. GPIO5 is a
manufacturer-wired strapping pin; avoid adding loads to it during startup. No SD
card is needed. Radio advertisements use 2 dBm, one small packet every two seconds.

[Licenses and third-party notices](NOTICE.md).
