# nRF52840 DK — Low Power IoT Sensing System
ECE 4013/4014 — Senior Design

Energy-harvested IoT sensor node running on a Dracula Technologies indoor OPV solar panel. Reads ambient light, temperature, and humidity over I2C, duty-cycles to stay within an ultra-low power budget, and streams readings over BLE to a browser dashboard.

---

## Daniel — What Needs To Be Done

1. **Wire PMIC** — AEM10330 status pins to P0.03/04/28/29 (A0–A3 on DK header). Full details in the PMIC section below.
2. **Add PMIC firmware** — read the 4 GPIO pins once per duty cycle in `main.c` using Zephyr `gpio_pin_configure` + `gpio_pin_get`. Reference your ESP32 code for logic.
3. **Replace sleep method** — `main.c` currently uses `k_msleep` which keeps the CPU on. Swap for `pm_state_force` (system-off) + RTC wakeup so the board hits its lowest power state between cycles. Required before running untethered on battery.
4. **Resolve PMIC ambiguity** — ST_STO_RDY and ST_STO_OVDIS have been seen asserting simultaneously (flagged by advisor). Fix before trusting battery state.
5. **Activate PMIC dashboard tiles** — the web app already has the 4 placeholder tiles. Wire them to a BLE characteristic once firmware is reading the pins.

---

## Environment Setup

**Everything runs through nRF Connect for VS Code.**

1. Install [nRF Connect for Desktop](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-Desktop)
2. Install the **Toolchain Manager** and **nRF Connect SDK** from within that app (use the latest stable SDK version)
3. Install the **nRF Connect for VS Code** extension pack in VS Code
4. Open `apps/sensor_bringup` as the application folder
5. Add a build configuration — board target: `nrf52840dk_nrf52840`
6. Build → Flash using the nRF Connect sidebar

**Debug output is via RTT, not UART.** After flashing:
- Connected Devices panel → click **RTT** → Search for RTT memory address automatically
- Immediately press the **RESET button** on the DK to catch the boot log
- If RTT fails ("Could not connect to port") — rebuild and reflash, then retry
- If device shows as **NRF52_FAMILY** instead of **NRF52840_xxAA** — unplug/replug USB and refresh

---

## Hardware

| Component | Interface | Address / Pins |
|---|---|---|
| VEML7700 (light) | I2C | 0x10 |
| SHTC3 (temp/humidity) | I2C | 0x70 |
| INA219 (voltage/current) | I2C | 0x40 — deferred |
| AEM10330 PMIC | GPIO | see below |

I2C bus: SDA = P0.26, SCL = P0.27, 3.3V only. All three sensors share these two lines in parallel.

---

## PMIC — AEM10330 (Milestone 12, not yet wired)

The AEM10330 is GPIO-only (no I2C). Wire 4 status pins as digital inputs to the nRF52840:

| AEM10330 Pin | nRF52840 | DK Header |
|---|---|---|
| ST_LOAD | P0.03 | A0 |
| ST_STO | P0.04 | A1 |
| ST_STO_RDY | P0.28 | A2 |
| ST_STO_OVDIS | P0.29 | A3 |

Common GND required. 3.3V logic — not 5V tolerant.

**Battery state:**

| ST_STO_RDY | ST_STO_OVDIS | Meaning |
|---|---|---|
| 1 | 0 | > 3.5 V — healthy |
| 0 | 0 | 3.1–3.5 V — mid |
| 0 | 1 | < 3.1 V — over-discharge protection |
| 1 | 1 | Invalid — should not happen |

**Known issue (advisor-flagged):** both RDY and OVDIS asserting simultaneously has been observed. Likely noise or wrong pin mapping — do not trust battery state until resolved.

**Firmware:** the existing `main.c` does not read PMIC pins yet. Add `gpio_pin_configure()` + `gpio_pin_get()` for each pin and read them once per duty cycle alongside the sensor reads. Reference your ESP32 code for the logic — only the GPIO API calls differ.

The web dashboard already has a PMIC status panel with 4 placeholder tiles ready to activate once wired.

---

## Web Dashboard

Open `webapp/index.html` directly in Chrome. No server needed.

Requires **Web Bluetooth** — Chrome or Edge only. Click **Connect to NordicIoT** and select the board.

Features: live sensor cards with min/max/avg, history charts, sample interval slider (writes to firmware over BLE), solar harvest estimate, lux threshold alert, CSV export, PMIC status panel (placeholder until Milestone 12).

**Solar model:** linear fit to two measured OPV panel data points — 400 lux → 0.21125 mWh, 3000 lux → 3.063 mWh. Break-even for 0.5% duty cycle is ~208 lux. Lux card turns red below that.

---

## Milestone Status

Milestones 0–11 complete. Milestone 12 (PMIC) is next — wiring and firmware steps above.
