#!/usr/bin/env bash
# Flash a Katooni over USB: bootloader, partition table, OTA data and the signed app, plus its prov
# partition from certs/<thing>/prov.bin (infra/scripts/provision-device.sh).
#   embedded/flash.sh <port> [thing] [--breadboard]
# Never the nvs partition: the sorting session survives. Writing OTA data boots ota_0, whichever slot
# an update had moved to. The app must be signed by this environment's key (ota-signing/{env}.pub.pem,
# env from NPM_ENVIRONMENT, default sandbox): one signed by another would boot, then refuse every
# update from Fleet. --after watchdog_reset starts the new firmware without a reset tap.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
port=${1:?usage: flash.sh <port> [thing] [--breadboard]}
thing=
dir=build
for arg in "${@:2}"; do
    case "$arg" in
        --breadboard) dir=build-breadboard ;;
        *) thing=$arg ;;
    esac
done
cd "$here/$dir"
IDF_PATH="${IDF_PATH:-$HOME/esp/v5.5/esp-idf}"
# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" > /dev/null

env=${NPM_ENVIRONMENT:-sandbox}
espsecure.py verify_signature --version 2 --keyfile "$here/ota-signing/${env}.pub.pem" katooni.bin > /dev/null 2>&1 || {
    echo "$dir/katooni.bin isn't signed by the $env key (ota-signing/${env}.pub.pem): run build.sh" >&2
    exit 1
}

prov=()
if [ -n "$thing" ]; then
    prov=("$here/certs/$thing/prov.bin")
else
    shopt -s nullglob
    prov=("$here"/certs/*/prov.bin)
fi
extra=()
if [ "${#prov[@]}" -eq 1 ] && [ -s "${prov[0]}" ]; then
    offset=$(awk -F', *' '$1 == "prov" { print $4 }' "$here/partitions.csv")
    echo "prov: ${prov[0]} at ${offset}"
    extra=("$offset" "${prov[0]}")
elif [ -n "$thing" ] || [ "${#prov[@]}" -gt 1 ]; then
    echo "need exactly one prov.bin (have ${#prov[@]}): name the thing, or run infra/scripts/provision-device.sh" >&2
    exit 1
else
    echo "no prov.bin: flashing without Fleet (Wi-Fi from sdkconfig.secrets, if any)" >&2
fi
python -m esptool --chip esp32s3 -p "$port" -b 460800 --before default_reset --after watchdog_reset \
    write_flash @flash_args ${extra[@]+"${extra[@]}"}
