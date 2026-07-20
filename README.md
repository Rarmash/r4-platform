# R4 Platform

R4 Platform is an experimental hardware and software ecosystem for custom devices, lightweight agents and a central management Hub.

The current primary hardware target is a custom Batocera-based handheld console built around:

- Orange Pi 3 LTS as the main Linux system;
- RP2040 as the embedded controller;
- a composite USB HID and CDC connection between them.

The project also includes a general-purpose Hub, shared protocol contracts and Linux agents for future R4 devices.

## Current status

The current handheld prototype supports:

- composite USB HID gamepad and CDC ACM service interface;
- two analog sticks;
- two face buttons;
- left and right stick clicks;
- automatic stick-center calibration;
- RGB status LED;
- persistent system LED states;
- non-blocking temporary LED effects;
- Batocera controller watchdog;
- automatic USB disconnect and reconnect handling;
- Batocera game start and stop integration;
- physical RetroAchievements indication;
- Batocera integration installer.

Current controller firmware:

```text
R4_CONTROLLER_FW 0.5.1
```

## Architecture

```text
Physical controls
       │
       ▼
RP2040 embedded controller
       │
       ├── USB HID ──► Batocera game input
       │
       └── USB CDC ◄─► system commands and hardware state
                         │
                         ▼
                  Orange Pi 3 LTS
                         │
                         ├── EmulationStation
                         ├── emulators
                         ├── RetroAchievements
                         └── R4 Batocera integration
```

The gameplay path and the service path are intentionally separated:

- HID is used for regular controller input;
- CDC is used for status, diagnostics, LED control and future embedded peripherals.

## Modules

### Platform services

- `hub` — central Spring Boot service.
- `protocol` — shared API contracts.
- `simulator` — test device agent.
- `agent-linux` — Linux host agent.

### Handheld firmware and integration

- `firmware/r4-controller-fw` — RP2040 controller firmware.
- `integration/batocera` — Batocera-side controller integration.

## Repository layout

```text
r4-platform/
├── agent-linux/
├── firmware/
│   └── r4-controller-fw/
├── hub/
├── integration/
│   └── batocera/
├── protocol/
├── simulator/
├── build.gradle.kts
├── settings.gradle.kts
└── README.md
```

## Hub and agent requirements

- JDK 21

## Build the JVM modules

Linux and macOS:

```bash
./gradlew clean check
```

Windows:

```powershell
.\gradlew.bat clean check
```

## Run the Hub

```bash
./gradlew :hub:bootRun
```

## Run the Simulator

```bash
./gradlew :simulator:bootRun
```

## Install the Linux Agent

Java 21 and a systemd-based Linux distribution are required.

A tagged GitHub Release must exist before running the installer.

```bash
curl -fsSL https://raw.githubusercontent.com/Rarmash/r4-platform/main/agent-linux/install.sh \
  | sudo bash -s -- install \
      --hub-url "https://hub.example.com" \
      --name "server-01"
```

See [agent-linux/README.md](agent-linux/README.md) for installation, updates, status, removal and manual deployment.

## RP2040 controller firmware

The controller firmware is located in:

```text
firmware/r4-controller-fw
```

Current hardware capabilities:

- composite USB HID and CDC device;
- left analog stick;
- right analog stick;
- two face buttons;
- left and right stick clicks;
- RGB status indicator;
- automatic analog-center calibration;
- CDC command protocol;
- non-blocking LED effects.

Current prototype pin assignments:

| Function | RP2040 pin |
|---|---|
| Left stick X | GP26 / ADC0 |
| Left stick Y | GP27 / ADC1 |
| Right stick X | GP28 / ADC2 |
| Right stick Y | GP29 / ADC3 |
| Left stick click | GP11 |
| Right stick click | GP12 |
| Face button A | GP13 |
| Face button B | GP14 |
| RGB LED | GP16 |

Both analog stick modules are powered from `3V3`.

The firmware currently targets Pico SDK `2.3.0`.

## Build the controller firmware

Configure the project with the installed Pico SDK and toolchain, then build:

```powershell
cmake --build .\firmware\r4-controller-fw\build
```

The generated UF2 file is located at:

```text
firmware/r4-controller-fw/build/r4-controller-fw.uf2
```

## Batocera integration

The Batocera integration is located in:

```text
integration/batocera
```

It provides:

- automatic RP2040 discovery;
- firmware version validation;
- controller watchdog;
- hot-plug recovery;
- game lifecycle hooks;
- RetroAchievements hooks;
- persistent and temporary LED states;
- automated installation and updates.

See [integration/batocera/README.md](integration/batocera/README.md) for installation, commands and diagnostics.

## Controller service protocol

Currently supported CDC commands:

```text
PING
VERSION
INPUT
STATUS
LED <R> <G> <B>
LED FLASH <R> <G> <B> <MS>
LED OFF
HELP
```

Example status response:

```text
FW=0.5.1 LED=0,16,0 BASE=0,16,0 FLASH=0 LX=0 LY=0 RX=0 RY=0 BUTTONS=0x00000000
```

## Development USB identity

The controller currently uses temporary development identifiers:

```text
VID: cafe
PID: 4005
Manufacturer: Rarmash
Product: R4 Controller
Serial: R4-0001
```

These identifiers are suitable for local development but must be reconsidered before public distribution.

## Planned handheld controls

The planned final controller includes:

- D-pad;
- ABXY;
- two analog sticks;
- L3 and R3;
- L1 and R1;
- analog L2 and R2;
- Start and Select;
- Home;
- Capture;
- R4;
- Trophy.

The next hardware milestone is the D-pad and complete ABXY input set.

## License

No public license has been selected yet.
