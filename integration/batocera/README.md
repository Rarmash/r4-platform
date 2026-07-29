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
- an opt-in experimental previous-game replay implementation for Capture LONG,
  disabled by default on Orange Pi 3 LTS;
- one-filesystem R4 Game Card management with read-only ROM and writable
  capture views;
- game, RetroAchievements and display-state forwarding to RP2040;
- complete EmulationStation controller mapping;
- standard `R4 + Start` emulator exit handling.

## 0.12.0 release notes

`0.12.0` adds a guarded **host-assisted USB firmware update** for compatible
RP2040 firmware. It is not OTA: Orange Pi asks the running firmware to enter
the RP2040 ROM USB bootloader, mounts the uniquely identified `RPI-RP2`
filesystem, copies a validated UF2, syncs and unmounts it, then verifies the
returned CDC/HID device and exact firmware version.

The updater requires an R4 release manifest, RP2040 UF2 family ID and matching
SHA-256. SHA-256 is an integrity check, not authentication. It refuses to run
during a game, with zero or multiple R4 Controllers, or with zero/multiple
matching bootloaders. A candidate must match USB VID `2e8a`, PID `0003` and
filesystem label `RPI-RP2`; it never selects the first `/dev/sdX` and never
uses `dd`.

Firmware `0.11.1` cannot enter the ROM bootloader by CDC. The first installation
of firmware `0.12.0` therefore requires physical BOOTSEL one final time. Once
`0.12.0` is running, the same release can be reflashed to verify the complete
host-assisted path.

The same release disables Instant Replay after both a fresh installation and
the first
upgrade from an older integration. Capture SHORT remains a supported
`batocera-screenshot` action. Capture LONG is intentionally ignored unless a
developer explicitly sets `R4_REPLAY_ENABLED=1`; status and logs identify this
path as experimental and warn about its high CPU load. Reinstalling `0.12.0`
preserves a deliberate opt-in, while the one-time upgrade migration prevents a
previous default-on configuration from silently continuing to record.

Orange Pi 3 LTS has no hardware H.264 encoder. The available `cedrus` and
`allwinner,sun50i-h6-vpu-g2-dec` devices are decoders, and
`h264_v4l2m2m` found no encode device. Software capture is unsuitable as a user
feature: 720p produced about 5 FPS, while even 640x360 at a requested 10 FPS
produced only about 7-8 unique FPS and noticeably loaded all four Cortex-A53
cores. Instant Replay is therefore unsupported on the current hardware
revision and deferred until a board with hardware video encoding is used.
Screenshots remain fully supported.

The preserved experimental FFmpeg path includes `-vsync 0` to avoid duplicate
frames causing accelerated video or A/V drift. It scales only when the source
is wider than the configured maximum and preserves aspect ratio; it neither
forces a 1280x720 mode nor changes Batocera videomode settings.

## 0.11.1 release notes

`0.11.1` fixes the Batocera 40 startup regression introduced by synchronous
`kmsgrab` startup in `0.11.0`. `R4GameState` still performs Game Card, LED and
`HOST GAME` work during `gameStart`, but `r4-replay start` now returns after
creating a background replay session. A bounded waiter records the current
DRM-card holders, waits for a new stable PID/start-time identity on the same
card, verifies an enabled/connected connector plus a non-zero active fbdev
framebuffer, and only then launches FFmpeg. This avoids competing with
RetroArch or a standalone emulator while it acquires DRM/KMS.

`gameStop` invalidates the session before stopping both waiter and encoder.
Repeated starts are idempotent, stale waiters cannot launch into a later game,
and readiness timeout or encoder failure leaves replay unavailable without
failing the game hook. Existing `0.11.0` replay configuration remains valid:
the new readiness settings have backward-compatible defaults.

## Components

- `bin/r4-ecctl` — discovers the RP2040 CDC interface and sends service commands.
- `bin/r4-firmware-update` — validates and performs the guarded host-assisted
  USB firmware update.
- `bin/r4-led-state` — stores and applies the current persistent LED mode.
- `bin/r4-game-title` — resolves a ROM path to its `<name>` in
  EmulationStation `gamelist.xml`.
