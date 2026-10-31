#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Melodee Remote's tests: the protocol on its own, the app in fm1-base's host simulator (app/tests/*.txt, screenshots
# in app/build/tests), and Bluetooth MIDI SysEx as the overlay patches it. Builds the simulator first.
#   tests/run_tests.sh      (PYTHON: a Python with Pillow; FM1_BASE_MIRROR: see tools/build_base.py)
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
OUT=build/tests
FAIL=0
mkdir -p "$OUT" app/build/tests
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        grep -E "FAIL|error" "$OUT/$name.log" | head -15 | sed 's/^/       /'
        FAIL=1
    fi
}
run proto sh -c "$CC -std=c99 -Wall -Wextra -I app -o $OUT/proto_test tests/proto_test.c && $OUT/proto_test"
run host-build env PYTHON="$PY" ./build.sh --host
for t in app/tests/*.txt; do
    run "host-$(basename "$t" .txt)" app/build/host/melodee-remote_host "$t" app/build/tests
done
run ble-midi "$PY" tests/ble_midi_test.py
exit $FAIL
