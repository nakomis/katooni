# What a published firmware image must pass (KATOO-10, from tinycrypt's CRYPT-14/21). Source it; each
# check prints why and exits on failure. Needs esptool and espsecure on PATH (ESP-IDF's export.sh).

# The image says this version.
check_image_version() { # bin version
    local built
    built=$(python -m esptool --chip esp32s3 image_info --version 2 "$1" 2> /dev/null | awk -F': *' '/^App version/ { print $2 }')
    [ "$built" = "$2" ] || { echo "the image says version '$built', not $2" >&2; exit 1; }
}

# The build's configuration (its sdkconfig.h) is one that may be published.
check_image_config() { # sdkconfig.h test_rollback(true|false)
    local config=$1 test_rollback=$2 option deadline
    # It can be updated (Fleet), it's checked on probation (rollback), and it checks the next one's
    # signature.
    for option in CONFIG_KATOONI_FLEET CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT; do
        grep -q "^#define ${option} 1$" "$config" || { echo "the build lacks ${option}: not publishing" >&2; exit 1; }
    done
    # No Wi-Fi from sdkconfig.secrets: a published image is the same for every Katooni.
    for option in CONFIG_KATOONI_WIFI_SSID CONFIG_KATOONI_WIFI_PASSWORD CONFIG_KATOONI_AP_PASSWORD; do
        grep -q "^#define ${option} \"\"$" "$config" || { echo "the build has ${option} set (sdkconfig.secrets?): not publishing" >&2; exit 1; }
    done
    if grep -q '^#define CONFIG_KATOONI_OTA_FAIL_SELFTEST 1$' "$config" && ! $test_rollback; then
        echo "this build fails its self-test on purpose (CONFIG_KATOONI_OTA_FAIL_SELFTEST): not publishing" >&2
        exit 1
    fi
    # A short download deadline is only for testing it: published, it would fail real updates.
    deadline=$(awk '/^#define CONFIG_KATOONI_OTA_DOWNLOAD_DEADLINE_S / { print $3 }' "$config")
    [ "${deadline:-0}" -ge 120 ] || { echo "the download deadline is ${deadline:-unset} s (< 120): not publishing" >&2; exit 1; }
}

# No secrets or per-device settings (they live in the prov partition): no Thing name or endpoint, and
# none of the extra needles (the Wi-Fi SSID, lines of a provisioned device's certificate). Not a bare
# "PRIVATE KEY": mbedTLS's PEM parser has that string in every image.
check_image_strings() { # bin env [needle...]
    local bin=$1 env=$2 needle
    shift 2
    for needle in "katooni-${env}-" "-ats.iot." "$@"; do
        [ -n "$needle" ] || continue
        if LC_ALL=C grep -qaF -- "$needle" "$bin"; then
            echo "the image contains a per-device string (\"${needle:0:12}...\"): not publishing" >&2
            exit 1
        fi
    done
}

# Signed by this environment's key, or every Katooni in the field refuses it.
check_image_signed() { # bin env
    local pub
    pub="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/ota-signing/$2.pub.pem"
    espsecure.py verify_signature --version 2 --keyfile "$pub" "$1" > /dev/null 2>&1 || {
        echo "the image isn't signed by the $2 key (ota-signing/$2.pub.pem): not publishing" >&2
        exit 1
    }
}
