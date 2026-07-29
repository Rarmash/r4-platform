#!/bin/sh

set -eu

TEST_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
INTEGRATION_DIR="$(CDPATH= cd -- "$TEST_DIR/.." && pwd)"
TEMP_DIR="${TMPDIR:-/tmp}/r4-game-card-tests-$$"

cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT HUP INT TERM

PRIVATE="$TEMP_DIR/private"
ROMS="$TEMP_DIR/roms"
SCREENSHOTS="$TEMP_DIR/screenshots"
VIDEOS="$TEMP_DIR/videos"
mkdir -p \
    "$TEMP_DIR/data" \
    "$TEMP_DIR/runtime" \
    "$PRIVATE" \
    "$ROMS" \
    "$SCREENSHOTS" \
    "$VIDEOS"

CONFIG="$TEMP_DIR/data/game-card.conf"
CANDIDATES="$TEMP_DIR/candidates"
DEVICE_DB="$TEMP_DIR/devices"
IDENTITIES="$TEMP_DIR/identities"
PROC_MOUNTS="$TEMP_DIR/mounts"
COMMANDS="$TEMP_DIR/commands"
MOUNT_LOG="$TEMP_DIR/mount.log"
UMOUNT_LOG="$TEMP_DIR/umount.log"
SYNC_LOG="$TEMP_DIR/sync.log"
MODPROBE_LOG="$TEMP_DIR/modprobe.log"
FUSE_READY="$TEMP_DIR/fuse-ready"
ECCTL="$TEMP_DIR/mock-ecctl"
BLKID="$TEMP_DIR/mock-blkid"
MOUNT="$TEMP_DIR/mock-mount"
UMOUNT="$TEMP_DIR/mock-umount"
SYNC="$TEMP_DIR/mock-sync"
MODPROBE="$TEMP_DIR/mock-modprobe"
MANAGER="$INTEGRATION_DIR/bin/r4-game-card"

write_config() {
    printf '%s\n' \
        'R4_GAME_CARD_ENABLED=1' \
        'R4_GAME_CARD_LABEL=R4CARD' \
        'R4_GAME_CARD_UUID=' \
        "R4_GAME_CARD_PRIVATE_MOUNTPOINT=$PRIVATE" \
        "R4_GAME_CARD_ROMS_MOUNTPOINT=$ROMS" \
        "R4_GAME_CARD_SCREENSHOTS_MOUNTPOINT=$SCREENSHOTS" \
        "R4_GAME_CARD_VIDEOS_MOUNTPOINT=$VIDEOS" \
        'R4_GAME_CARD_POLL_INTERVAL=1' \
        > "$CONFIG"
}

cat > "$ECCTL" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_TEST_COMMANDS"
echo OK
EOF

cat > "$BLKID" <<'EOF'
#!/bin/sh
field="$2"
device="$5"
awk -F '|' -v device="$device" -v field="$field" '
    $1 == device && field == "LABEL" { print $2; exit }
    $1 == device && field == "UUID" { print $3; exit }
    $1 == device && field == "TYPE" { print $4; exit }
' "$R4_TEST_DEVICE_DB"
EOF

cat > "$MOUNT" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_TEST_MOUNT_LOG"
[ ! -f "$R4_TEST_MOUNT_FAIL" ] || exit 1

if [ "$1" = "--bind" ]; then
    source_path="$2"
    target_path="$3"
    root_device="$(
        awk -v point="$R4_TEST_PRIVATE" \
            '$2 == point { print $1; exit }' \
            "$R4_TEST_PROC_MOUNTS"
    )"
    printf '%s %s bind rw,nosuid,nodev,noexec 0 0\n' \
        "$root_device" "$target_path" \
        >> "$R4_TEST_PROC_MOUNTS"
elif [ "$1" = "-o" ] &&
     [ "$2" = "remount,bind,ro" ]; then
    target_path="$3"
    awk -v point="$target_path" '
        $2 == point {
            print $1, $2, $3, "ro,nosuid,nodev,noexec,bind", $5, $6
            next
        }
        { print }
    ' "$R4_TEST_PROC_MOUNTS" > "$R4_TEST_PROC_MOUNTS.tmp"
    mv "$R4_TEST_PROC_MOUNTS.tmp" "$R4_TEST_PROC_MOUNTS"