- `bin/r4-oled-tcp` — optional framed diagnostic TCP relay for the OLED GUI.
- `bin/r4-game-card` — validates, mounts and safely ejects the configured ROM
  card without opening USB CDC directly.
- `bin/r4-replay` — owns the bounded FFmpeg replay ring and capture routing.
- `integration-version.conf` — defines the Batocera integration release
  version (`0.12.0`).
- `firmware-version.conf` — defines the expected RP2040 firmware version.
- `oled-tcp.conf` — opt-in TCP bind address and port.
- `game-card.conf` — preserved Game Card identity, mountpoint and polling
  configuration.
- `replay.conf` — preserved replay, encoding and capture-storage settings.
- `CHANGELOG.md` — Batocera integration release history.
- `services/R4Controller` — monitors the embedded controller and handles reconnection.
- `services/R4GameCard` — polls the removable-card state machine.
- `scripts/R4GameState` — changes the LED state when a game starts or stops.
- `emulationstation/game-start/R4GameMetadata` — shared `game-selected` /
  `game-start` hook that caches and forwards the EmulationStation metadata
  title instead of the ROM filename.
- `emulationstation/achievements/R4Achievement` — triggers a temporary achievement flash.
- `install.sh` — installs or updates the complete Batocera integration.
- `uninstall.sh` — stops replay and services, removes installed executables and
  every `R4GameState*` hook, and preserves configuration, logs and captures.
- `tests/run-tests.sh` — shell syntax and mock service-event checks.

## Repository layout

```text
integration/batocera/
├── integration-version.conf
├── firmware-version.conf
├── game-card.conf
├── oled-tcp.conf
├── replay.conf
├── CHANGELOG.md
├── bin/
│   ├── r4-ecctl
│   ├── r4-firmware-update
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
├── uninstall.sh
├── tests/
│   └── run-tests.sh
└── README.md
```

## Installation paths

| Repository file | Batocera path |
|---|---|
| `bin/r4-ecctl` | `/userdata/system/r4/r4-ecctl` |
| `bin/r4-firmware-update` | `/userdata/system/r4/r4-firmware-update` |
| `bin/r4-led-state` | `/userdata/system/r4/r4-led-state` |
| `bin/r4-game-title` | `/userdata/system/r4/r4-game-title` |
| `bin/r4-game-card` | `/userdata/system/r4/r4-game-card` |
| `bin/r4-oled-tcp` | `/userdata/system/r4/r4-oled-tcp` |
| `bin/r4-replay` | `/userdata/system/r4/r4-replay` |
| `integration-version.conf` | `/userdata/system/r4/integration-version.conf` |
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
tar -C ./integration/batocera -czf ./r4-batocera-0.12.0.tar.gz .
scp ./r4-batocera-0.12.0.tar.gz root@192.168.1.154:/tmp/
ssh root@192.168.1.154 "rm -rf /userdata/system/r4-installer && mkdir -p /userdata/system/r4-installer && tar -C /userdata/system/r4-installer -xzf /tmp/r4-batocera-0.12.0.tar.gz"
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
- preserves existing `oled-tcp.conf`, `game-card.conf` and `replay.conf`, except
  that the first `0.12.0` upgrade forces the old replay opt-in to `0`;
- stops an installed replay waiter and encoder before replacing `r4-replay`;
- removes `R4GameState.disabled` from the active scripts directory, removes the
  executable bit from other `R4GameState.*` backups, and never restores the
  parked `/userdata/system/r4/R4GameState.disabled` as a hook;
- enables and starts the controller and Game Card services;
- checks the connected firmware version;
- prints the resulting controller and Game Card status.

Updating an existing `0.12.0` integration does not replace
`/userdata/system/r4/game-card.conf`. To verify the real card UUID before and
after the update:

```sh
cp /userdata/system/r4/game-card.conf \
    /userdata/system/r4/game-card.conf.before-0.12.0
/userdata/system/r4-installer/install.sh
grep '^R4_GAME_CARD_UUID=2902-F590$' \
    /userdata/system/r4/game-card.conf

for hook in /userdata/system/scripts/R4GameState*; do
    [ -x "$hook" ] && printf '%s\n' "$hook"
done
```

