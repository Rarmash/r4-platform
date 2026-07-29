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
- left and right stick clicks (L3 and R3);
- Start and Select buttons;
- R4 system button mapped as the Batocera/RetroArch Hotkey;
- analog LT and RT axes in the `0.8.0` HID descriptor;
- polling of queued R4, Capture and Trophy service events;
- end-to-end Capture SHORT screenshots through `batocera-screenshot`;
- a bounded segmented previous-game replay buffer for Capture LONG;
- one-filesystem R4 Game Card management with read-only ROM and writable
  capture views;
- game, RetroAchievements and display-state forwarding to RP2040;
- complete EmulationStation controller mapping;
- standard `R4 + Start` emulator exit handling.

## Components

- `bin/r4-ecctl` — discovers the RP2040 CDC interface and sends service commands.
- `bin/r4-led-state` — stores and applies the current persistent LED mode.
- `bin/r4-game-title` — resolves a ROM path to its `<name>` in
  EmulationStation `gamelist.xml`.
- `bin/r4-oled-tcp` — optional framed diagnostic TCP relay for the OLED GUI.
- `bin/r4-game-card` — validates, mounts and safely ejects the configured ROM
  card without opening USB CDC directly.
- `bin/r4-replay` — owns the bounded FFmpeg replay ring and capture routing.
- `firmware-version.conf` — defines the expected RP2040 firmware version.
- `oled-tcp.conf` — opt-in TCP bind address and port.
- `game-card.conf` — preserved Game Card identity, mountpoint and polling
  configuration.
- `replay.conf` — preserved replay, encoding and capture-storage settings.
- `services/R4Controller` — monitors the embedded controller and handles reconnection.
- `services/R4GameCard` — polls the removable-card state machine.
- `scripts/R4GameState` — changes the LED state when a game starts or stops.
- `emulationstation/game-start/R4GameMetadata` — shared `game-selected` /
  `game-start` hook that caches and forwards the EmulationStation metadata
  title instead of the ROM filename.
- `emulationstation/achievements/R4Achievement` — triggers a temporary achievement flash.
- `install.sh` — installs or updates the complete Batocera integration.
- `tests/run-tests.sh` — shell syntax and mock service-event checks.

## Repository layout

```text
integration/batocera/
├── firmware-version.conf
├── game-card.conf
├── oled-tcp.conf
├── replay.conf
├── bin/
│   ├── r4-ecctl
│   ├── r4-game-title
│   ├── r4-game-card
│   ├── r4-led-state
│   ├── r4-oled-tcp
│   └── r4-replay
├── services/
│   ├── R4Controller
│   └── R4GameCard
├── scripts/
│   └── R4GameState
├── emulationstation/
│   ├── achievements/
│   │   └── R4Achievement
│   ├── game-start/
│   │   └── R4GameMetadata
│   └── game-selected/
│       └── R4GameMetadata
├── install.sh
├── tests/
│   └── run-tests.sh
└── README.md
```

## Installation paths

| Repository file | Batocera path |
|---|---|
| `bin/r4-ecctl` | `/userdata/system/r4/r4-ecctl` |
| `bin/r4-led-state` | `/userdata/system/r4/r4-led-state` |
| `bin/r4-game-title` | `/userdata/system/r4/r4-game-title` |
| `bin/r4-game-card` | `/userdata/system/r4/r4-game-card` |
| `bin/r4-oled-tcp` | `/userdata/system/r4/r4-oled-tcp` |
| `bin/r4-replay` | `/userdata/system/r4/r4-replay` |
| `firmware-version.conf` | `/userdata/system/r4/firmware-version.conf` |
| `game-card.conf` | `/userdata/system/r4/game-card.conf` |
| `oled-tcp.conf` | `/userdata/system/r4/oled-tcp.conf` |
| `replay.conf` | `/userdata/system/r4/replay.conf` |
| `services/R4Controller` | `/userdata/system/services/R4Controller` |
| `services/R4GameCard` | `/userdata/system/services/R4GameCard` |
| `scripts/R4GameState` | `/userdata/system/scripts/R4GameState` |
| `emulationstation/game-start/R4GameMetadata` | `/userdata/system/configs/emulationstation/scripts/game-start/R4GameMetadata` |
| `emulationstation/game-start/R4GameMetadata` (same hook) | `/userdata/system/configs/emulationstation/scripts/game-selected/R4GameMetadata` |
| `emulationstation/achievements/R4Achievement` | `/userdata/system/configs/emulationstation/scripts/achievements/R4Achievement` |

