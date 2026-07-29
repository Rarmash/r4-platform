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
    "$INTEGRATION_DIR/uninstall.sh" \
    "$INTEGRATION_DIR/bin/r4-ecctl" \
    "$INTEGRATION_DIR/bin/r4-firmware-update" \
    "$INTEGRATION_DIR/bin/r4-led-state" \
    "$INTEGRATION_DIR/bin/r4-oled-tcp" \
    "$INTEGRATION_DIR/bin/r4-game-card" \
    "$INTEGRATION_DIR/bin/r4-replay" \
    "$INTEGRATION_DIR/services/R4Controller" \
    "$INTEGRATION_DIR/services/R4GameCard" \
    "$INTEGRATION_DIR/scripts/R4GameState" \
    "$INTEGRATION_DIR/emulationstation/game-start/R4GameMetadata" \
    "$INTEGRATION_DIR/emulationstation/achievements/R4Achievement" \
    "$TEST_DIR/test-game-card.sh" \
    "$TEST_DIR/test-replay.sh" \
    "$TEST_DIR/test-firmware-update.sh" \
    "$0"
do
    sh -n "$script"
done

install_root="$TEMP_DIR/install-root"
install_r4="$install_root/r4"
install_services="$install_root/services"
install_scripts="$install_root/scripts"
install_achievements="$install_root/achievements"
install_game_start="$install_root/game-start"
install_game_selected="$install_root/game-selected"
install_runtime="$TEMP_DIR/install-runtime"
mock_batocera_services="$TEMP_DIR/mock-batocera-services"
install_service_commands="$TEMP_DIR/install-service-commands.log"

mkdir -p \
    "$install_r4" \
    "$install_services" \
    "$install_scripts" \
    "$install_achievements" \
    "$install_game_start" \
    "$install_game_selected" \
    "$install_runtime"

sed \
    -e '/^R4_REPLAY_DRM_DEVICE=/d' \
    -e '/^R4_REPLAY_READY_/d' \
    -e "s|^R4_REPLAY_BUFFER_DIR=.*|R4_REPLAY_BUFFER_DIR=$TEMP_DIR/install-buffer|" \
    "$INTEGRATION_DIR/replay.conf" \
    > "$install_r4/replay.conf"
sed -i 's/^R4_REPLAY_ENABLED=.*/R4_REPLAY_ENABLED=1/' \
    "$install_r4/replay.conf"

printf '%s\n' '#!/bin/sh' 'exit 0' \
    > "$install_scripts/R4GameState.disabled"
cp "$install_scripts/R4GameState.disabled" \
    "$install_scripts/R4GameState.backup"
cp "$install_scripts/R4GameState.disabled" \
    "$install_r4/R4GameState.disabled"
chmod +x \
    "$install_scripts/R4GameState.disabled" \
    "$install_scripts/R4GameState.backup" \
    "$install_r4/R4GameState.disabled"

cat > "$mock_batocera_services" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_INSTALL_SERVICE_COMMANDS"
EOF
chmod +x "$mock_batocera_services"
: > "$install_service_commands"

install_once() {
    R4_INSTALL_R4_DIR="$install_r4" \
    R4_INSTALL_SERVICE_DIR="$install_services" \
    R4_INSTALL_SCRIPT_DIR="$install_scripts" \
    R4_INSTALL_ACHIEVEMENT_DIR="$install_achievements" \
    R4_INSTALL_GAME_START_DIR="$install_game_start" \
    R4_INSTALL_GAME_SELECTED_DIR="$install_game_selected" \
    R4_BATOCERA_SERVICES="$mock_batocera_services" \
    R4_INSTALL_SETTLE_SECONDS=0 \
    R4_INSTALL_SERVICE_COMMANDS="$install_service_commands" \
    R4_DATA_DIR="$install_r4" \
    R4_RUNTIME_DIR="$install_runtime" \
    R4_REPLAY_STATE_DIR="$install_runtime/replay-state" \
    R4_REPLAY_CONFIG="$install_r4/replay.conf" \
        "$INTEGRATION_DIR/install.sh" >/dev/null 2>&1
}

install_once

