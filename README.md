# R4 Platform

R4 Platform is an experimental hardware and software ecosystem for custom devices, lightweight agents and a central management Hub.

The current primary hardware target is a custom Batocera-based handheld console built around an Orange Pi 3 LTS main system and an RP2040 embedded controller. The project also includes a general-purpose Hub, shared protocol contracts and Linux agents for future R4 devices.

## Current status

The handheld prototype currently has a working composite USB HID and CDC controller with two analog sticks, D-pad, ABXY, L1/R1, left and right stick switches (commonly called L3/R3), Start, Select and the R4 system button, which Batocera/RetroArch uses as its Hotkey. Automatic stick calibration, RGB status indication, reconnect handling, game lifecycle hooks and physical RetroAchievements feedback are implemented.

Firmware `0.12.0` provides the current controller and OLED behavior plus a
two-step transition to the RP2040 ROM USB bootloader. R4 Batocera integration
`0.12.0` adds a guarded host-assisted USB firmware update command. It keeps
Capture SHORT screenshots fully supported
but disables Instant Replay by default. Software H.264 capture on the Orange Pi
3 LTS cannot sustain useful gameplay frame rates and heavily loads all four
Cortex-A53 cores, so replay remains available only as an explicitly enabled
experimental developer feature. Capture LONG is ignored in the normal
configuration. R4 Game Card uses one filesystem with a read-only ROM view and
writable screenshot/video views. The exFAT Game Card, read-only ROM view,
writable capture views, BUSY protection and safe eject are verified on
Batocera 40; the physical Capture input, external ADC and physical OLED are not
yet hardware-verified.

The Batocera integration can be installed and updated automatically. The complete controller layout, gameplay path and RetroAchievements indication have been verified on hardware.

The Hub, shared protocol, simulator and Linux agent are available as JVM modules.

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
                  ├── replay/capture manager
                  ├── R4 Game Card logical views
                         └── R4 Batocera integration
```

The gameplay and service paths are intentionally separated: HID carries regular controller input, while CDC handles status, diagnostics, LED control and future embedded peripherals.

Separately, R4 platform devices communicate with the central Hub through shared protocol contracts. The simulator exercises this path without physical hardware, and the Linux agent provides the device-side runtime for Linux hosts.

## Modules

- `hub` — central Spring Boot service.
- `protocol` — shared API contracts.
- `simulator` — test device agent.
- `agent-linux` — Linux host agent.
- `firmware/r4-controller-fw` — RP2040 controller firmware.
- `integration/batocera` — Batocera-side controller integration.

## Repository layout

```text
r4-platform/
├── agent-linux/
│   └── README.md
├── docs/
│   └── deployment.md
├── firmware/
│   └── r4-controller-fw/
│       └── README.md
├── hub/
├── integration/
│   └── batocera/
│       └── README.md
├── protocol/
├── simulator/
├── build.gradle.kts
├── compose.production.yml
├── docker-compose.yml
├── settings.gradle.kts
└── README.md
```

## Quick start

JDK 21 is required for the JVM modules.

Build and check all JVM modules:

```bash
./gradlew clean check
```

On Windows, use `.\gradlew.bat clean check`.

Run the Hub:

```bash
./gradlew :hub:bootRun
```

Run the Simulator:

```bash
./gradlew :simulator:bootRun
```

Install the Linux Agent on a systemd-based Linux distribution after publishing a tagged GitHub Release:

```bash
curl -fsSL https://raw.githubusercontent.com/Rarmash/r4-platform/main/agent-linux/install.sh \
  | sudo bash -s -- install \
      --hub-url "https://hub.example.com" \
      --name "server-01"
```

See the [Linux Agent README](agent-linux/README.md) for updates, status, removal and manual deployment. Production deployment is described in [docs/deployment.md](docs/deployment.md).

## Controller firmware

The RP2040 firmware exposes the physical controls as a USB HID gamepad and
provides a CDC service interface for diagnostics, version reporting, host
state, Capture/Game Card feedback and LED control. It also performs
stick-center calibration and drives the WS2812 status LED.

The Windows OLED emulator can connect directly to the controller over USB CDC and display the framebuffer rendered by RP2040 firmware.

See the [controller firmware README](firmware/r4-controller-fw/README.md) for pin assignments, wiring rules, HID and Linux mappings, build instructions, the CDC protocol and USB identity.

## Batocera integration

The Batocera integration discovers and monitors the controller, validates its
firmware version, recovers from USB reconnects, handles supported screenshots
and retains an opt-in experimental replay path, publishes safe ROM/capture
views from one removable Game Card filesystem, maintains persistent LED states
and connects game and RetroAchievements events to temporary LED effects.

See the [Batocera integration README](integration/batocera/README.md) for installation, controller configuration, service operation and diagnostics.

## Planned handheld controls

The planned final controller includes:

- D-pad;
- ABXY;
- two analog sticks;
- left and right stick clicks (L3 and R3);
- L1 and R1;
- analog L2 and R2;
- Start and Select;
- Home;
- Capture;
- R4 system button (Batocera/RetroArch Hotkey);
- Trophy.

The current prototype already implements the D-pad, ABXY, both analog sticks,
both physical stick switches (L3/R3), L1/R1, Start, Select and R4. Software
contracts now exist for analog L2/R2 and Capture/Trophy; the host-side Capture
screenshot path is implemented, while their physical input hardware remains a
future milestone alongside Home.

All four external RP2040 ADC channels are occupied by the two analog sticks, so analog L2 and R2 require an external ADC or another analog input solution. Additional digital controls require a GPIO expander, button matrix or another bus-based input solution.

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
