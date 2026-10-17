# E-Garden

An automated hydroponic greenhouse controlled over LoRa (433 MHz) with a web dashboard. E-Garden is the group capstone project of a team of 5 students at Chitralada vocational school.

This repository contains the **original code as it was at the end of the project** (file dates Sep 2022 – Feb 2023), tagged `v1-original`. Nothing has been refactored or modernized. The only changes from the original files are the removal of private values (Wi-Fi credentials and the server URL), see [Configuration](#configuration).

## My role

I wrote the ESP32 firmware (controller boxes + LoRa link to the Master box) and part of the web dashboard with one teammate. 5 members in total.

## Credits

E-Garden was a team project. I only claim the parts listed under *My role*. The web dashboard was a joint effort with one teammate, and everything else in the project (outside this code) was done by the other team members.

Third-party material:

- The form/SPIFFS parts of `Wed_server_F.3.13` and `Wed_server_test_Auto` (the `/get` handler with `inputString` / `inputInt` / `inputFloat`, `readFile` / `writeFile` / `notFound`, and the ESP32/ESP8266 `#ifdef` include block) and the whole of `Dev/web_server/web_server.ino` are adapted from example projects by [Random Nerd Tutorials](https://randomnerdtutorials.com) (Rui Santos). The original examples carry their own terms, see the site.
- Libraries: [arduino-LoRa](https://github.com/sandeepmistry/arduino-LoRa), ESPAsyncWebServer, AsyncTCP, Adafruit AHTX0, BH1750, OneWire, DallasTemperature (all through the Arduino IDE).
- The dashboard pages load [Highcharts](https://www.highcharts.com), Font Awesome and Google Fonts from their CDNs. Check the Highcharts license before any commercial use.

## Hardware

- ESP32 boards (one per box)
- LoRa transceiver modules, 433 MHz (SX127x class, used through arduino-LoRa)
- RS485 / Modbus pH + temperature sensor and RS485 / Modbus EC sensor, with an RS485 transceiver module (DE/RE control)
- DS18B20 temperature sensor
- BH1750 light sensor
- AM2305 temperature / humidity sensor (read with the Adafruit AHTX0 library)
- Stepper-driven dosing pumps (channels A, B and Acid)
- Relay module for the water pump
- Battery voltage measurement through an ADC pin, with deep sleep on the battery-powered sensor boxes

## Architecture

```
 Air box ──┐                                          ┌── web dashboard (served from SPIFFS)
 Water box ┼── LoRa 433 MHz ──► Master box (ESP32) ───┤
 Pump box ◄┘◄── commands ───────        │              └── optional HTTP GET to a cloud server
                                    Wi-Fi / NTP
```

- **Controller boxes (ESP32 + LoRa)**
  - *Air box*: light (BH1750), air temperature and humidity, battery level.
  - *Water box*: pH, water temperature and EC from the RS485/Modbus sensors; receives dosing commands (`Add(A)`, `Add(B)`, `Add(Acid)`) and drives the stepper pumps.
  - *Pump box*: LoRa-controlled relay for the water pump (`On` / `oFF`), with an automatic off timer.
- **Master box (ESP32 + LoRa + Wi-Fi)**: receives the sensor packets, keeps time over NTP, serves the dashboard, and sends commands back to the boxes. One sketch also forwards readings to a cloud server with an HTTP GET.
- **LoRa link**: 433 MHz with a sync word per network (`0xAA` / `0xAB`, depending on the sketch). Packets are plain text, for example a reading is `readingID@address/value1&value2$value3#value4^` and a command is `readingID@address/command&amount$`.
- **Dashboard**: a Highcharts page (`data/index.html`) served by the Master box from SPIFFS, plus static page mockups in `web project/`.

## Repository layout

| Folder | What it is |
|---|---|
| `LoRa_Air__v1/` | Air box firmware |
| `LoRa_Water3.2/` | Water box firmware (pH/EC, dosing pumps) and its dashboard page |
| `Receive_PUMP_F.1/` | Pump box firmware |
| `Wed_server_F.3.13/` | Master box with web server, SPIFFS dashboard (`data/`) and LoRa commands |
| `Wed_server_test_Auto/` | Same as above plus a timed auto-dosing test |
| `LoRa_Master/` | Earlier Master box that forwards readings to a cloud server |
| `time/` | NTP time test |
| `Dev/` | Earlier development snapshot (Oct 2022) of the same boxes |
| `web project/` | Static dashboard page mockups and styles |

Addresses, pins and sync words differ between snapshots, so check the constants at the top of each sketch before flashing.

## Configuration

Wi-Fi credentials (and the cloud server URL for the `LoRa_Master` sketches) are not in the repository. In every sketch folder that has a `secrets.h.example`:

1. Copy `secrets.h.example` to `secrets.h` in the same folder.
2. Fill in your own values. `secrets.h` is git-ignored.

`SERVER_URL` is a placeholder: the original server is private and is not part of this repository. Sensor values are appended to the URL path as `id|temperature|humidity|light|battery`.

## Build and run

Arduino IDE with the ESP32 board package and the libraries above. For the sketches that have a `data/` folder (the Master box), upload it to the board with the ESP32 sketch data upload tool.

## Notes

- Published as-is as a record of the project. Some sketches contain experimental or commented-out code.
- The team photos that the original `web project/index3.html` page shows are not included, so those images appear broken.