## Installation

Copy the complete integration directory to Batocera. In PowerShell, create a
temporary archive first; do not pipe native `tar` bytes through the PowerShell
object pipeline:

```powershell
tar -C ./integration/batocera -czf ./r4-batocera-0.10.0.tar.gz .
scp ./r4-batocera-0.10.0.tar.gz root@192.168.1.154:/tmp/
ssh root@192.168.1.154 "rm -rf /userdata/system/r4-installer && mkdir -p /userdata/system/r4-installer && tar -C /userdata/system/r4-installer -xzf /tmp/r4-batocera-0.10.0.tar.gz"
```

Run the installer:

```sh
chmod +x /userdata/system/r4-installer/install.sh

/userdata/system/r4-installer/install.sh
```

The installer is idempotent and can also be used to update an existing installation.

It:

- stops the currently installed services;
- creates the required directories;
- replaces the installed files;
- restores executable permissions;
- preserves existing `oled-tcp.conf`, `game-card.conf` and `replay.conf`;
- enables and starts the controller and Game Card services;
- checks the connected firmware version;
- prints the resulting controller and Game Card status.

## Manual executable permissions

All installed scripts must be executable:

```sh
chmod +x /userdata/system/r4/r4-ecctl
chmod +x /userdata/system/r4/r4-led-state
chmod +x /userdata/system/r4/r4-game-title
chmod +x /userdata/system/r4/r4-game-card
chmod +x /userdata/system/r4/r4-oled-tcp
chmod +x /userdata/system/r4/r4-replay
chmod +x /userdata/system/services/R4Controller
chmod +x /userdata/system/services/R4GameCard
chmod +x /userdata/system/scripts/R4GameState
chmod +x /userdata/system/configs/emulationstation/scripts/game-start/R4GameMetadata
chmod +x /userdata/system/configs/emulationstation/scripts/game-selected/R4GameMetadata
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
Controller: FW=&lt;configured-version&gt; LED=0,16,0 BASE=0,16,0 FLASH=0 LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000
```

The service:

- automatically discovers the RP2040 by USB VID, PID, serial number and interface;
- checks the reported firmware version;
- monitors controller availability;
- detects USB disconnection;
- handles USB reconnection without restarting Batocera;
- restores the current persistent LED mode after reconnection;
- sends the initial `HOST STATE MODE=HOME` display state;
- sends `HOST HEARTBEAT` every two seconds so RP2040 can detect a lost host;
- sends the current `HH:MM` clock and configured Batocera volume as
  `HOST TELEMETRY`;
- polls `EVENT NEXT` without mixing asynchronous events into command replies;
- serializes helper and optional TCP-relay requests through the service so it
  remains the only process that opens `/dev/ttyACM*`;
- displays an amber status when the firmware version is unexpected.

The two-second maintenance interval keeps the RP2040 seven-second host watchdog
refreshed. Broker requests are checked independently at a shorter interval.
This preserves reconnect behavior while allowing firmware to show
`HOST LINK LOST` if the Orange Pi service disappears.

## Optional OLED diagnostic TCP relay

The relay is disabled by default. After installation, enable it only on a
trusted test network:

