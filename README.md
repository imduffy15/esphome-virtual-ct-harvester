# Virtual CT Harvester for ESPHome

Independent community project; not affiliated with or endorsed by myenergi.
The harvi and zappi names identify myenergi products and the protocol targeted
by this experimental implementation. Virtual CT Harvester is this project’s own name.

A radio gateway for the **LILYGO T3 LoRa32 V1.6.1 (classic ESP32, SX1276,
868 MHz, 0.96-inch OLED)**. It is set up and run from its own web page, and
takes power readings from any Home Assistant sensor or from any device or
script that can send an HTTP request. No vendor-specific dependencies.

**Experimental:** pairs with a Zappi as a Harvi and sends three power readings
as its CT1–CT3, with no extra hardware. The board has no mains
reference; it infers the Zappi's mains phase from the Zappi's own radio
broadcasts (see [How it works](#how-it-works)). It has been validated on one
installation (single phase, 230 V, 50 Hz), including an overnight soak from
export through 9 kW import. Treat it as experimental until you have checked
its readings against your own meter.

## Set up a preflashed device

You need the device's private setup card and your 2.4 GHz Wi-Fi password.
Home Assistant is optional. Attach the 868 MHz antenna, then power the board
with a USB cable.

1. **Wi-Fi.** On your phone, join **Virtual CT Harvester …** using the password
   on the setup card. If the Wi-Fi page doesn't open by itself, open
   **http://192.168.4.1**. Pick your home network and enter its password. If
   your phone says the network has no internet, stay connected.
2. **Open the device's page.** Rejoin your home Wi-Fi and open the address on
   the OLED (or the `.local` address on the card). Log in as **admin** with the
   card's password. The page works without an internet connection.
3. **Follow the setup guide on the page.** It ticks off each step as the device
   sees it happen:
   1. *Find the Zappi:* done automatically on first start.
   2. *Pair:* press **Start pairing**, then on the Zappi open **Linked Devices →
      Pairing** and confirm the serial shown. The step completes when the Zappi
      lists the device.
   3. *Set the CT types on the Zappi* (table below).
   4. *Connect your power readings* from Home Assistant or by HTTP push (below).
   5. *Turn on Send to Zappi.* The status goes *Locking on* → *Live*, usually
      within 10–45 seconds. Then compare the Zappi's grid figure with your
      meter, in both import and export.

The builder supplies the setup card. Each device has its own credentials;
keep the card for recovery.

## The device's web page

Everything is on the page at `http://<device>/`, with or without Home
Assistant:

- **Status** with the next step to take, and a hint if something is stuck.
- **Readings:** the three values being sent, where each came from and how old it is.
- **Send to Zappi:** the output switch.
- **Settings:** readings source, entities, units and signs. Saving applies at
  once; changing a Home Assistant entity restarts the device (about 15 s) and
  the page reconnects by itself.
- **Advanced:** device serial, grid-only feedback, reference Harvi serial.
- **Radio channel**, **Diagnostics** (phase, radio and packet counters) and
  **Device** (restart, factory reset).

The same JSON API backs the page, if you want to script it (digest auth,
user `admin`): `GET /api/status`, and `POST /api/<command>` with a JSON body
for `output` `{"on": true}`, `pair`, `stop_pairing`, `scan`,
`channel` `{"channel": 0}`, `settings`, `restart` and `factory_reset`.

## Power readings

| Reading | Sent as | Set on the Zappi | Sign |
| --- | --- | --- | --- |
| Grid | CT1 | Grid | positive import, negative export |
| Solar | CT2 | **Generation (required)** | positive generation |
| Battery | CT3 | **AC Battery** (or Storage, or any other type) | battery: positive discharging, negative charging |

myenergi CTs are positive when power flows towards the house. Many battery
sensors report charging as positive (Anker SOLIX does, for example); tick
**Invert sign** for those. With the wrong sign, the myenergi app shows a
charging battery as supplying the house and overstates the house load.

Leave out readings you don't have; they are sent as 0 W. A reading you did
set up but that isn't available holds back **all** measurements until it is.

### From Home Assistant

Add the device in **Settings → Devices & services** (it's discovered; use the
card's encryption key). On the device's page choose **Home Assistant** and
enter each entity ID, such as `sensor.grid_power` (find it in the entity's
settings in HA). **Auto** takes W or kW from the entity's unit; choose W or kW
if the entity has none.

### By HTTP push (no Home Assistant)

Choose **HTTP push**. Any device or script on your network can then send
readings, in watts, to `/api/power`, authenticated with the card's password
as `key` (or digest auth):

```sh
curl -X POST http://virtual-ct-harvester-XXXXXX.local/api/power \
  -d key=SETUP_PASSWORD -d grid=1520 -d solar=2310 -d battery=-400
```

Form fields, query parameters and a JSON body all work. Send everything you
have in one request, every few seconds. Fields you leave out keep their last
value; one never sent is unused. If no reading arrives within **Readings
expire after** (30 s by default), measurements are held back until one does.
The page shows a ready-made Shelly script for, for example, a Pro 3EM on the
grid.

## Home Assistant entities

The device keeps its Home Assistant footprint small:

| Entity | Kind | |
| --- | --- | --- |
| Status | sensor | *Live*, *Locking on*, *Waiting for readings*, *Not paired*, … |
| Zappi Link | binary sensor (connectivity) | on while live readings reach the Zappi |
| Send to Zappi | switch | the output |
| Start Pairing | button (config) | |
| Restart | button (diagnostic) | |
| Phase Uncertainty, Radio Noise Floor, Wi-Fi Signal, Uptime | diagnostic | disabled by default |

Settings and the detailed counters live on the device's page.

## Radio channel and pairing

The channel search runs at first start and locks onto the first channel that
carries a Zappi's status broadcasts. It takes up to 96 seconds. Search again
from **Radio channel** if the Zappi was moved, or pick a channel if a
neighbour's myenergi system is on another one. Channel numbers are this
project's candidates, not Zappi channel labels. If a working genuine Harvi is
still installed, you can enter its serial under **Advanced → Reference Harvi
serial**; the search then waits for two distinct packets from it (it needs
enough CT current to power its transmitter).

Pairing advertises for two minutes, until a transmit failure, or until you
press **Stop pairing**. A short (0.5–3 s) press of BOOT also starts it.
Pairing sends no CT readings; measurements start only with **Send to Zappi**.

On the Zappi, CT2 must be the **only** Generation CT: its generation total is
the device's phase feedback. Set CT2 to Generation even without solar. Remove
the Grid role from any other Grid CT, or the Zappi adds both. Once no other
device has a Grid CT on the Zappi, turn on **Advanced → Grid total comes only
from this device** for a second feedback channel.

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
3. On the device's page, open **Settings → Advanced** and set **Device serial**
   to the genuine Harvi's serial (printed on its label, and shown on the
   Zappi's Linked Devices list), then **Save**. No reflashing is needed; a
   factory reset returns to the built-in serial. Leave **Reference Harvi
   serial** at 0.
4. Pair again and set the CT types as in [Power readings](#power-readings).
5. Check that the app shows solar and battery bubbles, and that the house
   figure is roughly solar + grid import − battery charging − charger power.

Never use a serial that another working device still transmits.

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
  under 25° (status *Live (holding)*). If the feedback shows the phase has moved,
  they are withheld at once: a held phase is then wrong and can flip the sign.
- While acquiring, CT2 carries only the pilot, so it reads within ±300 W.
- If a configured reading has no usable value (none since the HA connection
  (re)started, unit unknown, or a pushed reading older than its expiry), all
  measurements are withheld and the pilot keeps the lock. An unused reading
  is sent as 0 W.
- If the board stops transmitting, the Zappi holds the last values for at least
  34 s, then clears them.
- Airtime: about 4 ms per packet, one packet every 1–3 s, at 2 dBm.
- Residual risk: lock is inferred, not measured against mains. A source that
  freezes in HA without going unavailable keeps being sent. Re-check the Grid
  reading after any firmware change before relying on it for charging control.

Weak reception of the Zappi's broadcasts is the main cause of lock loss.
Keep the antenna vertical and clear of inverters and metal enclosures.
The page's **Diagnostics** show phase uncertainty, fit gain (≈ 1.00 when the
scaling is right), held packets, radio noise floor, receiver restarts and the
Zappi's serial.

## Recovery

- **Wi-Fi changed or unavailable:** reconnect to the setup network and enter
  the new Wi-Fi details. Its password is on the original card.
- **Reset saved settings:** use **Device → Reset to factory settings** on the
  page, or hold BOOT for 10 seconds after normal startup. This erases saved
  Wi-Fi, settings and a changed serial, and returns to Wi-Fi setup. The
  built-in serial and card credentials stay unchanged. Do not hold BOOT while
  powering on: that enters the ESP32 USB bootloader.
- **Waiting for readings:** each reading on the page says what it is waiting
  for (Home Assistant, a unit, a fresh push). Missing readings don't prevent
  pairing.
- **No packets / Radio failed:** check the board variant, antenna and frequency.
  A blank OLED can be an I²C/display issue; USB logs work independently.

Settings are written to flash when you press Save.

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
build toolchain. The web page, font and radio component are bundled.
`prepare_device.py` creates unique `secrets.yaml` and a private `device-card.txt`;
it refuses to overwrite existing credentials. An optional `--serial NUMBER`
sets the built-in 24-bit identity; the owner can change it later on the web
page (see
[Solar and battery in the myenergi app](#solar-and-battery-in-the-myenergi-app)).
Never flash multiple units with the same generated secrets: prepare a separate
working directory and setup card per unit. Preserve each unit's secrets for updates.

Earlier checkouts used `harvi-radio.yaml`; the main file is now
`virtual-ct-harvester.yaml`. Existing secrets remain compatible: keep the same keys,
virtual serial and device name when updating an already configured unit.

**Updating from a release before the web page:** firmware updates now use
the encryption key instead of an OTA password. Units built with ESPHome
2026.9.0 already accept encrypted updates, so update over the air as usual;
the old `harvi_radio_ota_password` secret is no longer used. Saved entities,
units, signs, channel and reference serial carry over. Home Assistant drops the old
configuration and diagnostic entities (remove them in HA if they linger as
unavailable). The output switch is now **Send to Zappi** and starts off after
the update; turn it on again. Settings are now edited on the device's page
rather than as HA config entities.

For ESPHome Device Builder, copy the YAML, packages, components, fonts and that
unit's secrets into its configuration directory, retaining existing unrelated
secrets. Choose **Install** and USB for the first flash. A separate USB installer
needs the **factory** image; later OTA updates are encrypted with that unit's
encryption key (`harvi_radio_api_key`).
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
