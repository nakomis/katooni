#!/usr/bin/env bash
# Build the firmware (or pass any idf.py command through).
#   embedded/build.sh                 idf.py build
#   embedded/build.sh flash monitor   anything else idf.py takes
set -euo pipefail
cd "$(dirname "$0")"
IDF_PATH="${IDF_PATH:-$HOME/esp/v5.5/esp-idf}"
# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" > /dev/null
if [ $# -gt 0 ]; then
    exec idf.py "$@"
fi
idf.py build
