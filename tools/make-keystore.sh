#!/usr/bin/env bash
# Create a signing key for your apps (once). Keep the .jks file and its password safe, and back it
# up: if you lose it you can never publish an update that installs over the old app.
#   usage: tools/make-keystore.sh my-release-key.jks [alias]     (alias defaults to "key0")
# The password is read from TCLDROID_KS_PASS or asked for.
set -euo pipefail
out=${1:?usage: make-keystore.sh file.jks [alias]}; alias=${2:-key0}
[ ! -e "$out" ] || { echo "$out exists; refusing to overwrite" >&2; exit 1; }
if [ -z "${TCLDROID_KS_PASS:-}" ]; then
  read -r -s -p "New keystore password (6+ characters): " TCLDROID_KS_PASS; echo
fi
read -r -p "Your name or organisation (shown in the certificate): " cn
keytool -genkeypair -keystore "$out" -alias "$alias" -keyalg RSA -keysize 4096 -validity 36500 \
  -storepass "$TCLDROID_KS_PASS" -keypass "$TCLDROID_KS_PASS" -dname "CN=${cn:-Tcl app}"
echo "Created $out (alias $alias). Use it with:"
echo "  TCLDROID_KS_PASS=... tools/pack-app.sh APPDIR --keystore $out --key-alias $alias"
