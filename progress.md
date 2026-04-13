# Progress Log

## Session: 2026-04-13 — Power Optimization & Release Build

### Phase 1: Code Review & Analysis
- Read all source files, identified 6 power optimization opportunities
- Confirmed Channel 0 always-on is unavoidable (SDK limitation)
- Presented 7 questions to user — all accepted with defaults

### Phase 2-4: Implementation
- `config.h`: Added `BUILD_RELEASE`/`BUILD_DEBUG` toggle at top of file,
  `FIRMWARE_VERSION`, conditional `DETECT_FPS` (5/10), `BATTERY_CHECK_INTERVAL_MS`
  (120s/60s), `LOOP_DELAY_IDLE_MS` (500/100), `CONFIG_CHECK_INTERVAL_MS` (300s/60s),
  `WDT_ENABLED` + `WDT_TIMEOUT_MS` (30s), reworked LOG/LOGF macros (no-op in release)
- `amb82_rec.ino`: Conditional `Serial.begin()` + boot delay, version banner with
  `FIRMWARE_VERSION`/`FIRMWARE_BUILD`, WDT init+start in setup, `wdt.refresh()` in
  loop, adaptive `delay()` based on `streamState`, MD logging gated behind
  `#ifndef BUILD_RELEASE`, fixed `configMD` to use `DETECT_FPS` instead of hardcoded 10,
  fixed include order (WDT.h after config.h)
- `mqtt_manager.cpp`: Added `firmware`+`build` fields to status JSON, replaced
  hardcoded 60000 with `CONFIG_CHECK_INTERVAL_MS`
- `README.md`: Added "Build Modes" section with comparison table and switching guide
- `CLAUDE.md`: Added WDT API reference section and Build Modes reference

### Bugs found and fixed during implementation
1. `configMD(VIDEO_VGA, 10, ...)` had hardcoded FPS — changed to `DETECT_FPS`
2. `#if WDT_ENABLED` used before `#include "config.h"` — reordered includes

---

## Prior Sessions (archived)

### 2026-04-11 — Bench Deployment (Phases 1-7)
- Full bench deployment completed: Mosquitto, recorder.py, firmware flashed
- 108 clips recorded successfully, end-to-end verified

### 2026-04-06 — Initial Implementation (Phases 1-8)
- Full firmware + server stack + documentation delivered
