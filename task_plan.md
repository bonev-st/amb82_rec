# Task Plan: Power Optimization & Release Build

## Goal
Review all firmware code for power optimization opportunities and prepare a
release-ready build configuration. Both DEBUG and RELEASE builds must be
preserved — a single `#define` toggle in `config.h`.

## Current Phase
Phase 5 — Complete

## Phases

### Phase 1 — Code Review & Analysis
- [x] Read all source files
- [x] Identify power waste
- [x] Identify debug-only code paths
- [x] Draft plan and questions for user
- **Status:** complete

### Phase 2 — Build Configuration (DEBUG / RELEASE)
- [x] Create `BUILD_MODE` toggle in `config.h` (BUILD_RELEASE / BUILD_DEBUG)
- [x] Gate all verbose serial output behind DEBUG
- [x] Gate MD region-count spam (500ms logging) behind `#ifndef BUILD_RELEASE`
- [x] Add firmware version string + build mode to boot banner
- [x] Skip `Serial.begin()` and 2s boot delay in RELEASE
- [x] Use configurable `CONFIG_CHECK_INTERVAL_MS` (1min debug / 5min release)
- **Status:** complete

### Phase 3 — Power Optimizations
- [x] Adaptive main loop delay (500ms idle / 100ms streaming in RELEASE)
- [x] Reduce detection FPS (10fps debug / 5fps release)
- [x] Fix hardcoded `10` in configMD constructor → use `DETECT_FPS`
- [x] Increase battery check interval (60s debug / 120s release)
- [x] Reduce `checkConfig()` polling frequency (1min debug / 5min release)
- **Status:** complete

### Phase 4 — Reliability Hardening (Release)
- [x] Add WDT (30s timeout) gated behind `WDT_ENABLED` / `BUILD_RELEASE`
- [x] Fix include order: `WDT.h` after `config.h` (needs `WDT_ENABLED` defined)
- [x] Add firmware version + build mode to MQTT status payload
- **Status:** complete

### Phase 5 — Documentation
- [x] Add build mode section to README.md (table of differences, switching instructions)
- [x] Add WDT API reference to CLAUDE.md
- [x] Add build mode reference to CLAUDE.md
- [x] Update MQTT status topic payload example in README.md
- **Status:** complete

## Files Modified
| File | Changes |
|------|---------|
| `config.h` | BUILD_RELEASE/DEBUG toggle, FIRMWARE_VERSION, conditional FPS/intervals/delays/WDT, reworked LOG macros |
| `amb82_rec.ino` | WDT include+init+refresh, conditional Serial.begin, version banner, adaptive loop delay, configMD uses DETECT_FPS, debug-only MD logging |
| `mqtt_manager.cpp` | firmware/build fields in status JSON, configurable config poll interval |
| `README.md` | Build modes section with table, switching instructions |
| `CLAUDE.md` | WDT API reference, build mode reference |

## Power Audit Summary

### Optimized
| Issue | Fix | Savings |
|-------|-----|---------|
| Serial logging always active | Gated behind BUILD_DEBUG; Serial.begin skipped in release | UART off, no printf overhead |
| MD logging every 500ms | `#ifndef BUILD_RELEASE` block | No printf formatting in release |
| Main loop fixed 100ms delay | 500ms when idle (release) | 5x less CPU wake-ups when idle |
| Detection at 10fps | 5fps in release | ~50% less RGB processing |
| Battery check every 60s | 120s in release | Half the ADC sampling frequency |
| Config poll every 60s | 300s (5 min) in release | 5x fewer disposable MQTT connections |
| Boot delay 2s | Skipped in release | 2s faster boot |

### Cannot Optimize (Documented)
| Item | Reason |
|------|--------|
| Channel 0 always on | Toggling Ch0/RTSP dynamically produced broken H264 (static image) |
| 8s motion warm-up | Required for AE + background model |
| Deep sleep | Camera pipeline must be active for motion detection |
