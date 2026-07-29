# R4 Batocera Integration Changelog

## 0.12.0

- Added `r4-ecctl firmware update` for a guarded host-assisted USB firmware
  update through the RP2040 ROM bootloader.
- Added UF2 structure/family, R4 manifest and SHA-256 integrity checks.
- Refused updates during gameplay or with ambiguous controller/bootloader
  devices.
- Added bounded mount, copy, sync, unmount, USB-return and version-verification
  stages with cleanup and physical BOOTSEL recovery instructions.
- Updated the expected RP2040 firmware to `0.12.0`, matching this integration
  release.
- Disabled Instant Replay by default on fresh installs and upgrades.
- Kept Capture SHORT screenshots fully supported.
- Retained replay behind the explicit experimental
  `R4_REPLAY_ENABLED=1` developer flag with high-load status/log warnings.
- Prevented disabled replay from creating its ring or starting a waiter or
  FFmpeg.
- Added `-vsync 0` to the experimental FFmpeg capture path.
- Added a Batocera integration version file.
- Documented why Orange Pi 3 LTS software replay is unsupported.

See [README.md](README.md#0120-release-notes) for the hardware findings and
operational details.