grep -q '^R4_REPLAY_ENABLED=0$' "$install_r4/replay.conf"
grep -q '^R4_BATOCERA_VERSION=0.12.0$' \
    "$install_r4/integration-version.conf"

# A deliberate experimental opt-in is preserved after the 0.12.0 migration.
sed -i 's/^R4_REPLAY_ENABLED=.*/R4_REPLAY_ENABLED=1/' \
    "$install_r4/replay.conf"
install_once
grep -q '^R4_REPLAY_ENABLED=1$' "$install_r4/replay.conf"

[ -x "$install_r4/r4-replay" ]
[ -x "$install_r4/r4-firmware-update" ]
cmp "$install_r4/r4-firmware-update" \
    "$INTEGRATION_DIR/bin/r4-firmware-update"
cmp "$install_r4/r4-replay" "$INTEGRATION_DIR/bin/r4-replay"
[ -x "$install_scripts/R4GameState" ]
[ ! -e "$install_scripts/R4GameState.disabled" ]
[ ! -x "$install_scripts/R4GameState.backup" ]
[ ! -x "$install_r4/R4GameState.disabled" ]
[ "$(
    find "$install_scripts" -maxdepth 1 \
        -type f -name 'R4GameState*' -perm /111 |
        wc -l
)" -eq 1 ]
if grep -q '^R4_REPLAY_READY_' "$install_r4/replay.conf"; then
    echo "installer replaced the preserved 0.11.0 replay configuration" >&2
    exit 1
fi

R4_INSTALL_R4_DIR="$install_r4" \
R4_INSTALL_SERVICE_DIR="$install_services" \
R4_INSTALL_SCRIPT_DIR="$install_scripts" \
R4_INSTALL_ACHIEVEMENT_DIR="$install_achievements" \
R4_INSTALL_GAME_START_DIR="$install_game_start" \
R4_INSTALL_GAME_SELECTED_DIR="$install_game_selected" \
R4_BATOCERA_SERVICES="$mock_batocera_services" \
R4_INSTALL_SERVICE_COMMANDS="$install_service_commands" \
R4_DATA_DIR="$install_r4" \
R4_REPLAY_STATE_DIR="$install_runtime/replay-state" \
R4_REPLAY_BUFFER_DIR="$TEMP_DIR/install-buffer" \
R4_REPLAY_CONFIG="$install_r4/replay.conf" \
    "$INTEGRATION_DIR/uninstall.sh" >/dev/null

[ ! -e "$install_r4/r4-replay" ]
[ ! -e "$install_services/R4Controller" ]
[ ! -e "$install_scripts/R4GameState" ]
[ -f "$install_r4/replay.conf" ]
[ ! -e "$install_runtime/replay-state" ]
grep -q '^disable R4Controller$' "$install_service_commands"
grep -q '^disable R4GameCard$' "$install_service_commands"

service="$INTEGRATION_DIR/services/R4Controller"
log_file="$TEMP_DIR/controller.log"
capture_ecctl="$TEMP_DIR/capture-ecctl"
capture_commands="$TEMP_DIR/capture-commands.log"
capture_action_log="$TEMP_DIR/capture-actions.log"
capture_success="$TEMP_DIR/batocera-screenshot-success"
capture_failure="$TEMP_DIR/batocera-screenshot-failure"
capture_replay="$TEMP_DIR/capture-replay"
capture_replay_commands="$TEMP_DIR/capture-replay-commands.log"

cat > "$capture_ecctl" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_CAPTURE_COMMANDS"
echo OK
EOF

cat > "$capture_success" <<'EOF'
#!/bin/sh
echo screenshot >> "$R4_CAPTURE_ACTIONS"
exit 0
EOF

cat > "$capture_failure" <<'EOF'
#!/bin/sh
echo screenshot >> "$R4_CAPTURE_ACTIONS"
exit 1
EOF

cat > "$capture_replay" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_CAPTURE_REPLAY_COMMANDS"
case "$1" in
    save)
        if [ "${R4_CAPTURE_REPLAY_DISABLED:-0}" = "1" ]; then
            echo DISABLED
            exit 0
        fi
        if [ "${R4_CAPTURE_REPLAY_UNAVAILABLE:-0}" = "1" ]; then
            echo UNAVAILABLE
            exit 3
        fi
        echo ACCEPTED
        ;;
    *) exit 1 ;;
