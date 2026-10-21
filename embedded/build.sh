#!/usr/bin/env bash
# Build the firmware (or pass any idf.py command through).
#   embedded/build.sh                       idf.py build, PCB pin profile, then sign it (sign.sh)
#   embedded/build.sh flash monitor         anything else idf.py takes (don't flash with idf.py:
#                                           use flash.sh, which writes the signed image and prov)
#   embedded/build.sh --breadboard [...]    the breadboard profile (DevKitC + GMT020-02), kept in
#                                           build-breadboard/ with its own sdkconfig
# A build signs its image when ota-signing/{env}.pub.pem exists (KATOO-10): an unsigned image boots,
# but then refuses every update from Fleet, since an update must be signed like the running image.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
cd "$here"
IDF_PATH="${IDF_PATH:-$HOME/esp/v5.5/esp-idf}"
# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" > /dev/null
args=()
dir=build
if [ "${1:-}" = "--breadboard" ]; then
    shift
    dir=build-breadboard
    args=(-B build-breadboard -D SDKCONFIG=build-breadboard/sdkconfig
          -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.breadboard")
fi
if [ $# -gt 0 ]; then
    exec idf.py ${args[@]+"${args[@]}"} "$@"
fi
idf.py ${args[@]+"${args[@]}"} build
if [ -s "$here/ota-signing/${NPM_ENVIRONMENT:-sandbox}.pub.pem" ]; then
    "$here/sign.sh" "$here/$dir/katooni.bin"
else
    echo "warning: no ota-signing/${NPM_ENVIRONMENT:-sandbox}.pub.pem, so $dir/katooni.bin is unsigned and will refuse every update" >&2
fi