```sh
cat >/userdata/system/r4/oled-tcp.conf <<'EOF'
R4_OLED_TCP_ENABLED=1
R4_OLED_TCP_BIND=0.0.0.0
R4_OLED_TCP_PORT=4274
EOF

batocera-services restart R4Controller
```

`R4_OLED_TCP_BIND` and `R4_OLED_TCP_PORT` are configurable. An existing
installed configuration is preserved when `install.sh` is rerun. The service
starts the listener only when `R4_OLED_TCP_ENABLED=1`.

On Windows:

```powershell
$batoceraIp = '192.168.1.154'
./r4-oled-emulator-gui.exe --tcp "${batoceraIp}:4274"
```

Wi-Fi terminates at the Orange Pi. RP2040 stays connected to Orange Pi through
USB CDC. The TCP child never accesses CDC directly: `R4Controller` brokers all
commands, including GUI host-state changes and input/event polls. For a
framebuffer request, the service opens CDC once and collects the existing
bounded RP2040 chunks before returning one `FRAMEBUFFER FULL` TCP payload.
Unchanged hashes return `FRAMEBUFFER UNCHANGED`. TCP framing remains one
`REQ <id> ...` line followed by one matching `RES <id> OK|ERROR ...` line.

## Capture and previous-game replay

`R4Controller` consumes every queued Capture event exactly once. SHORT invokes
Batocera's real `batocera-screenshot` command exactly once. LONG does not make a
screenshot and does not start a future recording: it asks `r4-replay` to freeze
and finalize the preceding buffered gameplay.

While a supported game runs, `r4-replay` launches the FFmpeg available in
Batocera 40:

```text
DRM/KMS framebuffer --kmsgrab--> libx264 video --+
Pulse default monitor ---------> AAC audio ------+--> rotating MPEG-TS segments
                                                         |
Capture LONG --> snapshot complete segments --> concat stream copy --> MP4
```

The ring defaults to two-second segments in `/tmp/r4-replay`. On the checked
Orange Pi 3 LTS, `/tmp` is a roughly 963 MiB tmpfs. Both duration and total
buffer size are bounded; old segments are overwritten/trimmed, stale data is
removed at startup, and only the finished MP4 is written to persistent storage.
Finalization uses stream copy rather than a second full encode, validates video
and audio streams with `ffprobe` when available, then atomically renames the
file. A short game may produce a clip shorter than 30 seconds.

The actual Batocera 40 audit found FFmpeg 4.4.4 with `kmsgrab`, Pulse input,
segment muxing, `libx264`, AAC and MP4 support. KMS capture, Pulse-monitor
audio, segmented encoding and a playable H.264/AAC MP4 finalization were
smoke-tested on the Orange Pi. Its exposed V4L2 devices did not provide a
working H.264 encoder to FFmpeg, so the current backend deliberately uses CPU
`libx264`. RetroArch includes recording support but Batocera's launch path does
not expose a persistent previous-game ring. No fictitious production backend
is used.

`/userdata/system/r4/replay.conf` controls the limits and routing:

```sh
R4_REPLAY_ENABLED=1
R4_REPLAY_SECONDS=30
R4_REPLAY_SEGMENT_SECONDS=2
R4_REPLAY_BUFFER_DIR=/tmp/r4-replay
R4_REPLAY_MAX_BUFFER_MB=128
R4_REPLAY_FPS=30
R4_REPLAY_MAX_WIDTH=1280
R4_REPLAY_VIDEO_BITRATE=4000000
R4_REPLAY_VIDEO_PRESET=ultrafast
R4_REPLAY_AUDIO_ENABLED=1
R4_REPLAY_DISABLED_SYSTEMS=dreamcast
R4_CAPTURE_STORAGE=auto
R4_CAPTURE_MIN_FREE_MB=256
R4_CAPTURE_FALLBACK_INTERNAL=1
R4_CAPTURE_INTERNAL_SCREENSHOTS=/userdata/screenshots
R4_CAPTURE_INTERNAL_VIDEOS=/userdata/recordings
```

