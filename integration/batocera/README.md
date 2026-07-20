# R4 Batocera Integration

Batocera-side integration for the R4 embedded controller based on RP2040.

The RP2040 is exposed to the host as a composite USB device:

- HID gamepad for regular controller input;
- CDC ACM service interface for system commands and status exchange.

## Components

- `bin/r4-ecctl` — discovers the RP2040 CDC interface and sends service commands.
- `bin/r4-led-state` — stores and applies the current persistent LED mode.
- `services/R4Controller` — monitors the embedded controller and handles reconnection.
- `scripts/R4GameState` — changes the LED state when a game starts or stops.
- `emulationstation/achievements/R4Achievement` — triggers a temporary achievement flash.

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

All installed scripts must be executable.

```sh
chmod +x /userdata/system/r4/r4-ecctl
chmod +x /userdata/system/r4/r4-led-state
chmod +x /userdata/system/services/R4Controller
chmod +x /userdata/system/scripts/R4GameState
chmod +x /userdata/system/configs/emulationstation/scripts/achievements/R4Achievement
```

## Installation

Copy the integration directory to Batocera and execute the installer:

```sh
chmod +x install.sh
./install.sh
```

The installer is idempotent and can also be used to update an existing installation. It stops the current controller service, replaces the installed files, restores executable permissions, enables the service and starts it again.

## Controller service

Enable and start the watchdog:

```sh
batocera-services enable R4Controller
batocera-services start R4Controller
```

Check its state:

```sh
/userdata/system/services/R4Controller status
```

The service:

- automatically discovers the RP2040 by USB VID, PID, serial number and interface;
- checks the firmware version;
- monitors controller availability;
- handles USB disconnection and reconnection;
- restores the current LED mode after reconnection.

## LED states

- Green — Batocera menu is ready.
- Blue — a game is running.
- Gold — a RetroAchievement event was received.
- Amber — the connected firmware version is unexpected.
- Off — the controller service is stopped.

Persistent states are set by Batocera:

```sh
r4-led-state set ready
r4-led-state set playing
```

Temporary effects are handled by the RP2040 firmware:

```sh
r4-ecctl LED FLASH 32 12 0 1000
```

The firmware restores the current base color when the effect finishes.

## Service commands

Examples:

```sh
r4-ecctl PING
r4-ecctl VERSION
r4-ecctl INPUT
r4-ecctl STATUS

r4-ecctl LED 0 16 0
r4-ecctl LED 0 0 16
r4-ecctl LED FLASH 32 12 0 1000
r4-ecctl LED OFF
```

## Supported firmware

```text
R4_CONTROLLER_FW 0.4.0
```

The USB identifiers currently used for development are:

```text
VID: cafe
PID: 4005
Serial: R4-0001
CDC interface: 00
```

The VID and PID are temporary development identifiers and must be reconsidered before public distribution.