The final command must print only
`/userdata/system/scripts/R4GameState`. After the first `0.12.0` update,
`R4_REPLAY_ENABLED=0` must be present in the preserved `replay.conf`.

## Host-assisted USB firmware update

Build the firmware release target and copy all three release files to Orange
Pi, for example under `/userdata/system/r4/firmware`:

```powershell
scp ./firmware/r4-controller-fw/build/release/r4-controller-fw-0.12.0.uf2 `
  root@192.168.1.154:/userdata/system/r4/firmware/
scp ./firmware/r4-controller-fw/build/release/r4-controller-fw-0.12.0.uf2.manifest `
  root@192.168.1.154:/userdata/system/r4/firmware/
scp ./firmware/r4-controller-fw/build/release/r4-controller-fw-0.12.0.uf2.sha256 `
  root@192.168.1.154:/userdata/system/r4/firmware/
```

Interactive update:

```sh
/userdata/system/r4/r4-ecctl firmware update \
    /userdata/system/r4/firmware/r4-controller-fw-0.12.0.uf2
```

Documented non-interactive form:

```sh
/userdata/system/r4/r4-ecctl firmware update --yes \
    /userdata/system/r4/firmware/r4-controller-fw-0.12.0.uf2
```

Before arming the controller, the updater validates every UF2 block, RP2040
family ID `0xE48BFF56`, the R4 product/version manifest and SHA-256; verifies
that no game process or game marker is active; requires exactly one R4
Controller; and displays current and target versions. The interactive form
requires typing `UPDATE`.

After `UPDATE ARM` / `UPDATE CONFIRM`, it accepts only a unique device matching
all of USB VID `2e8a`, PID `0003` and filesystem label `RPI-RP2`. It mounts that
partition in a private temporary directory, copies the UF2 normally, calls
`sync`, unmounts it, waits for the bootloader to disappear and for the composite
CDC/HID device to return, then checks `VERSION`. Each stage has a timeout and
cleanup restarts `R4Controller`.

Do not use `dd`. Do not choose a disk by `/dev/sdX` ordering. The updater does
not provide rollback or guarantee preservation of the previous firmware.
After a failed copy or a version/return timeout, recover by holding physical
BOOTSEL while connecting RP2040 and manually copy the already verified UF2 to
`RPI-RP2`. The ROM bootloader is independent of application firmware and
remains available if the application image is damaged.

Bootstrap limitation: the currently installed `0.11.1` firmware has no
`UPDATE` commands. Install `0.12.0` through physical BOOTSEL once. After it
reports `R4_CONTROLLER_FW 0.12.0`, repeat the command above to test a complete
host-assisted update without pressing BOOTSEL.

## Uninstallation

Run the uninstaller from the extracted integration package:

```sh
chmod +x /userdata/system/r4-installer/uninstall.sh
/userdata/system/r4-installer/uninstall.sh
```

It stops and disables both services, stops a waiting or running replay session,
removes the installed programs and all `R4GameState*` hooks, and clears replay
runtime state. It deliberately preserves `replay.conf`, the other user
configuration, logs, screenshots and recordings under `/userdata`.

## Manual executable permissions

All installed scripts must be executable:

```sh
chmod +x /userdata/system/r4/r4-ecctl
chmod +x /userdata/system/r4/r4-firmware-update
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
Batocera's real `batocera-screenshot` command exactly once. This remains a
supported user feature regardless of the replay setting.

Replay is completely off by default. `r4-replay start` records the disabled
status and returns without creating its ring directory, starting its readiness
waiter, or launching FFmpeg. Capture LONG is logged as ignored and sends no clip
feedback to the firmware. Nothing changes the system videomode.

The old implementation remains available only for development experiments.
To opt in, edit `/userdata/system/r4/replay.conf`:

```sh
sed -i 's/^R4_REPLAY_ENABLED=.*/R4_REPLAY_ENABLED=1/' \
    /userdata/system/r4/replay.conf
```

