#!/bin/sh

set -eu

TEST_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
INTEGRATION_DIR="$(CDPATH= cd -- "$TEST_DIR/.." && pwd)"
TEMP_DIR="${TMPDIR:-/tmp}/r4-replay-tests-$$"

cleanup() {
    if [ -x "$INTEGRATION_DIR/bin/r4-replay" ] &&
       [ -f "$TEMP_DIR/data/replay.conf" ]; then
        "$INTEGRATION_DIR/bin/r4-replay" stop >/dev/null 2>&1 || true
    fi
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT HUP INT TERM

DATA="$TEMP_DIR/data"
STATE="$TEMP_DIR/state"
BUFFER="$TEMP_DIR/buffer"
INTERNAL_SCREENSHOTS="$TEMP_DIR/internal-screenshots"
INTERNAL_VIDEOS="$TEMP_DIR/internal-videos"
CARD_SCREENSHOTS="$TEMP_DIR/card-screenshots"
CARD_VIDEOS="$TEMP_DIR/card-videos"
CONFIG="$DATA/replay.conf"
COMMANDS="$TEMP_DIR/commands"
GAME_CARD_COMMANDS="$TEMP_DIR/game-card-commands"
FFMPEG_LOG="$TEMP_DIR/ffmpeg.log"
SELECTED_LOG="$TEMP_DIR/selected.log"
REPLAY_LOG="$TEMP_DIR/replay.log"
MOCK_FFMPEG="$TEMP_DIR/mock-ffmpeg"
MOCK_FFPROBE="$TEMP_DIR/mock-ffprobe"
MOCK_SCREENSHOT="$TEMP_DIR/mock-screenshot"
MOCK_ECCTL="$TEMP_DIR/mock-ecctl"
MOCK_GAME_CARD="$TEMP_DIR/mock-game-card"
MOCK_DF="$TEMP_DIR/mock-df"
REPLAY="$INTEGRATION_DIR/bin/r4-replay"

mkdir -p \
    "$DATA" \
    "$STATE" \
    "$INTERNAL_SCREENSHOTS" \
    "$INTERNAL_VIDEOS" \
    "$CARD_SCREENSHOTS" \
    "$CARD_VIDEOS"

write_config() {
    printf '%s\n' \
        'R4_REPLAY_ENABLED=1' \
        'R4_REPLAY_SECONDS=30' \
        'R4_REPLAY_SEGMENT_SECONDS=2' \
        "R4_REPLAY_BUFFER_DIR=$BUFFER" \
        'R4_REPLAY_MAX_BUFFER_MB=16' \
        'R4_REPLAY_FPS=30' \
        'R4_REPLAY_MAX_WIDTH=640' \
        'R4_REPLAY_VIDEO_BITRATE=1000000' \
        'R4_REPLAY_VIDEO_PRESET=ultrafast' \
        'R4_REPLAY_AUDIO_ENABLED=1' \
        'R4_REPLAY_DISABLED_SYSTEMS=dreamcast' \
        'R4_CAPTURE_STORAGE=auto' \
        'R4_CAPTURE_MIN_FREE_MB=16' \
        'R4_CAPTURE_FALLBACK_INTERNAL=1' \
        "R4_CAPTURE_INTERNAL_SCREENSHOTS=$INTERNAL_SCREENSHOTS" \
        "R4_CAPTURE_INTERNAL_VIDEOS=$INTERNAL_VIDEOS" \
        > "$CONFIG"
}

cat > "$MOCK_ECCTL" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_TEST_COMMANDS"
echo OK
EOF

cat > "$MOCK_SCREENSHOT" <<'EOF'
#!/bin/sh
printf 'P1\n1 1\n1\n' > "$1"
printf '%s\n' "$1" >> "$R4_TEST_SCREENSHOT_LOG"
EOF

cat > "$MOCK_GAME_CARD" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_TEST_GAME_CARD_COMMANDS"
case "$1:$2" in
    capture-dir:screenshots)
        [ "$R4_TEST_CARD_AVAILABLE" = "1" ] || exit 1
        echo "$R4_TEST_CARD_SCREENSHOTS"
        ;;
    capture-dir:videos)
        [ "$R4_TEST_CARD_AVAILABLE" = "1" ] || exit 1
        echo "$R4_TEST_CARD_VIDEOS"
        ;;
esac
EOF

