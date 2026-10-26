# PixelCharm

> 🚧 **Work in progress.** Follow along as it gets built.

A tiny ESP32-S3 keychain with a 1.54" colour screen. Pick a theme in the phone app (an animated face, a character, your own photo) and it is sent to the keychain over Bluetooth Low Energy. It also rolls a d20.

## Planned features

- Animated face, image and GIF themes on a 240×240 IPS display
- Phone app (React Native) that sends themes over BLE, no Wi-Fi needed
- Upload your own photos: crop, resize and send them from the app
- A fair d20 with nat 20 / nat 1 animations, backed by the ESP32's hardware random number generator
- Battery powered with deep sleep, in a 3D-printed case

## Roadmap

| Phase | Goal | Status |
|---|---|---|
| 0 | Setup | 🚧 |
| 1 | Images, animations and a d20 on the device | ⬜ |
| 2 | Phone ↔ device over BLE | ⬜ |
| 3 | Upload your own photos | ⬜ |
| 4 | Battery, sleep and case: a real keychain | ⬜ |
| 5 | Docs and release | ⬜ |

## Repository layout

| Folder | Contents |
|---|---|
| [`firmware/`](firmware/) | ESP32-S3 firmware (C++, PlatformIO) |
| [`app/`](app/) | Phone app (React Native + Expo, TypeScript) |
| [`tools/`](tools/) | Helper scripts |
| [`hardware/`](hardware/) | Wiring diagrams, schematics, bill of materials |
| [`enclosure/`](enclosure/) | 3D-printable case |
| [`docs/`](docs/) | Protocol, architecture and build guides |

## License

- **Code** (`firmware/`, `app/`, `tools/`): [MIT](LICENSE)
- **Enclosure, hardware and docs** (`enclosure/`, `hardware/`, `docs/`): [CC BY-NC 4.0](LICENSE-CC-BY-NC.md)

## Inspiration

Desk-buddy and robo-eyes projects shared by the maker community, such as [FluxGarage RoboEyes](https://github.com/FluxGarage/RoboEyes). PixelCharm's code is written from scratch.