`R4_CAPTURE_MIN_FREE_MB` is only a refusal threshold, not a quota or reserved
space. `auto` prefers a writable Game Card capture view and otherwise uses the
internal paths when fallback is enabled. Dreamcast is disabled until real
performance testing. A missing backend, disabled system or inactive ring
returns `CLIP UNAVAILABLE`; concurrent saves return `BUSY` without starting a
second finalizer.

Firmware feedback distinguishes screenshots and clips:

```text
HOST CAPTURE TYPE=SCREENSHOT STATUS=BUSY|SAVED|ERROR
HOST CAPTURE TYPE=CLIP STATUS=BUFFERING|SAVING|SAVED|ERROR|UNAVAILABLE
```

The OLED shows `RPL` while the ring is healthy, followed by bounded
`CLIP SAVING`, `CLIP SAVED`, `CLIP ERROR` or `CLIP UNAVAILABLE` notifications.
RGB uses amber while saving, green on success and red on failure, then restores
the persistent game/menu color.

Until Capture is electrically connected through the future GPIO expander, use
the service test entry point:

```sh
R4_ENABLE_TEST_EVENTS=1 /userdata/system/services/R4Controller test-event CAPTURE SHORT
R4_ENABLE_TEST_EVENTS=1 /userdata/system/services/R4Controller test-event CAPTURE LONG
/userdata/system/r4/r4-replay status
tail -n 50 /userdata/system/r4/r4-replay.log
ls -lt /userdata/screenshots /userdata/recordings
```

This verifies the host path but does not claim that the physical Capture input
or gameplay performance is hardware-verified.

## R4 Game Card

The Game Card is one removable physical partition with one ordinary filesystem.
It is not Batocera's boot microSD and never stores BIOS, saves, settings or
RetroAchievements data. No loop images, quotas, reserved capture space,
automatic formatting or repartitioning are used.

```text
R4CARD/                          one shared filesystem/free-space pool
├── ROMS/        --RO bind-->   /userdata/roms/r4-card
└── CAPTURES/
    ├── Screenshots/ --RW-->    /userdata/screenshots/r4-card
    └── Videos/      --RW-->    /userdata/recordings/r4-card

whole card RW private mount --> /userdata/system/r4/game-card
```

Batocera 40 was checked with a temporary filesystem: its util-linux `mount`
supports bind mounts and `remount,bind,ro`; a write through the ROM view was
rejected while a writable sibling view remained writable. The physical reader
and final SD card still require verification.

The preserved configuration is `/userdata/system/r4/game-card.conf`:

```sh
R4_GAME_CARD_ENABLED=1
R4_GAME_CARD_LABEL=R4CARD
R4_GAME_CARD_UUID=
R4_GAME_CARD_PRIVATE_MOUNTPOINT=/userdata/system/r4/game-card
R4_GAME_CARD_ROMS_MOUNTPOINT=/userdata/roms/r4-card
R4_GAME_CARD_SCREENSHOTS_MOUNTPOINT=/userdata/screenshots/r4-card
R4_GAME_CARD_VIDEOS_MOUNTPOINT=/userdata/recordings/r4-card
R4_GAME_CARD_POLL_INTERVAL=1
```

UUID has priority; otherwise the exact label is required. Only removable
partitions are considered and ambiguous, wrong-label and system devices are
refused. The idempotent initialization command creates only the missing
directories after selecting the exact configured card:

```sh
/userdata/system/r4/r4-game-card init
```

The public states remain `INSERTED`, `READY`, `BUSY`, `EJECTED` and `ERROR`.
BUSY is represented by independent owners, currently `rom`,
`capture-screenshot` and `capture-clip`; overlapping owners cannot clear each
other. Safe eject refuses every active owner, calls `sync`, unmounts the three
published views before the private mount, then latches `EJECTED` until physical
removal/reinsertion. Unsafe physical removal becomes `ERROR`.

