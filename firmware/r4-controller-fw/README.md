# R4 Controller Firmware

RP2040 firmware for the R4 handheld controller. Version `0.8.0` keeps the
working `0.7.0` USB HID + CDC behavior and adds hardware-independent foundations
for analog triggers, service buttons and a future monochrome OLED.

The [Batocera integration](../../integration/batocera/README.md) owns host-side
discovery and lifecycle events. The [root README](../../README.md) describes the
wider platform.

## Implemented and planned hardware

The current physical prototype has a D-pad, ABXY, L1/R1, two analog sticks,
Start, Select, the R4 system button, the switches built into both sticks, and
one WS2812 LED. R4 is exposed as HID `Mode` and configured as the
Batocera/RetroArch Hotkey; there is no separate Hotkey button.
The stick switches on GP11/GP12 are conventionally called L3/R3; they are not
extra inputs invented by the firmware.

LT/RT now exist in the logical controller state and USB report, but no physical
trigger ADC is claimed. With the production RP2040 backend they remain released
and report `NO_SOURCE`. R4 on GP10 feeds both the unchanged HID Hotkey bit and
the service-gesture recognizer. Capture and Trophy have tested gesture logic and
CDC events, but no physical GPIO or expander backend yet. There is no OLED
hardware driver.

## Architecture

The main loop keeps TinyUSB and RGB timing on the RP2040. Reusable logic lives
under `core/` and has no Pico SDK dependency:

- `r4_controller` — logical state, analog normalization and HID report;
- `r4_input_source` — contracts for direct GPIO, built-in/external ADC,
  GPIO expander and mock sources;
- `r4_protocol` — bounded text-protocol parser and response formatting;
- `r4_service_buttons` — debounced short/long/double gesture recognizer;
- `r4_display` — OLED state model and monochrome framebuffer renderer.

`rp2040_input.c` is the only layer that knows the current pins and built-in ADC
channels. Future external ADC and GPIO-expander implementations should plug into
the same source/backend interfaces instead of entering the controller logic.

## Current prototype pinout

| Function | RP2040 pin |
|---|---|
| L1 / R1 | GP0 / GP1 |
| D-pad Up / Down / Left / Right | GP2 / GP3 / GP4 / GP5 |
| X / Y | GP6 / GP7 |
| Select / Start / R4 system button | GP8 / GP9 / GP10 |
| Left / right stick switch (L3 / R3) | GP11 / GP12 |
| A / B | GP13 / GP14 |
| Unused prototype GPIO | GP15 |
| WS2812 RGB LED | GP16 |
| Left stick X / Y | GP26 ADC0 / GP27 ADC1 |
| Right stick X / Y | GP28 ADC2 / GP29 ADC3 |

Connect every digital button between its GPIO and GND. Internal pull-ups make a
released input HIGH and a pressed input LOW. Do not connect buttons to 3V3 or
5V. Power stick modules from 3V3, share ground, and connect their outputs only
to the listed ADC pins. Keep both sticks centered during startup calibration.
All four exposed RP2040 ADC inputs are occupied, so LT/RT need a future external
ADC or another verified analog solution.

## Analog trigger model

Each trigger is stored internally as an independent `uint16_t` in `0..65535`.
Normalization accepts a source sample plus per-trigger `raw_min`, `raw_max`,
released-end dead zone and inversion:

- values outside the calibrated range are clamped;
- dead-zone values normalize to zero;
- `min >= max` or a dead zone covering the span is
  `INVALID_CALIBRATION`;
- an absent source is `NO_SOURCE`;
- an unavailable converter is `ADC_UNAVAILABLE`.

These cases produce a released trigger instead of stale or undefined input. A
future ADC backend may use any raw resolution without changing the logical or
CDC representation.

## HID mapping and compatibility

The report remains 11 bytes and retains the existing field order. X/Y and Rx/Ry
are signed centered axes, while LT and RT use the former Z and Rz bytes as
independent unsigned `0..255` axes. The internal 16-bit values are rounded to
those HID bytes. D-pad and button bit positions are unchanged.

