#!/usr/bin/env bash
# Build the firmware (or pass any idf.py command through).
#   embedded/build.sh                       idf.py build, PCB pin profile
#   embedded/build.sh flash monitor         anything else idf.py takes
#   embedded/build.sh --breadboard [...]    the breadboard profile (DevKitC + GMT020-02), kept in
#                                           build-breadboard/ with its own sdkconfig
set -euo pipefail
cd "$(dirname "$0")"
IDF_PATH="${IDF_PATH:-$HOME/esp/v5.5/esp-idf}"
# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" > /dev/null
args=()
if [ "${1:-}" = "--breadboard" ]; then
    shift
    args=(-B build-breadboard -D SDKCONFIG=build-breadboard/sdkconfig
          -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.breadboard")
fi
if [ $# -gt 0 ]; then
    exec idf.py ${args[@]+"${args[@]}"} "$@"
fi
exec idf.py ${args[@]+"${args[@]}"} build
