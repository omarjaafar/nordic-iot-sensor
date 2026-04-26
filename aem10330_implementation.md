# AEM10330 PMIC Implementation Guide — nRF52840 DK

## Overview

The AEM10330 is a GPIO-only energy harvesting PMIC (no I2C). It takes power from the solar panel, charges the battery, and outputs a regulated voltage to the nRF52840. The nRF52840 only communicates with it by reading 4 status pins and optionally driving 2 control pins.

Reference: Daniel's Arduino code has been validated to work. This doc translates that to the nRF52840 using Zephyr GPIO API.

---

## Battery Configuration — Li-Po

Daniel's Arduino code sets `STO_CFG[3:0]` to **H, H, L, H** (STO_CFG3=H, STO_CFG2=H, STO_CFG1=L, STO_CFG0=H).

Per Table 2 of the AEM10330 EVK User Guide, this matches:

| VOVDIS | VCHRDY | VOVCH | Battery type |
|--------|--------|-------|--------------|
| 3.00 V | 3.50 V | 4.35 V | **Li-Po** ✓ |

Meaning:
- Battery considered **depleted** below 3.00 V — over-discharge protection triggers
- Battery considered **charged/healthy** above 3.50 V
- Overcharge protection at 4.35 V

Load output (`LOAD_CFG[1:0]` = L, L) gives **VLOAD,TYP = 3.28 V** — this is the regulated output powering the nRF52840.

---

## What the AEM10330 Tells the nRF52840 (Status Pins)

These are **digital inputs** to the nRF52840. The AEM10330 drives them.

| AEM10330 Pin | nRF52840 Pin | DK Header | Meaning |
|---|---|---|---|
| ST_LOAD | P0.03 | A0 | Load voltage is stable (VLOAD is up) |
| ST_STO | P0.04 | A1 | Battery above VCHRDY (3.50V) threshold |
| ST_STO_RDY | P0.28 | A2 | Battery ready — same as ST_STO but VLOAD-level output |
| ST_STO_OVDIS | P0.29 | A3 | Battery hit over-discharge threshold (< 3.00V) |

**Battery state logic:**

| ST_STO_RDY | ST_STO_OVDIS | Meaning |
|---|---|---|
| 1 | 0 | Battery > 3.50 V — healthy, ready to operate |
| 0 | 0 | Battery 3.00–3.50 V — mid-range, caution |
| 0 | 1 | Battery < 3.00 V — over-discharge, conserve power |
| 1 | 1 | **Invalid — should never happen** — if seen, check wiring |

> **Advisor-flagged issue:** `1 1` (both RDY and OVDIS high simultaneously) has been observed. This indicates either noise on the GPIO lines, incorrect pin mapping, or floating pins being read as high. **Do not trust battery state until this is resolved.** See the troubleshooting section below.

---

## What the nRF52840 Tells the AEM10330 (Control Pins)

These are **digital outputs** from the nRF52840. They control AEM10330 behavior.

| Signal | Recommended nRF52840 Pin | Arduino Value | Meaning |
|---|---|---|---|
| EN_STO_CH | P0.05 | HIGH | Enable battery charging — keep HIGH normally |
| EN_SLEEP | P0.06 | LOW | Enable PMIC sleep mode — keep LOW for normal operation |

> **CRITICAL:** `EN_SLEEP` **must never be left floating.** If the nRF52840 pin is not configured before the GPIO is initialized, the PMIC could be damaged. Always drive it LOW first in firmware init.

---

## Configuration Pins — Wire Directly to 3.3V or GND on the EVK Board

These pins never change at runtime, so instead of wiring them to nRF52840 GPIO, connect them directly to either the **3.3V rail** or **GND** on the EVK board using short jumper wires. This is equivalent to what Daniel's Arduino `setup()` does — it just sets them once and never touches them again.

> **Alternative (if you get jumper caps):** Each config header on the EVK board is a 3-pin header with an H side and an L side. A jumper cap slides onto two pins to short them — no wires needed. Either approach works identically.

### Wiring Table — Config Pins (Li-Po, 3.28V output)

| EVK Board Pin | Wire to | Why |
|---|---|---|
| STO_CFG[3] | 3.3V | Li-Po config (H) |
| STO_CFG[2] | 3.3V | Li-Po config (H) |
| STO_CFG[1] | GND | Li-Po config (L) |
| STO_CFG[0] | 3.3V | Li-Po config (H) |
| LOAD_CFG[1] | GND | 3.28V output (L) |
| LOAD_CFG[0] | GND | 3.28V output (L) |
| EN_STO_CH | 3.3V | Charging enabled (H) |
| EN_SLEEP | GND | Sleep mode off (L) — **never leave floating** |
| BAL | GND | Not using dual-cell supercap — **never leave floating** |
| EN_HP | 3.3V | High power mode on (H) |
| STO_PRIO | 3.3V | Charge battery before load (H) |
| EN_STO_FT | GND | Feed-through disabled (L) |
| R_MPP[2] | GND | 60% MPP ratio (L) |
| R_MPP[1] | GND | 60% MPP ratio (L) |
| R_MPP[0] | GND | 60% MPP ratio (L) |
| T_MPP[1] | GND | 70.8ms sample / 4.5s period (L) |
| T_MPP[0] | 3.3V | 70.8ms sample / 4.5s period (H) |

