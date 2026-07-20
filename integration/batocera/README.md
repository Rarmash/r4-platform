# R4 Batocera Integration

Batocera-side integration for the R4 embedded controller based on RP2040.

The RP2040 is exposed to the host as a composite USB device:

- HID gamepad for regular controller input;
- CDC ACM service interface for system commands, status exchange and hardware control.

The current implementation supports:

- automatic embedded-controller discovery;
- firmware version validation;
- USB disconnect and reconnect handling;
- persistent LED states;
- temporary non-blocking LED effects;
- game start and stop events;
- RetroAchievements notifications;
- dual analog sticks;
- D-pad input;
- ABXY buttons;
- L1 and R1 shoulder buttons;
- L3 and R3 buttons;
- Start and Select buttons;
- dedicated Hotkey button;
- complete EmulationStation controller mapping;
- standard `Hotkey + Start` emulator exit handling.

## Components

- `bin/r4-ecctl` — discovers the RP2040 CDC interface and sends service commands.
- `bin/r4-led-state` — stores and applies the current persistent LED mode.
- `services/R4Controller` — monitors the embedded controller and handles reconnection.
- `scripts/R4GameState` — changes the LED state when a game starts or stops.
- `emulationstation/achievements/R4Achievement` — triggers a temporary achievement flash.
- `install.sh` — installs or updates the complete Batocera integration.

## Repository layout

```text
integration/batocera/
├── bin/
│   ├── r4-ecctl
│   └── r4-led-state
├── services/
│   └── R4Controller
├── scripts/
│   └── R4GameState
├── emulationstation/
│   └── achievements/
│       └── R4Achievement
├── install.sh
└── README.md
```

## Installation paths

| Repository file | Batocera path |
|---|---|
| `bin/r4-ecctl` | `/userdata/system/r4/r4-ecctl` |
| `bin/r4-led-state` | `/userdata/system/r4/r4-led-state` |
| `services/R4Controller` | `/userdata/system/services/R4Controller` |
| `scripts/R4GameState` | `/userdata/system/scripts/R4GameState` |
| `emulationstation/achievements/R4Achievement` | `/userdata/system/configs/emulationstation/scripts/achievements/R4Achievement` |

## Installation

Copy the complete integration directory to Batocera:

```sh
scp -r integration/batocera \
    root@batocera:/userdata/system/r4-installer
```

Run the installer:

```sh
chmod +x /userdata/system/r4-installer/install.sh

/userdata/system/r4-installer/install.sh
```

The installer is idempotent and can also be used to update an existing installation.

It:

- stops the currently installed service;
- creates the required directories;
- replaces the installed files;
- restores executable permissions;
- enables the controller service;
- starts the controller service;
- checks the connected firmware version;
- prints the resulting controller status.

## Manual executable permissions

All installed scripts must be executable:

```sh
chmod +x /userdata/system/r4/r4-ecctl
chmod +x /userdata/system/r4/r4-led-state
chmod +x /userdata/system/services/R4Controller
chmod +x /userdata/system/scripts/R4GameState
chmod +x /userdata/system/configs/emulationstation/scripts/achievements/R4Achievement
```

## Controller service

Enable the watchdog:

```sh
batocera-services enable R4Controller
```

Start it:

```sh
batocera-services start R4Controller
```

Restart it:

```sh
batocera-services restart R4Controller
```

Check its state:

```sh
/userdata/system/services/R4Controller status
```

Example:

```text
Service: running (PID 13095)
State: online
Controller: FW=0.7.0 LED=0,16,0 BASE=0,16,0 FLASH=0 LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000
```

The service:

- automatically discovers the RP2040 by USB VID, PID, serial number and interface;
- checks the reported firmware version;
- monitors controller availability;
- detects USB disconnection;
- handles USB reconnection without restarting Batocera;
- restores the current persistent LED mode after reconnection;
- displays an amber status when the firmware version is unexpected.

## LED states

- Green — the Batocera menu is ready.
- Blue — a game is running.
- Gold — a RetroAchievements event was received.
- Amber — the connected firmware version is unexpected.
- Red — reserved for controller or system errors.
- Off — the controller service is stopped.

Persistent modes are stored by Batocera:

```sh
/userdata/system/r4/r4-led-state set ready
/userdata/system/r4/r4-led-state set playing
/userdata/system/r4/r4-led-state set warning
/userdata/system/r4/r4-led-state set error
/userdata/system/r4/r4-led-state set off
```

Read the current persistent mode:

```sh
/userdata/system/r4/r4-led-state get
```

Reapply the stored mode:

```sh
/userdata/system/r4/r4-led-state apply
```

Temporary effects are handled directly by the RP2040 firmware:

```sh
/userdata/system/r4/r4-ecctl LED FLASH 32 12 0 1000
```

The firmware keeps the persistent base color separately from the temporary output color and restores the current base color when the effect finishes.

## Game state integration

`R4GameState` receives Batocera game lifecycle events.

When a game starts:

```text
gameStart
```

the persistent LED mode becomes blue.

When a game stops:

```text
gameStop
```

the persistent LED mode becomes green.

Game events are logged to:

```text
/userdata/system/r4/r4-game-events.log
```

## RetroAchievements integration

`R4Achievement` receives RetroAchievements events from EmulationStation.

Each event:

- is written to the achievement log;
- triggers a non-blocking gold LED flash;
- leaves timing and base-color restoration to the RP2040 firmware.