else
    printf '%s %s mockfs rw,nosuid,nodev,noexec 0 0\n' \
        "$3" "$4" >> "$R4_TEST_PROC_MOUNTS"
    if [ -f "$R4_TEST_CARD_INITIALIZED" ]; then
        mkdir -p \
            "$4/ROMS" \
            "$4/CAPTURES/Screenshots" \
            "$4/CAPTURES/Videos"
    fi
fi
EOF

cat > "$UMOUNT" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >> "$R4_TEST_UMOUNT_LOG"
[ ! -f "$R4_TEST_UMOUNT_FAIL" ] || exit 1
awk -v point="$1" '$2 != point' "$R4_TEST_PROC_MOUNTS" \
    > "$R4_TEST_PROC_MOUNTS.tmp"
mv "$R4_TEST_PROC_MOUNTS.tmp" "$R4_TEST_PROC_MOUNTS"
if [ "$1" = "$R4_TEST_PRIVATE" ]; then
    rm -rf \
        "$R4_TEST_PRIVATE/ROMS" \
        "$R4_TEST_PRIVATE/CAPTURES"
fi
EOF

cat > "$SYNC" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_TEST_SYNC_LOG"
EOF

cat > "$MODPROBE" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_TEST_MODPROBE_LOG"
[ "$1" = "fuse" ] || exit 1
[ ! -f "$R4_TEST_MODPROBE_FAIL" ] || exit 1
[ ! -f "$R4_TEST_MODPROBE_NO_DEVICE" ] || exit 0
: > "$R4_TEST_FUSE_READY"
EOF

chmod +x \
    "$ECCTL" "$BLKID" "$MOUNT" "$UMOUNT" "$SYNC" "$MODPROBE"
write_config

export R4_DATA_DIR="$TEMP_DIR/data"
export R4_RUNTIME_DIR="$TEMP_DIR/runtime"
export R4_GAME_CARD_CONFIG="$CONFIG"
export R4_GAME_CARD_CANDIDATES_FILE="$CANDIDATES"
export R4_GAME_CARD_IDENTITIES_FILE="$IDENTITIES"
export R4_GAME_CARD_ALLOW_UNSAFE_TEST_PATHS=1
export R4_GAME_CARD_DEFAULT_PRIVATE_MOUNTPOINT="$PRIVATE"
export R4_GAME_CARD_DEFAULT_ROMS_MOUNTPOINT="$ROMS"
export R4_GAME_CARD_DEFAULT_SCREENSHOTS_MOUNTPOINT="$SCREENSHOTS"
export R4_GAME_CARD_DEFAULT_VIDEOS_MOUNTPOINT="$VIDEOS"
export R4_ECCTL="$ECCTL"
export R4_BLKID="$BLKID"
export R4_MOUNT="$MOUNT"
export R4_UMOUNT="$UMOUNT"
export R4_SYNC="$SYNC"
export R4_MODPROBE="$MODPROBE"
export R4_FUSE_READY_FILE="$FUSE_READY"
export R4_PROC_MOUNTS="$PROC_MOUNTS"
export R4_GAME_CARD_LOG="$TEMP_DIR/game-card.log"
export R4_TEST_COMMANDS="$COMMANDS"
export R4_TEST_DEVICE_DB="$DEVICE_DB"
export R4_TEST_MOUNT_LOG="$MOUNT_LOG"
export R4_TEST_UMOUNT_LOG="$UMOUNT_LOG"
export R4_TEST_SYNC_LOG="$SYNC_LOG"
export R4_TEST_MODPROBE_LOG="$MODPROBE_LOG"
export R4_TEST_FUSE_READY="$FUSE_READY"
export R4_TEST_MODPROBE_FAIL="$TEMP_DIR/modprobe-fail"
export R4_TEST_MODPROBE_NO_DEVICE="$TEMP_DIR/modprobe-no-device"
export R4_TEST_PROC_MOUNTS="$PROC_MOUNTS"
export R4_TEST_PRIVATE="$PRIVATE"
export R4_TEST_CARD_INITIALIZED="$TEMP_DIR/card-initialized"
export R4_TEST_MOUNT_FAIL="$TEMP_DIR/mount-fail"
export R4_TEST_UMOUNT_FAIL="$TEMP_DIR/umount-fail"