> **Note on R_MPP:** If all three R_MPP pins are left floating, the AEM10330 internally pulls them high (H,H,H = ZMPP mode), which requires an external resistor that we don't have. Wire all three to GND to select 60% MPP ratio instead.

> **Note on T_MPP:** The Arduino code doesn't set these — Daniel's board likely had them floating (defaulting to H,H = 1.12s/71.7s period, which is very slow). We've set L,H here for a faster 4.5s period which is better for testing. Adjust later if needed.

---

## Difference from Arduino Code — What Changes for nRF52840

| Arduino | nRF52840 / Zephyr |
|---|---|
| `pinMode(pin, INPUT_PULLUP)` | `gpio_pin_configure(dev, pin, GPIO_INPUT \| GPIO_PULL_DOWN)` — use pull-down since AEM outputs are active-high logic |
| `pinMode(pin, OUTPUT)` + `digitalWrite` | `gpio_pin_configure(dev, pin, GPIO_OUTPUT_INACTIVE)` then `gpio_pin_set` |
| `digitalRead(pin)` | `gpio_pin_get(dev, pin)` |
| Arduino pin numbers (7, 6, 10, 11, etc.) | nRF52840 port/pin numbers (P0.xx) |
| No device reference needed | Requires `DEVICE_DT_GET(DT_NODELABEL(gpio0))` |

> **Important:** The Arduino code uses `INPUT_PULLUP` for status pins. On the nRF52840, use `GPIO_INPUT | GPIO_PULL_DOWN` instead. The AEM10330 drives these pins high (active high logic), so a pull-down ensures the pin reads LOW when the AEM is not asserting — which is the correct idle state.

### Bug in Arduino Code Worth Noting

The `collectData()` function re-reads the same pins as `loop()` — this is redundant. In the nRF52840 port, read pins once per duty cycle and pass the values to the logging/decision logic.

Also: the Arduino code does not read `ST_STO` (pin connected to A1 / P0.04). The README added that pin. Read all 4 status pins.

---

## Voltage Level Compatibility

The AEM10330 status pin outputs are at VLOAD level (~3.28V typical). The nRF52840 GPIO is 3.3V tolerant — this is fine. No level shifting needed.

---

## Troubleshooting — The Simultaneous RDY + OVDIS Issue

If `ST_STO_RDY` and `ST_STO_OVDIS` are both reading HIGH at the same time, check these in order:

1. **Wrong pin mapping** — verify physical wire goes to the correct AEM10330 pin. Swap the A2/A3 wires and see if the problem follows the wire.
2. **Floating input** — a floating GPIO input on nRF52840 reads unpredictably. Confirm pull-down is configured in firmware before reading.
3. **Noise** — add a short `k_msleep(5)` after configuring GPIO before the first read. Also debounce by reading twice and confirming the same value.
4. **Battery fully discharged** — if battery voltage is near 3.0V, the AEM could be rapidly toggling the threshold comparator. Check actual battery voltage with a multimeter.
5. **Configuration mismatch** — if the STO_CFG jumpers are set differently than what the Arduino code assumed, the threshold voltages will be wrong and both states could trigger.

---

## Firmware Implementation Plan — Milestones

### Pre-conditions — ALL COMPLETE

- [x] Milestones 0–11 in `milestones.md` are complete
- [x] Config pins wired to 3.3V or GND per the wiring table above
- [x] EN_SLEEP confirmed wired to GND
- [x] BAL confirmed wired to GND
- [x] Battery connected to STO terminal
- [x] Solar panel connected to SRC terminal
- [x] AEM10330 LOAD terminal connected to nRF52840 VDD (when running untethered)

---

### Milestone 12a — Wire and Verify Status Pins (Read Only) ✅ COMPLETE

**Goal:** Confirm the 4 AEM10330 status pins are wired correctly and readable before writing any control logic.

**Wiring:**
1. Connect AEM10330 `ST_LOAD` pin → nRF52840 `P0.03` (DK header A0)
2. Connect AEM10330 `ST_STO` pin → nRF52840 `P0.04` (DK header A1)
3. Connect AEM10330 `ST_STO_RDY` pin → nRF52840 `P0.28` (DK header A2)
4. Connect AEM10330 `ST_STO_OVDIS` pin → nRF52840 `P0.29` (DK header A3)
5. Confirm common GND between AEM10330 EVK board and nRF52840 DK

