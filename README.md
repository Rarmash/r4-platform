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
- D-pad;
- A, B, X and Y buttons;
- L1 and R1 shoulder buttons;
- left and right stick clicks;
- Start and Select;
- dedicated Hotkey button;
- automatic stick-center calibration;
- RGB status LED;
- persistent system LED states;
- non-blocking temporary LED effects;
- Batocera controller watchdog;
- automatic USB disconnect and reconnect handling;
- Batocera game start and stop integration;
- physical RetroAchievements indication;
- Batocera integration installer;
- complete controller configuration in EmulationStation;
- verified gameplay and RetroAchievements operation.

Current controller firmware:

```text
R4_CONTROLLER_FW 0.7.0
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
- D-pad;
- A, B, X and Y buttons;
- L1 and R1 shoulder buttons;
- left analog stick;
- right analog stick;
- left and right stick clicks;
- Start and Select buttons;
- dedicated Hotkey button;
- RGB status indicator;
- automatic analog-center calibration;
- CDC command protocol;
- non-blocking LED effects.

## Current prototype pin assignments

| Function | RP2040 pin |
|---|---|
| L1 | GP0 |
| R1 | GP1 |
| D-pad Up | GP2 |
| D-pad Down | GP3 |
| D-pad Left | GP4 |
| D-pad Right | GP5 |
| X | GP6 |
| Y | GP7 |
| Select | GP8 |
| Start | GP9 |
| Hotkey | GP10 |
| Left stick click | GP11 |
| Right stick click | GP12 |
| A | GP13 |
| B | GP14 |
| Unused prototype GPIO | GP15 |
| RGB LED | GP16 |
| Left stick X | GP26 / ADC0 |
| Left stick Y | GP27 / ADC1 |
| Right stick X | GP28 / ADC2 |
| Right stick Y | GP29 / ADC3 |

All digital buttons are connected between their GPIO pin and GND.

The firmware enables internal pull-up resistors:

```text
Released: HIGH
Pressed: LOW
```

The buttons must not be connected to `3V3` or `5V`.

Both analog stick modules are powered from `3V3`.

`GP15` is currently the only unused directly accessible GPIO in the breadboard prototype. Additional controls and peripherals should use bus-based expansion instead of relying on the remaining single pin.

## Current HID mapping

| Physical input | HID input |
|---|---|
| D-pad | Hat switch |
| Left stick | X and Y |
| Right stick | Rx and Ry |
| A | Gamepad A |
| B | Gamepad B |
| X | Gamepad X |
| Y | Gamepad Y |
| L1 | Gamepad TL |
| R1 | Gamepad TR |
| Select | Gamepad Select |
| Start | Gamepad Start |
| Hotkey | Gamepad Mode |
| Left stick click | Gamepad Thumb Left |
| Right stick click | Gamepad Thumb Right |

The current Linux joystick layout is:

```text
Axis 0: left stick X
Axis 1: left stick Y
Axis 2: unused Z
Axis 3: right stick X
Axis 4: right stick Y
Axis 5: unused Rz
Axis 6: Hat0X
Axis 7: Hat0Y
```

Relevant Linux joystick buttons:

```text
Button 0: A
Button 1: B
Button 3: X
Button 4: Y
Button 6: L1
Button 7: R1
Button 10: Select
Button 11: Start
Button 12: Hotkey
Button 13: L3
Button 14: R3
```

The generic TinyUSB gamepad report exposes additional unused axes and buttons. Unused inputs remain inactive.

## Firmware toolchain

The firmware currently targets:

```text
Pico SDK 2.3.0
```

The project uses:

- Pico SDK;
- TinyUSB;
- RP2040 ADC;
- RP2040 PIO for the onboard WS2812 RGB LED.

## Build the controller firmware

Configure the project with the installed Pico SDK and toolchain, then build:

```powershell
cmake --build .\firmware\r4-controller-fw\build
```

The generated UF2 file is located at:

```text
firmware/r4-controller-fw/build/r4-controller-fw.uf2
```

During startup calibration, both analog sticks must remain released and centered.

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

## Controller configuration in Batocera

The controller is exposed as:

```text
Rarmash R4 Controller
```

The dedicated `Hotkey` button is exposed as `BtnMode` and is assigned to Batocera's `Hotkey Enable` action.

The standard emulator exit combination is:

```text
Hotkey + Start
```

The controller has been tested in EmulationStation and in an emulator using the complete current control layout.

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

Example input response:

```text
LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000
```

Example status response:

```text
FW=0.7.0 LED=0,16,0 BASE=0,16,0 FLASH=0 LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000
```

## LED integration

The RP2040 onboard WS2812 LED is currently used as a development status indicator.

Persistent states are controlled by Batocera:

```text
Green: menu ready
Blue: game running
Amber: firmware mismatch
Red: controller or system error
Off: controller service stopped
```

RetroAchievements trigger a temporary gold flash.

Temporary effects are timed by the RP2040 and do not block Batocera scripts.

The complete chain has been verified during real gameplay:

```text
RetroAchievements event
→ Batocera achievement hook
→ USB CDC command
→ RP2040 LED effect
→ persistent LED state restoration
```

## Development USB identity

The controller currently uses temporary development identifiers:

```text
VID: cafe
PID: 4005
Manufacturer: Rarmash
Product: R4 Controller
Serial: R4-0001
CDC interface: 00
HID interface: 02
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
- Hotkey;
- Home;
- Capture;
- R4;
- Trophy.

The following inputs are already implemented:

- D-pad;
- ABXY;
- two analog sticks;
- L3 and R3;
- L1 and R1;
- Start and Select;
- dedicated Hotkey.

The next controller milestones are:

- analog L2 and R2;
- Home;
- Capture;
- R4;
- Trophy.

Because all four RP2040 external ADC channels are already occupied by the two analog sticks, analog L2 and R2 will require an external ADC or another analog input solution.

Additional digital controls will require a GPIO expander, a button matrix or another bus-based input solution.

## Planned embedded-controller features

Future RP2040 responsibilities may include:

- vibration control;
- secondary display output;
- battery telemetry;
- charging and power-state indication;
- power sequencing;
- watchdog functionality;
- communication with battery-management hardware.

The expected expansion architecture is:

```text
RP2040
├── GPIO expander → additional digital controls
├── external ADC  → analog L2 and R2
├── display bus   → secondary display
└── service bus   → power and battery hardware
```

## License

No public license has been selected yet.
