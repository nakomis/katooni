#!/usr/bin/env bash
# KATOO-10 (from tinycrypt's CRYPT-14): make the RSA-3072 key that signs Katooni firmware (the secure
# boot v2 scheme), once per environment. Refuses if one exists: replacing it would strand every
# Katooni in the field, since one only accepts updates signed like the image it's running.
#
#   infra/scripts/ota-signing-key.sh
#
# The key lives in SSM Parameter Store, /katooni/{env}/ota/signing-key (SecureString, the master copy,
# tagged MH-Project/Project katooni), and in the login Keychain (service katooni-ota-signing, account
# {env}, base64) for embedded/sign.sh. Only its public half goes in the repo
# (embedded/ota-signing/{env}.pub.pem: commit it); the private PEM made here is deleted, and never
# passed on a command line.
#
# Needs AWS_PROFILE (default nakom.is-sandbox) and NPM_ENVIRONMENT (default sandbox).
set -euo pipefail
umask 077

env=${NPM_ENVIRONMENT:-sandbox}
export AWS_PROFILE=${AWS_PROFILE:-nakom.is-sandbox}
export AWS_REGION=${AWS_REGION:-eu-west-2}
param="/katooni/${env}/ota/signing-key"
service=katooni-ota-signing

if aws ssm get-parameter --name "$param" > /dev/null 2>&1; then
    echo "$param already exists; not replacing it" >&2
    exit 1
fi
if security find-generic-password -s "$service" -a "$env" > /dev/null 2>&1; then
    echo "Keychain already has $service/$env; not replacing it" >&2
    exit 1
fi

IDF_PATH="${IDF_PATH:-$HOME/esp/v5.5/esp-idf}"
# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" > /dev/null

dir=$(mktemp -d)
trap 'rm -P "$dir"/* 2>/dev/null; rmdir "$dir"' EXIT
espsecure.py generate_signing_key --version 2 --scheme rsa3072 "$dir/key.pem" > /dev/null

# The public half first: if anything after SSM fails, sign.sh can still restore the Keychain from SSM.
pub="$(cd "$(dirname "$0")/../.." && pwd)/embedded/ota-signing/${env}.pub.pem"
mkdir -p "$(dirname "$pub")"
espsecure.py extract_public_key --version 2 --keyfile "$dir/key.pem" "$pub" > /dev/null
chmod 644 "$pub"
aws ssm put-parameter --name "$param" --type SecureString --value "file://$dir/key.pem" \
    --description "Katooni firmware signing key (RSA-3072, secure boot v2 scheme)" \
    --tags Key=MH-Project,Value=katooni Key=Project,Value=katooni > /dev/null
# Through security's stdin, not its argv (where ps would show it). Base64 needs no quoting.
printf 'add-generic-password -s %s -a %s -T /usr/bin/security -j "%s" -w %s\n' "$service" "$env" \
    "Katooni firmware signing key ($env); master copy in SSM $param" "$(base64 < "$dir/key.pem" | tr -d '\n')" |
    security -i

digest=$(espsecure.py digest_sbv2_public_key --keyfile "$dir/key.pem" --output "$dir/digest.bin" > /dev/null && xxd -p -c 64 "$dir/digest.bin")
echo "made $param and Keychain $service/$env; public key digest $digest"
echo "commit $pub"