clear_files() {
    : > "$CANDIDATES"
    : > "$DEVICE_DB"
    : > "$IDENTITIES"
    : > "$PROC_MOUNTS"
    : > "$COMMANDS"
    : > "$MOUNT_LOG"
    : > "$UMOUNT_LOG"
    : > "$SYNC_LOG"
    : > "$MODPROBE_LOG"
    : > "$R4_GAME_CARD_LOG"
}

reset_state() {
    rm -rf "$TEMP_DIR/runtime"
    mkdir -p "$TEMP_DIR/runtime"
    rm -rf "$PRIVATE" "$ROMS" "$SCREENSHOTS" "$VIDEOS"
    mkdir -p "$PRIVATE" "$ROMS" "$SCREENSHOTS" "$VIDEOS"
    rm -f \
        "$R4_TEST_MOUNT_FAIL" \
        "$R4_TEST_UMOUNT_FAIL" \
        "$R4_TEST_MODPROBE_FAIL" \
        "$R4_TEST_MODPROBE_NO_DEVICE" \
        "$FUSE_READY"
    rm -f "$R4_TEST_CARD_INITIALIZED"
    clear_files
    write_config
}

insert_card() {
    card_device="${1:-/dev/sda1}"
    card_identity="${2:-DISKSEQ=1}"
    printf '%s\n' "$card_device" > "$CANDIDATES"
    printf '%s|R4CARD|CARD-UUID|exfat\n' \
        "$card_device" > "$DEVICE_DB"
    printf '%s|%s\n' \
        "$card_device" "$card_identity" > "$IDENTITIES"
}

state_is() {
    "$MANAGER" status | grep -q "^STATE=$1 "
}

reset_state
"$MANAGER" scan
state_is EJECTED

# Existing usable FUSE must not call modprobe.
: > "$FUSE_READY"
insert_card
"$MANAGER" scan
state_is INSERTED
[ ! -s "$MODPROBE_LOG" ]

# Missing FUSE is loaded once and verified before the exFAT mount.
reset_state
insert_card
"$MANAGER" prepare
[ ! -s "$MOUNT_LOG" ]
[ -e "$FUSE_READY" ]
[ "$(grep -c '^fuse$' "$MODPROBE_LOG")" -eq 1 ]
"$MANAGER" scan
state_is INSERTED
[ "$(grep -c '^fuse$' "$MODPROBE_LOG")" -eq 1 ]
"$MANAGER" scan
[ "$(grep -c '^fuse$' "$MODPROBE_LOG")" -eq 1 ]

# A failed module load is a stable, diagnostic ERROR, not a poll-time loop.
reset_state
insert_card
: > "$R4_TEST_MODPROBE_FAIL"
if "$MANAGER" scan >/dev/null 2>&1; then
    echo "missing FUSE was accepted" >&2
    exit 1
fi
state_is ERROR
grep -q 'fuse-modprobe-failed' "$R4_GAME_CARD_LOG"
[ "$(grep -c '^fuse$' "$MODPROBE_LOG")" -eq 1 ]
"$MANAGER" scan >/dev/null 2>&1 || true
[ "$(grep -c '^fuse$' "$MODPROBE_LOG")" -eq 1 ]

# A successful exit without a usable /dev/fuse is still an ERROR.
reset_state
insert_card
: > "$R4_TEST_MODPROBE_NO_DEVICE"
if "$MANAGER" scan >/dev/null 2>&1; then
    echo "modprobe success without a FUSE device was accepted" >&2
    exit 1
fi
state_is ERROR
grep -q 'fuse-device-unavailable' "$R4_GAME_CARD_LOG"
[ "$(grep -c '^fuse$' "$MODPROBE_LOG")" -eq 1 ]

reset_state
: > "$FUSE_READY"
insert_card
"$MANAGER" scan
state_is INSERTED
[ -d "$PRIVATE" ]
grep -q "^-o rw,nosuid,nodev,noexec /dev/sda1 $PRIVATE$" \
    "$MOUNT_LOG"
[ ! -d "$PRIVATE/ROMS" ]

