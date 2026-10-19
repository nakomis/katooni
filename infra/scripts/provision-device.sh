#!/usr/bin/env bash
# KATOO-10 (from tinycrypt's provision-key.sh, CRYPT-15): give a Katooni its IoT certificate and the
# settings its firmware reads. Safe to re-run.
#
#   infra/scripts/provision-device.sh [unit-N] [--new]       (default unit-1)
#
# Each run makes sure the device's certificate is active and attached to its Thing and the policy
# (both from KatooniDeviceStack: add the device to infra/devices.json and deploy first). --new retires
# the Thing's existing certificates (detach, deactivate, delete) and makes a fresh one; use it if a
# private key may have leaked.
#
# Writes, never committed, under embedded/certs/<thing>/:
#   device.pem.crt, device.pem.key, certificate-arn
#   prov.bin   the device's prov partition: Wi-Fi, IoT endpoint, Thing name, certificate and private
#              key. embedded/flash.sh writes it alongside the app; none of it is in the firmware image,
#              so every Katooni runs the same (published) image.
#
# Certificates can't be tagged; each is attached to its Thing, which carries project=katooni.
# Needs AWS_PROFILE (default nakom.is-sandbox) and NPM_ENVIRONMENT (default sandbox). The house Wi-Fi
# credentials are read from /Nakomis/Wifi/* via WIFI_PROFILE (default nakom.is-sandbox: they live only
# in the sandbox account).
set -euo pipefail
umask 077

device=unit-1
new=false
for arg in "$@"; do
    case "$arg" in
        --new) new=true ;;
        -*) echo "unknown option: $arg" >&2; exit 2 ;;
        *) device=$arg ;;
    esac
done
env=${NPM_ENVIRONMENT:-sandbox}
export AWS_PROFILE=${AWS_PROFILE:-nakom.is-sandbox}
export AWS_REGION=${AWS_REGION:-eu-west-2}
wifi_profile=${WIFI_PROFILE:-nakom.is-sandbox}

here=$(cd "$(dirname "$0")" && pwd)
fw="${here}/../../embedded"
thing="katooni-${env}-${device}"
policy="katooni-device-${env}"
dir="${fw}/certs/${thing}"
mkdir -p "$dir"

aws iot describe-thing --thing-name "$thing" > /dev/null   # fails here if KatooniDeviceStack lacks it

retire() {
    local arn=$1 id=${1##*/}
    aws iot detach-policy --policy-name "$policy" --target "$arn" 2>/dev/null || true
    aws iot detach-thing-principal --thing-name "$thing" --principal "$arn"
    aws iot update-certificate --certificate-id "$id" --new-status INACTIVE
    # Detaching from a Thing is eventually consistent; deletion can lag a few seconds behind.
    for _ in 1 2 3 4 5 6; do aws iot delete-certificate --certificate-id "$id" 2> /dev/null && break; sleep 5; done
    echo "retired certificate ${id}"
}

if $new; then
    for arn in $(aws iot list-thing-principals --thing-name "$thing" --query 'principals' --output text); do
        retire "$arn"
    done
    rm -f "${dir}/device.pem.crt" "${dir}/device.pem.key" "${dir}/certificate-arn"
fi

if [ ! -s "${dir}/device.pem.key" ] || [ ! -s "${dir}/certificate-arn" ]; then
    aws iot create-keys-and-certificate --set-as-active \
        --certificate-pem-outfile "${dir}/device.pem.crt" \
        --private-key-outfile "${dir}/device.pem.key" \
        --query certificateArn --output text > "${dir}/certificate-arn"
    echo "new certificate $(cut -d/ -f2 < "${dir}/certificate-arn") for ${thing}"
fi
cert_arn=$(cat "${dir}/certificate-arn")

# Both attachments are idempotent, so every run repairs a half-finished earlier one.
aws iot attach-policy --policy-name "$policy" --target "$cert_arn"
aws iot attach-thing-principal --thing-name "$thing" --principal "$cert_arn"
status=$(aws iot describe-certificate --certificate-id "${cert_arn##*/}" --query certificateDescription.status --output text)
[ "$status" = ACTIVE ] || { echo "certificate ${cert_arn##*/} is ${status}: rerun with --new" >&2; exit 1; }

endpoint=$(aws iot describe-endpoint --endpoint-type iot:Data-ATS --query endpointAddress --output text)
# Through the environment, not argv, so the password never shows up in `ps`.
export KT_SSID KT_PASSWORD
KT_SSID=$(aws ssm get-parameter --profile "$wifi_profile" --name /Nakomis/Wifi/SSID --query Parameter.Value --output text)
KT_PASSWORD=$(aws ssm get-parameter --profile "$wifi_profile" --name /Nakomis/Wifi/Password --with-decryption \
    --query Parameter.Value --output text)

IDF_PATH="${IDF_PATH:-$HOME/esp/v5.5/esp-idf}"
# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" > /dev/null 2>&1   # for its Python, which has the NVS partition generator
trap 'rm -P "${dir}/prov.csv" 2> /dev/null || true' EXIT   # it holds the Wi-Fi password
prov_size=$(awk -F', *' '$1 == "prov" { print $5 }' "${fw}/partitions.csv")
python3 - "$dir" "$endpoint" "$thing" <<'PY'
import csv, os, sys
d, endpoint, thing = sys.argv[1:]
ssid, password = os.environ["KT_SSID"], os.environ["KT_PASSWORD"]
if len(ssid.encode()) > 32 or len(password.encode()) > 63:
    sys.exit("Wi-Fi SSID or password too long for ESP-IDF (32 / 63 bytes)")
# Keys and sizes as embedded/main/prov.c reads them.
rows = [
    ("katooni", "namespace", "", ""),
    ("wifi_ssid", "data", "string", ssid),
    ("wifi_pass", "data", "string", password),
    ("endpoint", "data", "string", endpoint),
    ("thing", "data", "string", thing),
    ("cert", "file", "string", os.path.join(d, "device.pem.crt")),
    ("key", "file", "string", os.path.join(d, "device.pem.key")),
]
with open(os.path.join(d, "prov.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(("key", "type", "encoding", "value"))
    w.writerows(rows)
PY
python -m esp_idf_nvs_partition_gen generate "${dir}/prov.csv" "${dir}/prov.bin" "$prov_size" > /dev/null
echo "wrote ${dir}/prov.bin for ${thing} (${endpoint}); flash it with embedded/flash.sh <port> ${thing}"
