#!/bin/bash
# ============================================================
# generate-passwords.sh
#
# Generates a Mosquitto password file with two accounts (camera + recorder)
# using randomly-generated passwords. Prints the plaintext passwords once
# -- save them, they cannot be recovered from the hashed password file.
#
# Requires: mosquitto_passwd (from the `mosquitto-clients` package on
# Debian/Ubuntu, `mosquitto` on macOS/Homebrew).
#
# Usage:
#   ./generate-passwords.sh
#   ./generate-passwords.sh /path/to/output-dir
#
# Output:
#   passwd  -- Mosquitto password file (deploy to /etc/mosquitto/certs/)
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${1:-$SCRIPT_DIR/out}"

mkdir -p "$OUT_DIR"
PASSWD_FILE="$OUT_DIR/passwd"

if ! command -v mosquitto_passwd >/dev/null 2>&1; then
    echo "ERROR: mosquitto_passwd not found. Install the mosquitto-clients package."
    exit 1
fi

# Generate random passwords (16 chars, alphanumeric).
CAM_PASS=$(openssl rand -base64 12)
REC_PASS=$(openssl rand -base64 12)

echo "==> Creating password file with two accounts..."
: > "$PASSWD_FILE"
mosquitto_passwd -b "$PASSWD_FILE" amb82_cam_01 "$CAM_PASS"
mosquitto_passwd -b "$PASSWD_FILE" recorder     "$REC_PASS"
chmod 600 "$PASSWD_FILE"

echo
echo "=================================================================="
echo "  SAVE THESE PASSWORDS -- they are NOT recoverable from the file."
echo "=================================================================="
echo "  amb82_cam_01  ->  $CAM_PASS   (set MQTT_PASSWORD in firmware)"
echo "  recorder      ->  $REC_PASS   (set MQTT_PASS in recorder env)"
echo "=================================================================="
echo
echo "Wrote: $PASSWD_FILE"
