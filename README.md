# nRF52840 DK — Low Power IoT Sensing System
**ECE 4013/4014 Senior Design**

Energy-harvested indoor IoT sensor node powered by a Dracula Technologies indoor OPV solar panel. The system reads ambient light, temperature, and humidity over I2C, duty-cycles to minimize power consumption, and streams sensor data over BLE to a browser-based dashboard.

---

## System Architecture

Solar Panel → AEM10330 PMIC → Li-Po Battery → nRF52840 → I2C Sensors → BLE → Browser Dashboard

---

## Hardware

| Component | Interface | Address / Pins |
|---|---|---|
| VEML7700 (ambient light) | I2C | 0x10 |
| SHTC3 (temperature / humidity) | I2C | 0x70 |
| INA219 (voltage / current) | I2C | 0x40 — deferred |
| AEM10330 PMIC | GPIO | see below |

I2C bus: SDA = P0.26, SCL = P0.27, 3.3V. All sensors share the same two lines.

### PMIC — AEM10330 Status Pins

| AEM10330 Pin | nRF52840 | DK Header |
|---|---|---|
| ST_LOAD | P0.03 | A0 |
| ST_STO | P0.04 | A1 |
| ST_STO_RDY | P0.28 | A2 |
| ST_STO_OVDIS | P0.29 | A3 |

Battery state is threshold-based. ST_STO_RDY = 1, ST_STO_OVDIS = 0 indicates healthy (above 3.5 V). ST_STO_OVDIS = 1 indicates over-discharge protection active (below 3.0 V).

---

## Firmware

Built with nRF Connect SDK (Zephyr RTOS), board target `nrf52840dk_nrf52840`, written in C.

**Duty cycle loop:** wake on timer → read VEML7700 and SHTC3 → read AEM10330 PMIC status pins → read battery ADC → BLE notify all characteristics → sleep. Sleep interval is configurable from 5 seconds to 60 minutes via the dashboard without reflashing.

**Sleep:** currently uses `k_msleep()` (CPU-on sleep with RAM retention). True system-off sleep (`pm_state_force`) is planned as a future improvement.

---

## Web Dashboard

Open `webapp/index.html` directly in Chrome or Edge. No server or backend required.

Requires Web Bluetooth. Click **Connect to NordicIoT** and select the board from the browser prompt.

**Features:**
- Live sensor cards (lux, temperature, humidity) with min/max/avg
- Scrolling history charts
- Sample interval slider — writes to firmware over BLE, takes effect next cycle
- Solar harvest estimate computed live from lux reading using a linear model fit to measured OPV panel data
- Lux threshold alert at ~208 lux (break-even point for 0.5% duty cycle at 400 lux baseline)
- PMIC battery status panel with live ST_LOAD, ST_STO, ST_STO_RDY, ST_STO_OVDIS tiles
- CSV export of all session readings

**Solar model:** linear fit to two measured data points — 400 lux → 0.211 mWh, 3000 lux → 3.063 mWh.

---

## Build and Flash

1. Install [nRF Connect for Desktop](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-Desktop)
2. Open Toolchain Manager and install the nRF Connect SDK (latest stable)
3. Install the nRF Connect for VS Code extension pack
4. Open `apps/sensor_bringup` as the application folder
5. Add a build configuration with board target `nrf52840dk_nrf52840`
6. Build and flash using the nRF Connect sidebar

**Debug output is via RTT.** After flashing, open Connected Devices → RTT → Search for RTT memory address automatically, then press the RESET button on the DK to catch the boot log.

---

## What's Working

- VEML7700 lux readings and SHTC3 temperature/humidity over I2C
- Duty-cycled firmware with BLE advertising on each wake cycle
- Custom GATT service — connects from phone or laptop without pairing
- Web dashboard with live sensor data, history charts, and CSV export
- Adjustable sample interval from the dashboard over BLE
- AEM10330 PMIC status panel with live battery state
- Solar harvest estimate and lux threshold alert in dashboard

## In Progress

- Battery ADC readout (AIN6 / P0.30) — firmware written, hardware wiring under retest
- True system-off deep sleep (`pm_state_force`) to minimize sleep current

---

## Team

| Name | Major |
|---|---|
| Daniel O'Connor | EE |
| Devin Bourgeois | EE |
| Yashad Gurude | EE |
| Omar Jaafar | CmpE |
| Grant Elder | EE |

Georgia Institute of Technology — ECE 4013/4014 Senior Design