esac
EOF

chmod +x \
    "$capture_ecctl" \
    "$capture_success" \
    "$capture_failure" \
    "$capture_replay"
: > "$capture_commands"
: > "$capture_action_log"
: > "$capture_replay_commands"

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
R4_ECCTL="$capture_ecctl" \
R4_BATOCERA_SCREENSHOT="$capture_success" \
R4_CAPTURE_COMMANDS="$capture_commands" \
R4_CAPTURE_ACTIONS="$capture_action_log" \
R4_ENABLE_TEST_EVENTS=1 \
    "$service" test-event CAPTURE SHORT

[ "$(wc -l < "$capture_action_log")" -eq 1 ]
grep -q '^HOST CAPTURE TYPE=SCREENSHOT STATUS=BUSY$' "$capture_commands"
grep -q '^HOST CAPTURE TYPE=SCREENSHOT STATUS=SAVED$' "$capture_commands"
grep -q '^LED FLASH 0 32 0 500$' "$capture_commands"
grep -q 'Capture short event seq=TEST result=SAVED detail=saved' \
    "$log_file"

: > "$capture_commands"
: > "$capture_action_log"
R4_DATA_DIR="$TEMP_DIR/data" \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_LOG_FILE="$log_file" \
R4_ECCTL="$capture_ecctl" \
R4_BATOCERA_SCREENSHOT="$capture_failure" \
R4_CAPTURE_COMMANDS="$capture_commands" \
R4_CAPTURE_ACTIONS="$capture_action_log" \
R4_ENABLE_TEST_EVENTS=1 \
    "$service" test-event CAPTURE SHORT

[ "$(wc -l < "$capture_action_log")" -eq 1 ]
grep -q '^HOST CAPTURE TYPE=SCREENSHOT STATUS=ERROR$' "$capture_commands"
grep -q '^LED FLASH 32 0 0 700$' "$capture_commands"
grep -q 'Capture short event seq=TEST result=ERROR detail=command-failed' \
    "$log_file"

: > "$capture_commands"
: > "$capture_action_log"
R4_DATA_DIR="$TEMP_DIR/data" \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_LOG_FILE="$log_file" \
R4_ECCTL="$capture_ecctl" \
R4_BATOCERA_SCREENSHOT="$TEMP_DIR/missing-batocera-screenshot" \
R4_CAPTURE_COMMANDS="$capture_commands" \
R4_CAPTURE_ACTIONS="$capture_action_log" \
R4_ENABLE_TEST_EVENTS=1 \
    "$service" test-event CAPTURE SHORT

[ ! -s "$capture_action_log" ]
grep -q '^HOST CAPTURE TYPE=SCREENSHOT STATUS=ERROR$' "$capture_commands"
grep -q 'Capture short event seq=TEST result=ERROR detail=command-missing' \
    "$log_file"

: > "$capture_commands"
: > "$capture_action_log"
R4_DATA_DIR="$TEMP_DIR/data" \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_LOG_FILE="$log_file" \
R4_ECCTL="$capture_ecctl" \
R4_REPLAY_MANAGER="$capture_replay" \
R4_BATOCERA_SCREENSHOT="$capture_success" \
R4_CAPTURE_COMMANDS="$capture_commands" \
R4_CAPTURE_ACTIONS="$capture_action_log" \
R4_CAPTURE_REPLAY_COMMANDS="$capture_replay_commands" \
R4_ENABLE_TEST_EVENTS=1 \
    "$service" test-event CAPTURE LONG

[ ! -s "$capture_action_log" ]
if grep -q 'TYPE=SCREENSHOT' "$capture_commands"; then
    echo "CAPTURE:LONG sent screenshot feedback" >&2
    exit 1
fi
[ "$(grep -c '^save TEST$' "$capture_replay_commands")" -eq 1 ]
grep -q '^HOST CAPTURE TYPE=CLIP STATUS=SAVING$' "$capture_commands"
grep -q '^LED FLASH 24 12 0 700$' "$capture_commands"
grep -q 'Capture long event seq=TEST result=ACCEPTED' "$log_file"

