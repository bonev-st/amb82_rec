# System Architecture — AMB82 Motion Camera

## 1. Feasibility Assessment

The AMB82 Mini (RTL8735B) is well-suited for this project:

- **Motion detection**: Built-in `MotionDetection` class in the SDK compares RGB frames from the JXF37 sensor — no external PIR needed
- **MP4 recording**: `MP4Recording` class handles H.264 encoding and MP4 muxing to SD card
- **Dual video channels**: The SDK supports multiple simultaneous video channels, enabling low-res detection + high-res recording
- **WiFi + HTTP**: `WiFiClient` supports HTTP PUT for S3-compatible uploads
- **MQTT**: Built-in `AmebaMQTTClient` or third-party `PubSubClient`
- **Limitation**: Camera-based motion detection requires the video pipeline to stay active — deep sleep is not compatible with continuous motion watching

## 2. Server Architecture

**Recommended stack: Home Assistant + MQTT + MinIO**

| Component | Role |
|-----------|------|
| **Mosquitto (MQTT)** | Device status, battery level, motion events, clip metadata |
| **MinIO** | S3-compatible local object storage for MP4 clips |
| **Home Assistant** | Automation hub — notifications, dashboards, clip links |
| **n8n / Node-RED** | Optional workflow automation (e.g., video post-processing) |

**Why this is the best tradeoff:**
- **Simplicity** — all components run as Docker containers on a single machine
- **Local control** — no cloud dependency, no subscriptions, privacy-preserving
- **Low cost** — free/open-source software on any old PC or Raspberry Pi 4+
- **Maintainability** — standard tools with large communities
- **Extensibility** — easy to add more cameras, object detection, or integrations

**Rejected alternatives:**
- NAS/shared folder — too basic, no automation or notifications
- Cloud storage — ongoing cost, latency, privacy concerns
- NVR (Frigate/ZoneMinder) — designed for RTSP streams, not event-driven clip uploads

## 3. Power Optimization Strategy

### Realistic Power Strategy
| Mode | Camera | MCU | WiFi | Notes |
|------|--------|-----|------|-------|
| **Idle (watching)** | Low-res 640×480 @ 5fps | Active | Connected | Minimum practical for detection |
| **Recording** | High-res 1080p @ 20fps | Active | Connected | Full quality during events |
| **Uploading** | Off (recording channel stopped) | Active | Active | Upload after clip finalized |

### Best Practical Compromise
- Detection channel: 640×480 @ 5fps H.264 — minimal processing and sensor power
- Recording channel: 1920×1080 @ 20fps H.264 — only active during motion events
- Recording channel is started on motion and stopped after finalization
- Battery measured every 60 seconds, status reported every 5 minutes
- Upload queue processed only during idle periods

### What Cannot Be Optimized Further
The fundamental requirement for camera-based motion-triggered video means:
1. The camera sensor must remain powered and capturing frames for motion detection
2. The MCU must remain active to process motion detection results
3. WiFi must stay connected for timely uploads and MQTT
4. **Deep sleep is not an option** while watching for motion — the entire video pipeline resets on wake

A PIR sensor could enable deep sleep between events, but at the cost of losing video-based motion detection quality and requiring additional hardware.

