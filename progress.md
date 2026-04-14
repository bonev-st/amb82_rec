# Progress Log

## Session: 2026-04-13 — MQTT Security (mTLS + Auth)

### Phase 1: Research
- Explored WiFiSSLClient SDK API (full mTLS support, PubSubClient-compatible)
- Checked broker: Mosquitto 2.0.21, anonymous on 1883, no TLS
- Checked recorder.py: already has MQTT_USER/MQTT_PASS, paho-mqtt supports tls_set()
- User confirmed: self-signed CA, keep anon 1883, mTLS, generate passwords, certs in repo

### Phase 2: Cert Generation (on test-system)
- Generated self-signed CA (10yr), server cert, camera client cert, recorder client cert
- Created Mosquitto password file: `amb82_cam_01` / `recorder`
- All stored at `~/mqtt_certs/` on test-system

### Phase 3: Broker Config
- Wrote `secure.conf`: dual listeners (1883 anon + 8883 mTLS+auth)
- Wrote `install.sh` for sudo-required file copy into `/etc/mosquitto/`
- **Blocked:** needs user to run install.sh interactively (sudo password)

### Phase 4: Firmware Changes
- `config.h`: MQTT_USE_TLS=1, MQTT_PORT conditional (8883/1883), credentials set
- `mqtt_certs.h`: new file with CA + client cert + client key PEM strings
- `mqtt_manager.h`: `begin(Client&)` instead of `begin(WiFiClient&)`, conditional includes
- `mqtt_manager.cpp`: pullConfig() uses WiFiSSLClient+mTLS when TLS on, passes credentials
- `amb82_rec.ino`: conditional WiFiSSLClient global, setRootCA+setClientCertificate in setup

### Phase 5: Recorder Changes
- `recorder.py`: added TLS env vars, `client.tls_set()` with mTLS support
- Deployed to test-system, copied recorder certs, updated systemd service
- Reloaded systemd daemon

### Phase 6: Documentation
- README.md: full MQTT Security section (levels, config, broker setup, cert gen, verify)
- CLAUDE.md: WiFiSSLClient API reference, MQTT Security section

### Verification (all passed)
- Test 1: Anonymous publish on 1883 — OK
- Test 2: mTLS+auth publish on 8883 — OK
- Test 3: No-client-cert attempt on 8883 — correctly rejected
- Test 4: Wrong-password attempt on 8883 — correctly rejected
- Recorder connects via TLS+mTLS+auth (after fixing MQTT_BROKER to sbbu01.local)
- End-to-end: test motion event over TLS → recorder spawned ffmpeg

### Fixes during verification
- `install.sh` used `~` which expands to `/root` under sudo → fixed with `$(dirname "$0")`
- recorder connected to `127.0.0.1` but server cert CN is `sbbu01.local` → updated MQTT_BROKER in systemd service

---

## Prior Sessions (archived)

### 2026-04-13 — Power Optimization & Release Build
- Added BUILD_RELEASE/BUILD_DEBUG, WDT, adaptive delay, firmware version

### 2026-04-11 — Bench Deployment
- Full deployment, 108 clips recorded

### 2026-04-06 — Initial Implementation
- Full firmware + server stack + documentation