This setting is deliberately not exposed as a normal user option. It carries a
high-load warning and is unsupported on Orange Pi 3 LTS. Start a new game after
changing it. While enabled, LONG does not start a future recording: it asks
`r4-replay` to freeze and finalize the preceding buffered gameplay.

For an enabled experimental session, `r4-replay start` creates a unique
background session and returns to `R4GameState`. The waiter detects a new,
stable DRM-card holder and an active framebuffer before launching FFmpeg:

```text
DRM/KMS framebuffer --kmsgrab--> libx264 video --+
Pulse default monitor ---------> AAC audio ------+--> rotating MPEG-TS segments
                                                         |
Capture LONG --> snapshot complete segments --> concat stream copy --> MP4
```

The ring uses bounded rotating segments in `/tmp/r4-replay`; that directory is
created only after explicit opt-in. The experimental encoder uses CPU
`libx264`, preserves the source aspect ratio, and includes `-vsync 0`.
Finalization uses stream copy and validates the output where `ffprobe` exists.

`/userdata/system/r4/replay.conf` controls the limits and routing:

```sh
R4_REPLAY_ENABLED=0
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
R4_REPLAY_DRM_DEVICE=/dev/dri/card0
R4_REPLAY_READY_TIMEOUT_SECONDS=20
R4_REPLAY_READY_POLL_MS=250
R4_REPLAY_READY_STABLE_POLLS=4
R4_CAPTURE_STORAGE=auto
R4_CAPTURE_MIN_FREE_MB=256
R4_CAPTURE_FALLBACK_INTERNAL=1
R4_CAPTURE_INTERNAL_SCREENSHOTS=/userdata/screenshots
R4_CAPTURE_INTERNAL_VIDEOS=/userdata/recordings
```

`R4_CAPTURE_MIN_FREE_MB` is only a refusal threshold, not a quota or reserved
space. `auto` prefers a writable Game Card capture view and otherwise uses the
internal paths when fallback is enabled. With replay disabled, status is
explicit:

```text
STATE=DISABLED EXPERIMENTAL=1 ENABLED=0 BACKEND=ffmpeg-kms-pulse-experimental RUNNING=0 ...
```

With the dev flag enabled, status reports `WAITING` and then `BUFFERING`, always
with `EXPERIMENTAL=1 ENABLED=1`. Logs include
`warning=high-cpu-load-unsupported-hardware`. Firmware feedback distinguishes
supported screenshots from experimental clips:

```text
HOST CAPTURE TYPE=SCREENSHOT STATUS=BUSY|SAVED|ERROR
HOST CAPTURE TYPE=CLIP STATUS=BUFFERING|SAVING|SAVED|ERROR|UNAVAILABLE
```

The OLED replay indicator and clip notifications are reachable only after the
dev opt-in. In the normal configuration, LONG produces no replay notification.

Until Capture is electrically connected through the future GPIO expander, use
the service test entry point:

```sh
R4_ENABLE_TEST_EVENTS=1 /userdata/system/services/R4Controller test-event CAPTURE SHORT
R4_ENABLE_TEST_EVENTS=1 /userdata/system/services/R4Controller test-event CAPTURE LONG
/userdata/system/r4/r4-replay status
tail -n 50 /userdata/system/r4/r4-replay.log
ls -lt /userdata/screenshots /userdata/recordings
```

During a normal launch, the log records that experimental replay is disabled.
It must not contain a new `ffmpeg launched` line.

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
rejected while a writable sibling view remained writable. A physical exFAT
card with UUID `2902-F590` was subsequently verified with the same
private/RO/RW mounts, BUSY protection and safe eject.

Batocera's exFAT userspace mount requires `/dev/fuse`. At service startup,
`R4GameCard` asks the manager to prepare the current card. For exFAT only, the
manager checks that `/dev/fuse` is a readable and writable character device,
runs `modprobe fuse` when necessary, and verifies the device again. Every scan
keeps the same defensive check. A failed command becomes
`ERROR / fuse-modprobe-failed`; a successful command without a usable device
becomes `ERROR / fuse-device-unavailable`. The failed physical identity is
remembered, so polling does not repeatedly run `modprobe`; a newly available
`/dev/fuse` or a new card session can recover.

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
published views before the private mount, then latches `EJECTED`.

