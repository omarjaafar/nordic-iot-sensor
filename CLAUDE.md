# nRF52840 DK – Low Power IoT System (ECE 4014)

## Project Overview

This project is part of ECE 4013/4014 and focuses on building a **low-power, energy-harvested IoT sensing system** capable of operating under indoor lighting (~400 lux).

The system will:
- Collect environmental and light data
- Operate using **ultra-low-power duty cycling**
- Use **BLE for communication**
- Eventually integrate:
  - Solar panel
  - PMIC (power management IC)
  - Small rechargeable battery

---

## Current Hardware

- nRF52840 DK (main development platform)
- INA219 (voltage/current sensor)
- VEML7700 (ambient light sensor)
- SHTC3 (temperature + humidity sensor)
- **AEM10330** (PMIC / energy harvesting IC)

All sensors use **I2C communication**.

## PMIC Notes – AEM10330

- Fully GPIO-controlled — no I2C
- Battery state is threshold-based (not a voltage reading or percentage)
- 4 status pins to read as digital inputs from nRF52840 GPIO
- Several config/control pins set as digital outputs
- Reference code from Daniel targets ESP32 — nRF52840 pin mapping TBD
- **TODO:** document which nRF52840 GPIO pins we assign to each AEM10330 pin, and share with Daniel

---

## System Architecture (Target)

Solar → PMIC → Battery → nRF52840 → Sensors → BLE → Phone/Receiver

---

## Development Stage

### Phase 1: Environment Setup ✅
- nRF Connect SDK installed
- Toolchain working
- Blinky firmware built and flashed successfully

### Phase 2: Sensor Bring-Up (CURRENT)
- Wire sensors via I2C
- Implement I2C scan to detect devices
- Read sensor data

### Phase 3: Low Power Firmware
- Implement:
  - sleep → wake → read → transmit → sleep
- Minimize active time
- Validate timing behavior

### Phase 4: Power Integration
- Integrate PMIC + solar
- Measure power using INA219
- Evaluate energy harvesting performance

---

## Advisor Feedback & Open Questions

Recent advisor communication raised the following points that must be addressed:

- Verify whether the system can support **~0.5% duty cycle** even under low-light conditions (e.g., flashlight worst-case scenario)
- Experimentally confirm that **duty cycle control is functioning correctly**
- Investigate **battery measurement ambiguity**:
  - System currently reports both >3.5V and <3.0V simultaneously
  - Likely causes: noise, incorrect pin/channel configuration, or threshold logic issues
- Double-check **PMIC pin/channel configuration**
- Prepare a **final demo setup for expo**, including:
  - Full hardware system (solar + PMIC + MCU + sensors)
  - Sensor outputs
  - GUI / visualization of collected data

---

## Development Principles

- Make **small, incremental changes**
- Only implement **one feature at a time**
- Always **test after each change**
- Do not skip steps (hardware + firmware must be validated together)
- Prefer simple, debuggable code over complex abstractions

---

## Current Focus

Right now, the goal is:

> Get all sensors working over I2C and verify communication

Do NOT:
- Optimize power yet
- Add BLE features yet
- Integrate PMIC yet

---

## Firmware Approach

Using:
- nRF Connect SDK (Zephyr-based)
- Board target: nrf52840dk_nrf52840

Initial firmware tasks:
1. I2C scan
2. Detect sensor addresses
3. Read raw sensor values
4. Print output via RTT/logging

---

## Important Constraints

- System must eventually support **very low duty cycle (30+ min intervals)**
- Power consumption must be minimized
- Sensors should only be active when needed
- BLE should be used efficiently (short bursts)

---

## Notes

- All sensors are I2C → shared SDA/SCL lines
- Use 3.3V only
- Pull-up resistors are likely already present on breakout boards

---

## RTT Viewer – Known Quirks

- RTT is accessed via **CONNECTED DEVICES** panel in nRF Connect for VS Code → click **RTT** → **Search for RTT memory address automatically**
- After starting RTT, immediately press the **RESET button** on the DK — RTT must catch the boot log
- If RTT fails to connect ("Could not connect to port"), rebuild and reflash first, then retry
- If device shows as **NRF52_FAMILY** or **unknown** instead of **NRF52840_xxAA**, unplug and replug the USB cable, then refresh Connected Devices
- Do not interact with J-Link device selection dialogs — just click OK/Cancel and dismiss them, they are not needed
- The RTT terminal will show "unknown" in the J-Link header line but still work fine — that is not an error

---

## Expectations for Assistance

When modifying or suggesting code:

- Only propose **small, testable steps**
- Always include **how to test**
- Do not introduce unnecessary complexity
- Stay aligned with current phase
- Avoid skipping ahead in development stages

### How to Present Steps

When giving the user something to do, always format it like this:

**Numbered steps** — clear, one action per step, no ambiguity:
1. Do X
2. Do Y
3. Do Z

**Expected output** — always tell the user exactly what they should see if it worked:
- What text/values should appear in logs
- What behavior to look for on the board
- What a passing result looks like vs. a failing one

Never end with vague prompts like "let me know what you see." Instead say something like:
> "If it worked, you should see: `[exact expected output]`. If you see something different, paste it here."

---

## Session Tracking

`milestones.md` is the active progress tracker for this project. Claude should:

- Read `milestones.md` at the start of each session to understand current position
- Update the checkbox (`- [ ]` → `- [x]`) in `milestones.md` when a milestone is confirmed complete by the user
- Never mark a milestone complete without the user confirming it worked on hardware
- Use the current unchecked milestone to guide what to work on next