#!/bin/sh

SOURCE_DIR="$(
    cd "$(dirname "$0")" 2>/dev/null &&
    pwd
)"

R4_DIR="${R4_INSTALL_R4_DIR:-/userdata/system/r4}"
SERVICE_DIR="${R4_INSTALL_SERVICE_DIR:-/userdata/system/services}"
SCRIPT_DIR="${R4_INSTALL_SCRIPT_DIR:-/userdata/system/scripts}"

ACHIEVEMENT_DIR="${R4_INSTALL_ACHIEVEMENT_DIR:-/userdata/system/configs/emulationstation/scripts/achievements}"
GAME_START_DIR="${R4_INSTALL_GAME_START_DIR:-/userdata/system/configs/emulationstation/scripts/game-start}"
GAME_SELECTED_DIR="${R4_INSTALL_GAME_SELECTED_DIR:-/userdata/system/configs/emulationstation/scripts/game-selected}"
BATOCERA_SERVICES="${R4_BATOCERA_SERVICES:-batocera-services}"
SETTLE_SECONDS="${R4_INSTALL_SETTLE_SECONDS:-3}"

SERVICE_NAME="R4Controller"
GAME_CARD_SERVICE_NAME="R4GameCard"
VERSION_CONFIG_NAME="firmware-version.conf"
INTEGRATION_VERSION_CONFIG_NAME="integration-version.conf"
OLED_TCP_CONFIG_NAME="oled-tcp.conf"
GAME_CARD_CONFIG_NAME="game-card.conf"
REPLAY_CONFIG_NAME="replay.conf"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

require_source_file() {
    [ -f "$1" ] ||
        fail "Required source file is missing: $1"
}

load_firmware_version() {
    version_config_file="$1"

    [ -f "$version_config_file" ] ||
        fail "Firmware version configuration is missing: $version_config_file"

    version_config_line_count="$(wc -l < "$version_config_file" 2>/dev/null)" ||
        fail "Unable to read firmware version configuration: $version_config_file"

    [ "$version_config_line_count" -eq 1 ] 2>/dev/null ||
        fail "Invalid firmware version configuration: $version_config_file"

    version_config_line="$(cat "$version_config_file" 2>/dev/null)" ||
        fail "Unable to read firmware version configuration: $version_config_file"

    printf '%s\n' "$version_config_line" |
        grep -Eq '^R4_FIRMWARE_VERSION=[0-9]+\.[0-9]+\.[0-9]+(-dev)?$' ||
        fail "Invalid firmware version configuration: $version_config_file"

    R4_FIRMWARE_VERSION="${version_config_line#R4_FIRMWARE_VERSION=}"
}

VERSION_CONFIG_SOURCE="$SOURCE_DIR/$VERSION_CONFIG_NAME"
INTEGRATION_VERSION_CONFIG_SOURCE="$SOURCE_DIR/$INTEGRATION_VERSION_CONFIG_NAME"

load_firmware_version "$VERSION_CONFIG_SOURCE"
EXPECTED_VERSION="R4_CONTROLLER_FW $R4_FIRMWARE_VERSION"

require_source_file "$INTEGRATION_VERSION_CONFIG_SOURCE"
integration_version_line="$(cat "$INTEGRATION_VERSION_CONFIG_SOURCE" 2>/dev/null)" ||
    fail "Unable to read integration version configuration"
printf '%s\n' "$integration_version_line" |
    grep -Eq '^R4_BATOCERA_VERSION=[0-9]+\.[0-9]+\.[0-9]+$' ||
    fail "Invalid integration version configuration"
R4_BATOCERA_VERSION="${integration_version_line#R4_BATOCERA_VERSION=}"

installed_integration_version=""
if [ -f "$R4_DIR/$INTEGRATION_VERSION_CONFIG_NAME" ]; then
    installed_integration_version="$(
        sed -n 's/^R4_BATOCERA_VERSION=//p' \
            "$R4_DIR/$INTEGRATION_VERSION_CONFIG_NAME" 2>/dev/null
    )"
fi

echo "Installing R4 Batocera integration $R4_BATOCERA_VERSION..."

require_source_file \
    "$SOURCE_DIR/bin/r4-ecctl"

require_source_file \
    "$SOURCE_DIR/bin/r4-firmware-update"

require_source_file \
    "$SOURCE_DIR/bin/r4-led-state"

require_source_file \
    "$SOURCE_DIR/bin/r4-game-title"

require_source_file \
    "$SOURCE_DIR/bin/r4-oled-tcp"

require_source_file \
    "$SOURCE_DIR/bin/r4-game-card"

require_source_file \
    "$SOURCE_DIR/bin/r4-replay"

require_source_file \
    "$SOURCE_DIR/services/R4Controller"

require_source_file \
    "$SOURCE_DIR/services/R4GameCard"

require_source_file \
    "$SOURCE_DIR/scripts/R4GameState"

require_source_file \
    "$SOURCE_DIR/emulationstation/achievements/R4Achievement"

require_source_file \
    "$SOURCE_DIR/emulationstation/game-start/R4GameMetadata"

