#!/bin/sh

set -eu

TEST_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
INTEGRATION_DIR="$(CDPATH= cd -- "$TEST_DIR/.." && pwd)"
TEMP_DIR="${TMPDIR:-/tmp}/r4-firmware-update-tests-$$"
UPDATER="$INTEGRATION_DIR/bin/r4-firmware-update"

cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT HUP INT TERM

mkdir -p "$TEMP_DIR/state" "$TEMP_DIR/bin" "$TEMP_DIR/runtime"

UF2="$TEMP_DIR/r4-controller-fw-0.12.0.uf2"
MANIFEST="$UF2.manifest"

make_uf2() {
    dd if=/dev/zero of="$UF2" bs=512 count=1 2>/dev/null
    printf '\125\106\062\012\127\121\135\236' |
        dd of="$UF2" bs=1 seek=0 conv=notrunc 2>/dev/null
    printf '\000\040\000\000' |
        dd of="$UF2" bs=1 seek=8 conv=notrunc 2>/dev/null
    printf '\126\377\213\344' |
        dd of="$UF2" bs=1 seek=28 conv=notrunc 2>/dev/null
    printf '\060\157\261\012' |
        dd of="$UF2" bs=1 seek=508 conv=notrunc 2>/dev/null
    uf2_sha="$(sha256sum "$UF2" | awk '{print $1}')"
    printf '%s\n' \
        'R4_UF2_PRODUCT=R4 Controller' \
        'R4_UF2_FAMILY_ID=0xE48BFF56' \
        'R4_FIRMWARE_VERSION=0.12.0' \
        'R4_UF2_FILE=r4-controller-fw-0.12.0.uf2' \
        "R4_UF2_SHA256=$uf2_sha" \
        > "$MANIFEST"
    printf '%s  %s\n' "$uf2_sha" "$(basename "$UF2")" \
        > "$UF2.sha256"
}

cat > "$TEMP_DIR/bin/ecctl" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$TEST_COMMANDS"
case "$1" in
    VERSION)
        if [ -f "$TEST_STATE/updated" ]; then
            if [ -f "$TEST_STATE/version-mismatch" ]; then
                echo 'R4_CONTROLLER_FW 9.9.9'
            else
                echo 'R4_CONTROLLER_FW 0.12.0'
            fi
        else
            echo 'R4_CONTROLLER_FW 0.11.1'
        fi
        ;;
    UPDATE)
        case "$2" in
            ARM)
                if [ -f "$TEST_STATE/old-no-update" ]; then
                    echo 'ERR UNKNOWN_COMMAND'
                    exit 0
                fi
                echo 'UPDATE ARMED TOKEN=A1B2C3D4 TTL_MS=10000'
                ;;
            CONFIRM)
                [ "$3" = A1B2C3D4 ] || exit 1
                : > "$TEST_STATE/confirmed"
                echo 'OK UPDATE BOOTLOADER'
                ;;
        esac
        ;;
    LED) echo OK ;;
    *) exit 1 ;;
esac
EOF

cat > "$TEMP_DIR/bin/controller-list" <<'EOF'
#!/bin/sh
if [ -f "$TEST_STATE/controllers-none" ]; then
    exit 0
fi
if [ -f "$TEST_STATE/controllers-multiple" ]; then
    printf '%s\n' \
        '/dev/ttyACM0|:020201:030000:' \
        '/dev/ttyACM1|:020201:030000:'
    exit 0
fi
if [ -f "$TEST_STATE/confirmed" ] &&
   [ ! -f "$TEST_STATE/updated" ]; then
    exit 0
fi
if [ -f "$TEST_STATE/return-none" ] &&
   [ -f "$TEST_STATE/updated" ]; then
    exit 0
fi
echo '/dev/ttyACM0|:020201:030000:'
EOF

cat > "$TEMP_DIR/bin/boot-list" <<'EOF'
#!/bin/sh
if [ -f "$TEST_STATE/boot-multiple" ]; then
    printf '%s\n' \
        '/dev/mock-rpi1|2e8a|0003|RPI-RP2' \
        '/dev/mock-rpi2|2e8a|0003|RPI-RP2'
    exit 0
