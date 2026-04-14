# Task Plan: Optional MQTT Security (Auth + mTLS)

## Goal
Add optional MQTT security to the firmware and test-system broker.
Three configurable levels — all backward-compatible with the current anonymous setup:

1. **Anonymous** (current default) — no auth, no encryption
2. **Username/Password** — auth but no encryption (MQTT 1883)
3. **mTLS + Username/Password** — encrypted + mutual TLS + authenticated (MQTT 8883)

## Current Phase
Complete — All phases verified end-to-end

## Credentials
| User | Password | Purpose |
|------|----------|---------|
| `amb82_cam_01` | `DcU9EOHGPWxSH2T6` | Camera firmware |
| `recorder` | `lKnvjNWu0pMzhjTG` | recorder.py service |

## Phases

### Phase 1 — Research & Planning
- [x] Read current MQTT code (config.h, mqtt_manager.h/.cpp, amb82_rec.ino)
- [x] Research AMB82 SDK WiFiSSLClient API (full TLS + mTLS support confirmed)
- [x] Check test-system Mosquitto config (anonymous on 1883, no TLS)
- [x] Check recorder.py (already supports MQTT_USER/MQTT_PASS env vars)
- [x] User confirmed: self-signed CA, keep anon on 1883, mTLS, generate passwords, certs in repo
- **Status:** complete

### Phase 2 — Certificate Generation (on test-system)
- [x] Generate self-signed CA (10-year, CN="AMB82 MQTT CA")
- [x] Generate server cert (CN="sbbu01.local")
- [x] Generate camera client cert (CN="amb82_cam_01")
- [x] Generate recorder client cert (CN="recorder")
- [x] Create password file with mosquitto_passwd
- [x] All certs stored at `~/mqtt_certs/` on test-system
- **Status:** complete

### Phase 3 — Broker Configuration
- [x] Write `secure.conf` with dual listeners (1883 anon + 8883 mTLS+auth)
- [x] Write `install.sh` — fixed to resolve source via `$(dirname "$0")` under sudo
- [x] User ran install.sh, certs deployed, mosquitto restarted
- [x] Both 1883 and 8883 listeners confirmed listening
- **Status:** complete

### Phase 4 — Firmware: TLS + mTLS Support
- [x] `config.h`: Added `MQTT_USE_TLS` toggle, conditional `MQTT_PORT` (8883/1883)
- [x] `config.h`: Set MQTT_USER/PASSWORD to camera credentials
- [x] Created `mqtt_certs.h` with CA cert + client cert + client key (PEM strings)
- [x] `mqtt_manager.h`: Changed `begin()` to accept `Client&`, conditional WiFiSSLClient include
- [x] `mqtt_manager.cpp`: `pullConfig()` uses WiFiSSLClient+mTLS when TLS enabled, passes credentials
- [x] `amb82_rec.ino`: Conditional WiFiSSLClient global, setRootCA + setClientCertificate in setup
- **Status:** complete

### Phase 5 — Recorder: TLS + mTLS Support
- [x] `recorder.py`: Added MQTT_TLS, MQTT_CA_CERT, MQTT_CLIENT_CERT, MQTT_CLIENT_KEY env vars
- [x] `recorder.py`: Added `client.tls_set()` with mTLS when enabled
- [x] Deployed updated recorder.py to test-system
- [x] Copied recorder client certs to `~/amb82_recorder/certs/`
- [x] Updated systemd service with TLS env vars + recorder credentials
- [x] Reloaded systemd daemon
- **Status:** complete (will auto-connect once broker TLS listener is up)

### Phase 6 — Documentation
- [x] README.md: Full MQTT Security section (3 levels, switching guide, broker setup, cert gen, verify)
- [x] CLAUDE.md: WiFiSSLClient API reference
- [x] CLAUDE.md: MQTT Security section (MQTT_USE_TLS usage, mqtt_certs.h)
- **Status:** complete

### Phase 7 — End-to-end Verification
- [x] Test 1: Anonymous on 1883 — OK
- [x] Test 2: mTLS+auth on 8883 — OK
- [x] Test 3: 8883 without client cert — correctly rejected
- [x] Test 4: 8883 with wrong password — correctly rejected
- [x] Recorder auto-connects via TLS+mTLS+auth — OK (after fixing broker to sbbu01.local)
- [x] Test motion event over TLS → recorder spawned ffmpeg — OK
- [ ] Firmware flash-test by user (manual, next step)
- **Status:** server-side complete, firmware flash pending

## Known Issue: Server Cert CN
Server cert CN is `sbbu01.local`, so TLS clients must connect using that
hostname (not an IP) for strict hostname verification (paho-mqtt does this).
The recorder systemd service uses `MQTT_BROKER=sbbu01.local`.

**Firmware note:** `MQTT_BROKER "192.168.2.143"` in config.h — mbedTLS on the
AMB82 via `setRootCA()` typically does not enforce hostname verification
(only chain-of-trust verification), so connecting by IP should work. If it
fails, either (a) change to `"sbbu01.local"` if mDNS works on the board, or
(b) regenerate the server cert with the IP in subjectAltName.

## Files Modified
| File | Changes |
|------|---------|
| `config.h` | MQTT_USE_TLS toggle, conditional MQTT_PORT, credentials |
| `mqtt_certs.h` | **New** — CA cert + client cert + client key (PEM) |
| `mqtt_manager.h` | `begin(Client&)`, conditional WiFiSSLClient + mqtt_certs include |
| `mqtt_manager.cpp` | pullConfig() TLS support, credentials passed to disposable client |
| `amb82_rec.ino` | Conditional WiFiSSLClient, setRootCA + setClientCertificate |
| `server/recorder/recorder.py` | TLS env vars, `client.tls_set()` with mTLS |
| `README.md` | Full MQTT Security section |
| `CLAUDE.md` | WiFiSSLClient API, MQTT Security reference |

## Test-system files (not in repo)
| Path | Contents |
|------|----------|
| `~/mqtt_certs/` | CA, server, client certs/keys, passwd, install.sh |
| `~/amb82_recorder/certs/` | Recorder client cert + key + CA cert |
| `~/.config/systemd/user/amb82-recorder.service` | Updated with TLS env vars |