require_source_file \
    "$SOURCE_DIR/$OLED_TCP_CONFIG_NAME"

require_source_file \
    "$SOURCE_DIR/$GAME_CARD_CONFIG_NAME"

require_source_file \
    "$SOURCE_DIR/$REPLAY_CONFIG_NAME"

if [ -x "$R4_DIR/r4-replay" ]; then
    "$R4_DIR/r4-replay" stop >/dev/null 2>&1 || true
fi

"$BATOCERA_SERVICES" stop "$GAME_CARD_SERVICE_NAME" \
    >/dev/null 2>&1 ||
    true

"$BATOCERA_SERVICES" stop "$SERVICE_NAME" \
    >/dev/null 2>&1 ||
    true

mkdir -p "$R4_DIR" ||
    fail "Unable to create $R4_DIR"

mkdir -p "$SERVICE_DIR" ||
    fail "Unable to create $SERVICE_DIR"

mkdir -p "$SCRIPT_DIR" ||
    fail "Unable to create $SCRIPT_DIR"

mkdir -p "$ACHIEVEMENT_DIR" ||
    fail "Unable to create $ACHIEVEMENT_DIR"

mkdir -p "$GAME_START_DIR" ||
    fail "Unable to create $GAME_START_DIR"

mkdir -p "$GAME_SELECTED_DIR" ||
    fail "Unable to create $GAME_SELECTED_DIR"

rm -f "$SCRIPT_DIR/R4GameState.disabled"
for stale_game_hook in "$SCRIPT_DIR"/R4GameState.*; do
    [ -e "$stale_game_hook" ] || continue
    chmod -x "$stale_game_hook" 2>/dev/null || true
done
if [ -e "$R4_DIR/R4GameState.disabled" ]; then
    chmod -x "$R4_DIR/R4GameState.disabled" 2>/dev/null || true
fi

cp \
    "$SOURCE_DIR/bin/r4-ecctl" \
    "$R4_DIR/r4-ecctl" ||
    fail "Unable to install r4-ecctl"

cp \
    "$SOURCE_DIR/bin/r4-firmware-update" \
    "$R4_DIR/r4-firmware-update" ||
    fail "Unable to install r4-firmware-update"

cp \
    "$SOURCE_DIR/bin/r4-led-state" \
    "$R4_DIR/r4-led-state" ||
    fail "Unable to install r4-led-state"

cp \
    "$SOURCE_DIR/bin/r4-game-title" \
    "$R4_DIR/r4-game-title" ||
    fail "Unable to install r4-game-title"

cp \
    "$SOURCE_DIR/bin/r4-oled-tcp" \
    "$R4_DIR/r4-oled-tcp" ||
    fail "Unable to install r4-oled-tcp"

cp \
    "$SOURCE_DIR/bin/r4-game-card" \
    "$R4_DIR/r4-game-card" ||
    fail "Unable to install r4-game-card"

cp \
    "$SOURCE_DIR/bin/r4-replay" \
    "$R4_DIR/r4-replay" ||
    fail "Unable to install r4-replay"

cp \
    "$VERSION_CONFIG_SOURCE" \
    "$R4_DIR/$VERSION_CONFIG_NAME" ||
    fail "Unable to install $VERSION_CONFIG_NAME"

if [ ! -f "$R4_DIR/$OLED_TCP_CONFIG_NAME" ]; then
    cp \
        "$SOURCE_DIR/$OLED_TCP_CONFIG_NAME" \
        "$R4_DIR/$OLED_TCP_CONFIG_NAME" ||
        fail "Unable to install $OLED_TCP_CONFIG_NAME"
fi

if [ ! -f "$R4_DIR/$GAME_CARD_CONFIG_NAME" ]; then
    cp \
        "$SOURCE_DIR/$GAME_CARD_CONFIG_NAME" \
        "$R4_DIR/$GAME_CARD_CONFIG_NAME" ||
        fail "Unable to install $GAME_CARD_CONFIG_NAME"
fi

if [ ! -f "$R4_DIR/$REPLAY_CONFIG_NAME" ]; then
    cp \
        "$SOURCE_DIR/$REPLAY_CONFIG_NAME" \
        "$R4_DIR/$REPLAY_CONFIG_NAME" ||
        fail "Unable to install $REPLAY_CONFIG_NAME"
fi

# 0.12.0 changes replay from a normal default-on feature to an unsupported,
# explicit developer experiment. Migrate older installations once, while
# preserving a deliberate opt-in on subsequent 0.12.0 reinstalls.
if [ -z "$installed_integration_version" ]; then
    replay_config_tmp="$R4_DIR/$REPLAY_CONFIG_NAME.tmp.$$"
    sed 's/^R4_REPLAY_ENABLED=.*/R4_REPLAY_ENABLED=0/' \
        "$R4_DIR/$REPLAY_CONFIG_NAME" > "$replay_config_tmp" ||
        fail "Unable to disable replay during the 0.12.0 migration"
    mv "$replay_config_tmp" "$R4_DIR/$REPLAY_CONFIG_NAME" ||
        fail "Unable to install migrated replay configuration"
    echo \
        "Instant Replay is disabled: Orange Pi 3 LTS software encoding is unsupported."