: > "$capture_commands"
R4_DATA_DIR="$TEMP_DIR/data" \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_LOG_FILE="$log_file" \
R4_ECCTL="$capture_ecctl" \
R4_REPLAY_MANAGER="$capture_replay" \
R4_CAPTURE_COMMANDS="$capture_commands" \
R4_CAPTURE_REPLAY_COMMANDS="$capture_replay_commands" \
R4_CAPTURE_REPLAY_DISABLED=1 \
R4_ENABLE_TEST_EVENTS=1 \
    "$service" test-event CAPTURE LONG

[ ! -s "$capture_commands" ]
grep -q \
    'Capture long event seq=TEST result=IGNORED detail=experimental-replay-disabled' \
    "$log_file"

: > "$capture_commands"
R4_DATA_DIR="$TEMP_DIR/data" \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_LOG_FILE="$log_file" \
R4_ECCTL="$capture_ecctl" \
R4_REPLAY_MANAGER="$capture_replay" \
R4_CAPTURE_COMMANDS="$capture_commands" \
R4_CAPTURE_REPLAY_COMMANDS="$capture_replay_commands" \
R4_CAPTURE_REPLAY_UNAVAILABLE=1 \
R4_ENABLE_TEST_EVENTS=1 \
    "$service" test-event CAPTURE LONG

grep -q '^HOST CAPTURE TYPE=CLIP STATUS=UNAVAILABLE$' "$capture_commands"
grep -q '^LED FLASH 32 0 0 900$' "$capture_commands"

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
printf '%s\n' READY > "$TEMP_DIR/game-card-state"

mock_ecctl="$TEMP_DIR/mock-ecctl"
mock_commands="$TEMP_DIR/mock-commands.log"
mock_game_card="$TEMP_DIR/mock-game-card"
mock_game_card_commands="$TEMP_DIR/mock-game-card-commands.log"
mock_replay="$TEMP_DIR/mock-replay"
mock_replay_commands="$TEMP_DIR/mock-replay-commands.log"

cat > "$mock_ecctl" <<'EOF'
#!/bin/sh

printf '%s\n' "$*" >> "$R4_MOCK_COMMANDS"

case "$*" in
    PING)
        echo PONG
        ;;
    VERSION)
        echo "R4_CONTROLLER_FW 0.12.0"
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

cat > "$mock_game_card" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_MOCK_GAME_CARD_COMMANDS"
EOF

chmod +x "$mock_game_card"
: > "$mock_game_card_commands"

cat > "$mock_replay" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_MOCK_REPLAY_COMMANDS"
[ -z "${R4_MOCK_REPLAY_ARGUMENTS:-}" ] || {
    printf 'argc=%s\n' "$#" >> "$R4_MOCK_REPLAY_ARGUMENTS"
    argument_index=1
    for argument in "$@"; do
        printf 'arg%s=<%s>\n' "$argument_index" "$argument" \
            >> "$R4_MOCK_REPLAY_ARGUMENTS"
        argument_index=$((argument_index + 1))
    done
}
[ "${R4_MOCK_REPLAY_FAIL:-0}" != "1" ] || exit 1
EOF

chmod +x "$mock_replay"
: > "$mock_replay_commands"
mock_replay_arguments="$TEMP_DIR/mock-replay-arguments.log"
: > "$mock_replay_arguments"

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
R4_GAME_CARD_MANAGER="$mock_game_card" \
R4_REPLAY_MANAGER="$mock_replay" \
R4_LOG_FILE="$TEMP_DIR/game-events.log" \
R4_MOCK_COMMANDS="$mock_commands" \
R4_MOCK_GAME_CARD_COMMANDS="$mock_game_card_commands" \
R4_MOCK_REPLAY_COMMANDS="$mock_replay_commands" \
    "$INTEGRATION_DIR/scripts/R4GameState" \
    gameStart \
    psx \
    libretro \
    pcsx_rearmed \
    "$quake_rom"