cat > "$MOCK_DF" <<'EOF'
#!/bin/sh
path="$2"
available="$R4_TEST_INTERNAL_FREE_MB"
case "$path" in
    "$R4_TEST_CARD_SCREENSHOTS"|"$R4_TEST_CARD_VIDEOS")
        available="$R4_TEST_CARD_FREE_MB"
        ;;
esac
printf '%s\n' \
    'Filesystem 1048576-blocks Used Available Capacity Mounted on' \
    "mock 1000 1 $available 1% $path"
EOF

cat > "$MOCK_FFMPEG" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$R4_TEST_FFMPEG_LOG"

concat=0
previous=""
concat_list=""
output=""
for argument in "$@"; do
    if [ "$previous" = "-f" ] &&
       [ "$argument" = "concat" ]; then
        concat=1
    fi
    if [ "$previous" = "-i" ] &&
       [ "$concat" -eq 1 ]; then
        concat_list="$argument"
    fi
    output="$argument"
    previous="$argument"
done

if [ "$concat" -eq 1 ]; then
    if [ -f "$R4_TEST_MUX_DELAY" ]; then
        sleep 3
    fi
    [ ! -f "$R4_TEST_MUX_FAIL" ] || exit 1
    {
        echo "---"
        cat "$concat_list"
    } >> "$R4_TEST_SELECTED_LOG"
    printf 'mock mp4 with video and audio\n' > "$output"
    exit 0
fi

[ ! -f "$R4_TEST_ENCODER_FAIL" ] || exit 1

segment_count="${R4_TEST_SEGMENT_COUNT:-17}"
segment_index=0
while [ "$segment_index" -lt "$segment_count" ]; do
    segment_path="$(printf "$output" "$segment_index")"
    mkdir -p "$(dirname "$segment_path")"
    printf 'segment-%s\n' "$segment_index" > "$segment_path"
    segment_index=$((segment_index + 1))
    sleep 0.03
done

trap 'exit 0' TERM INT
while true; do
    sleep 1
done
EOF

cat > "$MOCK_FFPROBE" <<'EOF'
#!/bin/sh
case "$*" in
    *"-select_streams v:0"*)
        [ ! -f "$R4_TEST_PROBE_VIDEO_FAIL" ] && echo video
        ;;
    *"-select_streams a:0"*)
        [ ! -f "$R4_TEST_PROBE_AUDIO_FAIL" ] && echo audio
        ;;
esac
EOF

chmod +x \
    "$MOCK_ECCTL" \
    "$MOCK_SCREENSHOT" \
    "$MOCK_GAME_CARD" \
    "$MOCK_DF" \
    "$MOCK_FFMPEG" \
    "$MOCK_FFPROBE"

write_config
: > "$COMMANDS"
: > "$GAME_CARD_COMMANDS"
: > "$FFMPEG_LOG"
: > "$SELECTED_LOG"
: > "$TEMP_DIR/screenshots.log"

export R4_DATA_DIR="$DATA"
export R4_REPLAY_STATE_DIR="$STATE"
export R4_REPLAY_CONFIG="$CONFIG"
export R4_ECCTL="$MOCK_ECCTL"
export R4_GAME_CARD_MANAGER="$MOCK_GAME_CARD"
export R4_FFMPEG="$MOCK_FFMPEG"
export R4_FFPROBE="$MOCK_FFPROBE"
export R4_BATOCERA_SCREENSHOT="$MOCK_SCREENSHOT"
export R4_DF="$MOCK_DF"
export R4_REPLAY_LOG="$REPLAY_LOG"
export R4_REPLAY_TEST_BACKEND=1
export R4_TEST_COMMANDS="$COMMANDS"
export R4_TEST_GAME_CARD_COMMANDS="$GAME_CARD_COMMANDS"
export R4_TEST_FFMPEG_LOG="$FFMPEG_LOG"
export R4_TEST_SELECTED_LOG="$SELECTED_LOG"
export R4_TEST_SCREENSHOT_LOG="$TEMP_DIR/screenshots.log"
export R4_TEST_CARD_SCREENSHOTS="$CARD_SCREENSHOTS"
export R4_TEST_CARD_VIDEOS="$CARD_VIDEOS"
export R4_TEST_CARD_AVAILABLE=0
export R4_TEST_CARD_FREE_MB=1000
export R4_TEST_INTERNAL_FREE_MB=1000
export R4_TEST_ENCODER_FAIL="$TEMP_DIR/encoder-fail"
export R4_TEST_MUX_FAIL="$TEMP_DIR/mux-fail"
export R4_TEST_MUX_DELAY="$TEMP_DIR/mux-delay"
export R4_TEST_PROBE_VIDEO_FAIL="$TEMP_DIR/probe-video-fail"
export R4_TEST_PROBE_AUDIO_FAIL="$TEMP_DIR/probe-audio-fail"
export R4_TEST_SEGMENT_COUNT=17

