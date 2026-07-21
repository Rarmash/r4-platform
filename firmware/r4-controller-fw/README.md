# R4 Controller Firmware

RP2040 firmware for the R4 handheld controller. It exposes a composite USB device with a HID gamepad interface for regular input and a CDC ACM interface for diagnostics, status exchange and LED control.

For host-side discovery, monitoring and event integration, see the [Batocera integration README](../../integration/batocera/README.md). For the wider project architecture, see the [root README](../../README.md).

## Capabilities

The current firmware supports:

- D-pad;
- A, B, X and Y buttons;
- L1 and R1 shoulder buttons;
- left and right analog sticks;
- left and right stick clicks (commonly called L3 and R3);
- Start and Select buttons;
- dedicated Hotkey button;
- automatic analog-stick center calibration at startup;
- composite USB HID and CDC operation;
- controller input and status diagnostics over CDC;
- persistent RGB LED colors;
- timed, non-blocking RGB LED flashes that restore the base color.

## Prototype pin assignments

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
| Left stick click (L3) | GP11 |
| Right stick click (R3) | GP12 |
| A | GP13 |
| B | GP14 |
| Unused prototype GPIO | GP15 |
| RGB LED | GP16 |
| Left stick X | GP26 / ADC0 |
| Left stick Y | GP27 / ADC1 |
| Right stick X | GP28 / ADC2 |
| Right stick Y | GP29 / ADC3 |

`GP15` is the only unused directly accessible GPIO in the current breadboard prototype. Additional controls and peripherals should use bus-based expansion rather than relying on this final pin.

## Wiring rules

Connect every digital button between its assigned GPIO and GND. The firmware enables the RP2040 internal pull-up resistors, so the electrical states are:

```text
Released: HIGH
Pressed: LOW
```

Do not connect the buttons to `3V3` or `5V`.

Power both analog stick modules from `3V3` and connect their grounds to the RP2040 ground. Connect each X/Y output only to its assigned ADC pin from the table above. All four external RP2040 ADC channels are currently occupied by the two sticks.

Both sticks must remain released and centered while the firmware performs its startup calibration.

## HID mapping

The firmware calls the physical inputs on GP11 and GP12 `PIN_LEFT_STICK_BUTTON` and `PIN_RIGHT_STICK_BUTTON`. They are the switches activated by pressing the analog sticks, conventionally called L3 and R3; there are no separate L3/R3 inputs.

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
| Left stick click (L3) | `GAMEPAD_BUTTON_THUMBL` (Gamepad Thumb Left) |
| Right stick click (R3) | `GAMEPAD_BUTTON_THUMBR` (Gamepad Thumb Right) |

The generic TinyUSB gamepad report also exposes unused `Z` and `Rz` axes. Unused inputs remain centered or inactive.

## Linux joystick mapping

The current axis layout is:

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

Relevant Linux joystick buttons are:

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
Button 13: left stick click (L3 / `BtnThumbL`)
Button 14: right stick click (R3 / `BtnThumbR`)
```

The controller appears as `Rarmash R4 Controller`. The Hotkey is exposed as `BtnMode`; Batocera maps it to `Hotkey Enable`, making `Hotkey + Start` the standard emulator exit combination.

## SDK and dependencies

The firmware targets the RP2040 using Pico SDK 2.3.0 and CMake. It uses:

- Pico SDK and `pico_stdlib`;
- TinyUSB for the composite HID and CDC USB device;
- the RP2040 ADC peripheral for both analog sticks;
- the RP2040 PIO and clock peripherals for the WS2812 RGB LED.

An installed Arm GNU toolchain, Pico SDK and its host tools (`pioasm` and `picotool`) are required to configure and build the project.

## Build

From the repository root, configure a Release build with the installed Pico SDK:

```bash
cmake -S firmware/r4-controller-fw \
  -B firmware/r4-controller-fw/build \
  -DCMAKE_BUILD_TYPE=Release
```

Build the firmware:

```bash
cmake --build firmware/r4-controller-fw/build
```

PowerShell uses the same CMake options; paths may be written with backslashes:

```powershell
cmake -S .\firmware\r4-controller-fw `
  -B .\firmware\r4-controller-fw\build `
  -DCMAKE_BUILD_TYPE=Release

cmake --build .\firmware\r4-controller-fw\build
```

The flashable output is `firmware/r4-controller-fw/build/r4-controller-fw.uf2`.

## CDC command protocol

Commands are sent as text lines over the CDC ACM interface. Responses use CRLF line endings.

| Command | Purpose | Example response |
|---|---|---|
| `PING` | Check connectivity | `PONG` |
| `VERSION` | Read the firmware identity and version | `R4_CONTROLLER_FW <version>` |
| `INPUT` | Read the latest HID input state | `LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000` |
| `STATUS` | Read firmware, LED and input state | `FW=<version> LED=0,16,0 BASE=0,16,0 FLASH=0 LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000` |
| `LED <R> <G> <B>` | Set the persistent base color | `OK LED 0 16 0` |
| `LED FLASH <R> <G> <B> <MS>` | Start a temporary color effect | `OK LED FLASH 32 12 0 1000` |
| `LED OFF` | Clear the base color and active flash | `OK LED OFF` |
| `HELP` | List supported commands | `COMMANDS PING VERSION INPUT STATUS LED <R> <G> <B> LED FLASH <R> <G> <B> <MS> LED OFF HELP` |

Color components must be in the range `0..255`. Flash duration must be in the range `1..10000` milliseconds. Invalid syntax or values return an `ERR ...` response, and unknown commands return `ERR UNKNOWN_COMMAND`.

The `VERSION` response format is stable: the actual version is generated from the version configured in `CMakeLists.txt`. The `STATUS` response reports the same version without the `R4_CONTROLLER_FW` prefix.

The D-pad is reported as a HID hat value:

| Value | Direction |
|---:|---|
| 0 | Centered |
| 1 | Up |
| 2 | Up-right |
| 3 | Right |
| 4 | Down-right |
| 5 | Down |
| 6 | Down-left |
| 7 | Left |
| 8 | Up-left |

Opposite directions on the same axis cancel each other.

## Development USB identity

The firmware currently uses temporary development identifiers:

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