grep -q \
    '^HOST GAME ACTION=START SYSTEM_HEX=707378 GAME_HEX=5175616b65204949$' \
    "$mock_commands"
grep -Fq "busy $quake_rom" "$mock_game_card_commands"
grep -Fq "start psx $quake_rom Quake II" "$mock_replay_commands"

special_rom="$fixture_dir/Odd & [Test] (USA).cue"
: > "$mock_replay_arguments"
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_TITLE_LOOKUP="$title_lookup" \
R4_ECCTL="$mock_ecctl" \
R4_LED_STATE=/bin/true \
R4_GAME_CARD_MANAGER="$mock_game_card" \
R4_REPLAY_MANAGER="$mock_replay" \
R4_LOG_FILE="$TEMP_DIR/game-events.log" \
R4_MOCK_COMMANDS="$mock_commands" \
R4_MOCK_GAME_CARD_COMMANDS="$mock_game_card_commands" \
R4_MOCK_REPLAY_COMMANDS="$mock_replay_commands" \
R4_MOCK_REPLAY_ARGUMENTS="$mock_replay_arguments" \
R4_MOCK_REPLAY_FAIL=1 \
    "$INTEGRATION_DIR/scripts/R4GameState" \
    gameStart \
    psx \
    libretro \
    pcsx_rearmed \
    "$special_rom"

grep -q '^argc=4$' "$mock_replay_arguments"
grep -Fq 'arg1=<start>' "$mock_replay_arguments"
grep -Fq 'arg2=<psx>' "$mock_replay_arguments"
grep -Fq "arg3=<$special_rom>" "$mock_replay_arguments"
grep -Fq 'arg4=<Odd & [Test]>' "$mock_replay_arguments"
grep -Fq "busy $special_rom" "$mock_game_card_commands"
grep -q \
    '^HOST GAME ACTION=START SYSTEM_HEX=707378 GAME_HEX=4f64642026205b546573745d$' \
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
R4_GAME_CARD_MANAGER="$mock_game_card" \
R4_REPLAY_MANAGER="$mock_replay" \
R4_LOG_FILE="$TEMP_DIR/game-events.log" \
R4_MOCK_COMMANDS="$mock_commands" \
R4_MOCK_GAME_CARD_COMMANDS="$mock_game_card_commands" \
R4_MOCK_REPLAY_COMMANDS="$mock_replay_commands" \
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

R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_ECCTL="$mock_ecctl" \
R4_LED_STATE=/bin/true \
R4_GAME_CARD_MANAGER="$mock_game_card" \
R4_REPLAY_MANAGER="$mock_replay" \
R4_LOG_FILE="$TEMP_DIR/game-events.log" \
R4_MOCK_COMMANDS="$mock_commands" \
R4_MOCK_GAME_CARD_COMMANDS="$mock_game_card_commands" \
R4_MOCK_REPLAY_COMMANDS="$mock_replay_commands" \
    "$INTEGRATION_DIR/scripts/R4GameState" \
    gameStop \
    gba \
    libretro \
    mgba \
    "$mario_rom"

grep -q '^idle$' "$mock_game_card_commands"
grep -q '^stop$' "$mock_replay_commands"

R4_DATA_DIR="$TEMP_DIR/data" \
R4_RUNTIME_DIR="$TEMP_DIR/runtime" \
R4_LOG_FILE="$log_file" \
R4_ECCTL="$mock_ecctl" \
R4_LED_STATE=/bin/true \
R4_MOCK_COMMANDS="$mock_commands" \
R4_GAME_CARD_STATE_FILE="$TEMP_DIR/game-card-state" \
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
grep -q '^HOST CARD STATE=READY$' "$mock_commands"
grep -Eq '^HOST TELEMETRY TIME_HEX=[0-9a-f]{10} VOLUME=NA$' \
    "$mock_commands"
grep -q 'Invalid R4_OLED_TCP_ENABLED' "$log_file"

"$TEST_DIR/test-game-card.sh"
"$TEST_DIR/test-replay.sh"
"$TEST_DIR/test-firmware-update.sh"

echo "Batocera shell, Capture, Replay, Game Card, broker, TCP framing, heartbeat and event mocks passed"