wait_for() {
    wait_attempt=0
    while ! sh -c "$1"; do
        wait_attempt=$((wait_attempt + 1))
        if [ "$wait_attempt" -ge 100 ]; then
            echo "timeout waiting for: $1" >&2
            return 1
        fi
        sleep 0.1
    done
}

"$REPLAY" cleanup
"$REPLAY" start psx '/userdata/roms/psx/Quake II (USA).cue' \
    'Quake II / Unsafe:*?'
"$REPLAY" status | grep -q '^STATE=BUFFERING '
grep -q '^HOST CAPTURE TYPE=CLIP STATUS=BUFFERING$' "$COMMANDS"

segment_total="$(
    find "$BUFFER/current" -name 'segment-*.ts' -type f |
        wc -l
)"
[ "$segment_total" -le 17 ]

save_started="$(date +%s)"
[ "$("$REPLAY" save SEQ30)" = ACCEPTED ]
wait_for "find '$INTERNAL_VIDEOS' -name '*.mp4' -type f | grep -q ."
save_elapsed=$(($(date +%s) - save_started))
[ "$save_elapsed" -lt 10 ]
clip_path="$(find "$INTERNAL_VIDEOS" -name '*.mp4' -type f | head -n 1)"
printf '%s\n' "$clip_path" |
    grep -q 'psx-Quake_II_Unsafe-clip-SEQ30.mp4$'

selected_count="$(
    tail -n 15 "$SELECTED_LOG" |
        grep -c "^file '"
)"
[ "$selected_count" -eq 15 ]
if tail -n 15 "$SELECTED_LOG" | grep -q '/current/'; then
    echo "saved clip used future/current segments" >&2
    exit 1
fi
grep -q '^HOST CAPTURE TYPE=CLIP STATUS=SAVED$' "$COMMANDS"
grep -q '^LED FLASH 0 32 0 700$' "$COMMANDS"
grep -q 'clip result=SAVED .*backend=ffmpeg-kms-pulse system=psx' \
    "$REPLAY_LOG"

"$REPLAY" stop
R4_TEST_SEGMENT_COUNT=3
export R4_TEST_SEGMENT_COUNT
"$REPLAY" start nes short.rom 'Short Game'
[ "$("$REPLAY" save SHORT)" = ACCEPTED ]
wait_for "grep -q 'clip result=SAVED seq=SHORT' '$REPLAY_LOG'"
short_selected="$(
    awk '
        /^---$/ { count=0; next }
        /^file / { count++ }
        END { print count }
    ' "$SELECTED_LOG"
)"
[ "$short_selected" -eq 3 ]

"$REPLAY" stop
R4_TEST_SEGMENT_COUNT=17
export R4_TEST_SEGMENT_COUNT
"$REPLAY" start psx game.cue 'Concurrent'
: > "$R4_TEST_MUX_DELAY"
[ "$("$REPLAY" save FIRST)" = ACCEPTED ]
[ "$("$REPLAY" save SECOND 2>/dev/null || true)" = BUSY ]
wait_for "grep -q 'clip result=SAVED seq=FIRST' '$REPLAY_LOG'"
rm -f "$R4_TEST_MUX_DELAY"

"$REPLAY" stop
if "$REPLAY" start dreamcast game.chd Dreamcast >/dev/null 2>&1; then
    echo "disabled Dreamcast replay unexpectedly started" >&2
    exit 1
fi
"$REPLAY" status | grep -q '^STATE=UNAVAILABLE '

R4_FFMPEG=/missing-ffmpeg
export R4_FFMPEG
if "$REPLAY" start psx game.cue Missing >/dev/null 2>&1; then
    echo "missing backend unexpectedly started" >&2
    exit 1
fi
"$REPLAY" status | grep -q '^STATE=UNAVAILABLE '
R4_FFMPEG="$MOCK_FFMPEG"
export R4_FFMPEG

