#!/bin/bash
#
# Build and run CoffeeShop in Cemu, capturing all available logs.
#
# Usage:
#   ./test_in_cemu.sh              # build, run for 15s, dump logs
#   ./test_in_cemu.sh 30           # run for 30 seconds before killing cemu
#   ./test_in_cemu.sh skip-build   # don't rebuild, just rerun
#   ./test_in_cemu.sh skip-build 30
#   ./test_in_cemu.sh tail         # don't run cemu, just dump latest captured log
#
# Output: ./logs/cemu_run_<timestamp>.log with everything concatenated:
#   - udplogserver capture (WHBLog* output via UDP)
#   - cemu stdout/stderr (might contain app prints, SDL errors)
#   - app's early.log + app.log (from sd:/wiiu/apps/coffeeshop/)
#   - cemu's own log.txt

set -e
cd "$(dirname "$0")"

ROOT="$(pwd)"
LOGS_DIR="$ROOT/logs"
CEMU_DATA="$HOME/.local/share/Cemu"
APP_LOG_DIR="$CEMU_DATA/sdcard/wiiu/apps/coffeeshop"
CEMU_LOG="$CEMU_DATA/log.txt"
BUILD_DIR="$ROOT/build"
UDPLOG_BIN="/opt/devkitpro/tools/bin/udplogserver"

mkdir -p "$LOGS_DIR"

# --- "tail" mode: just print the latest run log ---
if [ "$1" = "tail" ]; then
    LATEST=$(ls -t "$LOGS_DIR"/cemu_run_*.log 2>/dev/null | head -1)
    if [ -z "$LATEST" ]; then
        echo "No previous run log found in $LOGS_DIR"
        exit 1
    fi
    echo "Latest run: $LATEST"
    echo "================================================================"
    cat "$LATEST"
    exit 0
fi

# --- Parse args ---
SKIP_BUILD=0
WAIT_SEC=15
for arg in "$@"; do
    case "$arg" in
        skip-build) SKIP_BUILD=1 ;;
        ''|*[!0-9]*) ;; # not a number, ignore
        *) WAIT_SEC="$arg" ;;
    esac
done

# --- Build ---
if [ "$SKIP_BUILD" = "0" ]; then
    echo "=== Building wuhb (cemu mode) ==="
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/wut/share/wut.toolchain.cmake \
             -DBUILD_MODE=cemu 2>/dev/null || true
    make -j$(nproc) 2>&1 | tail -5
    cd "$ROOT"
    if [ ! -f "$BUILD_DIR/wiiu_mod_store.wuhb" ]; then
        echo "ERROR: build did not produce wuhb"
        exit 1
    fi
fi

# --- Clear old logs so we capture only this run ---
echo "=== Clearing old logs ==="
mkdir -p "$APP_LOG_DIR"
: > "$APP_LOG_DIR/early.log"
: > "$APP_LOG_DIR/app.log"
: > "$CEMU_LOG"

# --- Set up capture targets ---
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUT="$LOGS_DIR/cemu_run_$TIMESTAMP.log"
CEMU_STDOUT="$LOGS_DIR/.cemu_stdout_$TIMESTAMP.tmp"
UDP_STDOUT="$LOGS_DIR/.udp_$TIMESTAMP.tmp"

# --- Start udplogserver (captures WHBLog* output from the app) ---
UDP_PID=""
if [ -x "$UDPLOG_BIN" ]; then
    "$UDPLOG_BIN" > "$UDP_STDOUT" 2>&1 &
    UDP_PID=$!
    sleep 0.3
fi

# --- Launch cemu, capturing stdout+stderr ---
echo "=== Launching Cemu (will run for ${WAIT_SEC}s) ==="
cemu -g "$BUILD_DIR/wiiu_mod_store.wuhb" > "$CEMU_STDOUT" 2>&1 &
CEMU_PID=$!

# Wait, but check if cemu died early
for i in $(seq 1 "$WAIT_SEC"); do
    if ! kill -0 "$CEMU_PID" 2>/dev/null; then
        echo "Cemu exited early after ${i}s"
        break
    fi
    sleep 1
done

# --- Stop cemu cleanly if still running ---
if kill -0 "$CEMU_PID" 2>/dev/null; then
    echo "=== Stopping Cemu ==="
    kill "$CEMU_PID" 2>/dev/null || true
    sleep 1
    kill -9 "$CEMU_PID" 2>/dev/null || true
fi
wait "$CEMU_PID" 2>/dev/null || true

# --- Stop udplogserver ---
if [ -n "$UDP_PID" ]; then
    sleep 0.5  # let pending packets flush
    kill "$UDP_PID" 2>/dev/null || true
    wait "$UDP_PID" 2>/dev/null || true
fi

# --- Dump everything into one timestamped file ---
{
    echo "================================================================"
    echo "CoffeeShop run @ $(date)"
    echo "wuhb: $BUILD_DIR/wiiu_mod_store.wuhb"
    echo "ran for: ${WAIT_SEC}s"
    echo "================================================================"
    echo ""
    echo "--------------------- udplogserver (WHBLog*) -------------------"
    if [ -s "$UDP_STDOUT" ]; then
        cat "$UDP_STDOUT"
    else
        echo "(empty -- WHBLogUdpInit/WHBLogPrintf output not captured;"
        echo " app may not have reached WHBLogUdpInit, or UDP path differs)"
    fi
    echo ""
    echo "--------------------- cemu stdout/stderr -----------------------"
    if [ -s "$CEMU_STDOUT" ]; then
        cat "$CEMU_STDOUT"
    else
        echo "(empty)"
    fi
    echo ""
    echo "--------------------- app early.log ----------------------------"
    if [ -s "$APP_LOG_DIR/early.log" ]; then
        cat "$APP_LOG_DIR/early.log"
    else
        echo "(empty -- app didn't reach WHBProcInit/elog or fopen failed)"
    fi
    echo ""
    echo "--------------------- app app.log ------------------------------"
    if [ -s "$APP_LOG_DIR/app.log" ]; then
        cat "$APP_LOG_DIR/app.log"
    else
        echo "(empty -- Logger::init never ran or wrote)"
    fi
    echo ""
    echo "--------------------- cemu log.txt (last 100 lines) ------------"
    tail -100 "$CEMU_LOG"
} > "$OUT"

echo ""
echo "=== Run complete ==="
echo "Full log: $OUT"
echo ""
echo "Summary:"
echo "  udp:          $(wc -l < "$UDP_STDOUT" 2>/dev/null || echo 0) lines (WHBLog*)"
echo "  cemu stdout:  $(wc -l < "$CEMU_STDOUT" 2>/dev/null || echo 0) lines"
echo "  early.log:    $(wc -l < "$APP_LOG_DIR/early.log" 2>/dev/null || echo 0) lines"
echo "  app.log:      $(wc -l < "$APP_LOG_DIR/app.log" 2>/dev/null || echo 0) lines"
echo "  cemu log.txt: $(wc -l < "$CEMU_LOG" 2>/dev/null || echo 0) lines"
echo ""
echo "View:    ./test_in_cemu.sh tail"

# Clean up tmp files
rm -f "$CEMU_STDOUT" "$UDP_STDOUT"
