#!/bin/bash
# ============================================================
# generate-passwords.sh
#
# Generates a Mosquitto password file with two accounts (camera + recorder)
# using randomly-generated passwords. Prints the plaintext passwords once
# -- save them, they cannot be recovered from the hashed password file.
#
# Tool resolution order:
#   1. Native `mosquitto_passwd` (from the `mosquitto-clients` package)
#   2. Fallback: `docker run --rm eclipse-mosquitto:2 mosquitto_passwd ...`
#      so Docker-only hosts don't need mosquitto-clients installed.
#
# Usage:
#   ./generate-passwords.sh
#   ./generate-passwords.sh /path/to/output-dir
#
# Output:
#   passwd  -- Mosquitto password file (mounted read-only into the
#              mosquitto container at /mosquitto/certs/passwd)
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${1:-$SCRIPT_DIR/out}"

mkdir -p "$OUT_DIR"
PASSWD_FILE="$OUT_DIR/passwd"

# Generate random passwords (16 chars of base64 → safe in C string literals
# and shell, no quoting hazards).
CAM_PASS=$(openssl rand -base64 12)
REC_PASS=$(openssl rand -base64 12)

if command -v mosquitto_passwd >/dev/null 2>&1; then
    MODE="native"
    run_mpasswd() {
        mosquitto_passwd "$@"
    }
elif command -v docker >/dev/null 2>&1; then
    MODE="docker"
    # Pull the image up-front so its output doesn't interleave with the
    # password-generation steps below.
    docker pull -q eclipse-mosquitto:2 >/dev/null
    run_mpasswd() {
        # Bind-mount OUT_DIR at /out, run as the host user so the file
        # comes out owned by the caller (not root).
        docker run --rm \
            -v "$OUT_DIR":/out \
            -u "$(id -u):$(id -g)" \
            eclipse-mosquitto:2 \
            mosquitto_passwd "$@"
    }
    # The wrapped command references /out/passwd instead of the host
    # path. Rewrite $PASSWD_FILE accordingly when calling run_mpasswd.
    PASSWD_CONTAINER_PATH=/out/passwd
else
    echo "ERROR: need either the 'mosquitto_passwd' binary or 'docker' installed."
    echo "  Install mosquitto-clients (Debian/Ubuntu) OR ensure Docker is available."
    exit 1
fi

echo "==> Creating password file with two accounts ($MODE mode)..."
: > "$PASSWD_FILE"

if [[ "$MODE" == "native" ]]; then
    run_mpasswd -b "$PASSWD_FILE" amb82_cam_01 "$CAM_PASS"
    run_mpasswd -b "$PASSWD_FILE" recorder     "$REC_PASS"
else
    run_mpasswd -b "$PASSWD_CONTAINER_PATH" amb82_cam_01 "$CAM_PASS"
    run_mpasswd -b "$PASSWD_CONTAINER_PATH" recorder     "$REC_PASS"
fi

chmod 600 "$PASSWD_FILE"

echo
echo "=================================================================="
echo "  SAVE THESE PASSWORDS -- they are NOT recoverable from the file."
echo "=================================================================="
echo "  amb82_cam_01  ->  $CAM_PASS   (set MQTT_PASSWORD in firmware)"
echo "  recorder      ->  $REC_PASS   (set MQTT_RECORDER_PASSWORD in server/.env)"
echo "=================================================================="
echo
echo "Wrote: $PASSWD_FILE"
