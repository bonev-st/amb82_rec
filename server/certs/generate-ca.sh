#!/bin/bash
# ============================================================
# generate-ca.sh
#
# Generates the self-signed root Certificate Authority (CA) that will
# sign the server and client certificates for the MQTT broker.
#
# Output files (in $OUT_DIR, default ./out):
#   ca.key  — CA private key (KEEP SECRET, never ship it)
#   ca.crt  — CA public certificate (safe to distribute, embedded in firmware)
#
# Usage:
#   ./generate-ca.sh
#   ./generate-ca.sh /path/to/output-dir
#
# Re-running this script OVERWRITES ca.key and ca.crt. If you regenerate
# the CA, you must also regenerate every cert it signed (server + clients).
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${1:-$SCRIPT_DIR/out}"
CONFIG="$SCRIPT_DIR/config/ca.cnf"

# On Windows Git Bash, OpenSSL needs Windows paths for file arguments.
winpath() {
    if command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else echo "$1"; fi
}

mkdir -p "$OUT_DIR"

if [[ -f "$OUT_DIR/ca.crt" ]]; then
    read -rp "$OUT_DIR/ca.crt already exists. Overwrite? [y/N] " ans
    [[ "$ans" =~ ^[Yy]$ ]] || { echo "Aborted."; exit 0; }
fi

echo "==> Generating CA private key (2048-bit RSA)..."
openssl genrsa -out "$OUT_DIR/ca.key" 2048

echo "==> Generating self-signed CA certificate (10-year validity)..."
openssl req -new -x509 -days 3650 \
    -key "$(winpath "$OUT_DIR/ca.key")" \
    -out "$(winpath "$OUT_DIR/ca.crt")" \
    -config "$(winpath "$CONFIG")" \
    -extensions v3_ca

chmod 600 "$OUT_DIR/ca.key"

echo
echo "==> Done. Summary:"
openssl x509 -in "$OUT_DIR/ca.crt" -noout -subject -ext basicConstraints -ext keyUsage
echo
echo "Files:"
echo "  $OUT_DIR/ca.key  (SECRET — do not share)"
echo "  $OUT_DIR/ca.crt  (public; embed in firmware and install on broker)"
