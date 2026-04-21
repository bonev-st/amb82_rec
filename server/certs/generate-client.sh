#!/bin/bash
# ============================================================
# generate-client.sh
#
# Generates a client certificate signed by the CA. For mTLS, each client
# that connects to the broker needs its own key + cert.
#
# Prerequisites:
#   ./generate-ca.sh (must run first)
#
# Usage:
#   ./generate-client.sh <client-name> [output-dir]
#
# Examples:
#   ./generate-client.sh amb82_cam_01
#   ./generate-client.sh recorder
#   ./generate-client.sh amb82_cam_02 ./out
#
# Output files (in $OUT_DIR, default ./out):
#   client_<name>.key  -- Client private key (SECRET, deploy on client)
#   client_<name>.crt  -- Client certificate (deploy on client)
#   client_<name>.csr  -- CSR (intermediate, can be deleted)
#
# Where to use the output:
# - Camera firmware: copy PEM contents of client_camera.crt + client_camera.key
#   into mqtt_certs.h (along with ca.crt). Recompile and flash.
# - Recorder service: copy the files into ~/amb82_recorder/certs/ on the
#   server, update MQTT_CLIENT_CERT / MQTT_CLIENT_KEY env vars in the
#   systemd unit.
# ============================================================
set -euo pipefail

# On Windows Git Bash, OpenSSL needs Windows paths for file arguments but
# the -subj "/CN=..." argument must NOT be path-converted.
winpath() {
    if command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else echo "$1"; fi
}

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <client-name> [output-dir]"
    echo "Example: $0 amb82_cam_01"
    exit 1
fi

CLIENT_NAME="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${2:-$SCRIPT_DIR/out}"

if [[ ! -f "$OUT_DIR/ca.key" || ! -f "$OUT_DIR/ca.crt" ]]; then
    echo "ERROR: CA files not found in $OUT_DIR."
    echo "Run ./generate-ca.sh first."
    exit 1
fi

KEY="$OUT_DIR/client_${CLIENT_NAME}.key"
CSR="$OUT_DIR/client_${CLIENT_NAME}.csr"
CRT="$OUT_DIR/client_${CLIENT_NAME}.crt"

echo "==> Generating client private key for '$CLIENT_NAME' (2048-bit RSA)..."
openssl genrsa -out "$KEY" 2048

echo "==> Generating client CSR for CN=$CLIENT_NAME..."
MSYS_NO_PATHCONV=1 openssl req -new \
    -key "$(winpath "$KEY")" \
    -out "$(winpath "$CSR")" \
    -subj "/CN=$CLIENT_NAME"

echo "==> Signing client certificate with CA (1-year validity)..."
openssl x509 -req -days 365 \
    -in "$(winpath "$CSR")" \
    -CA "$(winpath "$OUT_DIR/ca.crt")" \
    -CAkey "$(winpath "$OUT_DIR/ca.key")" \
    -CAcreateserial \
    -out "$(winpath "$CRT")"

chmod 600 "$KEY"

echo
echo "==> Done. Summary:"
openssl x509 -in "$CRT" -noout -subject -issuer -dates
echo
echo "Verification:"
openssl verify -CAfile "$OUT_DIR/ca.crt" "$CRT"
echo
echo "Files:"
echo "  $KEY  (SECRET -- belongs to this client only)"
echo "  $CRT  (presented to broker during mTLS handshake)"
