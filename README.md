# E-Garden (legacy v1)

> **This is the original code** of E-Garden as it ran at the end of the project (2022–2023), kept for reference. Only the secrets were removed (see [Configuration](#configuration)).
> The current, modernized version is on the [`main`](https://github.com/Putinun-te/E-Garden) branch. The exact original commit is also tagged [`v1-original`](https://github.com/Putinun-te/E-Garden/tree/v1-original).

E-Garden is an automated hydroponic greenhouse controlled over LoRa (433 MHz) with a web dashboard: the group capstone project of a team of 5 students at Chitralada vocational school.

## My role

I wrote the ESP32 firmware (controller boxes + LoRa link to the Master box) and part of the web dashboard with one teammate. 5 members in total.

## Credits

E-Garden was a team project of 5 students: 2 on hardware, 1 on the website, and 2 on IoT development (firmware and dashboard: me and one teammate). I only claim the parts listed under *My role*. The hardware and the website were done by the other team members.

- The form/SPIFFS parts of `Wed_server_F.3.13` and `Wed_server_test_Auto` and the whole of `Dev/web_server/web_server.ino` are adapted from example projects by [Random Nerd Tutorials](https://randomnerdtutorials.com) (Rui Santos).
- The dashboard pages load Highcharts, Font Awesome and Google Fonts from their CDNs.

## Folders

| Folder | What it is |
|---|---|
| `Wed_server_F.3.13/` | Master box: LoRa receiver, web server with the dashboard in `data/` (SPIFFS) |
| `Wed_server_test_Auto/` | Same as above plus a timed auto-dosing test |
| `LoRa_Master/` | Earlier Master box that forwards readings to a cloud server over HTTP |
| `LoRa_Water3.2/` | Water box: pH/EC over RS485/Modbus, dosing pumps |
| `LoRa_Air__v1/` | Air box: light, temperature, humidity, battery |
| `Receive_PUMP_F.1/` | Pump box: LoRa-controlled relay with a 90 s timer |
| `time/` | NTP time test |
| `Dev/` | Development snapshot from October 2022 |
| `web project/` | Static dashboard page mockups |

Each folder is an Arduino IDE sketch for ESP32 (built with arduino-esp32 2.0.17).

## Configuration

Sketches that need Wi-Fi (or the cloud server URL) include a `secrets.h` that is not in the repository. Copy `secrets.h.example` in that sketch folder to `secrets.h` and fill in your own values.

## Known issues

This code is kept unchanged on purpose. Problems found later, and fixed on `main`, include:

- Dosing amounts have no upper limit, the auto mode can dose with missing sensor data, and the dashboard's stop buttons have no effect on the water box.
- Modbus replies are read before they arrive, so failed sensor reads can be reported as values.
- The air box uses a different sync word and address than the Master box expects.
- Wi-Fi waits forever at boot and never reconnects; data is lost on reboot.

Do not use this version to control real dosing pumps. See `main` for the reworked firmware.
