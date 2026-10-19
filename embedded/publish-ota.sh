#!/usr/bin/env bash
# KATOO-10 (from tinycrypt's CRYPT-14): build, sign and publish a firmware version for Fleet to offer.
#
#   embedded/publish-ota.sh <x.y.z> [--test-rollback]
#
# Uploads s3://nakomis-firmware-{env}/katooni/<x.y.z>/firmware.bin with metadata signed=true and its
# sha256, which Fleet's manifest picks up (the first upload is what makes "katooni" a Fleet project);
# then update a Katooni from Fleet's UI. The version must be embedded/version.txt, the tree clean (the
# image says which commit it is), and the version new: a published version is never replaced. The
# build comes from sdkconfig.defaults alone (the local sdkconfig is replaced; sdkconfig.secrets must
# not exist). --test-rollback builds with CONFIG_KATOONI_OTA_FAIL_SELFTEST, an image that rolls back
# after installing, to test that; never otherwise.
#
# Needs AWS_PROFILE (default nakom.is-sandbox) and NPM_ENVIRONMENT (default sandbox).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
version=${1:?usage: publish-ota.sh <x.y.z> [--test-rollback]}
test_rollback=false
[ "${2:-}" = --test-rollback ] && test_rollback=true
env=${NPM_ENVIRONMENT:-sandbox}
export AWS_PROFILE=${AWS_PROFILE:-nakom.is-sandbox}
export AWS_REGION=${AWS_REGION:-eu-west-2}
IDF_PATH="${IDF_PATH:-$HOME/esp/v5.5/esp-idf}"
# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" > /dev/null 2>&1   # for esptool and espsecure
bucket="nakomis-firmware-${env}"
object="katooni/${version}/firmware.bin"

[[ "$version" =~ ^[0-9]{1,4}\.[0-9]{1,4}\.[0-9]{1,4}$ ]] || { echo "version must be x.y.z" >&2; exit 1; }
[ "$(tr -d '[:space:]' < "$here/version.txt")" = "$version" ] || {
    echo "embedded/version.txt says $(cat "$here/version.txt"), not $version" >&2
    exit 1
}
[ -z "$(git -C "$here" status --porcelain)" ] || { echo "the tree has uncommitted changes" >&2; exit 1; }
[ ! -e "$here/sdkconfig.secrets" ] || { echo "move sdkconfig.secrets aside: it would put Wi-Fi in the image" >&2; exit 1; }
if aws s3api head-object --bucket "$bucket" --key "$object" > /dev/null 2>&1; then
    echo "s3://${bucket}/${object} already exists: publish a new version" >&2
    exit 1
fi
# Read before building, so a missing credential fails early; the image is checked for it below.
ssid=$(aws ssm get-parameter --name /Nakomis/Wifi/SSID --query Parameter.Value --output text) || {
    echo "can't read the Wi-Fi SSID to check the image for it: not publishing" >&2
    exit 1
}

rm -f "$here/sdkconfig"
defaults="sdkconfig.defaults"
if $test_rollback; then
    mkdir -p "$here/build"
    printf 'CONFIG_KATOONI_OTA_FAIL_SELFTEST=y\n' > "$here/build/rollback-test.defaults"
    defaults="sdkconfig.defaults;build/rollback-test.defaults"
fi
"$here/build.sh" "-DSDKCONFIG_DEFAULTS=${defaults}" reconfigure > /dev/null
"$here/build.sh"
bin="$here/build/katooni.bin"
if $test_rollback; then
    # Don't leave the failing self-test in the dev config (or in CMake's cached defaults).
    cp "$bin" "$here/build/rollback-test.bin"
    rm -f "$here/sdkconfig" "$here/build/rollback-test.defaults"
    "$here/build.sh" "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults" reconfigure > /dev/null
    bin="$here/build/rollback-test.bin"
fi
config="$here/build/config/sdkconfig.h"
# shellcheck source=tools/image-checks.sh
. "$here/tools/image-checks.sh"
check_image_version "$bin" "$version"
check_image_config "$config" "$test_rollback"
check_image_signed "$bin" "$env"
needles=("$ssid")
shopt -s nullglob
for pem in "$here"/certs/*/device.pem.key "$here"/certs/*/device.pem.crt; do
    needles+=("$(sed -n 2p "$pem")")
done
check_image_strings "$bin" "$env" "${needles[@]}"

sha=$(shasum -a 256 "$bin" | cut -d' ' -f1)
# --if-none-match: a published version is never replaced, even by a publish racing this one.
aws s3api put-object --bucket "$bucket" --key "$object" --body "$bin" --if-none-match '*' \
    --content-type application/octet-stream --metadata "signed=true,sha256=${sha}" > /dev/null
echo "published katooni ${version} (sha256 ${sha}) to s3://${bucket}/${object}"
echo "now update a Katooni from Fleet: https://app.${env}.nakomis.com/fleet"