fi

cp \
    "$INTEGRATION_VERSION_CONFIG_SOURCE" \
    "$R4_DIR/$INTEGRATION_VERSION_CONFIG_NAME" ||
    fail "Unable to install $INTEGRATION_VERSION_CONFIG_NAME"

cp \
    "$SOURCE_DIR/services/R4Controller" \
    "$SERVICE_DIR/R4Controller" ||
    fail "Unable to install R4Controller service"

cp \
    "$SOURCE_DIR/services/R4GameCard" \
    "$SERVICE_DIR/R4GameCard" ||
    fail "Unable to install R4GameCard service"

cp \
    "$SOURCE_DIR/scripts/R4GameState" \
    "$SCRIPT_DIR/R4GameState" ||
    fail "Unable to install R4GameState"

cp \
    "$SOURCE_DIR/emulationstation/achievements/R4Achievement" \
    "$ACHIEVEMENT_DIR/R4Achievement" ||
    fail "Unable to install R4Achievement"

cp \
    "$SOURCE_DIR/emulationstation/game-start/R4GameMetadata" \
    "$GAME_START_DIR/R4GameMetadata" ||
    fail "Unable to install R4GameMetadata"

cp \
    "$SOURCE_DIR/emulationstation/game-start/R4GameMetadata" \
    "$GAME_SELECTED_DIR/R4GameMetadata" ||
    fail "Unable to install game-selected R4GameMetadata"

chmod +x "$R4_DIR/r4-ecctl"
chmod +x "$R4_DIR/r4-firmware-update"
chmod +x "$R4_DIR/r4-led-state"
chmod +x "$R4_DIR/r4-game-title"
chmod +x "$R4_DIR/r4-oled-tcp"
chmod +x "$R4_DIR/r4-game-card"
chmod +x "$R4_DIR/r4-replay"
chmod +x "$SERVICE_DIR/R4Controller"
chmod +x "$SERVICE_DIR/R4GameCard"
chmod +x "$SCRIPT_DIR/R4GameState"
chmod +x "$ACHIEVEMENT_DIR/R4Achievement"
chmod +x "$GAME_START_DIR/R4GameMetadata"
chmod +x "$GAME_SELECTED_DIR/R4GameMetadata"

"$R4_DIR/r4-replay" cleanup ||
    fail "Unable to clean stale replay buffer"

"$BATOCERA_SERVICES" enable "$SERVICE_NAME" ||
    fail "Unable to enable $SERVICE_NAME"

"$BATOCERA_SERVICES" enable "$GAME_CARD_SERVICE_NAME" ||
    fail "Unable to enable $GAME_CARD_SERVICE_NAME"

"$BATOCERA_SERVICES" start "$SERVICE_NAME" ||
    fail "Unable to start $SERVICE_NAME"

"$BATOCERA_SERVICES" start "$GAME_CARD_SERVICE_NAME" ||
    fail "Unable to start $GAME_CARD_SERVICE_NAME"

[ "$SETTLE_SECONDS" -eq 0 ] 2>/dev/null ||
    sleep "$SETTLE_SECONDS"

echo
echo "Installed files:"
echo "  $R4_DIR/r4-ecctl"
echo "  $R4_DIR/r4-firmware-update"
echo "  $R4_DIR/r4-led-state"
echo "  $R4_DIR/r4-game-title"
echo "  $R4_DIR/r4-oled-tcp"
echo "  $R4_DIR/r4-game-card"
echo "  $R4_DIR/r4-replay"
echo "  $R4_DIR/$VERSION_CONFIG_NAME"
echo "  $R4_DIR/$INTEGRATION_VERSION_CONFIG_NAME"
echo "  $R4_DIR/$OLED_TCP_CONFIG_NAME"
echo "  $R4_DIR/$GAME_CARD_CONFIG_NAME"
echo "  $R4_DIR/$REPLAY_CONFIG_NAME"
echo "  $SERVICE_DIR/R4Controller"
echo "  $SERVICE_DIR/R4GameCard"
echo "  $SCRIPT_DIR/R4GameState"
echo "  $ACHIEVEMENT_DIR/R4Achievement"
echo "  $GAME_START_DIR/R4GameMetadata"
echo "  $GAME_SELECTED_DIR/R4GameMetadata"
echo

firmware_version="$(
    "$R4_DIR/r4-ecctl" VERSION 2>/dev/null
)"

if [ "$firmware_version" = "$EXPECTED_VERSION" ]; then
    echo "Embedded controller: $firmware_version"
else
    echo \
        "WARNING: expected '$EXPECTED_VERSION'" \
        >&2

    if [ -n "$firmware_version" ]; then
        echo \
            "WARNING: connected controller reports '$firmware_version'" \
            >&2
    else
        echo \
            "WARNING: embedded controller is unavailable" \
            >&2
    fi
fi

echo

"$SERVICE_DIR/R4Controller" status ||
    true

echo
"$SERVICE_DIR/R4GameCard" status ||
    true

echo
echo "R4 Batocera integration $R4_BATOCERA_VERSION installed successfully."
