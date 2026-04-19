# Milestones – nRF52840 DK Bring-Up

## Purpose

This file defines the incremental bring-up plan for the nRF52840 DK and attached sensors.

Current hardware:
- nRF52840 DK
- INA219 voltage/current sensor
- VEML7700 ambient light sensor
- SHTC3 temperature/humidity sensor

Current goal:
- Get the development workflow working
- Bring up I2C
- Detect all sensors
- Read real sensor values
- Build toward low-power duty-cycled firmware

---

## Ground Rules

- Make one small change at a time
- Test after every change
- Do not wire or code multiple unknowns at once
- Keep working states stable before moving forward
- Prefer simple logging and observable behavior

---

## Toolchain / Project Rules

- Use nRF Connect for VS Code
- Use board target: `nrf52840dk_nrf52840`
- Use a local path with no spaces, for example:
  - `C:\nordic\apps\`
- Do not use OneDrive paths for builds
- Use RTT/logging for early debug visibility
- Keep each milestone in a separate app folder if that makes testing easier

---

## Hardware Wiring Plan

### Power Rules
- Use **3.3V only**
- Use common GND for all sensors
- All three sensors are I2C and will share SDA and SCL

### Initial I2C Pin Plan
Use these DK pins for the first bring-up:
- SDA: `P0.26`
- SCL: `P0.27`

### Shared Wiring
Connect all sensors in parallel:

- DK `3.3V` → sensor `VIN` / `VCC`
- DK `GND` → sensor `GND`
- DK `P0.26` → all sensor `SDA`
- DK `P0.27` → all sensor `SCL`

### Sensor List
#### INA219
- VCC → 3.3V
- GND → GND
- SDA → P0.26
- SCL → P0.27

#### VEML7700
- VCC → 3.3V
- GND → GND
- SDA → P0.26
- SCL → P0.27

#### SHTC3
- VCC → 3.3V
- GND → GND
- SDA → P0.26
- SCL → P0.27

### Notes
- Most breakout boards already include I2C pull-ups
- If one board is labeled `VIN`, verify it accepts 3.3V logic and power
- Do not connect 5V to the sensor boards

---

## Milestone 0 – Toolchain and Flashing Verified

- [x] Complete

### Objective
Confirm that the firmware build and flash pipeline works.

### Status
Completed if:
- Blinky builds successfully
- Blinky flashes successfully
- LED behavior changes on the DK

### Test
1. Create a sample app from `blinky`
2. Use board target `nrf52840dk_nrf52840`
3. Build and flash
4. Confirm LED blinking on board

### Exit Criteria
- Build succeeds
- Flash succeeds
- Board runs flashed firmware

---

## Milestone 1 – Create a Clean Sensor Bring-Up App

- [x] Complete

### Objective
Create a dedicated application for sensor bring-up.

### Steps
1. In nRF Connect for VS Code, create a new application from sample
2. Use a simple sample as a base, or create from a minimal Zephyr app if preferred
3. Save the app in a clean folder, for example:
   - `C:\nordic\apps\sensor_bringup`
4. Add a build configuration
5. Set board target to:
   - `nrf52840dk_nrf52840`

### Recommended app naming
- `sensor_bringup`
- `i2c_scan`
- `sensor_readout`

### Test
- App builds successfully before any sensor code is added

### Exit Criteria
- New dedicated app exists
- Build configuration is valid
- Build succeeds

---

## Milestone 2 – Enable Logging

- [x] Complete

### Objective
Make the firmware observable before adding sensor logic.

### Files to edit
- `prj.conf`

### Add
- enable logging
- enable RTT backend

### Expected configuration direction
Add the usual logging and RTT options so logs appear in VS Code / RTT viewer.

### Test
1. Add a startup log message in `main()`
2. Build and flash
3. Open RTT log viewer
4. Confirm startup message appears

### Exit Criteria
- Logs visible after boot
- Can observe firmware behavior without guessing

---

## Milestone 3 – Bring Up I2C Bus

- [x] Complete

### Objective
Configure the chosen I2C peripheral and pins.

### Pin plan
- SDA = `P0.26`
- SCL = `P0.27`

### Approach
Use Zephyr devicetree overlay to define an enabled I2C instance on those pins.

### Files to edit
- `app.overlay` or board overlay file
- `src/main.c`
- `prj.conf`

### What the code should do
- Get the I2C device handle
- Check `device_is_ready()`
- Print whether the bus initialized successfully

### Test
1. Build and flash
2. Open logs
3. Confirm:
   - I2C device found
   - device is ready
   - no init failure

### Exit Criteria
- I2C bus initializes successfully
- Pins are defined and active in firmware

---

## Milestone 4 – I2C Scan

- [x] Complete

### Objective
Scan the I2C bus to discover connected sensors.

### Wiring required
At this stage, wire all three sensors to:
- 3.3V
- GND
- SDA = P0.26
- SCL = P0.27

### What the code should do
- Iterate through valid I2C addresses
- Attempt a simple probe at each address
- Print every responding address

### Expected likely addresses
These may vary by breakout board, but typical defaults are:
- INA219: `0x40`
- VEML7700: `0x10`
- SHTC3: `0x70`

Treat these as expectations, not assumptions.

### Test
1. Wire one sensor only
2. Run scan
3. Confirm one device appears
4. Add second sensor
5. Run scan again
6. Add third sensor
7. Run scan again

### Exit Criteria
- All intended devices appear on the bus
- Wiring is validated incrementally
- No address conflicts

---

## Milestone 5 – Read One Sensor Only

- [x] Complete

### Objective
Read one sensor end-to-end before combining everything.

### Recommended first sensor
Use **VEML7700** first.

Reason:
- It is directly relevant to the solar / light-harvesting part of the project
- Light measurement is a core project need

### What the code should do
- Initialize the sensor
- Read raw light data or lux
- Print values periodically

### Test
1. Build and flash
2. Observe repeated light readings
3. Change room lighting
4. Confirm values respond in the expected direction

### Exit Criteria
- Real changing light values observed
- Data path proven from sensor to firmware log

---

## Milestone 6 – Add Temperature / Humidity

- [x] Complete

### Objective
Bring up SHTC3 after light sensing is stable.

### What the code should do
- Initialize SHTC3
- Read temperature and humidity
- Print values periodically

### Test
1. Build and flash
2. Confirm realistic temperature and humidity output
3. Warm sensor slightly by hand or breath nearby carefully
4. Confirm values change plausibly

### Exit Criteria
- Stable temperature and humidity readings
- No I2C contention introduced

---

## Milestone 7 – Add INA219

- [ ] Complete

### Objective
Measure voltage/current for energy visibility.

### What the code should do
- Initialize INA219
- Read bus voltage and current
- Print values periodically

### Important note
INA219 is most useful when actually inserted into a measurement path. During early bring-up, first prove communication and basic readout.

### Test
1. Build and flash
2. Confirm sensor responds over I2C
3. Log reported voltage/current values
4. Later validate values against a known setup

### Exit Criteria
- INA219 communication confirmed
- Raw measurement pipeline works

---

## Milestone 8 – Combined Sensor Readout

- [x] Complete (VEML7700 + SHTC3; INA219 deferred)

### Objective
Read all connected sensors in one firmware application.

### What the code should do
- Initialize all sensors
- Read each on a periodic schedule
- Print structured logs like:
  - lux
  - temperature
  - humidity
  - voltage
  - current

### Test
1. Build and flash
2. Confirm all sensors initialize
3. Confirm all values print repeatedly
4. Vary light and observe lux response
5. Confirm environment readings remain stable

### Exit Criteria
- Single firmware app reads all intended sensors
- Logging is clear and reliable

---

## Milestone 9 – Slow Periodic Sampling

- [x] Complete

> Note: k_msleep() is running at ~2x the specified interval (60000ms → ~2 min gap). Consistent and periodic, but timing multiplier needs investigation before Milestone 10.

### Objective
Move from continuous bring-up logging to interval-based measurement.

### What the code should do
- Wake on timer
- Read sensors once
- Log values once
- Wait for next interval

### Starting interval
Use something easy to observe first:
- 5 seconds
- then 10 seconds
- then 30 seconds
- then 60 seconds

Do not jump to long intervals immediately.

### Test
1. Confirm timestamps between readings
2. Verify reads occur at expected cadence
3. Confirm firmware remains stable between reads

### Exit Criteria
- Periodic sampling works reliably
- Timing behavior is understood

---

## Milestone 10 – Duty Cycle Skeleton

- [x] Complete

### Objective
Implement the first version of:
- sleep
- wake
- sample
- log/transmit
- sleep

### What the code should do
- Stay inactive most of the time
- Wake on timer
- Read sensors
- Optionally advertise or log
- Return to low-power wait state

### Test
1. Add clear wake-cycle logs
2. Confirm one burst of activity per interval
3. Confirm no unnecessary constant work in main loop

### Exit Criteria
- Basic duty cycle architecture exists
- Firmware structure matches project direction

---

## Milestone 11 – BLE Integration

- [x] Complete

### Objective
Expose sensor data over BLE after sensor bring-up is stable.

### What the code should do
- Advertise briefly
- Provide readings over BLE
- Shut down or reduce radio activity when not needed

### Important note
BLE should come after sensor and timing bring-up, not before.

### Test
1. Confirm advertising starts
2. Confirm a phone or receiver can see the board
3. Confirm sensor values are accessible
4. Confirm device is not transmitting constantly unless intended

### Exit Criteria
- Sensor data visible over BLE
- Radio behavior is deliberate

---

## Milestone 12 – PMIC / Battery Integration

- [ ] Complete

### Objective
Add the actual energy system interface once MCU + sensors are stable.

### What the code should do
- Read PMIC or threshold pins
- Monitor battery state inputs
- Resolve threshold ambiguity
- Log battery / energy status alongside sensor data

### Test
1. Validate pin/channel mapping carefully
2. Confirm thresholds behave as expected
3. Confirm no simultaneous impossible states
4. Compare readings against external measurement if possible

### Exit Criteria
- PMIC/battery signals are trusted
- Energy-state logic can be built on top of them

---

## Bring-Up Order Summary

Always follow this order:

1. Toolchain works
2. Flashing works
3. Logging works
4. I2C bus works
5. I2C scan works
6. One sensor works
7. Two sensors work
8. Three sensors work
9. Combined readout works
10. Timed sampling works
11. Duty cycle skeleton works
12. BLE works
13. PMIC integration works

Do not skip ahead.

---

## Exact Workflow for New Code Changes

Whenever you add code:

1. Open the existing app in nRF Connect for VS Code
2. Edit one thing only
3. Build
4. Flash
5. Open RTT/logs
6. Verify expected behavior
7. Record result before moving on

If creating a new app:
1. Create new application from sample
2. Save in local path with no spaces
3. Add build configuration
4. Choose board `nrf52840dk_nrf52840`
5. Keep defaults unless there is a specific reason to change them
6. Build
7. Flash
8. Verify logs/behavior

---

## Recommended File Structure

A clean setup could be:

- `C:\nordic\apps\i2c_scan`
- `C:\nordic\apps\veml7700_test`
- `C:\nordic\apps\shtc3_test`
- `C:\nordic\apps\ina219_test`
- `C:\nordic\apps\sensor_bringup`
- `C:\nordic\apps\duty_cycle_test`

This makes rollback and debugging easier.

---

## Current Immediate Next Step

### Next milestone to do now
**Milestone 3 → Milestone 4**

Meaning:
- wire sensors to:
  - 3.3V
  - GND
  - P0.26 SDA
  - P0.27 SCL
- create I2C bus config
- run I2C scan
- confirm addresses

---

## What Success Looks Like Right Now

Right now, success means:
- all three sensors are wired correctly
- firmware sees the I2C bus
- scan finds the devices
- logs show sensor addresses

That is the next real checkpoint.