Stable scans are edge-triggered. If the same physical session is already
`READY` or `BUSY` and all views are intact, a scan does not call mount, rewrite
the state file, or publish another OLED/RGB notification. The manager records
the kernel `DISKSEQ` from the partition's sysfs `uevent`. After safe eject, the
same `DISKSEQ` stays latched even though the partition remains visible.
Observed absence clears the old session. A different `DISKSEQ` is accepted as
a new session even if polling missed the brief absence and the device changed
from `/dev/sda1` to `/dev/sdb1`. UUID/label still select the card; `/dev/sdX`
is never configured.

On kernels without `DISKSEQ`, the manager falls back to sysfs major:minor and
finally the device path. With those fallbacks, a very fast reconnect that
reuses the same identity while polling misses absence cannot be guaranteed;
the safe behavior is to retain the eject latch until absence is observed.
Unsafe physical removal without safe eject becomes `ERROR`.

```sh
/userdata/system/r4/r4-game-card status
/userdata/system/r4/r4-game-card prepare
/userdata/system/r4/r4-game-card busy-list
/userdata/system/r4/r4-game-card scan
/userdata/system/r4/r4-game-card eject
```

Useful Batocera diagnostics:

```sh
ls -l /dev/fuse
grep -w fuse /proc/modules
/userdata/system/r4/r4-game-card status
device="$(
    /userdata/system/r4/r4-game-card status |
        sed -n 's/.* DEVICE=\([^ ]*\).*/\1/p'
)"
[ "$device" = NONE ] ||
    grep -E '^(DEVNAME|MAJOR|MINOR|DISKSEQ)=' \
        "/sys/class/block/${device##*/}/uevent"
tail -n 50 /userdata/system/r4/r4-game-card.log
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
recognizes Capture, R4 and Trophy events. Capture SHORT is a supported
screenshot action. Capture LONG is ignored by default and reaches the retained
previous-game replay implementation only with the explicit experimental flag.
The achievement browser, future R4 system panel, and standalone long/double R4
host actions remain explicit stubs; the service logs them without claiming
those features.

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

## 0.12.0 replay verification on Batocera 40

After a normal install, verify the supported default:

```sh
cat /userdata/system/r4/integration-version.conf
grep '^R4_REPLAY_ENABLED=0$' /userdata/system/r4/replay.conf
/userdata/system/r4/r4-replay status
ps | grep -E '[r]4-replay (waiter|monitor)|[f]fmpeg'
test ! -d /tmp/r4-replay
```

The version is `0.12.0`, status contains `STATE=DISABLED EXPERIMENTAL=1
ENABLED=0`, and both process and ring-directory checks are empty. Launch a game
and repeat the checks; they must remain empty.

Verify both Capture actions:

```sh
R4_ENABLE_TEST_EVENTS=1 \
    /userdata/system/services/R4Controller test-event CAPTURE SHORT

R4_ENABLE_TEST_EVENTS=1 \
    /userdata/system/services/R4Controller test-event CAPTURE LONG