**Firmware steps:**
1. Add `#include <zephyr/drivers/gpio.h>` to `main.c`
2. Get GPIO device: `const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));`
3. Configure all 4 pins as `GPIO_INPUT | GPIO_PULL_DOWN`
4. At the start of each duty cycle, read all 4 pins and log them
5. Log the interpreted battery state alongside raw pin values

**Expected output (battery charged, load available):**
```
[PMIC] ST_LOAD=1  ST_STO=1  ST_STO_RDY=1  ST_STO_OVDIS=0
[PMIC] Battery: HEALTHY (>3.50V)
[PMIC] Load: AVAILABLE
```

**Expected output (battery low):**
```
[PMIC] ST_LOAD=0  ST_STO=0  ST_STO_RDY=0  ST_STO_OVDIS=1
[PMIC] Battery: DEPLETED (<3.00V)
[PMIC] Load: NOT AVAILABLE
```

**If you see `ST_STO_RDY=1` and `ST_STO_OVDIS=1` simultaneously:** do not continue. Follow the troubleshooting steps above before proceeding.

---

### Milestone 12b — Add Control Pins (EN_STO_CH, EN_SLEEP) ✅ COMPLETE

**Goal:** Drive EN_STO_CH and EN_SLEEP from nRF52840 GPIO instead of relying on EVK jumpers.

> Skip this milestone if you have already jumpered EN_STO_CH to VINT and EN_SLEEP to GND on the EVK board and do not need software control.

**Wiring:**
1. Connect nRF52840 `P0.05` → AEM10330 EVK `EN_STO_CH` header (remove existing jumper first)
2. Connect nRF52840 `P0.06` → AEM10330 EVK `EN_SLEEP` header (remove existing jumper first)

**Firmware steps:**
1. Configure P0.05 as `GPIO_OUTPUT_ACTIVE` (HIGH = charging enabled)
2. Configure P0.06 as `GPIO_OUTPUT_INACTIVE` (LOW = sleep mode disabled)
3. Set EN_STO_CH and EN_SLEEP **before** reading any status pins
4. Confirm behavior is unchanged from Milestone 12a

**Expected output:** Same as 12a — the only difference is the nRF52840 is now driving those lines.

---

### Milestone 12c — Gate Sensor Reads on Battery State ✅ COMPLETE

**Goal:** Only collect sensor data when the battery is healthy enough. Match the logic in Daniel's Arduino code.

**Logic to implement:**
```
if (ST_LOAD == 1 && ST_STO_RDY == 1 && ST_STO_OVDIS == 0) {
    // healthy — proceed with sensor reads and BLE transmit
} else {
    // insufficient power — log warning, skip sensor reads, sleep longer
}
```

**Test:**
1. Run with battery charged — confirm sensor reads happen normally
2. Disconnect solar panel and let battery drain (or simulate with a lower voltage supply on SRC)
3. Confirm firmware skips sensor reads and logs the power-insufficient state

**Expected output (power OK):**
```
[PMIC] Battery: HEALTHY — collecting sensor data
[SENSOR] lux=420  temp=23.1  hum=48.2
```

**Expected output (power insufficient):**
```
[PMIC] Battery: LOW — skipping sensor read
[PMIC] Sleeping for extended interval
```

---

### Milestone 12d — Activate PMIC Dashboard Tiles in Web App ✅ COMPLETE

**Goal:** Stream PMIC status over BLE so the web dashboard PMIC panel lights up.

**Prerequisites:** Milestones 12a–12c confirmed working on hardware.

**Steps:**
1. Add a new BLE characteristic for PMIC status (4 bytes: ST_LOAD, ST_STO, ST_STO_RDY, ST_STO_OVDIS)
2. Update PMIC status once per duty cycle alongside sensor reads
3. In `webapp/index.html`, wire the 4 placeholder tile values to the new BLE characteristic reads

**Expected result:** PMIC status panel in dashboard shows real-time battery state instead of greyed-out placeholders.

---

## Summary Checklist — COMPLETED

- [x] Config pins wired to 3.3V or GND on EVK board per the wiring table above
- [x] EN_SLEEP confirmed NOT floating
- [x] Battery connected to STO
- [x] Solar/source connected to SRC
- [x] 4 status wires connected to correct nRF52840 pins
- [x] Common GND confirmed
- [x] Firmware reads pins with `GPIO_PULL_DOWN` not `INPUT_PULLUP`
- [x] All 4 status pins read (not just 3 like the Arduino code)
- [x] Simultaneous RDY+OVDIS case handled in firmware (log as error, do not trust)
