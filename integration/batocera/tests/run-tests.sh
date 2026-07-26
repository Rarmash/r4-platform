#!/bin/sh

set -eu

TEST_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
INTEGRATION_DIR="$(CDPATH= cd -- "$TEST_DIR/.." && pwd)"
TEMP_DIR="${TMPDIR:-/tmp}/r4-batocera-tests-$$"

cleanup() {
    rm -rf "$TEMP_DIR"
}

trap cleanup EXIT HUP INT TERM
mkdir -p "$TEMP_DIR/data" "$TEMP_DIR/runtime"

for script in \
    "$INTEGRATION_DIR/install.sh" \
    "$INTEGRATION_DIR/bin/r4-ecctl" \
    "$INTEGRATION_DIR/bin/r4-led-state" \
    "$INTEGRATION_DIR/bin/r4-oled-tcp" \
    "$INTEGRATION_DIR/services/R4Controller" \
    "$INTEGRATION_DIR/scripts/R4GameState" \
    "$INTEGRATION_DIR/emulationstation/game-start/R4GameMetadata" \
    "$INTEGRATION_DIR/emulationstation/achievements/R4Achievement" \
    "$0"
do
    sh -n "$script"
done

service="$INTEGRATION_DIR/services/R4Controller"
log_file="$TEMP_DIR/controller.log"

if R4_DATA_DIR="$TEMP_DIR/data" \
   R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
   R4_LOG_FILE="$log_file" \
   "$service" test-event CAPTURE SHORT >/dev/null 2>&1; then
    echo "test-event unexpectedly worked without opt-in" >&2
    exit 1
fi

R4_DATA_DIR="$TEMP_DIR/data" \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_LOG_FILE="$log_file" \
R4_ENABLE_TEST_EVENTS=1 \
    "$service" test-event CAPTURE SHORT

grep -q \
    'Capture short event seq=TEST: screenshot action is not wired yet' \
    "$log_file"

R4_DATA_DIR="$TEMP_DIR/data" \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_LOG_FILE="$log_file" \
R4_ENABLE_TEST_EVENTS=1 \
    "$service" test-event R4 SHORT

grep -q \
    'R4 short event seq=TEST: system panel action stub' \
    "$log_file"

if R4_DATA_DIR="$TEMP_DIR/data" \
   R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
   R4_LOG_FILE="$log_file" \
   R4_ENABLE_TEST_EVENTS=1 \
   "$service" test-event INVALID SHORT >/dev/null 2>&1; then
    echo "invalid test event unexpectedly succeeded" >&2
    exit 1
fi

cp \
    "$INTEGRATION_DIR/firmware-version.conf" \
    "$TEMP_DIR/data/firmware-version.conf"

printf '%s\n' \
    'R4_OLED_TCP_ENABLED=invalid' \
    'R4_OLED_TCP_BIND=127.0.0.1' \
    'R4_OLED_TCP_PORT=4274' \
    > "$TEMP_DIR/data/oled-tcp.conf"

mock_ecctl="$TEMP_DIR/mock-ecctl"
mock_commands="$TEMP_DIR/mock-commands.log"

cat > "$mock_ecctl" <<'EOF'
#!/bin/sh

printf '%s\n' "$*" >> "$R4_MOCK_COMMANDS"

case "$*" in
    PING)
        echo PONG
        ;;
    VERSION)
        echo "R4_CONTROLLER_FW 0.8.0"
        ;;
    "HOST HEARTBEAT")
        echo "OK HOST HEARTBEAT"
        ;;
    "EVENT NEXT")
        echo "EVENT NONE"
        ;;
    "FRAMEBUFFER INFO")
        echo "FRAMEBUFFER INFO ID=1 WIDTH=128 HEIGHT=64 FORMAT=MONO1_MSB BYTES=1024 HASH=00000000 CHUNK_MAX=96"
        ;;
    --framebuffer)
        printf '%s' \
            "FRAMEBUFFER FULL WIDTH=128 HEIGHT=64 FORMAT=MONO1_MSB BYTES=1024 HASH=00000000 HEX="
        awk 'BEGIN { for (i = 0; i < 1024; i++) printf "00"; print "" }'
        ;;
    *)
        echo OK
        ;;
esac
EOF

chmod +x "$mock_ecctl"

title_lookup="$INTEGRATION_DIR/bin/r4-game-title"
fixture_dir="$TEST_DIR/fixtures"
quake_rom="$fixture_dir/Quake II (USA).cue"
mario_rom="$fixture_dir/Classic NES Series - Super Mario Bros. (USA, Europe).gba"

[ "$("$title_lookup" "$quake_rom")" = "Quake II" ]
[ "$("$title_lookup" "$mario_rom")" = \
    "Classic NES Series - Super Mario Bros." ]

R4_ES_EVENT=game-selected \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_TITLE_LOOKUP="$title_lookup" \
R4_LOG_FILE="$TEMP_DIR/game-events.log" \
    "$INTEGRATION_DIR/emulationstation/game-start/R4GameMetadata" \
    psx \
    "$quake_rom" \
    "Quake II (USA)"

grep -q '^TITLE_HEX=5175616b65204949$' \
    "$TEMP_DIR/runtime/r4-game-metadata"