"$MANAGER" init
: > "$R4_TEST_CARD_INITIALIZED"
state_is READY
[ -d "$PRIVATE/ROMS" ]
[ -d "$PRIVATE/CAPTURES/Screenshots" ]
[ -d "$PRIVATE/CAPTURES/Videos" ]
grep -q "^--bind $PRIVATE/ROMS $ROMS$" "$MOUNT_LOG"
grep -q "^-o remount,bind,ro $ROMS$" "$MOUNT_LOG"
grep -q "^--bind $PRIVATE/CAPTURES/Screenshots $SCREENSHOTS$" \
    "$MOUNT_LOG"
grep -q "^--bind $PRIVATE/CAPTURES/Videos $VIDEOS$" "$MOUNT_LOG"

primary_mounts="$(
    awk -v point="$PRIVATE" '$2 == point { count++ } END { print count + 0 }' \
        "$PROC_MOUNTS"
)"
[ "$primary_mounts" -eq 1 ]
[ "$(awk -v p="$ROMS" '$2 == p { print $1 }' "$PROC_MOUNTS")" = \
    /dev/sda1 ]
awk -v p="$ROMS" \
    '$2 == p && $4 ~ /(^|,)ro(,|$)/ { found=1 } END { exit !found }' \
    "$PROC_MOUNTS"
awk -v p="$SCREENSHOTS" \
    '$2 == p && $4 ~ /(^|,)rw(,|$)/ { found=1 } END { exit !found }' \
    "$PROC_MOUNTS"
awk -v p="$VIDEOS" \
    '$2 == p && $4 ~ /(^|,)rw(,|$)/ { found=1 } END { exit !found }' \
    "$PROC_MOUNTS"

mount_lines="$(wc -l < "$MOUNT_LOG")"
init_commands="$(wc -l < "$COMMANDS")"
"$MANAGER" init
[ "$(wc -l < "$MOUNT_LOG")" -eq "$mount_lines" ]
[ "$(wc -l < "$COMMANDS")" -eq "$init_commands" ]
state_is READY

stable_commands="$(wc -l < "$COMMANDS")"
stable_transitions="$(grep -c 'state=' "$R4_GAME_CARD_LOG")"
stable_mounts="$(wc -l < "$MOUNT_LOG")"
stable_scan=0
while [ "$stable_scan" -lt 5 ]; do
    "$MANAGER" scan
    stable_scan=$((stable_scan + 1))
done
[ "$(wc -l < "$COMMANDS")" -eq "$stable_commands" ]
[ "$(grep -c 'state=' "$R4_GAME_CARD_LOG")" -eq \
    "$stable_transitions" ]
[ "$(wc -l < "$MOUNT_LOG")" -eq "$stable_mounts" ]

"$MANAGER" busy "$ROMS/psx/Quake II.cue"
"$MANAGER" busy-add capture-clip
state_is BUSY
status="$("$MANAGER" status)"
printf '%s\n' "$status" | grep -q 'BUSY=.*capture-clip'
printf '%s\n' "$status" | grep -q 'BUSY=.*rom'
if "$MANAGER" eject >/dev/null 2>&1; then
    echo "card with overlapping BUSY owners was ejected" >&2
    exit 1
fi

"$MANAGER" busy-remove capture-clip
state_is BUSY
if "$MANAGER" eject >/dev/null 2>&1; then
    echo "card with ROM owner was ejected" >&2
    exit 1
fi
"$MANAGER" idle
state_is READY

[ "$("$MANAGER" capture-dir screenshots)" = "$SCREENSHOTS" ]
[ "$("$MANAGER" capture-dir videos)" = "$VIDEOS" ]

"$MANAGER" eject
state_is EJECTED
grep -q '^HOST CARD STATE=EJECTED PRESENT=1$' "$COMMANDS"
[ -s "$SYNC_LOG" ]
expected_unmounts="$VIDEOS
$SCREENSHOTS
$ROMS
$PRIVATE"
[ "$(cat "$UMOUNT_LOG")" = "$expected_unmounts" ]
[ ! -s "$PROC_MOUNTS" ]

mount_count="$(wc -l < "$MOUNT_LOG")"
"$MANAGER" scan
state_is EJECTED
[ "$(wc -l < "$MOUNT_LOG")" -eq "$mount_count" ]

# A changed DISKSEQ is a new physical session even when absence was missed.
insert_card /dev/sdb1 DISKSEQ=2
"$MANAGER" scan
state_is READY
grep -q 'new physical session detected after safe eject' \
    "$R4_GAME_CARD_LOG"

