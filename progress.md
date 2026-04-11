# Progress Log

## Session: 2026-04-11 — Bench deployment planning

- Read `amb82_rec.ino`, `config.h`, manager headers, `server/docker-compose.yml`,
  `server/recorder/recorder.py`, `server/recorder/Dockerfile`,
  `server/mosquitto/config/mosquitto.conf`.
- Inferred runtime dependencies (logged in `findings.md`).
- Asked user 8 clarifying questions; got answers:
  native Mosquitto, native recorder, no HA, USB power, Arduino IDE flashing,
  default device id, clips → `~/amb82_clips/`. Wi-Fi creds provided OOB.
- Wrote 7-phase deployment plan into `task_plan.md` (pre-flight → infra →
  config → build/flash → bring-up → end-to-end → teardown).
- Flagged: `config.h` is git-tracked; should be untracked before adding real
  creds. PubSubClient include path under `src/` is intentional.
- Next action: execute Phase 2 (test-system infra) when user gives the go-ahead.

### Phase 2 execution (same session)
- Verified mosquitto active, port 1883 listening, ffmpeg + python3 already on PATH.
- Anonymous `mosquitto_pub` to 127.0.0.1 succeeded.
- `scp` was rejected by user; switched to `ssh ... cat > file` for file transfer.
  **Lesson:** prefer ssh-stdin file transfer on this machine instead of scp.
- `python3 -m venv` initially failed (`ensurepip` missing); user installed
  `python3.13-venv` + `pipx`. Recreated venv successfully, `paho-mqtt==2.1.0` installed.
- Wrote `~/.config/systemd/user/amb82-recorder.service` (Type=simple, Restart=on-failure,
  env vars baked in: broker 127.0.0.1, anon, CLIPS_DIR=%h/amb82_clips).
- `loginctl enable-linger`, `systemctl --user enable --now amb82-recorder.service`.
- Service active, journal shows `Connected to MQTT broker (rc=Success)` and
  `Subscribed to camera/+/motion`. Phase 2 complete.

## Session: 2026-04-06

### Phase 1: Research & Architecture Design
- **Status:** complete
- **Started:** 2026-04-06
- Actions taken:
  - Explored AMB82 Mini Arduino SDK APIs via web research
  - Documented all relevant classes: MotionDetection, RecordMP4, VideoSetting, Camera, StreamIO, WiFiClient, AmebaMQTTClient, File, NTPClient, PowerMode
  - Analyzed server architecture options (HA+MQTT+MinIO vs NAS vs Cloud vs NVR)
  - Analyzed power strategy (deep sleep incompatible with camera motion detection)
- Files created/modified:
  - task_plan.md (created)
  - findings.md (created)
  - progress.md (created)

### Phase 2: Feasibility Assessment & Power Strategy
- **Status:** complete
- Actions taken:
  - Documented power strategy: dual-channel (low-res detect + high-res record)
  - Deep sleep ruled out — camera pipeline must stay active for motion detection
  - Power savings from: low FPS detection, stopping record channel when idle
- Files modified:
  - findings.md (updated with power strategy)

### Phase 3: Firmware Implementation — Core
- **Status:** complete
- Actions taken:
  - Created config.h with all configuration constants and placeholders
  - Created wifi_manager.h/.cpp with connect/reconnect/RSSI
  - Implemented NTP time sync in main .ino
  - Implemented dual-channel camera pipeline in amb82_rec.ino
  - Implemented motion detection via MotionDetection SDK class
- Files created:
  - config.h
  - wifi_manager.h
  - wifi_manager.cpp
  - amb82_rec.ino

### Phase 4: Firmware Implementation — Recording & Upload
- **Status:** complete
- Actions taken:
  - Implemented recording state machine (IDLE → RECORDING → POST_ROLL → FINALIZING)
  - 10-second post-roll with seamless resume if motion returns
  - Timestamp-based filenames via NTP epoch
  - HTTP PUT upload to MinIO
  - Upload retry queue (10 slots)
- Files created:
  - upload_manager.h
  - upload_manager.cpp

### Phase 5: Firmware Implementation — MQTT & Notifications
- **Status:** complete
- Actions taken:
  - Implemented MqttManager with PubSubClient
  - MQTT topics: status, battery, motion, clip, availability (LWT)
  - Edge-triggered battery alerts (only fires on threshold crossing)
  - Clip-ready notification with metadata (filename, duration, battery, RSSI)
  - Periodic status publishing (every 5 minutes)
- Files created:
  - mqtt_manager.h
  - mqtt_manager.cpp
  - battery_monitor.h
  - battery_monitor.cpp

### Phase 6: Power Optimization
- **Status:** complete
- Actions taken:
  - Detection channel: 640×480 @ 5fps (minimal power)
  - Recording channel: 1920×1080 @ 20fps (only during events)
  - Recording channel stopped after clip finalization
  - Battery check every 60s, status report every 5min
  - Upload queue processed only during idle

### Phase 7: Server-Side Integration
- **Status:** complete
- Actions taken:
  - Created docker-compose.yml (Mosquitto + MinIO + Home Assistant)
  - Created Mosquitto config with authentication
  - Created Home Assistant automation examples (clip notification, battery alert, offline alert)
  - Created MQTT sensor configuration examples
- Files created:
  - server/docker-compose.yml
  - server/mosquitto/config/mosquitto.conf
  - server/ha_automations.yaml

### Phase 8: Documentation & Delivery
- **Status:** complete
- Actions taken:
  - Created ARCHITECTURE.md with full system design, diagrams, setup procedure
  - Documented known limitations and assumptions
  - Updated CLAUDE.md
  - Updated task_plan.md (all phases complete)
- Files created:
  - ARCHITECTURE.md

## 5-Question Reboot Check
| Question | Answer |
|----------|--------|
| Where am I? | All 8 phases complete |
| Where am I going? | Delivery — awaiting user review and hardware testing |
| What's the goal? | AMB82 Mini motion-triggered video recorder with server integration |
| What have I learned? | See findings.md — SDK APIs, power constraints, architecture |
| What have I done? | Full firmware + server stack + documentation delivered |
