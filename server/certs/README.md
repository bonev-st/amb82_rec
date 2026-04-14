# Self-Signed Certificate Toolkit

This folder contains everything needed to generate the certificates used by
the MQTT broker (Mosquitto) and its clients (the AMB82 camera and the
recorder service) for **TLS + mTLS + password authentication**.

If you just want to get going, skip straight to **[Quick Start](#quick-start)**.
If you want to understand what's happening, read the **[Theory](#theory-in-five-minutes)**
section first.

---

## Contents

| File                       | Purpose |
|----------------------------|---------|
| `generate-ca.sh`           | Create a self-signed root Certificate Authority |
| `generate-server.sh`       | Create the broker's server certificate, signed by the CA |
| `generate-client.sh`       | Create a client certificate (one per device/service) |
| `generate-passwords.sh`    | Create a Mosquitto password file with random passwords |
| `generate-all.sh`          | Run all of the above in sequence — the usual entry point |
| `install-broker.sh`        | Deploy certs + config to `/etc/mosquitto/` (needs sudo) |
| `config/ca.cnf`            | OpenSSL config for the CA (ensures `CA:TRUE`) |
| `config/server.cnf`        | OpenSSL config for the server SANs (hostnames + IPs) |
| `.gitignore`               | Keeps generated keys/certs out of version control |

---

## Quick Start

### 1. Generate everything

```bash
cd server/certs
./generate-all.sh
```

This creates an `out/` directory containing the CA, server cert, two client
certs (camera + recorder), and a Mosquitto password file. **The script
prints the two passwords — copy them immediately, they can't be recovered
later.**

### 2. Edit `config/server.cnf` if your broker is NOT `sbbu01.local` / `192.168.2.143`

Open `config/server.cnf` and update the `DNS.*` / `IP.*` entries to whatever
names and addresses your clients will use to reach the broker. Then re-run
`./generate-server.sh` to regenerate just the server cert (the CA and client
certs are unaffected).

### 3. Deploy the broker-side certs

On the broker host (copy the `out/` folder over first if running from elsewhere):

```bash
sudo ./install-broker.sh
```

This copies `ca.crt`, `server.crt`, `server.key`, `passwd` into
`/etc/mosquitto/certs/`, writes `/etc/mosquitto/conf.d/secure.conf` with
both a plain (1883) and a TLS (8883) listener, and restarts Mosquitto.

### 4. Embed the CA + client cert into the firmware

Copy the PEM contents of three files from `out/` into
`../../mqtt_certs.h` — into the `mqtt_ca_cert`, `mqtt_client_cert`,
`mqtt_client_key` string constants respectively:

- `out/ca.crt`                 → `mqtt_ca_cert`
- `out/client_amb82_cam_01.crt` → `mqtt_client_cert`
- `out/client_amb82_cam_01.key` → `mqtt_client_key`

Set `MQTT_USE_TLS 1` in `../../config.h`, put the generated password into
`MQTT_PASSWORD`, recompile, flash.

### 5. Deploy the recorder certs

On the broker host:

```bash
mkdir -p ~/amb82_recorder/certs
cp out/ca.crt out/client_recorder.crt out/client_recorder.key ~/amb82_recorder/certs/
chmod 600 ~/amb82_recorder/certs/client_recorder.key
```

Then update `~/.config/systemd/user/amb82-recorder.service` with the recorder
password (`MQTT_PASS`) and restart: `systemctl --user restart amb82-recorder`.

---

## Theory in Five Minutes

### What is a certificate?

A certificate is a digital document saying "the owner of **this public key**
is **this identity**". Every cert contains:

- A **public key** (the half that's safe to share)
- A **subject** (who this cert identifies — e.g. `CN=amb82_cam_01`)
- A **signature** from the issuer, proving the subject is genuine
- **Extensions** like `subjectAltName` (other valid names/IPs) and
  `basicConstraints` (whether this cert is allowed to sign other certs)

The matching **private key** is kept secret by the owner. Anyone can
verify "this client really does own the private key for that cert" by
challenging them to sign a random value.

### What is a Certificate Authority (CA)?

A CA is a cert whose only job is to sign other certs. Its `basicConstraints`
has `CA:TRUE`, and it has `keyCertSign` in its `keyUsage`. On the public
internet, CAs are well-known organizations like Let's Encrypt. On a private
LAN you can run your own — that's what `generate-ca.sh` does.

Browsers and embedded TLS libraries ship with a list of trusted CA certs.
For a self-signed CA, **you** have to install the CA cert on every client
that needs to trust it. In this project that's:

- Firmware: CA cert embedded in `mqtt_certs.h` via `setRootCA()`
- Recorder: CA cert path passed via `MQTT_CA_CERT` env var to paho-mqtt

### The trust chain

```
  ca.crt   (self-signed, TRUSTED by all clients — the "anchor")
    │
    ├── signs → server.crt   (presented by broker during TLS handshake)
    │
    ├── signs → client_amb82_cam_01.crt  (presented by firmware for mTLS)
    │
    └── signs → client_recorder.crt      (presented by recorder for mTLS)
```

When the camera opens a TLS connection to the broker, the broker sends
`server.crt`. The camera walks up the signature chain — "who signed this?"
→ `ca.crt` → "do I trust this CA?" → yes (it's pinned in `mqtt_certs.h`) →
trust established.

### TLS vs. mTLS

**TLS** (aka "normal HTTPS-style") authenticates only the server. The client
verifies `server.crt` against its trusted CA. The server has no idea who
the client is at the TLS layer — that's handled separately (e.g. by a
username/password after the handshake).

**Mutual TLS (mTLS)** authenticates both sides. The broker also asks the
client to present its cert (`require_certificate true` in Mosquitto). Both
certs are verified against the same CA. This is strong cryptographic
proof-of-identity — a rogue device without a valid client key cannot even
complete the handshake.

### Why we layer password auth on top

Even with mTLS, this project also requires username/password. Why?

- **mTLS proves possession of a key**. If a device is stolen or cloned, the
  attacker gets the key.
- **Password adds a revocable secondary check** — delete a password file
  entry and that client can no longer connect, even if it still has a valid
  cert.
- They're cheap and complementary. Belt + suspenders.

### Subject Alternative Names (SAN)

Modern TLS libraries (including mbedTLS, which the AMB82 uses) check the
connection target (hostname or IP) against the cert's **Subject Alternative
Name** extension — *not* the old-fashioned `CN` field. This is why
`config/server.cnf` lists both `DNS:` and `IP:` entries: any hostname or IP
a client might connect to must be in that list, or the handshake fails with
`MBEDTLS_ERR_X509_CERT_VERIFY_FAILED` / `-0x2700`.

> **mbedTLS 2.28 quirk:** the AMB82's TLS stack only matches the SNI
> hostname string against **DNS** SAN entries — not IP SAN entries. If the
> client connects using an IP literal as a hostname, verification fails
> even if the IP is in the cert. The firmware side works around this by
> giving PubSubClient an `IPAddress` directly via the IPAddress overload
> of `setServer()`, which makes `WiFiSSLClient` skip SNI hostname matching
> while still verifying the full cert chain. See `findings.md` in the
> project root for the full story.

---

## How the certificates are used in this project

| Component     | Where certs live | How they're loaded |
|---------------|-----------------|--------------------|
| **Mosquitto (broker)** | `/etc/mosquitto/certs/{ca,server}.crt`, `server.key`, `passwd` | Referenced in `secure.conf` via `cafile` / `certfile` / `keyfile` / `password_file` |
| **AMB82 firmware** | Embedded as PEM strings in `mqtt_certs.h` | `WiFiSSLClient::setRootCA()` + `setClientCertificate()` |
| **recorder.py** | Files on disk in `~/amb82_recorder/certs/` | paho-mqtt `client.tls_set(ca_certs, certfile, keyfile, ...)` |

The firmware uses **Level 2** security (TLS + mTLS + password, port 8883).
The broker also keeps a **Level 0** anonymous listener on port 1883 so
legacy/test clients still work — this is a deliberate compromise for
backward compatibility in a dev/test setup. In a production deployment,
remove the plain listener from `/etc/mosquitto/conf.d/secure.conf`.

---

## FAQ

**Q: I regenerated the CA, now nothing connects.**
Every cert signed by the old CA is now worthless. Regenerate all client
and server certs (`./generate-all.sh` does this), redeploy to broker,
re-embed the new CA+client certs into `mqtt_certs.h`, reflash firmware,
redeploy recorder certs, restart everything.

**Q: The cert expired!**
By default `generate-*.sh` sets a 10-year validity. If you hit expiry
(nice problem to have), rerun the script for whichever cert expired. CA
expiry means everything must be regenerated. Leaf cert (server/client)
expiry only affects that one cert — you can reissue it against the same
CA.

**Q: Can I use Let's Encrypt / a real public CA instead?**
Yes for the server cert, if the broker has a public DNS name and you can
solve an ACME challenge. Clients then don't need any CA embedded (LE's
root is already in common trust stores). But mTLS client certs are
typically still self-signed or issued by a private CA.

**Q: Where should I store `ca.key`?**
It's the master key — anyone with it can mint new trusted certs. For a
demo/test setup, keeping it in `server/certs/out/` (gitignored) is fine.
For production, offline storage (encrypted USB, HSM, password manager)
is the norm.

**Q: The firmware still fails with `-0x2700`.**
Read `../../findings.md` — there's a specific list of four gotchas that
cause this on the AMB82 with mbedTLS 2.28, every one of which was hit
during development and is now documented.
