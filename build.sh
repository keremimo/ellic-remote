#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Build Melodee Remote: ./build.sh (Bluetooth), ./build.sh --no-ble (USB only), ./build.sh --host (the simulator).
# PYTHON: a Python with Pillow (BUILDING.md).
set -e
cd "$(dirname "$0")"
exec "${PYTHON:-python3}" tools/build_base.py "$@"
