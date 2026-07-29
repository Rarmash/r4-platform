#!/bin/sh

R4_DIR="${R4_INSTALL_R4_DIR:-/userdata/system/r4}"
SERVICE_DIR="${R4_INSTALL_SERVICE_DIR:-/userdata/system/services}"
SCRIPT_DIR="${R4_INSTALL_SCRIPT_DIR:-/userdata/system/scripts}"
ACHIEVEMENT_DIR="${R4_INSTALL_ACHIEVEMENT_DIR:-/userdata/system/configs/emulationstation/scripts/achievements}"
GAME_START_DIR="${R4_INSTALL_GAME_START_DIR:-/userdata/system/configs/emulationstation/scripts/game-start}"
GAME_SELECTED_DIR="${R4_INSTALL_GAME_SELECTED_DIR:-/userdata/system/configs/emulationstation/scripts/game-selected}"
BATOCERA_SERVICES="${R4_BATOCERA_SERVICES:-batocera-services}"
REPLAY_STATE_DIR="${R4_REPLAY_STATE_DIR:-/tmp/r4-replay-state}"
REPLAY_BUFFER_DIR="${R4_REPLAY_BUFFER_DIR:-/tmp/r4-replay}"

remove_runtime_tree() {
    case "$1" in
        ''|/|/tmp|/userdata|/userdata/system)
            echo "Refusing unsafe runtime cleanup path: $1" >&2
            return 1
            ;;
        /*)
            rm -rf "$1"
            ;;
        *)
            echo "Refusing non-absolute runtime cleanup path: $1" >&2
            return 1
            ;;
    esac
}

for service_name in R4GameCard R4Controller; do
    "$BATOCERA_SERVICES" stop "$service_name" >/dev/null 2>&1 || true
    "$BATOCERA_SERVICES" disable "$service_name" >/dev/null 2>&1 || true
done

if [ -x "$R4_DIR/r4-replay" ]; then
    R4_DATA_DIR="$R4_DIR" \
    R4_REPLAY_STATE_DIR="$REPLAY_STATE_DIR" \
        "$R4_DIR/r4-replay" stop >/dev/null 2>&1 || true
fi

rm -f \
    "$R4_DIR/r4-ecctl" \
    "$R4_DIR/r4-firmware-update" \
    "$R4_DIR/r4-led-state" \
    "$R4_DIR/r4-game-title" \
    "$R4_DIR/r4-oled-tcp" \
    "$R4_DIR/r4-game-card" \
    "$R4_DIR/r4-replay" \
    "$R4_DIR/firmware-version.conf" \
    "$R4_DIR/integration-version.conf" \
    "$SERVICE_DIR/R4Controller" \
    "$SERVICE_DIR/R4GameCard" \
    "$ACHIEVEMENT_DIR/R4Achievement" \
    "$GAME_START_DIR/R4GameMetadata" \
    "$GAME_SELECTED_DIR/R4GameMetadata"

for game_hook in "$SCRIPT_DIR"/R4GameState*; do
    [ -e "$game_hook" ] || continue
    rm -f "$game_hook"
done

remove_runtime_tree "$REPLAY_STATE_DIR" || true
remove_runtime_tree "$REPLAY_BUFFER_DIR" || true

echo "R4 Batocera integration removed."
echo "Configuration, logs and captures remain in $R4_DIR."