grep -q '^TITLE_TEXT=Quake II$' \
    "$TEMP_DIR/runtime/r4-game-metadata"

R4_ES_EVENT=game-start \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_TITLE_LOOKUP="$title_lookup" \
R4_ECCTL="$mock_ecctl" \
R4_LOG_FILE="$TEMP_DIR/game-events.log" \
R4_MOCK_COMMANDS="$mock_commands" \
    "$INTEGRATION_DIR/emulationstation/game-start/R4GameMetadata" \
    "$quake_rom" \
    "Quake II (USA)"

grep -q \
    '^HOST GAME ACTION=START SYSTEM_HEX=707378 GAME_HEX=5175616b65204949$' \
    "$mock_commands"

R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_TITLE_LOOKUP="$title_lookup" \
R4_ECCTL="$mock_ecctl" \
R4_LED_STATE=/bin/true \
R4_LOG_FILE="$TEMP_DIR/game-events.log" \
R4_MOCK_COMMANDS="$mock_commands" \
    "$INTEGRATION_DIR/scripts/R4GameState" \
    gameStart \
    psx \
    libretro \
    pcsx_rearmed \
    "$quake_rom"

grep -q \
    '^HOST GAME ACTION=START SYSTEM_HEX=707378 GAME_HEX=5175616b65204949$' \
    "$mock_commands"

R4_ES_EVENT=game-selected \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_TITLE_LOOKUP="$title_lookup" \
R4_LOG_FILE="$TEMP_DIR/game-events.log" \
    "$INTEGRATION_DIR/emulationstation/game-start/R4GameMetadata" \
    gba \
    "$mario_rom" \
    "Classic NES Series - Super Mario Bros. (USA, Europe)"

grep -q \
    '^TITLE_TEXT=Classic NES Series - Super Mario Bros.$' \
    "$TEMP_DIR/runtime/r4-game-metadata"

R4_ES_EVENT=game-start \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_TITLE_LOOKUP="$title_lookup" \
R4_ECCTL="$mock_ecctl" \
R4_LOG_FILE="$TEMP_DIR/game-events.log" \
R4_MOCK_COMMANDS="$mock_commands" \
    "$INTEGRATION_DIR/emulationstation/game-start/R4GameMetadata" \
    "$mario_rom" \
    "Classic NES Series - Super Mario Bros. (USA, Europe)"

R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_TITLE_LOOKUP="$title_lookup" \
R4_ECCTL="$mock_ecctl" \
R4_LED_STATE=/bin/true \
R4_LOG_FILE="$TEMP_DIR/game-events.log" \
R4_MOCK_COMMANDS="$mock_commands" \
    "$INTEGRATION_DIR/scripts/R4GameState" \
    gameStart \
    gba \
    libretro \
    mgba \
    "$mario_rom"

grep -q \
    '^HOST GAME ACTION=START SYSTEM_HEX=676261 GAME_HEX=436c6173736963204e455320536572696573202d205375706572204d6172696f2042726f732e$' \
    "$mock_commands"
if grep -q 'GAME_HEX=.*2855534129' "$mock_commands"; then
    echo "ROM region tag leaked into the OLED game title" >&2
    exit 1
fi
if grep -q 'SYSTEM_HEX=2f7573657264617461' "$mock_commands"; then
    echo "ROM path leaked into the OLED system name" >&2
    exit 1
fi

R4_DATA_DIR="$TEMP_DIR/data" \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_LOG_FILE="$log_file" \
R4_ECCTL="$mock_ecctl" \
R4_LED_STATE=/bin/true \
R4_MOCK_COMMANDS="$mock_commands" \
    "$service" run &

watchdog_pid=$!
sleep 3

broker_response="$(
    R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
        "$INTEGRATION_DIR/bin/r4-ecctl" PING
)"
[ "$broker_response" = "PONG" ]

relay_output="$(
    printf '%s\n' \
        "REQ 7 PING" \
        "REQ 8 FRAMEBUFFER INFO" \
        "REQ 9 FRAMEBUFFER READ" \
        "REQ INVALID" |
        R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
        R4_ECCTL="$INTEGRATION_DIR/bin/r4-ecctl" \
        "$INTEGRATION_DIR/bin/r4-oled-tcp" --stdio
)"

printf '%s\n' "$relay_output" |
    grep -q '^RES 7 OK PONG$'
printf '%s\n' "$relay_output" |
    grep -q '^RES 8 OK FRAMEBUFFER INFO ID=1 '
printf '%s\n' "$relay_output" |
    grep -q '^RES 9 OK FRAMEBUFFER FULL WIDTH=128 HEIGHT=64 '
printf '%s\n' "$relay_output" |
    grep -q '^RES 0 ERROR BAD_REQUEST_ID$'

kill "$watchdog_pid" 2>/dev/null || true
wait "$watchdog_pid" 2>/dev/null || true

grep -q '^HOST HEARTBEAT$' "$mock_commands"
grep -q '^HOST STATE MODE=HOME$' "$mock_commands"
grep -Eq '^HOST TELEMETRY TIME_HEX=[0-9a-f]{10} VOLUME=NA$' \
    "$mock_commands"
grep -q 'Invalid R4_OLED_TCP_ENABLED' "$log_file"

echo "Batocera shell, broker, TCP framing, heartbeat and event mocks passed"
