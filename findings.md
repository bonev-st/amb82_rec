# Findings — Power Optimization & Release Build

## Code Review: Power Waste Inventory

### 1. Serial logging is always active (biggest easy win)
- `checkMotion()` prints MD region count every 500ms — in a production device
  this is pure waste (serial TX draws current + CPU cycles for `printf` formatting)
- Every module (`wifi_manager`, `mqtt_manager`, `battery_monitor`) has LOG/LOGF calls
- The existing `DEBUG_ENABLED` flag already gates `LOG`/`LOGF` macros — good foundation
- **But:** the 2s `delay(2000)` in `setup()` and `Serial.begin()` itself are still active in release

### 2. Main loop timing is fixed at 100ms
- `delay(100)` in `loop()` regardless of state
- In `STATE_IDLE` (which is 99% of the time), nothing time-sensitive happens
  — motion check + battery update + WiFi check can easily tolerate 500ms
- In `STATE_STREAMING`/`STATE_POST_ROLL`, 100ms is fine for responsive state transitions

### 3. Channel 0 (H264 FHD 30fps) runs continuously
- This is the **single largest power consumer** in the system
- Comment in code explains why: dynamically toggling Ch0 produced broken H264 streams
- **Cannot optimize without risking recording quality** — this is a known SDK limitation
- Possible future experiment: reduce Ch0 to 15fps or HD when idle, bump to FHD 30fps on motion

### 4. Detection channel at 10fps
- `config.h` defines `DETECT_FPS 10` for Channel 3 (VGA RGB)
- SDK examples use 10fps, but 5fps should be sufficient for motion detection
- Reducing to 5fps halves the RGB processing load on the detection pipeline

### 5. Battery ADC sampling blocks for 20ms
- `readVoltage()` reads 10 samples with `delay(2)` between each = 20ms blocked
- Called every 60s, so total impact is low (20ms/60000ms = 0.03%)
- Could be optimized but **not worth the complexity** for this interval

### 6. checkConfig() creates a new TCP+MQTT connection every 60s
- Each `pullConfig()` call: TCP connect → MQTT CONNECT → SUBSCRIBE → 3×loop() → DISCONNECT
- This is ~200-500ms of network activity every minute
- In release, checking every 5 minutes is sufficient (config changes are rare)

## Release vs Debug: What Needs to Change

| Feature | DEBUG | RELEASE |
|---------|-------|---------|
| `Serial.begin()` | Yes (115200 baud) | Skip entirely |
| Boot delay (2s) | Yes | No |
| LOG/LOGF macros | Active | No-op (already implemented) |
| MD count logging (500ms) | Active | No-op |
| State transition logging | Active | No-op |
| MQTT payload logging | Active | No-op |
| Main loop delay | 100ms always | 500ms idle / 100ms streaming |
| checkConfig interval | 60s | 300s (5 min) |
| Detection FPS | 10 | 5 (power saving) |
| Boot banner with version | Verbose | Minimal or none |
| Firmware version in MQTT | Yes | Yes |

## Questions for User

### Q1: Detection FPS in release — 5fps ok?
Reducing from 10fps to 5fps saves processing power but increases the minimum
time to detect motion start/stop from ~100ms to ~200ms. For a security camera
this is negligible. **Recommendation: 5fps in release, 10fps in debug.**

### Q2: Main loop idle delay — 500ms ok?
In IDLE state, increasing `delay(100)` to `delay(500)` means:
- Motion detection response: up to 500ms slower (imperceptible for recording)
- WiFi reconnect check: 5× less frequent (fine, it's non-blocking)
- Battery update: unaffected (has its own 60s timer)
**Recommendation: 500ms idle, 100ms during streaming.**

### Q3: Battery check interval for release?
Currently 60s. On battery power, checking every 120s or even 300s is fine —
voltage doesn't change fast. **Recommendation: 120s in release.**

### Q4: Should RELEASE mode skip Serial entirely?
If `Serial.begin()` is never called, the UART peripheral stays powered down.
Downside: no way to debug a deployed unit without reflashing.
**Recommendation: Skip Serial in release — if you need to debug, flash DEBUG build.**

### Q5: Watchdog timer — desired?
The AMB82 SDK has a `WDT` (watchdog timer) class. If the main loop hangs
(e.g., WiFi stack deadlock), the watchdog reboots the board automatically.
**Recommendation: Enable in release with 30s timeout. Add `WDT.refresh()` in loop.**

### Q6: Firmware version scheme?
Suggest: `FIRMWARE_VERSION "1.0.0"` in `config.h`, included in MQTT status
messages and boot banner. Bump manually on each release.

### Q7: Should config polling be disabled entirely in release?
`checkConfig()` exists for runtime timezone changes via MQTT. If timezone is
set once and rarely changes, we could disable polling in release and only
read config at boot. **Recommendation: Keep but reduce to every 5 minutes.**
