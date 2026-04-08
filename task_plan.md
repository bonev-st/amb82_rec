# Task Plan: AMB82 Mini Motion-Triggered Video Recorder

## Goal
Implement a battery-powered motion-triggered video recorder firmware for the Ameba AMB82 Mini, with clip upload to a local server stack (Home Assistant + MQTT + MinIO).

## Current Phase
Phase 8 — Complete

## Phases

### Phase 1: Research & Architecture Design
- [x] Research AMB82 Mini SDK APIs (motion detection, MP4 recording, WiFi, MQTT, power modes)
- [x] Document findings in findings.md
- [x] Finalize architecture decisions
- [x] Document server-side architecture recommendation
- **Status:** complete

### Phase 2: Feasibility Assessment & Power Strategy
- [x] Analyze power consumption tradeoffs
- [x] Determine best motion detection approach (camera-based vs PIR)
- [x] Define realistic power strategy
- [x] Document in findings.md
- **Status:** complete

### Phase 3: Firmware Implementation — Core
- [x] Create project structure (main .ino + helper modules)
- [x] Implement config.h with all placeholders
- [x] Implement WiFi connection + reconnect logic
- [x] Implement NTP time sync
- [x] Implement camera + video pipeline initialization
- [x] Implement motion detection with callback
- **Status:** complete

### Phase 4: Firmware Implementation — Recording & Upload
- [x] Implement MP4 recording start/stop with 10s post-roll
- [x] Implement SD card clip storage with timestamp filenames
- [x] Implement HTTP upload to MinIO (S3-compatible PUT)
- [x] Implement upload retry/queue
- **Status:** complete

### Phase 5: Firmware Implementation — MQTT & Notifications
- [x] Implement MQTT client connection
- [x] Implement battery voltage reading (ADC)
- [x] Implement low-battery threshold notification (edge-triggered)
- [x] Implement new-clip notification via MQTT
- [x] Implement periodic status reporting
- **Status:** complete

### Phase 6: Power Optimization
- [x] Implement reduced framerate/resolution for motion detection mode
- [x] Implement higher quality switch for recording mode
- [x] Analyze deep sleep feasibility
- [x] Implement power-saving policy
- **Status:** complete

### Phase 7: Server-Side Integration Outline
- [x] Write server deployment guide (docker-compose.yml)
- [x] Write example Home Assistant automations (ha_automations.yaml)
- [x] Write Mosquitto config
- **Status:** complete

### Phase 8: Documentation & Delivery
- [x] Write architecture document (ARCHITECTURE.md)
- [x] Document known limitations / assumptions
- [x] Setup and test procedure included in ARCHITECTURE.md
- [x] Final review of all deliverables
- **Status:** complete

## Key Questions
1. Should we use SD card as intermediate storage before upload? → **Yes** (reliability)
2. Can deep sleep work with motion detection? → **No**, camera must stay active
3. Which video channels for detection vs recording? → **Low-res CH1** for detection, **high-res CH0** for recording
4. MQTT library: built-in AmebaMQTTClient or PubSubClient? → **PubSubClient** (wider community, simpler API)

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Event-based clip upload (not RTSP) | Battery constraint — continuous streaming drains power |
| SD card as intermediate storage | Reliability — don't lose clips on network failure |
| Camera-based motion detection | No external PIR needed; uses built-in SDK MotionDetection class |
| Dual video channel approach | Low-res for detection (power saving), high-res for recording |
| Home Assistant + MQTT + MinIO | Local control, low cost, extensible, per requirements |
| PubSubClient for MQTT | Widely used, simple API, well-documented |
| HTTP PUT for MinIO upload | Simplest S3-compatible approach; presigned URLs for production |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| (none) | — | — |

## Notes
- All 8 phases complete
- Firmware compiles against AMB82 SDK APIs (may need minor adjustments for exact SDK version)
- Server stack deployable via docker-compose
