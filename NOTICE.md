Project code is distributed under GPL-3.0-only; see LICENSE.

The SX127x component is based on ESPHome 2026.9.0:
https://github.com/esphome/esphome/tree/2026.9.0/esphome/components/sx127x
Its original MIT/Python and GPLv3/C++ license text is retained in
components/sx127x/LICENSE. Local changes add IRQ timestamps, register diagnostics,
FSK FIFO bounds/CRC checks, bounded transmit completion handling, transmit at a
scheduled microsecond, and a receiver profile (maximum LNA gain, AFC bandwidth
equal to the channel filter). This copy
is intended for this FSK profile, not as a general-purpose SX127x replacement.

DejaVu Sans Mono and its license are included under fonts/.
This project is independent of myenergi, ESPHome and LILYGO.
The harvi and zappi names are myenergi trademarks. References identify the
devices/protocol targeted by this experimental implementation; no affiliation
or endorsement is claimed.