"$MANAGER" eject
state_is EJECTED
: > "$CANDIDATES"
"$MANAGER" scan
grep -q '^HOST CARD STATE=EJECTED PRESENT=0$' "$COMMANDS"
insert_card /dev/sda1 DISKSEQ=3
"$MANAGER" scan
state_is READY
printf '%s\n' "$("$MANAGER" status)" |
    grep -q 'IDENTITY=DISKSEQ=3'

: > "$CANDIDATES"
if "$MANAGER" scan >/dev/null 2>&1; then
    echo "unsafe removal did not report ERROR" >&2
    exit 1
fi
state_is ERROR

reset_state
printf '%s\n' /dev/sda1 > "$CANDIDATES"
printf '%s\n' '/dev/sda1|WRONG|CARD-UUID' > "$DEVICE_DB"
"$MANAGER" scan
state_is EJECTED

reset_state
printf '%s\n' /dev/sda1 /dev/sdb1 > "$CANDIDATES"
printf '%s\n' \
    '/dev/sda1|R4CARD|ONE' \
    '/dev/sdb1|R4CARD|TWO' \
    > "$DEVICE_DB"
if "$MANAGER" scan >/dev/null 2>&1; then
    echo "multiple matching cards were accepted" >&2
    exit 1
fi
state_is ERROR

reset_state
insert_card
printf '/dev/other %s ext4 rw 0 0\n' "$PRIVATE" > "$PROC_MOUNTS"
if "$MANAGER" scan >/dev/null 2>&1; then
    echo "occupied private mountpoint was accepted" >&2
    exit 1
fi
state_is ERROR

reset_state
insert_card
: > "$R4_TEST_MOUNT_FAIL"
if "$MANAGER" scan >/dev/null 2>&1; then
    echo "primary mount failure was ignored" >&2
    exit 1
fi
state_is ERROR

reset_state
insert_card
"$MANAGER" init
: > "$R4_TEST_UMOUNT_FAIL"
if "$MANAGER" eject >/dev/null 2>&1; then
    echo "view unmount failure was ignored" >&2
    exit 1
fi
state_is ERROR

reset_state
printf '%s\n' \
    'R4_GAME_CARD_ENABLED=1' \
    'R4_GAME_CARD_LABEL=IGNORED' \
    'R4_GAME_CARD_UUID=EXACT-UUID' \
    "R4_GAME_CARD_PRIVATE_MOUNTPOINT=$PRIVATE" \
    "R4_GAME_CARD_ROMS_MOUNTPOINT=$ROMS" \
    "R4_GAME_CARD_SCREENSHOTS_MOUNTPOINT=$SCREENSHOTS" \
    "R4_GAME_CARD_VIDEOS_MOUNTPOINT=$VIDEOS" \
    'R4_GAME_CARD_POLL_INTERVAL=1' \
    > "$CONFIG"
printf '%s\n' /dev/sda1 > "$CANDIDATES"
printf '%s\n' '/dev/sda1|WRONG|EXACT-UUID' > "$DEVICE_DB"
"$MANAGER" init
state_is READY

reset_state
printf '%s\n' \
    'R4_GAME_CARD_ENABLED=1' \
    'R4_GAME_CARD_LABEL=R4CARD' \
    'R4_GAME_CARD_UUID=' \
    "R4_GAME_CARD_MOUNTPOINT=$ROMS" \
    'R4_GAME_CARD_POLL_INTERVAL=1' \
    > "$CONFIG"
insert_card
"$MANAGER" init
legacy_status="$("$MANAGER" status)"
printf '%s\n' "$legacy_status" | grep -q "PRIVATE=$PRIVATE"
printf '%s\n' "$legacy_status" | grep -q "ROMS=$ROMS"

reset_state
printf '%s\n' 'R4_GAME_CARD_ENABLED=invalid' > "$CONFIG"
if "$MANAGER" scan >/dev/null 2>&1; then
    echo "invalid configuration was accepted" >&2
    exit 1
fi

reset_state
insert_card
"$MANAGER" init
"$MANAGER" busy /userdata/roms/psx/internal.cue
state_is READY

echo "R4 Game Card single-filesystem/view tests passed"