fi
if [ -f "$TEST_STATE/boot-foreign" ]; then
    echo '/dev/foreign|1234|5678|RPI-RP2'
    exit 0
fi
if [ -f "$TEST_STATE/boot-none" ]; then
    exit 0
fi
if [ -f "$TEST_STATE/confirmed" ] &&
   [ ! -f "$TEST_STATE/updated" ]; then
    echo '/dev/mock-rpi|2e8a|0003|RPI-RP2'
fi
EOF

cat > "$TEMP_DIR/bin/game-check" <<'EOF'
#!/bin/sh
[ -f "$TEST_STATE/game-running" ]
EOF

cat > "$TEMP_DIR/bin/services" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$TEST_SERVICES"
EOF

cat > "$TEMP_DIR/bin/mount" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$TEST_MOUNTS"
exit 0
EOF

cat > "$TEMP_DIR/bin/umount" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$TEST_UMOUNTS"
rm -f "$1"/*
exit 0
EOF

cat > "$TEMP_DIR/bin/cp" <<'EOF'
#!/bin/sh
[ ! -f "$TEST_STATE/copy-fail" ] || exit 1
cp "$1" "$2" || exit 1
: > "$TEST_STATE/updated"
EOF

cat > "$TEMP_DIR/bin/sync" <<'EOF'
#!/bin/sh
[ ! -f "$TEST_STATE/sync-fail" ]
EOF

cat > "$TEMP_DIR/bin/sleep" <<'EOF'
#!/bin/sh
exit 0
EOF

chmod +x "$TEMP_DIR/bin/"*

export TEST_STATE="$TEMP_DIR/state"
export TEST_COMMANDS="$TEMP_DIR/commands"
export TEST_SERVICES="$TEMP_DIR/services"
export TEST_MOUNTS="$TEMP_DIR/mounts"
export TEST_UMOUNTS="$TEMP_DIR/umounts"
export R4_RUNTIME_DIR="$TEMP_DIR/runtime"
export R4_UPDATE_ECCTL="$TEMP_DIR/bin/ecctl"
export R4_BATOCERA_SERVICES="$TEMP_DIR/bin/services"
export R4_UPDATE_CONTROLLER_LIST="$TEMP_DIR/bin/controller-list"
export R4_UPDATE_BOOTLOADER_LIST="$TEMP_DIR/bin/boot-list"
export R4_UPDATE_GAME_CHECK="$TEMP_DIR/bin/game-check"
export R4_MOUNT="$TEMP_DIR/bin/mount"
export R4_UMOUNT="$TEMP_DIR/bin/umount"
export R4_CP="$TEMP_DIR/bin/cp"
export R4_SYNC="$TEMP_DIR/bin/sync"
export R4_SLEEP="$TEMP_DIR/bin/sleep"
export R4_UPDATE_BOOT_TIMEOUT=2
export R4_UPDATE_RETURN_TIMEOUT=2
export R4_UPDATE_POLL_INTERVAL=0

reset_state() {
    rm -rf "$TEST_STATE" "$R4_RUNTIME_DIR"
    mkdir -p "$TEST_STATE" "$R4_RUNTIME_DIR"
    : > "$TEST_COMMANDS"
    : > "$TEST_SERVICES"
    : > "$TEST_MOUNTS"
    : > "$TEST_UMOUNTS"
    make_uf2
}

expect_failure() {
    expected_text="$1"
    shift
    if "$@" > "$TEMP_DIR/output" 2>&1; then
        echo "update unexpectedly succeeded: $expected_text" >&2
        exit 1
    fi
    grep -q "$expected_text" "$TEMP_DIR/output" || {
        cat "$TEMP_DIR/output" >&2
        exit 1
    }
}

reset_state
"$UPDATER" --yes "$UF2" > "$TEMP_DIR/output"
grep -q 'Current firmware: 0.11.1' "$TEMP_DIR/output"
grep -q 'Target firmware:  0.12.0' "$TEMP_DIR/output"
grep -q 'integrity only, not authenticity' "$TEMP_DIR/output"
grep -q 'Firmware update completed: R4_CONTROLLER_FW 0.12.0' \
    "$TEMP_DIR/output"
grep -q '^stop R4Controller$' "$TEST_SERVICES"
grep -q '^start R4Controller$' "$TEST_SERVICES"
grep -q '^UPDATE ARM$' "$TEST_COMMANDS"
grep -q '^UPDATE CONFIRM A1B2C3D4$' "$TEST_COMMANDS"
[ -f "$TEST_STATE/updated" ]
if find "$R4_RUNTIME_DIR" -maxdepth 1 -type d \
    -name 'r4-firmware-update.*' | grep -q .; then
    echo "temporary firmware-update mountpoint was not removed" >&2
    exit 1
fi

reset_state
: > "$TEST_STATE/old-no-update"
expect_failure 'install 0.12.0 with physical BOOTSEL one final time' \
    "$UPDATER" --yes "$UF2"
grep -q '^stop R4Controller$' "$TEST_SERVICES"
grep -q '^start R4Controller$' "$TEST_SERVICES"
grep -q '^UPDATE ARM$' "$TEST_COMMANDS"

reset_state
sed -i 's/^R4_UF2_SHA256=.*/R4_UF2_SHA256=0000000000000000000000000000000000000000000000000000000000000000/' \
    "$MANIFEST"