```sh
/userdata/system/r4/r4-game-card status
/userdata/system/r4/r4-game-card busy-list
/userdata/system/r4/r4-game-card scan
/userdata/system/r4/r4-game-card eject
```

The legacy `R4_GAME_CARD_MOUNTPOINT=/userdata/roms/r4-card` remains accepted as
the ROM view only; it is never reinterpreted as the private writable root.
Installation preserves the user's existing config and never deletes or moves
ROMs or capture files.

State and capture feedback use the existing `r4-ecctl` broker, so
`R4Controller` remains the sole direct owner of `/dev/ttyACM*`. Manage polling
with:

```sh
batocera-services restart R4GameCard
/userdata/system/services/R4GameCard status
```

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
The EmulationStation `game-selected` event provides the real metadata title and
caches it with the selected ROM path. Its early `game-start` event immediately
sends the cached system/title to RP2040, avoiding the several-second delay
before Batocera's lifecycle hook. The later `gameStart` sends the same state
again so the session timer is aligned with actual emulator launch. If no
matching selection is cached, `<name>` is resolved from the system's
`gamelist.xml`; a cleaned ROM label is the final fallback.

When a game stops:

```text
gameStop
```

the persistent LED mode becomes green.
The hook also sends `HOST GAME ACTION=STOP`.

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
- sends RetroAchievements state and achievement title to the RP2040.

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
R4_CONTROLLER_FW &lt;configured-version&gt;
```

Read the current controller input:

```sh
/userdata/system/r4/r4-ecctl INPUT
```

Example:

```text
LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000 LT=0 RT=0 LT_STATUS=NO_SOURCE RT_STATUS=NO_SOURCE
```

Read complete controller state:

```sh
/userdata/system/r4/r4-ecctl STATUS
```

Example:

```text
FW=&lt;configured-version&gt; LED=0,16,0 BASE=0,16,0 FLASH=0 LX=0 LY=0 RX=0 RY=0 HAT=0 BUTTONS=0x00000000 LT=0 RT=0 LT_STATUS=NO_SOURCE RT_STATUS=NO_SOURCE
```

Poll the next queued service-button event:

```sh
/userdata/system/r4/r4-ecctl EVENT NEXT
```

It returns `EVENT NONE` or a single
`EVENT BUTTON=CAPTURE ACTION=SHORT TIME_MS=1450 SEQ=3` line. The service
recognizes Capture, R4 and Trophy events. Capture SHORT and LONG are implemented
as screenshot and previous-game replay respectively. The achievement browser,
future R4 system panel, and standalone long/double R4 host actions remain
explicit stubs; the service logs them without claiming those features.

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

The configured firmware version currently exposes:

- D-pad;
- A, B, X and Y buttons;
- L1 and R1 shoulder buttons;
- left analog stick;
- right analog stick;
- left stick button;
- right stick button;
- Start;
- Select;
- R4 system button.

Current HID mapping:

| Physical input | HID input |
|---|---|
| D-pad | `Hat0X` and `Hat0Y` |
| Left stick X | `X` |
| Left stick Y | `Y` |
| Right stick X | `Rx` |
| Right stick Y | `Ry` |
| LT | `Z` axis |
| RT | `Rz` axis |
| A | `BtnA` |
| B | `BtnB` |
| X | `BtnX` |
| Y | `BtnY` |
| L1 | `BtnTL` |
| R1 | `BtnTR` |
| Select | `BtnSelect` |
| Start | `BtnStart` |
| R4 system button | `BtnMode` / Hotkey Enable |
| Left stick click (L3) | `BtnThumbL` |
| Right stick click (R3) | `BtnThumbR` |

The `0.8.0` descriptor uses the former Z and Rz slots for unsigned analog
LT and RT. The production firmware reports them released until a verified
external ADC backend is added.

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
| R4 system button | `0x00001000` |
| Left stick click (L3) | `0x00002000` |
| Right stick click (R3) | `0x00004000` |

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
Axis 2: LT (Z)
Axis 3: right stick X
Axis 4: right stick Y
Axis 5: RT (Rz)
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
Button 12: R4 (BtnMode / Hotkey Enable)
Button 13: left stick click (L3 / BtnThumbL)
Button 14: right stick click (R3 / BtnThumbR)
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

The physical R4 system button is exposed as `BtnMode` and mapped to:

```text
Hotkey Enable
```

The standard emulator exit combination is:

```text
R4 + Start
```

After upgrading from `0.7.0`, Batocera may request a new controller mapping
because the HID descriptor changed. Map LT and RT as analog axes, then verify
ABXY, D-pad, both sticks, L1/R1, stick switches, Select, Start, R4 and
`R4 + Start`. The physical LT/RT ADC is not implemented yet, so a
test-enabled firmware image is needed to move those axes.

## Safe service-event testing

Test-event injection is disabled unless explicitly opted in:

```sh
R4_ENABLE_TEST_EVENTS=1 \
  /userdata/system/services/R4Controller test-event CAPTURE SHORT
