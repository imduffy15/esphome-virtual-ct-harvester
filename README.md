# Virtual CT Harvester for ESPHome

Independent community project; not affiliated with or endorsed by myenergi.
The harvi and zappi names identify myenergi products and the protocol targeted
by this experimental implementation. Virtual CT Harvester is this project’s own name.

A configurable Home Assistant radio gateway for the **LILYGO T3 LoRa32 V1.6.1
(classic ESP32, SX1276, 868 MHz, 0.96-inch OLED)**. Works with power sensors from
any Home Assistant integration, with no vendor-specific dependencies.

**Experimental:** pairs with a Zappi as a Harvi and sends three Home Assistant
power readings as its CT1–CT3, with no extra hardware. The board has no mains
reference; it infers the Zappi's mains phase from the Zappi's own radio
broadcasts (see [How it works](#how-it-works)). It has been validated on one
installation (single phase, 230 V, 50 Hz), including an overnight soak from
export through 9 kW import. Treat it as experimental until you have checked
its readings against your own meter.

## Set up a preflashed device

You need the device's private setup card, your 2.4 GHz Wi-Fi password and Home
Assistant. Attach the 868 MHz antenna, then power the board with a USB cable.

1. On your phone, join **Virtual CT Harvester …** using the password on the setup card.
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
5. Check **Power Input 1/2/3** against the originals. kW inputs are converted to W.
   Signs are preserved unless you set **Power N Sign** to **Inverted**; see
   [Virtual CT setup](#virtual-ct-setup) for the sign each CT needs. Power Input N is sent as CTN; see
   [Virtual CT setup](#virtual-ct-setup) for which input goes where. **Power Inputs Recently Received** indicates that all configured
   inputs delivered finite values within **Input Receipt Timeout** (120 seconds
   by default). This measures receipt age, not source measurement age. HA may not
   resend unchanged readings, so constant inputs can time out.

The builder supplies the setup card. Each device has its own credentials and
stable virtual serial; retain the card for future setup and recovery.

## Radio reception and pairing test

Press **Find Channel**. With **Reference Harvi Serial** left at 0, the scan locks
onto the first channel carrying a Zappi's status broadcasts; it also runs at
first boot. If a working genuine Harvi is still installed, enter its serial
under **Reference Harvi Serial** first: the scan then waits for two distinct
packets from that Harvi. Your real Harvi needs enough CT current to power its
transmitter. The scan takes up to 96 seconds. Alternatively, choose **Receive
Frequency** manually, for example if a neighbour's myenergi system is on another
channel. Candidate numbers are not verified Zappi channel labels.

Open **Linked Devices → Pairing** on Zappi. Press **Start Pairing** on the device's
page or in HA. You can also hold and release BOOT for 0.5–3 seconds after startup.
Select the virtual serial shown on the OLED/card and confirm it on Zappi.
Advertisements stop after two minutes, on transmit failure, or when you press
**Stop Pairing**. A sent counter alone does not confirm Zappi acceptance.

Pairing advertises no CT readings. Measurements start only when you turn on
**Virtual CT Output** (below).

## Virtual CT setup

| Input | Sent as | Set on the Zappi | Sign |
| --- | --- | --- | --- |
| Power 1 | CT1 | Grid | positive import, negative export |
| Power 2 | CT2 | **Generation (required)** | positive generation |
| Power 3 | CT3 | **AC Battery** (or Storage, or any other type) | battery: positive discharging, negative charging |

myenergi CTs are positive when power flows towards the house. Many battery
sensors report charging as positive (Anker SOLIX does, for example); set
**Power 3 Sign** to **Inverted** for those. With the wrong sign, the myenergi app
shows a charging battery as supplying the house and overstates the house load.

1. Pair (above). On the Zappi, set this device's CT2 to **Generation** and
   CT1/CT3 as in the table. CT2 must be the **only** Generation CT on that
   Zappi: its generation total is the board's phase feedback. Leave Power 2
   blank if you have no generation; CT2 must still be set to Generation.
2. Remove the Grid role from any other Grid CT, or the Zappi adds both.
3. Turn on **Virtual CT Output**. Gateway Status shows *waiting for the Zappi to
   list this serial*, then *acquiring phase*, then *phase locked, output live*.
   Lock takes 10–45 s. The OLED shows `CT OUT - LOCKED`.
4. Once no other device has a Grid CT on the Zappi, turn on **Grid Total Is
   Virtual Only**. The grid total then adds a second feedback channel.
5. Compare the Zappi's readings with your inputs, in both import and export.

## Solar and battery in the myenergi app

The app's energy-flow screen surrounds a leaf (the green-energy share) with
bubbles. Every flow is drawn through the leaf. Grid, house and chargers always
appear. The **solar** bubble appears only when a CT on the account has type
Generation, and the **battery** bubble only when one has type **AC Battery**.
Storage shows a separate storage icon, and Gen & Battery a combined one. These
types come from the myenergi cloud, which learns them from the Zappi. That
causes two traps:

- **This device must be the only Harvi linked to the Zappi.** The Zappi reports
  one Harvi's CT types per update: the first linked Harvi that has new data. A
  genuine Harvi linked ahead of this device is nearly always reported instead.
  This device's types then never reach the cloud, even though the Zappi uses
  its readings. In one installation, about 3,000 consecutive uplinks all
  carried the genuine Harvi.
- **The serial must be one the cloud accepts.** The myenergi website rejected a
  generated serial as invalid. Reusing a retired genuine Harvi's serial is
  the verified route. A generated serial as the only Harvi has not been tested.

To replace a genuine Harvi:

1. Retire the genuine Harvi permanently. Unclip all of its CT clamps: it powers
   itself from them. If both transmit the same serial, the Zappi mixes their
   readings.
2. On the Zappi, remove every Harvi from **Linked Devices**.
3. Give this device the genuine Harvi's serial (printed on its label, and shown
   on the Zappi's Linked Devices list). For a new unit, run
   `python prepare_device.py --serial NUMBER`. For an existing unit, change
   `virtual_harvi_serial` in its `secrets.yaml`, keeping the other values. Then
   build and flash. Leave **Reference Harvi Serial** at 0.
4. Pair and set the CT types as in [Virtual CT setup](#virtual-ct-setup).
5. Check that the app shows solar and battery bubbles, and that the house
   figure is roughly solar + grid import − battery charging − charger power.

The Zappi accepts a linked serial from any transmitter: it keeps no pairing key,
and in the firmware emulator it accepted backward and repeated packet sequences. Zappi CT type codes, as sent to the
cloud: 0 None, 1 Grid, 2 Generation, 3 Storage, 4 Gen & Battery, 5 Internal
Load, 6 Monitor, 7 AC Battery. Not analysed: whether the Zappi's own charging
behaviour differs between Storage and AC Battery.

## How it works

The Zappi computes `power = V × floor(|phasor| / 1.19) × cos(angle − its mains
phase)` for each CT. So each reading has to be rotated to the Zappi's mains
phase at the moment the packet arrives.

- **Feedback:** the Zappi broadcasts its grid and generation totals over RF once
  a second. The board learns its own Zappi from the record that lists this
  virtual serial. Frames from neighbouring installations are ignored.
- **Pilot:** CT2 always carries an extra 300 W at 90° ± 3° to its measurement.
  When locked, this shifts the generation reading by about ±16 W. Any phase
  error shows up in the generation total, so the loop stays observable at
  night.
- **Acquisition:** a least-squares fit over mains frequency, using golden-angle
  phase steps. **Tracking:** a Kalman filter on phase and frequency.
- **Timing:** one packet 450 ms after each heard total, in a quiet slot. The
  radio driver starts transmitting at a scheduled microsecond, so the angle is
  computed for the exact send time.

## Safety behaviour

- CT1 and CT3 are sent **only while the phase is locked**. Otherwise they are
  omitted, and the Zappi reads 0. A reading at an unknown phase is never sent.
- If feedback stops briefly while the tracker is still consistent, readings
  continue on the last good phase for up to 15 s while its uncertainty stays
  under 25° (`CT OUT - HOLDING`). If the feedback shows the phase has moved,
  they are withheld at once: a held phase is then wrong and can flip the sign.
- While acquiring, CT2 carries only the pilot, so it reads within ±300 W.
- If a configured input has no finite value since the HA connection
  (re)started, all measurements are withheld and the pilot keeps the lock.
  An unconfigured input is sent as 0 W.
- If the board stops transmitting, the Zappi holds the last values for at least
  34 s, then clears them.
- Airtime: about 4 ms per packet, one packet every 1–3 s, at 2 dBm.
- Residual risk: lock is inferred, not measured against mains. A source that
  freezes in HA without going unavailable keeps being sent. Re-check the Grid
  reading after any firmware change before relying on it for charging control.

Weak reception of the Zappi's broadcasts is the main cause of lock loss.
Keep the antenna vertical and clear of inverters and metal enclosures.
Diagnostics: **Phase Locked**, **Phase Uncertainty**, **Phase Fit Gain** (≈ 1.00
when the scaling is right), **Virtual CT Held Packets**, **Radio Noise Floor**,
**Receiver Restarts** and **Zappi Serial**.

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
esphome compile virtual-ct-harvester.yaml
esphome upload virtual-ct-harvester.yaml --device /dev/ttyUSB0
```

Replace the port with your board's port. The first build downloads the ESP32
build toolchain and web interface assets. The font and radio component are bundled.
`prepare_device.py` creates unique `secrets.yaml` and a private `device-card.txt`;
it refuses to overwrite existing credentials. An optional `--serial NUMBER`
selects a specific nonzero 24-bit identity. Use a retired genuine Harvi's serial to
get the app's solar and battery bubbles (see
[Solar and battery in the myenergi app](#solar-and-battery-in-the-myenergi-app)), and
never one that another working device still transmits.
Never flash multiple units with the same generated secrets: prepare a separate
working directory and setup card per unit. Preserve each unit's secrets for updates.

Earlier checkouts used `harvi-radio.yaml`; the main file is now
`virtual-ct-harvester.yaml`. Existing secrets remain compatible: keep the same keys,
virtual serial and device name when updating an already configured unit.

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
card is needed. All transmissions use 2 dBm.

[Licenses and third-party notices](NOTICE.md).
