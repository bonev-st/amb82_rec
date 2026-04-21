#!/bin/bash
# ============================================================
# generate-server.sh
#
# Generates the broker server certificate, signed by the CA.
# Uses config/server.cnf for the Subject Alternative Names (SANs).
# Edit that file to add the hostnames and IPs clients will use to
# connect to your broker.
#
# Prerequisites:
#   ./generate-ca.sh (must run first -- needs ca.key and ca.crt)
#
# Output files (in $OUT_DIR, default ./out):
#   server.key  -- Server private key (deploy to /etc/mosquitto/certs/, SECRET)
#   server.crt  -- Server certificate (deploy to /etc/mosquitto/certs/)
#   server.csr  -- Certificate Signing Request (intermediate, can be deleted)
#
# Usage:
#   ./generate-server.sh
#   ./generate-server.sh /path/to/output-dir
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${1:-$SCRIPT_DIR/out}"
SAN_CONFIG="$SCRIPT_DIR/config/server.cnf"

# On Windows Git Bash / MSYS, OpenSSL is a native Windows binary and needs
# Windows-format paths (C:/...) for FILE arguments (-extfile, -CA, etc.),
# but MSYS will NOT auto-convert those here. Meanwhile, the `-subj /CN=...`
# argument MUST NOT be path-converted (it starts with / but is not a path).
# So: convert file paths explicitly with cygpath, and use MSYS_NO_PATHCONV=1
# only on invocations that pass a -subj string.
winpath() {
    if command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else echo "$1"; fi
}

# The server's Common Name (CN). The actual hostname matching happens via
# SAN entries in server.cnf, but setting CN is still good hygiene.
CN="sbbu01.local"

if [[ ! -f "$OUT_DIR/ca.key" || ! -f "$OUT_DIR/ca.crt" ]]; then
    echo "ERROR: CA files not found in $OUT_DIR."
    echo "Run ./generate-ca.sh first."
    exit 1
fi

echo "==> Generating server private key (2048-bit RSA)..."
openssl genrsa -out "$OUT_DIR/server.key" 2048

echo "==> Generating server CSR (Certificate Signing Request) for CN=$CN..."
MSYS_NO_PATHCONV=1 openssl req -new \
    -key "$(winpath "$OUT_DIR/server.key")" \
    -out "$(winpath "$OUT_DIR/server.csr")" \
    -subj "/CN=$CN"

echo "==> Signing server certificate with CA (1-year validity, SANs from $SAN_CONFIG)..."
openssl x509 -req -days 365 \
    -in "$(winpath "$OUT_DIR/server.csr")" \
    -CA "$(winpath "$OUT_DIR/ca.crt")" \
    -CAkey "$(winpath "$OUT_DIR/ca.key")" \
    -CAcreateserial \
    -out "$(winpath "$OUT_DIR/server.crt")" \
    -extfile "$(winpath "$SAN_CONFIG")"

chmod 600 "$OUT_DIR/server.key"

echo
echo "==> Done. Summary:"
openssl x509 -in "$OUT_DIR/server.crt" -noout -subject -ext subjectAltName -dates
echo
echo "Verification:"
openssl verify -CAfile "$OUT_DIR/ca.crt" "$OUT_DIR/server.crt"
echo
echo "Files:"
echo "  $OUT_DIR/server.key  (SECRET -- deploy to /etc/mosquitto/certs/)"
echo "  $OUT_DIR/server.crt  (deploy to /etc/mosquitto/certs/)"