```

Valid buttons are `CAPTURE`, `R4`, `TROPHY`; valid actions are `PRESS`,
`RELEASE`, `SHORT`, `LONG`, `DOUBLE`. The command exercises service dispatch
only and does not inject HID input. Without `R4_ENABLE_TEST_EVENTS=1` it fails.

R4 is not separate from the Hotkey input: GP10 supplies the existing `BtnMode`
bit and the R4 gesture recognizer. A short standalone R4 is reserved for the
future system panel. If Start, another digital button or a D-pad direction is
pressed while R4 is held, firmware cancels the pending standalone action while
leaving both HID inputs visible. This preserves `R4 + Start` and other
RetroArch system combinations. Long and double presses remain distinct and use
the firmware's configurable timing thresholds.

Run repository-side shell and mock checks on a POSIX host:

```sh
sh integration/batocera/tests/run-tests.sh
```

## Batocera 40 verification checklist

This must be completed on the Orange Pi 3 LTS; it is not replaced by host mocks.

| Item | Current evidence |
|---|---|
| RP2040 firmware `0.9.0-dev`, HID/CDC and watchdog | Hardware-verified before this change |
| Capture SHORT to a real `/userdata/screenshots` file | Hardware-verified before this change |
| Batocera 40 FFmpeg/KMS/Pulse/segment capabilities | Checked on the Orange Pi |
| H.264/AAC segmented smoke clip and `ffprobe` validation | Checked on the Orange Pi outside gameplay |
| RO bind view beside RW views | Checked on Batocera with a temporary filesystem |
| Firmware `0.10.0` UF2 | Built, not flashed |
| 30-second replay FPS/CPU/quality in PS1 | Not yet hardware-verified |
| Dreamcast replay | Disabled and not hardware-verified |
| Physical Game Card reader/card and safe eject | Not yet hardware-verified |
| Physical Capture input through MCP23017 | Not yet hardware-verified |
| Physical OLED | Not yet hardware-verified |

1. Install the integration and confirm `R4Controller status` becomes `online`.
2. Check `PING`, `VERSION`, `INPUT`, `STATUS` and the configured
   `0.10.0` version.
3. Unplug/replug and reset the RP2040; confirm rediscovery, one service instance,
   restored LED state and no repetitive log flood.
4. Remap the changed descriptor; verify all old controls and
   `R4 + Start`, plus the other configured `R4 + button` combinations.
5. With a test-enabled firmware, inject LT/RT at 0, midpoint and 65535; confirm
   independent Z/Rz axes in `jstest`, EmulationStation and RetroArch.
6. Start/stop a game and confirm LED state plus `HOST GAME` acknowledgements.
7. Trigger a real RetroAchievement and confirm the existing gold flash/log plus
   the OLED-state message.
8. Inject `CAPTURE SHORT`; confirm exactly one new screenshot, typed `SAVED`
   feedback and base-color restoration.
9. Run a supported game for more than 30 seconds, inject `CAPTURE LONG`, and
   confirm exactly one preceding-game MP4 with H.264 video and AAC audio.
   Measure gameplay FPS, CPU load, FFmpeg warnings/dropped frames and playback
   quality. Repeat on PS1. Dreamcast must remain `UNAVAILABLE` until a separate
   performance run justifies enabling it.
10. With a correctly labeled/UUID Game Card, run `init` twice and verify one
   primary mount, a read-only ROM view, writable screenshot/video views and
   shared free space. Verify each BUSY owner blocks eject, overlapping owners
   coexist, `sync` precedes view/private unmounts, safe eject latches `EJECTED`,
   and reinsertion returns to `READY`.
11. Inspect malformed/unknown CDC commands and a line over 255 bytes; confirm a
   bounded `ERR` response and continued reconnect/service operation.

For the PS1 and later Dreamcast performance run, enable RetroArch's on-screen
FPS display, establish a no-recording baseline in the same scene, then start a
fresh session with replay enabled. From SSH, collect the encoder load and log:

```sh
while pidof ffmpeg >/dev/null 2>&1; do
    date
    top -b -n 1 | grep -E 'ffmpeg|retroarch|pcsx|flycast'
    sleep 2
