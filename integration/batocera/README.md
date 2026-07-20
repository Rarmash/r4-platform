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
- RetroAchievements notifications.

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
Controller: FW=0.5.1 LED=0,16,0 BASE=0,16,0 FLASH=0 LX=0 LY=0 RX=0 RY=0 BUTTONS=0x00000000
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

Read the current controller input:

```sh
/userdata/system/r4/r4-ecctl INPUT
```

Example:

```text
LX=0 LY=0 RX=0 RY=0 BUTTONS=0x00000000
```

Read complete controller state:

```sh
/userdata/system/r4/r4-ecctl STATUS
```

Example:

```text
FW=0.5.1 LED=0,16,0 BASE=0,16,0 FLASH=0 LX=0 LY=0 RX=0 RY=0 BUTTONS=0x00000000
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

Firmware `0.5.1` currently exposes:

- left analog stick;
- right analog stick;
- left stick button;
- right stick button;
- two face buttons.

Current HID mapping:

| Physical input | HID input |
|---|---|
| Left stick X | `X` |
| Left stick Y | `Y` |
| Right stick X | `Rx` |
| Right stick Y | `Ry` |
| Left stick click | `BtnThumbL` |
| Right stick click | `BtnThumbR` |
| Face button 1 | `BtnA` |
| Face button 2 | `BtnB` |

The D-pad, remaining face buttons, shoulder buttons and system buttons are planned but are not yet implemented.

## Supported firmware

```text
R4_CONTROLLER_FW 0.5.1
```

## Development USB identity

The USB identifiers currently used during development are:

```text
VID: cafe
PID: 4005
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