Achievement events are logged to:

```text
/userdata/system/r4/r4-achievements.log
```

Example:

```text
2026-07-20 21:18:28 ACHIEVEMENT ID=143820 TITLE=Looking Better than Ever DESCRIPTION=Collect a Mushroom
```

The complete achievement path has been verified during real gameplay.

## Service commands

Check connectivity:

```sh
/userdata/system/r4/r4-ecctl PING
```

Expected response:

```text
PONG
```

Read the firmware version:

```sh
/userdata/system/r4/r4-ecctl VERSION
```

Expected response:

```text
R4_CONTROLLER_FW 0.7.0
```

Read the current controller input:

```sh
/userdata/system/r4/r4-ecctl INPUT
```

Example:

```text
LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000
```

Read complete controller state:

```sh
/userdata/system/r4/r4-ecctl STATUS
```

Example:

```text
FW=0.7.0 LED=0,16,0 BASE=0,16,0 FLASH=0 LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000
```

Set a persistent LED color:

```sh
/userdata/system/r4/r4-ecctl LED 0 16 0
/userdata/system/r4/r4-ecctl LED 0 0 16
```

Start a temporary LED effect:

```sh
/userdata/system/r4/r4-ecctl LED FLASH 32 12 0 1000
```

Turn the LED off:

```sh
/userdata/system/r4/r4-ecctl LED OFF
```

Show available commands:

```sh
/userdata/system/r4/r4-ecctl HELP
```

## Current controller input

Firmware `0.7.0` currently exposes:

- D-pad;
- A, B, X and Y buttons;
- L1 and R1 shoulder buttons;
- left analog stick;
- right analog stick;
- left stick button;
- right stick button;
- Start;
- Select;
- dedicated Hotkey.

Current HID mapping:

| Physical input | HID input |
|---|---|
| D-pad | `Hat0X` and `Hat0Y` |
| Left stick X | `X` |
| Left stick Y | `Y` |
| Right stick X | `Rx` |
| Right stick Y | `Ry` |
| A | `BtnA` |
| B | `BtnB` |
| X | `BtnX` |
| Y | `BtnY` |
| L1 | `BtnTL` |
| R1 | `BtnTR` |
| Select | `BtnSelect` |
| Start | `BtnStart` |
| Hotkey | `BtnMode` |
| Left stick click | `BtnThumbL` |
| Right stick click | `BtnThumbR` |

The generic TinyUSB gamepad report also exposes unused `Z` and `Rz` axes. They remain centered at zero.

## D-pad values

The CDC protocol reports the D-pad as a HID hat value:

| Value | Direction |
|---:|---|
| `0` | Centered |
| `1` | Up |
| `2` | Up-right |
| `3` | Right |
| `4` | Down-right |
| `5` | Down |
| `6` | Down-left |
| `7` | Left |
| `8` | Up-left |

Opposite directions on the same axis cancel each other:

- Up and Down result in a neutral vertical direction.
- Left and Right result in a neutral horizontal direction.

## Button masks

The current CDC button masks are:

| Physical input | Button mask |
|---|---|
| A | `0x00000001` |
| B | `0x00000002` |
| X | `0x00000008` |
| Y | `0x00000010` |
| L1 | `0x00000040` |
| R1 | `0x00000080` |
| Select | `0x00000400` |
| Start | `0x00000800` |
| Hotkey | `0x00001000` |
| L3 | `0x00002000` |
| R3 | `0x00004000` |

Multiple pressed buttons are combined into one bit mask.

## Input testing

Check the short diagnostic response:

```sh
/userdata/system/r4/r4-ecctl INPUT
```

Test the Linux joystick interface:

```sh
jstest /dev/input/js0
```

The current joystick layout is:

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

Exit `jstest` with:

```text
Ctrl+C
```

## EmulationStation configuration

The controller is detected as:

```text
Rarmash R4 Controller
```

The physical Hotkey button is mapped to:

```text
Hotkey Enable
```

The standard emulator exit combination is:

```text
Hotkey + Start
```

Analog L2 and R2 are not implemented yet and should be skipped during controller configuration.

## Supported firmware

```text
R4_CONTROLLER_FW 0.7.0
```

## Development USB identity

The USB identifiers currently used during development are:

```text
VID: cafe
PID: 4005
Manufacturer: Rarmash
Product: R4 Controller
Serial: R4-0001
CDC interface: 00
HID interface: 02
```

The VID and PID are temporary development identifiers and must be reconsidered before public distribution.

## Runtime files

The integration creates temporary state files:

```text
/tmp/r4-ecctl.lock
/tmp/r4-controller-service.pid
/tmp/r4-controller-service.state
/tmp/r4-controller-led-mode
```

Persistent logs are stored in:

```text
/userdata/system/r4/r4-controller.log
/userdata/system/r4/r4-game-events.log
/userdata/system/r4/r4-achievements.log
```

Runtime files and logs are not part of the repository.

## Remaining controller work

The current prototype does not yet include:

- analog L2 and R2;
- Home;
- Capture;
- R4;
- Trophy;
- vibration;
- the secondary display;
- battery and power telemetry.

All four external RP2040 ADC channels are already occupied by the two analog sticks.

Additional analog inputs will require an external ADC or another analog input solution.

Additional digital controls should use a GPIO expander, button matrix or another bus-based expansion solution.