## 4. Firmware Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     AMB82 Mini Firmware                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────┐   ┌───────────────┐   ┌──────────────────┐   │
│  │ Camera   │──▶│MotionDetection│──▶│ State Machine    │   │
│  │ CH1 (det)│   │ (low-res)     │   │ IDLE → RECORDING │   │
│  └──────────┘   └───────────────┘   │ → POST_ROLL      │   │
│                                     │ → FINALIZING      │   │
│  ┌──────────┐   ┌───────────────┐   │ → IDLE            │   │
│  │ Camera   │──▶│ MP4Recording  │◀──┘                   │   │
│  │ CH0 (rec)│   │ (high-res)    │                       │   │
│  └──────────┘   │ → SD Card     │                       │   │
│                 └───────┬───────┘                        │   │
│                         │                                │   │
│                         ▼                                │   │
│                 ┌───────────────┐   ┌──────────────────┐ │   │
│                 │UploadManager  │──▶│ MinIO (HTTP PUT) │ │   │
│                 │ (retry queue) │   └──────────────────┘ │   │
│                 └───────────────┘                        │   │
│                                                          │   │
│  ┌──────────┐   ┌───────────────┐   ┌──────────────────┐│   │
│  │ Battery  │──▶│ MqttManager   │──▶│ Mosquitto (MQTT) ││   │
│  │ Monitor  │   │ (status/clips)│   └──────────────────┘│   │
│  └──────────┘   └───────────────┘                        │   │
│                                                          │   │
│  ┌──────────┐   ┌───────────────┐                        │   │
│  │ WiFi     │   │ NTP Client    │                        │   │
│  │ Manager  │   │ (timestamps)  │                        │   │
│  └──────────┘   └───────────────┘                        │   │
└─────────────────────────────────────────────────────────────┘
```

### Recording State Machine
```
IDLE ──(motion detected)──▶ RECORDING
                               │
                    (motion stops)
                               ▼
                          POST_ROLL ──(10s elapsed)──▶ FINALIZING ──▶ IDLE
                               │                           │
                    (motion resumes)              (stop MP4, upload,
                               │                  notify via MQTT)
                               ▼
                          RECORDING
```

## 5. File Structure

| File | Purpose |
|------|---------|
| `amb82_rec.ino` | Main firmware — setup, loop, state machine, recording control |
| `config.h` | All configuration constants with placeholders |
| `wifi_manager.h/.cpp` | WiFi connect/reconnect/RSSI |
| `mqtt_manager.h/.cpp` | MQTT publish (status, motion, clips, battery alerts) |
| `battery_monitor.h/.cpp` | ADC reading, percentage estimation, edge-triggered alerts |
| `upload_manager.h/.cpp` | HTTP PUT to MinIO with retry queue |

## 6. Known Limitations & Assumptions

1. **S3 authentication**: The upload uses plain HTTP PUT. For production, implement S3v4 signature or use presigned URLs generated by the server
2. **MP4 API**: The `mp4.setRecordingFileName()` and `mp4.begin()/end()` calls may need adjustment based on the exact SDK version — verify against the installed Ameba board package
3. **Motion detection sensitivity**: The `getResult()` return type and block counting logic should be verified against the SDK; the threshold may need tuning for your environment
4. **ADC calibration**: Battery voltage divider ratio and ADC reference voltage need calibration for your specific circuit
5. **SD card required**: An SD card must be inserted for recording
6. **No audio**: Current configuration uses video-only recording; adding audio requires `SingleVideoWithAudio` mode and microphone wiring
7. **Single camera**: Designed for one camera per device; multi-camera would need separate device IDs
8. **No TLS**: MQTT and HTTP connections are unencrypted; add TLS for production deployments on untrusted networks

## 7. Setup & Test Procedure

### Firmware
1. Install Arduino IDE and add Ameba board package URL: `https://github.com/ambiot/ambpro2_arduino`
2. Select board "AMB82 Mini" in Arduino IDE
3. Edit `config.h` — set WiFi credentials, MQTT broker IP, MinIO endpoint
4. Wire battery voltage divider to A0 (or change `BATTERY_ADC_PIN`)
5. Insert SD card into AMB82 Mini
6. Compile and upload via USB
7. Open Serial Monitor at 115200 baud to verify boot sequence

### Server
1. Install Docker and Docker Compose on your server
2. Run `docker-compose up -d` from the `server/` directory
3. Create MQTT password: `docker exec mosquitto mosquitto_passwd -c /mosquitto/config/password.txt mqtt_user`
4. Create MinIO bucket: Open `http://server-ip:9090`, login, create bucket `motion-clips`
5. Add HA automations from `server/ha_automations.yaml`
6. Add MQTT sensor config to Home Assistant `configuration.yaml`

### Verification Checklist
- [ ] Serial shows "Connected" for WiFi, MQTT, SD
- [ ] Wave hand in front of camera → Serial shows "IDLE → RECORDING"
- [ ] Stop moving → Serial shows "POST_ROLL" then "FINALIZING" after 10s
- [ ] Check SD card for `.mp4` file
- [ ] Check MinIO console for uploaded clip
- [ ] Check Home Assistant for new clip notification
- [ ] Check MQTT topics for status/battery/motion messages