expect_failure 'SHA-256 does not match the manifest' \
    "$UPDATER" --yes "$UF2"
[ ! -s "$TEST_SERVICES" ]

reset_state
printf '\000\000\000\000' |
    dd of="$UF2" bs=1 seek=28 conv=notrunc 2>/dev/null
expect_failure 'invalid UF2 structure or non-RP2040 family ID' \
    "$UPDATER" --yes "$UF2"

reset_state
: > "$TEST_STATE/game-running"
expect_failure 'forbidden while a game/emulator is running' \
    "$UPDATER" --yes "$UF2"

reset_state
: > "$TEST_STATE/controllers-none"
expect_failure 'expected exactly one R4 Controller, found 0' \
    "$UPDATER" --yes "$UF2"

reset_state
: > "$TEST_STATE/controllers-multiple"
expect_failure 'expected exactly one R4 Controller, found 2' \
    "$UPDATER" --yes "$UF2"

reset_state
: > "$TEST_STATE/boot-none"
expect_failure 'bootloader did not appear before timeout' \
    "$UPDATER" --yes "$UF2"
grep -q '^start R4Controller$' "$TEST_SERVICES"

reset_state
: > "$TEST_STATE/boot-multiple"
expect_failure 'multiple matching RPI-RP2 bootloaders found' \
    "$UPDATER" --yes "$UF2"

reset_state
: > "$TEST_STATE/boot-foreign"
expect_failure 'bootloader did not appear before timeout' \
    "$UPDATER" --yes "$UF2"
[ ! -s "$TEST_MOUNTS" ]

reset_state
: > "$TEST_STATE/copy-fail"
expect_failure 'UF2 copy failed or timed out' \
    "$UPDATER" --yes "$UF2"
grep -q '/dev/mock-rpi' "$TEST_MOUNTS"
grep -q 'r4-firmware-update' "$TEST_UMOUNTS"
grep -q '^start R4Controller$' "$TEST_SERVICES"

reset_state
: > "$TEST_STATE/version-mismatch"
expect_failure 'version mismatch after update' \
    "$UPDATER" --yes "$UF2"

reset_state
: > "$TEST_STATE/return-none"
expect_failure 'CDC/HID did not return before timeout' \
    "$UPDATER" --yes "$UF2"

cat > "$TEMP_DIR/bin/dispatch-updater" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" > "$TEST_STATE/dispatched"
EOF
chmod +x "$TEMP_DIR/bin/dispatch-updater"
R4_FIRMWARE_UPDATER="$TEMP_DIR/bin/dispatch-updater" \
    "$INTEGRATION_DIR/bin/r4-ecctl" \
    firmware update --yes "$UF2"
grep -Fq -- "--yes $UF2" "$TEST_STATE/dispatched"

echo "R4 host-assisted USB firmware update tests passed"