tail -n 30 /userdata/system/r4/r4-controller.log
ls -lt /userdata/screenshots
```

SHORT creates exactly one screenshot. LONG logs
`result=IGNORED detail=experimental-replay-disabled` and creates no process,
ring, or recording.

Only to audit the retained developer path, set `R4_REPLAY_ENABLED=1`, launch a
game, and inspect:

```sh
/userdata/system/r4/r4-replay status
tail -n 80 /userdata/system/r4/r4-replay.log
```

Status must include `EXPERIMENTAL=1 ENABLED=1`; the log must contain the
high-load warning. Restore `R4_REPLAY_ENABLED=0` immediately after the audit
and exit the game.

## Batocera 40 verification checklist

This must be completed on the Orange Pi 3 LTS; it is not replaced by host mocks.

| Item | Current evidence |
|---|---|
| RP2040 firmware `0.9.0-dev`, HID/CDC and watchdog | Hardware-verified before this change |
| Capture SHORT to a real `/userdata/screenshots` file | Hardware-verified before this change |
| Batocera 40 FFmpeg/KMS/Pulse/segment capabilities | Checked on the Orange Pi |
| H.264/AAC segmented smoke clip and `ffprobe` validation | Checked on the Orange Pi outside gameplay |
| RO bind view beside RW views | Checked on Batocera with a temporary filesystem |
| Firmware `0.12.0` UF2 | Built; first BOOTSEL installation required from `0.11.1` |
| Instant Replay on Orange Pi 3 LTS | Unsupported; default disabled in integration `0.12.0` |
| Software capture performance | 720p about 5 FPS; 640x360@10 about 7-8 unique FPS with high four-core load |
| Physical exFAT Game Card, views, BUSY and safe eject | Hardware-verified on Batocera 40 |
| Automatic FUSE recovery after a cold boot | Implemented; reboot verification pending |
| DISKSEQ rapid-reconnect recovery | Implemented and mocked; physical rapid-reconnect verification pending |
| Physical Capture input through MCP23017 | Not yet hardware-verified |
| Physical OLED | Not yet hardware-verified |

1. Install the integration and confirm `R4Controller status` becomes `online`.
2. Check `PING`, `VERSION`, `INPUT`, `STATUS` and the configured
   `0.12.0` version.
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
9. With the normal `R4_REPLAY_ENABLED=0`, launch a game and inject
   `CAPTURE LONG`; confirm the ignored log entry, no FFmpeg process, no ring
   directory and no new recording. Optionally enable the explicit dev flag only
   to confirm that the preserved path reports itself as experimental.
10. With a correctly labeled/UUID Game Card, run `init` twice and verify one
   primary mount, a read-only ROM view, writable screenshot/video views and
   shared free space. Verify each BUSY owner blocks eject, overlapping owners
   coexist, and repeated READY scans produce no new state messages. Reboot
   without manually running `modprobe`; `/dev/fuse` must be prepared
   automatically. Verify `sync` precedes view/private unmounts, the still
   visible old `DISKSEQ` stays `EJECTED`, physical removal clears the session,
   and reinsertion under any `/dev/sdX` returns to `READY`. Also try a quick
   reconnect and confirm a changed `DISKSEQ` releases the latch.
11. Inspect malformed/unknown CDC commands and a line over 255 bytes; confirm a
   bounded `ERR` response and continued reconnect/service operation.

For developer diagnosis only, enable RetroArch's on-screen FPS display,
establish a no-recording baseline in the same scene, then start a fresh session
with the experimental flag enabled. From SSH, collect the encoder load and log:

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

This procedure documents the unsupported path; it is not a release acceptance
test on Orange Pi 3 LTS.

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
/tmp/r4-game-running
/tmp/r4-game-card/
/tmp/r4-replay-state/waiter.pid
/tmp/r4-replay-state/encoder.pid
/tmp/r4-replay-state/monitor.pid
/tmp/r4-replay-state/session
/tmp/r4-replay-state/status
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

Instant Replay is unsupported and disabled by default on Orange Pi 3 LTS
because the board exposes H.264 decoders but no usable hardware encoder, and
software capture has unacceptable frame rate and CPU cost. The implementation
is retained only behind the explicit experimental flag for future boards. The
physical Game Card layout and eject path are verified;
automatic FUSE recovery after reboot and a deliberately fast physical
reconnect still need the final on-device pass.

All four external RP2040 ADC channels are already occupied by the two analog sticks.

Additional analog inputs will require an external ADC or another analog input solution.

Additional digital controls should use a GPIO expander, button matrix or another bus-based expansion solution.

The remaining hardware work is concrete: select and electrically validate an
external ADC, record independent LT/RT calibration values, select and verify a
GPIO expander for Home/Capture/Trophy, assign/debounce those real button inputs,
validate the full-size SD reader and card-removal behavior, select
the OLED model and bus/address, implement its framebuffer backend, and validate
battery/power/temperature sensors on the final power design.