done > /userdata/system/r4/r4-replay-performance.log

grep -Ei 'drop|duplicate|queue|non-monoton|error|failed' \
    /userdata/system/r4/r4-replay.log

clip="$(find /userdata/recordings /userdata/recordings/r4-card \
    -type f -name '*.mp4' 2>/dev/null | sort | tail -n 1)"
ffprobe -v error \
    -show_entries format=duration \
    -show_entries stream=index,codec_name,codec_type,avg_frame_rate \
    -of default=noprint_wrappers=1 "$clip"
```

Compare baseline and replay FPS for at least five minutes, trigger LONG twice
at separate moments, play both clips on a normal PC, and listen for continuous
game audio. Keep Dreamcast in `R4_REPLAY_DISABLED_SYSTEMS` unless this test
passes without turning gameplay into a slideshow.

## Supported firmware

The expected version is defined in `firmware-version.conf` and installed to
`/userdata/system/r4/firmware-version.conf`.

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
/tmp/r4-game-card/
/tmp/r4-replay-state/
/tmp/r4-replay/
```

Persistent logs are stored in:

```text
/userdata/system/r4/r4-controller.log
/userdata/system/r4/r4-game-events.log
/userdata/system/r4/r4-achievements.log
/userdata/system/r4/r4-game-card.log
/userdata/system/r4/r4-replay.log
```

Runtime files and logs are not part of the repository.

## Remaining controller work

The current prototype does not yet include:

- a physical ADC source or calibrated hardware for analog LT and RT;
- Home;
- a physical Capture input through the planned GPIO expander;
- Trophy;
- vibration;
- a selected OLED model, resolution or physical display driver;
- battery and power telemetry.

Replay support is implemented in software, but its gameplay FPS/CPU impact,
PS1 quality, Dreamcast viability and long-duration stability are not yet
hardware-verified. The physical Game Card reader/card and its real-media bind
mount/eject behavior are also still unverified.

All four external RP2040 ADC channels are already occupied by the two analog sticks.

Additional analog inputs will require an external ADC or another analog input solution.

Additional digital controls should use a GPIO expander, button matrix or another bus-based expansion solution.

The remaining hardware work is concrete: select and electrically validate an
external ADC, record independent LT/RT calibration values, select and verify a
GPIO expander for Home/Capture/Trophy, assign/debounce those real button inputs,
validate the full-size SD reader and card-removal behavior, select
the OLED model and bus/address, implement its framebuffer backend, and validate
battery/power/temperature sensors on the final power design.