| Physical/logical input | HID usage |
|---|---|
| Left stick | X, Y |
| LT | Z, logical range `0..255` |
| Right stick | Rx, Ry |
| RT | Rz, logical range `0..255` |
| D-pad | Hat switch |
| A/B/X/Y | Gamepad A/B/X/Y |
| L1/R1 | TL/TR |
| Select / Start / R4 | Select / Start / Mode |
| Stick switches | Thumb Left / Thumb Right |

Z and Rz were selected because Linux/SDL controller stacks conventionally expose
the two triggers as unipolar axes corresponding to `ABS_Z` and `ABS_RZ`.
Batocera/RetroArch may regard the changed descriptor as a controller
reconfiguration: after flashing `0.8.0`, remap LT and RT as analog axes and
verify every existing button plus `R4 + Start`. Do not map LT/RT as digital
buttons unless a core specifically requires it.

Reference behavior: Linux maps DualSense trigger values to
[`ABS_Z` and `ABS_RZ`](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c),
and Libretro documents that analog L2/R2 should be configured as
[axes](https://docs.libretro.com/guides/controller-autoconfiguration/).

Typical Linux joystick axes remain:

```text
Axis 0: left stick X
Axis 1: left stick Y
Axis 2: LT (Z, 0..255)
Axis 3: right stick X
Axis 4: right stick Y
Axis 5: RT (Rz, 0..255)
Axis 6/7: Hat0X/Hat0Y
```

The existing Linux button positions are unchanged: A 0, B 1, X 3, Y 4, L1 6,
R1 7, Select 10, Start 11, R4 (`BtnMode`) 12, left stick switch 13 and right
stick switch 14.

## Building

The firmware targets Pico SDK 2.3.0 and uses Pico stdlib, TinyUSB, ADC and PIO.
An Arm GNU toolchain plus the SDK host tools are required.

From the repository root on PowerShell:

```powershell
$env:PICO_SDK_PATH = 'C:/path/to/pico-sdk'
cmake -S ./firmware/r4-controller-fw `
  -B ./firmware/r4-controller-fw/build `
  -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build ./firmware/r4-controller-fw/build
```

On another machine, point `PICO_SDK_PATH` at its installed SDK. The flashable
artifact is `build/r4-controller-fw.uf2`.

The test-input commands are compile-time disabled by default. For a deliberately
test-only image:

```powershell
cmake -S ./firmware/r4-controller-fw `
  -B ./firmware/r4-controller-fw/build-test `
  -G Ninja -DCMAKE_BUILD_TYPE=Debug `
  -DR4_ENABLE_TEST_INPUT=ON
cmake --build ./firmware/r4-controller-fw/build-test
```

Do not deploy that image as the normal production build.

## CDC protocol

CDC uses one UTF-8/ASCII command per line and CRLF responses. The maximum input
line is 255 bytes excluding the terminator. On overflow the firmware discards
through the next newline and returns `ERR LINE_TOO_LONG`. Parsing is bounded and
never waits inside the USB loop.

The stable commands still work:

| Command | Response |
|---|---|
| `PING` | `PONG` |
| `VERSION` | `R4_CONTROLLER_FW 0.8.0` |
| `INPUT` | existing fields followed by `LT`, `RT`, `LT_STATUS`, `RT_STATUS` |
| `STATUS` | existing firmware/LED/input fields followed by trigger fields |
| `LED R G B` | `OK LED R G B` |
| `LED FLASH R G B MS` | `OK LED FLASH R G B MS` |
| `LED OFF` | `OK LED OFF` |
| `FRAMEBUFFER INFO` | freeze and describe a rendered OLED snapshot |
| `FRAMEBUFFER CHUNK ID=n OFFSET=n LENGTH=n` | read one packed snapshot chunk |
| `HOST HEARTBEAT` | `OK HOST HEARTBEAT` |
| `HELP` | command summary |

Example production input without trigger hardware:

```text
LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000 LT=0 RT=0 LT_STATUS=NO_SOURCE RT_STATUS=NO_SOURCE
```

New host-to-RP2040 state messages use `KEY=VALUE` fields:

```text
HOST STATE MODE=HOME
HOST STATE MODE=ERROR ERROR_HEX=4c494e4b204c4f5354
HOST GAME ACTION=START SYSTEM_HEX=6e6573 GAME_HEX=6d6172696f2e7a6970
HOST GAME ACTION=STOP
HOST RA ACTIVE=1
HOST ACHIEVEMENT ID=143820 TITLE_HEX=46495253542057494e
HOST HEARTBEAT
HOST TELEMETRY BATTERY=78 RUNTIME_MIN=155 VOLUME=65 POWER=BATTERY NETWORK=UP TEMP_MILLIC=42125 TIME_HEX=31323a3334
```

Hex fields carry arbitrary text without spaces confusing tokenization. Unknown
additional fields are ignored for forward compatibility; missing or invalid
required fields return a command-specific `ERR ...`. Unknown commands return
`ERR UNKNOWN_COMMAND`.

Framebuffer transfer is request/response only. `FRAMEBUFFER INFO` renders the
current RP2040 display model into the firmware-owned 128x64 test profile, packs
it as row-major `MONO1_MSB`, assigns a snapshot ID and returns its byte count,
FNV-1a hash and maximum chunk size. The client then requests that immutable ID
in chunks of at most 96 bytes. Every `FRAMEBUFFER DATA` response stays within
255 characters. A stale ID, invalid range or missing snapshot returns a bounded
`ERR FRAMEBUFFER_...` response. Only one chunk is returned per command, so the
main USB loop runs between chunks and framebuffer data cannot be confused with
ordinary command replies or button events.
Service-button events are queued on the RP2040 and are never sent
asynchronously into an unrelated response:

```text
EVENT NEXT
EVENT BUTTON=CAPTURE ACTION=SHORT TIME_MS=1450 SEQ=3
```

An empty queue returns `EVENT NONE`. Buttons are `CAPTURE`, `R4`, `TROPHY`;
actions are `PRESS`, `RELEASE`, `SHORT`, `LONG`, `DOUBLE`. `BATTERY` accepts
`0..100` or `NA`, `POWER` accepts `BATTERY` or `EXTERNAL`, `NETWORK` accepts
`UP` or `DOWN`, `RUNTIME_MIN` accepts `0..5999` or `NA`, and `TEMP_MILLIC`
is signed milli-degrees Celsius or `NA`. `VOLUME` accepts `0..100` or `NA`.
Telemetry and host messages form the software boundary for battery, power,
temperature, network, time, game, RetroAchievements and future OLED state.
Extra capability fields can be added
without redefining existing ones; no adaptive-trigger mechanism is implemented.

The 128x64 profile uses a readable 5x7 uppercase ASCII font. Its top status bar
shows time, external/battery power, charge percentage and estimated runtime.
Home identifies the device as `R4 BATOCERA` and shows firmware version,
RetroAchievements and volume. The game footer replaces firmware version with a
session timer; long game titles wrap onto a second line. Temperature remains
available on the diagnostic screen. An achievement temporarily replaces the
bottom area and is hidden by firmware after five seconds, after which the
normal status row returns. Runtime is supplied by the host because percentage
alone is insufficient for a reliable estimate.

`HOST HEARTBEAT` explicitly arms a seven-second firmware watchdog. Heartbeats
and other valid `HOST ...` traffic refresh it. If the host becomes silent,
RP2040 switches to `ERROR / HOST LINK LOST`; the previous display screen is
restored when host traffic resumes. The watchdog is not armed until the first
heartbeat, so standalone CDC/HID use and older clients do not receive a false
link error.

### Testing LT/RT without an ADC

Flash the explicitly test-enabled build, then use `r4-ecctl`:

```sh
/userdata/system/r4/r4-ecctl TEST TRIGGERS 0 65535
/userdata/system/r4/r4-ecctl INPUT
```

The second response must show `LT=0 RT=65535` and `OK` statuses; Linux should
show Z released and Rz fully pressed. Test midpoint and swapped extremes too.
A normal build must respond `ERR TEST_MODE_DISABLED`.

### Testing service-button gestures

The test-enabled firmware accepts deterministic virtual time:

```text
TEST BUTTON BUTTON=CAPTURE ACTION=DOWN TIME_MS=1000
TEST BUTTON BUTTON=CAPTURE ACTION=UP TIME_MS=1100
TEST BUTTON BUTTON=CAPTURE ACTION=TICK TIME_MS=1500
EVENT NEXT
```

This produces Capture `PRESS`, `RELEASE`, then `SHORT` after the configurable
double-click window. Unit tests cover debounce, R4 short/long/double press,
release and one-shot behavior. Physical R4 uses GP10; no physical Capture or
Trophy pins are assigned yet.

R4 combinations stay on the normal HID path. `R4 + Start` therefore continues
to exit a game, and other `R4 + button` combinations remain available to
Batocera/RetroArch. Once another button or D-pad direction joins a held R4, the
gesture recognizer cancels the pending standalone R4 action. A standalone short
R4 is reserved for the future system panel; long and double actions remain
distinct events governed by the configurable thresholds.

## Interactive OLED emulator with a real RP2040

The primary GUI mode does not render the OLED model on Windows. Firmware owns
the display state, renderer and framebuffer; the application acts as an Orange
Pi replacement, retrieves completed snapshots and scales black/white pixels by
an integer factor.

### 1. Build and flash the updated firmware

Use the normal Pico SDK build described above and flash:

```text
firmware/r4-controller-fw/build/r4-controller-fw.uf2
```

The connected board must answer `R4_CONTROLLER_FW 0.8.0` and support
`FRAMEBUFFER INFO`. This feature is available in the normal production build;
`R4_ENABLE_TEST_INPUT` is not required.

### 2. Install the Windows GUI dependency

Python 3 with Tkinter is required. Install the small serial dependency from the
repository root:

```powershell
py -3 -m pip install -r `
  ./firmware/r4-controller-fw/oled-emulator/requirements.txt
```

### 3. Connect and run

Connect the RP2040 over USB, then run:

```powershell
py -3 ./firmware/r4-controller-fw/oled-emulator/r4_oled_gui.py
```

No COM port is configured. The worker enumerates Windows serial devices and
accepts only the existing USB identity `VID CAFE`, `PID 4005`, serial
`R4-0001`. It verifies `PING` and `VERSION`, displays the selected COM port and
firmware version, and automatically searches again after reset, unplug or
protocol timeout.

The GUI provides controls for the existing semantic commands:

- Boot, Waiting, Home, Diagnostic and Error through `HOST STATE`;
- game start/stop plus system and game text through `HOST GAME`;
- RetroAchievements state and unlock popup through `HOST RA` and
  `HOST ACHIEVEMENT`;
- battery, estimated runtime, volume, external power, network, time and
  temperature through `HOST TELEMETRY`.

It also polls real `INPUT` state and `EVENT NEXT`, logs changed input/button
events, shows protocol errors, and checks the RP2040 framebuffer every 100 ms.
Unchanged framebuffer hashes skip chunk transfer, while changed frames are
retrieved immediately. The firmware toggles the clock colon every 500 ms. The
GUI also sends `HOST HEARTBEAT` every two seconds. All CDC transactions
are serialized in one worker thread, so a frame transfer cannot overlap another
response.

Select integer scale `1..10` in the GUI. Tk expands the received monochrome
image with integer pixel replication; it does not apply smoothing.

### Network mode through Batocera

In network mode the data path is:

```text
Windows GUI <-> TCP/Wi-Fi <-> R4Controller on Batocera <-> USB CDC <-> RP2040
```

The RP2040 has no wireless connection. Orange Pi provides the network hop and
keeps exclusive ownership of `/dev/ttyACM*`. Other integration helpers and the
diagnostic relay submit serialized requests to `R4Controller`; they do not open
CDC while the service is running.

Install or update the Batocera integration, then edit
`/userdata/system/r4/oled-tcp.conf`. For a trusted local test network,
explicitly enable the listener:

```sh
R4_OLED_TCP_ENABLED=1
R4_OLED_TCP_BIND=0.0.0.0
R4_OLED_TCP_PORT=4274
```

Restart the service with
`batocera-services restart R4Controller`. The installed default is disabled
and bound to loopback. The relay has no authentication or encryption, so do
not expose it outside a trusted test LAN. The installer preserves an existing
`oled-tcp.conf` during updates.

Run the Python GUI on Windows:

```powershell
$batoceraIp = '192.168.1.123'
py -3 ./firmware/r4-controller-fw/oled-emulator/r4_oled_gui.py `
  --tcp "${batoceraIp}:4274"
```

Or run the packaged GUI:

```powershell
$batoceraIp = '192.168.1.123'
./r4-oled-emulator-gui.exe --tcp "${batoceraIp}:4274"
```

The header distinguishes `PC <-> Batocera` from `Batocera <-> RP2040`.
Disconnecting either link triggers automatic reconnect. GUI state controls use
the same `HOST STATE`, `HOST GAME`, `HOST RA`, `HOST ACHIEVEMENT` and
`HOST TELEMETRY` CDC commands as direct USB mode.
The GUI schedules framebuffer checks every 100 ms. In TCP mode it sends one
`FRAMEBUFFER READ HASH=...` broker request. `R4Controller` opens CDC once,
collects the existing safe RP2040 chunks in that session and returns either
`FRAMEBUFFER UNCHANGED` or one complete diagnostic TCP payload. This removes
the former per-chunk process/open delay; actual refresh rate remains bounded by
USB and network latency.

TCP uses one newline-delimited request and one correlated response:

```text
REQ <id> <CDC command>
RES <id> OK <CDC response>
RES <id> ERROR <diagnostic>
```

Request IDs prevent responses from being mistaken for events. The aggregate
`FRAMEBUFFER FULL` response exists only between Batocera and the GUI. On USB,
RP2040 still returns separately requested responses of at most 255 characters,
so framebuffer data cannot enter an ordinary CDC reply or the event queue.

### Package the Windows GUI

Install the packaging dependency and build the dedicated CMake target:

```powershell
py -3 -m pip install -r `
  ./firmware/r4-controller-fw/oled-emulator/requirements-build.txt
cmake --build ./firmware/r4-controller-fw/build-host `
  --target r4-oled-gui-package
```

The executable is written to
`firmware/r4-controller-fw/build-host/oled-gui-dist/r4-oled-emulator-gui.exe`.

### Optional offline fallback

Offline mode is explicit and uses the existing C headless executable rather
than reimplementing the renderer in Python. Build the host tests first, then:

```powershell
py -3 ./firmware/r4-controller-fw/oled-emulator/r4_oled_gui.py `
  --offline `
  --headless-exe `
  ./firmware/r4-controller-fw/build-host/r4-oled-emulator.exe
```

Offline controls select reproducible headless scenarios. They do not emulate
USB input, reconnects or a live RP2040 and are not the primary validation mode.
## Headless snapshots and host tests

The portable suite tests analog normalization, mock sources, HID packing, CDC
parsing/formatting, virtual-time button gestures and display snapshots:

```sh
cmake -S firmware/r4-controller-fw/tests \
  -B firmware/r4-controller-fw/build-host
cmake --build firmware/r4-controller-fw/build-host
ctest --test-dir firmware/r4-controller-fw/build-host --output-on-failure
```

Generate the default headless 128x64 PBM snapshots:

```sh
firmware/r4-controller-fw/build-host/r4-oled-emulator \
  --output-dir oled-snapshots --scenario all
```

The emulator supports `boot`, `waiting`, `home`, `game`, `achievement`,
`diagnostic`, `error`, plus configurable `--width` and `--height`. At 128x64 it
checks stable framebuffer hashes. PBM viewers must use nearest-neighbor/integer
scaling for a pixel-accurate preview. `128x64` is only a test profile because
the physical 1.3-inch OLED model and resolution are not fixed.

## USB identity

The development identity is unchanged:

```text
VID: cafe
PID: 4005
Manufacturer: Rarmash
Product: R4 Controller
Serial: R4-0001
CDC interface: 00
HID interface: 02
```

These temporary identifiers must be reconsidered before public distribution.