: > "$R4_TEST_ENCODER_FAIL"
if "$REPLAY" start psx game.cue EncoderFail >/dev/null 2>&1; then
    echo "encoder failure unexpectedly started" >&2
    exit 1
fi
rm -f "$R4_TEST_ENCODER_FAIL"

"$REPLAY" start psx game.cue MuxFail
: > "$R4_TEST_MUX_FAIL"
[ "$("$REPLAY" save MUXFAIL)" = ACCEPTED ]
wait_for "grep -q 'clip result=ERROR seq=MUXFAIL' '$REPLAY_LOG'"
grep -q '^HOST CAPTURE TYPE=CLIP STATUS=ERROR$' "$COMMANDS"
rm -f "$R4_TEST_MUX_FAIL"

"$REPLAY" stop
"$REPLAY" start psx game.cue ProbeFail
: > "$R4_TEST_PROBE_AUDIO_FAIL"
[ "$("$REPLAY" save PROBEFAIL)" = ACCEPTED ]
wait_for "grep -q 'clip result=ERROR seq=PROBEFAIL' '$REPLAY_LOG'"
if find "$INTERNAL_VIDEOS" -name '*PROBEFAIL.mp4' -type f |
   grep -q .; then
    echo "clip with a missing audio stream was published" >&2
    exit 1
fi
rm -f "$R4_TEST_PROBE_AUDIO_FAIL"

"$REPLAY" stop
"$REPLAY" start psx game.cue NoSpace
R4_TEST_INTERNAL_FREE_MB=1
R4_TEST_CARD_AVAILABLE=0
export R4_TEST_INTERNAL_FREE_MB R4_TEST_CARD_AVAILABLE
[ "$("$REPLAY" save NOSPACE)" = ACCEPTED ]
wait_for "grep -q 'clip result=ERROR seq=NOSPACE reason=insufficient-space-or-storage' '$REPLAY_LOG'"
R4_TEST_INTERNAL_FREE_MB=1000
export R4_TEST_INTERNAL_FREE_MB

"$REPLAY" stop
R4_TEST_CARD_AVAILABLE=1
export R4_TEST_CARD_AVAILABLE
"$REPLAY" start psx game.cue CardSave
[ "$("$REPLAY" save CARDOK)" = ACCEPTED ]
wait_for "find '$CARD_VIDEOS' -name '*CARDOK.mp4' -type f | grep -q ."
grep -q '^busy-add capture-clip$' "$GAME_CARD_COMMANDS"
grep -q '^busy-remove capture-clip$' "$GAME_CARD_COMMANDS"

"$REPLAY" stop
"$REPLAY" start psx game.cue CardRemoved
: > "$R4_TEST_MUX_FAIL"
[ "$("$REPLAY" save CARDFAIL)" = ACCEPTED ]
wait_for "grep -q 'operation-error capture-clip clip-write-failed' '$GAME_CARD_COMMANDS'"
rm -f "$R4_TEST_MUX_FAIL"

R4_TEST_CARD_AVAILABLE=0
export R4_TEST_CARD_AVAILABLE
"$REPLAY" screenshot SHORT1
[ "$(wc -l < "$R4_TEST_SCREENSHOT_LOG")" -eq 1 ]
screenshot_path="$(
    find "$INTERNAL_SCREENSHOTS" -name '*.pbm' -type f |
        head -n 1
)"
printf '%s\n' "$screenshot_path" |
    grep -q 'psx-CardRemoved-screenshot-SHORT1.pbm$'

R4_TEST_CARD_AVAILABLE=1
export R4_TEST_CARD_AVAILABLE
"$REPLAY" screenshot SHORT2
find "$CARD_SCREENSHOTS" -name '*SHORT2.pbm' -type f |
    grep -q .
grep -q '^busy-add capture-screenshot$' "$GAME_CARD_COMMANDS"
grep -q '^busy-remove capture-screenshot$' "$GAME_CARD_COMMANDS"

"$REPLAY" stop
mkdir -p "$BUFFER/stale"
printf stale > "$BUFFER/stale/segment.ts"
"$REPLAY" cleanup
[ ! -e "$BUFFER/stale/segment.ts" ]

printf '%s\n' 'R4_REPLAY_ENABLED=invalid' > "$CONFIG"
if "$REPLAY" status >/dev/null 2>&1; then
    echo "invalid replay configuration was accepted" >&2
    exit 1
fi

echo "R4 replay ring, save, routing and failure tests passed"
