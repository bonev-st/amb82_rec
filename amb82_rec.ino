/*
 * AMB82 Mini Motion-Triggered RTSP Streamer
 *
 * Detects motion via on-board camera pipeline (Channel 3, RGB).
 *
 * RTSP streaming model: the Ch0 H264 FHD encoder and the RTSP server run
 * CONTINUOUSLY once started. Dynamically re-starting them per motion event
 * produced decodable-but-static streams (cached IDR), so we leave them
 * always-on after bring-up. Motion events become MQTT notifications that
 * tell the server "now is an interesting moment to record from :554 for a
 * while"; the recorder pulls the stream on demand.
 *
 * Boot is driven by a linear init state machine in loop(): WiFi -> NTP ->
 * MQTT -> CAMERA_START -> CAMERA_WARMUP -> READY. setup() only brings up
 * internal peripherals that have no external dependency (LEDs, UART, WDT,
 * ADC, NTP UDP socket, TLS cert material). Everything that touches the
 * network, AND the camera itself, is deferred to the state machine so the
 * Ch0 H264 encoder -- the largest steady-state power draw -- does NOT
 * start until the MQTT upload path is proven reachable. If WiFi or MQTT
 * never come up, the encoder never starts.
 *
 * On a WiFi drop during READY we demote back to INIT_WIFI and re-run the
 * NTP/MQTT handshake (pullConfig + fresh status publish) on recovery, but
 * leave the camera running (Option A -- see CLAUDE.md "Runtime WiFi-loss
 * behavior" for why, and how Option B would differ).
 *
 * Based on official Ameba Arduino SDK examples:
 *   - MotionDetection/CallbackPostProcessing
 *   - StreamRTSP/VideoOnly
 *   - MotionDetection/MaskingMP4Recording
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <StreamIO.h>
#include <VideoStream.h>
#include <RTSP.h>
#include <MotionDetection.h>
#include <time.h>  // time_t for rtc_write signature below
// rtc_write() from rtc_api.h is the SDK-linkable symbol (in liboutsrc.a)
// that writes the hardware RTC. mbedTLS reads from this RTC to check cert
// notBefore/notAfter during TLS handshake. Note: set_time() declared in
// rtc_time.h is NOT compiled into the platform libs -- use rtc_write().
extern "C" void rtc_init(void);
extern "C" void rtc_write(time_t t);

#include "config.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "battery_monitor.h"
#include "led_manager.h"

#if WDT_ENABLED
#include <WDT.h>
#endif

// ============================================================
// Global Objects
// ============================================================

// Channel 0: High-res H264 for RTSP streaming (configured + started in
// INIT_CAMERA_START once external services are up).
VideoSetting configStream(VIDEO_FHD, CAM_FPS, VIDEO_H264, 0);

// Channel 3: Low-res RGB for motion detection (same deferral).
VideoSetting configMD(VIDEO_VGA, DETECT_FPS, VIDEO_RGB, 0);

RTSP rtsp;
MotionDetection motionDet;

StreamIO videoToRtsp(1, 1);     // Camera Ch0 -> RTSP
StreamIO videoToMotion(1, 1);   // Camera Ch3 -> MotionDetection

// NTP
WiFiUDP ntpUDP;
// Start in UTC; timezone is set at runtime by the MQTT config message
// (see MqttManager::mqttCallback -> timeClient.setTimeOffset).
NTPClient timeClient(ntpUDP, NTP_SERVER, 0, NTP_UPDATE_INTERVAL);

// Managers
WifiManager wifiMgr;
MqttManager mqttMgr;
BatteryMonitor batteryMon;
LedManager ledMgr;

#if MQTT_USE_TLS
WiFiSSLClient mqttWifiClient;
#else
WiFiClient mqttWifiClient;
#endif

#if WDT_ENABLED
WDT wdt(0);  // SDK bug: WDT() declared but not defined; use WDT(int) constructor
#endif

// RTSP URL buffer
char rtspUrl[64] = {0};

// ============================================================
// State Machines
// ============================================================

// Motion/record state -- only meaningful while initState == READY.
//
// Two states, not three: the former POST_ROLL state was folded into
// MOTION_ACTIVE because the post-roll timer is simply "MOTION_POST_ROLL_MS
// since the last motion-positive tick." A flickering detector used to
// ping-pong MOTION_ACTIVE <-> POST_ROLL, which made the "time since last
// motion" semantic hard to reason about in logs. Now motionLastSeenMs is
// refreshed every tick motion is present, and the exit condition reads as
// plainly as the spec: stop 10 s after the last motion detection.
enum MotionRecordState {
    STATE_IDLE,          // no motion; server is not recording
    STATE_MOTION_ACTIVE  // motion seen within the last MOTION_POST_ROLL_MS
};

// Boot / recovery state -- drives loop() until READY, and demotes back
// to INIT_WIFI on any WiFi loss during READY.
enum InitState {
    INIT_WIFI,
    INIT_NTP,
    INIT_MQTT,
    INIT_CAMERA_START,
    INIT_CAMERA_WARMUP,
    READY
};

InitState   initState       = INIT_WIFI;
MotionRecordState streamState = STATE_IDLE;

unsigned long bootStartMs   = 0;     // millis() at setup() end; drives BOOT_INIT_TIMEOUT_MS
bool reachedReadyOnce       = false; // after first READY entry, WDT never self-reboots from init

unsigned long lastWifiAttemptMs = 0;
unsigned long lastNtpAttemptMs  = 0;
int           ntpAttemptCount   = 0; // resets on NTP entry; used for non-TLS soft-fail only
unsigned long lastMqttAttemptMs = 0;
unsigned long warmupStartMs     = 0;

// true after the camera + RTSP + motion pipeline is brought up once. Stays
// true across WiFi-loss demotions so INIT_CAMERA_START is skipped on
// recovery -- see "Runtime WiFi-loss behavior" in CLAUDE.md (Option A).
bool cameraStarted = false;

// millis() of the most recent motion-positive tick while in MOTION_ACTIVE.
// The post-roll expires MOTION_POST_ROLL_MS after this value. Refreshed
// every tick motionDetected is true; never reset when it's false. Kept
// around across state transitions so a resumed motion during post-roll
// just pushes the expiry forward naturally.
unsigned long motionLastSeenMs  = 0;
unsigned long streamStartTime   = 0;
unsigned long lastStatusTime    = 0;

// Rate-limits motion-start publish retries while in MOTION_ACTIVE. Without
// this the state machine would re-attempt (and re-log) every loop tick
// (~100 ms) whenever publish() keeps returning false -- e.g. a half-open
// TCP that PubSubClient::connected() still reports as alive. 0 = no attempt
// yet since entering MOTION_ACTIVE; reset on every IDLE->MOTION_ACTIVE.
unsigned long lastMotionStartPublishMs = 0;

// true after a successful motion-start publish, false after a successful
// motion-end publish. Drives the green LED (solid = server was notified,
// blink = motion detected but notification didn't get through).
bool startPublished = false;

// Set once, when a valid NTP epoch is first written to the hardware RTC.
// Used to keep the RTC write idempotent, to gate INIT_MQTT when TLS is
// enabled, and to drive the blue status LED.
bool rtcInitialized = false;

// ============================================================
// Forward declarations
// ============================================================
static void ensureRTC();
static void tickInitWifi(unsigned long now);
static void tickInitNtp(unsigned long now);
static void tickInitMqtt(unsigned long now);
static void tickInitCameraStart(unsigned long now);
static void tickInitCameraWarmup(unsigned long now);
static void tickReady(unsigned long now);
static void updateLeds();
bool checkMotion();
void handleMotionRecordState(bool motionDetected, unsigned long now);
bool onMotionStart();
bool onMotionEnd();

// Write the NTP epoch to the hardware RTC the first time we see a valid
// value. mbedTLS reads the RTC (via time()) to check cert notBefore/notAfter
// during TLS handshake -- without this, every cert looks "not yet valid"
// and handshake fails with -0x2700.
static void ensureRTC() {
    if (rtcInitialized) return;
    unsigned long epoch = timeClient.getEpochTime();
    if (epoch < NTP_VALID_EPOCH_MIN) return;
    rtc_init();
    rtc_write((time_t)epoch);
    rtcInitialized = true;
    LOGF("[RTC] System time set to epoch %lu\n", epoch);
}

// ============================================================
// Setup -- internal peripherals only
// ============================================================
// Deliberately does NOT touch WiFi, NTP, MQTT, or the camera. Those are
// driven by the init state machine in loop() so the Ch0 H264 encoder
// stays off while the upload path is down. NTP in particular is held off
// until WiFi is up: WiFiUDP::begin() calls socket()/bind() via LWIP, which
// fails (-> "start_server Opening socket") if called before the netif is
// ready. tickInitWifi() runs timeClient.end()/begin() on WL_CONNECTED.
void setup() {
    // LEDs first -- both pins driven LOW so neither floats during early boot.
    ledMgr.begin(REC_LED_PIN, STATUS_LED_PIN);

#ifndef BUILD_RELEASE
    Serial.begin(SERIAL_BAUD);
    delay(2000);  // Let serial monitor attach
#endif
    LOG("\n========================================");
    LOGF(" %s v%s [%s]\n", DEVICE_NAME, FIRMWARE_VERSION, FIRMWARE_BUILD);
    LOG("========================================");

    // Watchdog before anything that could hang. In RELEASE, if we don't
    // reach READY within BOOT_INIT_TIMEOUT_MS the main loop stops refreshing
    // and the board reboots clean.
#if WDT_ENABLED
    wdt.init(WDT_TIMEOUT_MS);
    wdt.start();
    LOG("[WDT] Watchdog started (30s timeout)");
#endif

    // ADC-only: safe, no external dependency.
    batteryMon.begin();

    // TLS cert material is pure memory state -- sets pointers inside the
    // global WiFiSSLClient. No I/O; safe in setup().
#if MQTT_USE_TLS
    mqttWifiClient.setRootCA((unsigned char*)mqtt_ca_cert);
    mqttWifiClient.setClientCertificate((unsigned char*)mqtt_client_cert,
                                        (unsigned char*)mqtt_client_key);
    LOG("[MQTT] TLS + mTLS configured (port 8883); handshake deferred to INIT_MQTT");
#else
    LOG("[MQTT] Plain connection configured (port 1883)");
#endif

    bootStartMs  = millis();
    initState    = INIT_WIFI;
    LOG("[Setup] Internal peripherals ready -- init state machine now drives loop()");
    LOG("========================================\n");
}

// ============================================================
// Init state handlers (non-blocking, timer-gated)
// ============================================================

static void tickInitWifi(unsigned long now) {
    if (WiFi.status() == WL_CONNECTED) {
        // Open the NTP UDP listen socket now that LWIP has a netif + DHCP
        // IP, then sync immediately -- the same back-to-back sequence that
        // tests/test_ntp/test_ntp.ino uses and that passes there. Calling
        // begin() earlier (e.g. in setup()) fails at socket() with
        // "[ERROR] start_server Opening socket" and leaves _sock=-1, which
        // silently breaks forceUpdate()'s receive path. end() is a no-op
        // on cold boot (_sock=-1) and releases the old socket on a
        // WiFi-reconnect demotion from READY.
        timeClient.end();
        timeClient.begin();
        LOGF("[Init] WIFI -> NTP (RSSI %d)\n", wifiMgr.getRSSI());
        ntpAttemptCount  = 1;
        lastNtpAttemptMs = 0;
		initState = INIT_NTP;
        return;
    }

    if (lastWifiAttemptMs != 0 && now - lastWifiAttemptMs < INIT_WIFI_RETRY_MS) return;

    LOG("[Init] Attempting WiFi association...");
    wifiMgr.begin();                  // blocking up to WIFI_BOOT_TIMEOUT_MS (8s)
    lastWifiAttemptMs = millis();     // timestamp AFTER the blocking call so the
                                      // 5s gate measures idle time between attempts,
                                      // matching the original ~13s boot retry cadence.
    // On success, next tick detects WL_CONNECTED and advances.
}

static void tickInitNtp(unsigned long now) {
    // Belt-and-braces: if the link dropped while we were in NTP, go back
    // to INIT_WIFI rather than blasting UDP into the void.
    if (WiFi.status() != WL_CONNECTED) {
        LOG("[Init] NTP: WiFi lost, demoting to INIT_WIFI");
        initState = INIT_WIFI;
        return;
    }

    // First call acts immediately; subsequent calls are spaced.
    if (lastNtpAttemptMs != 0 && now - lastNtpAttemptMs < INIT_NTP_RETRY_MS) return;
    lastNtpAttemptMs = now;
    ntpAttemptCount++;

    if (timeClient.forceUpdate()) {
        LOGF("[Init] NTP synced: %s (attempt %d)\n",
             timeClient.getFormattedTime().c_str(), ntpAttemptCount);
        ensureRTC();
    } else {
        LOGF("[Init] NTP attempt %d failed\n", ntpAttemptCount);
    }

    if (rtcInitialized) {
        LOG("[Init] NTP -> MQTT");
        initState         = INIT_MQTT;
        lastMqttAttemptMs = 0;
        return;
    }

#if !MQTT_USE_TLS
    // Plain MQTT: the broker doesn't need a real epoch for the handshake,
    // so after N failed NTP attempts we proceed and let timestamps fall
    // back to uptime-seconds (MqttManager::getEpochTime) until NTP later
    // recovers via the periodic timeClient.update() in tickReady().
    if (ntpAttemptCount >= INIT_NTP_MAX_ATTEMPTS_NONTLS) {
        LOG("[Init] NTP soft-fail (plain MQTT); proceeding with uptime-seconds timestamps");
        initState         = INIT_MQTT;
        lastMqttAttemptMs = 0;
    }
#endif
}

static void tickInitMqtt(unsigned long now) {
    if (WiFi.status() != WL_CONNECTED) {
        LOG("[Init] MQTT: WiFi lost, demoting to INIT_WIFI");
        initState = INIT_WIFI;
        return;
    }
#if MQTT_USE_TLS
    // TLS handshake reads the RTC. No point trying without it.
    if (!rtcInitialized) {
        LOG("[Init] MQTT: RTC unset, returning to INIT_NTP");
        initState = INIT_NTP;
        return;
    }
#endif

    if (lastMqttAttemptMs != 0 && now - lastMqttAttemptMs < INIT_MQTT_RETRY_MS) return;

    // MqttManager::begin() does pullConfig (retained config exchange) +
    // connect + will/LWT setup. Safe to call repeatedly -- on re-entry
    // after a demotion, this re-pulls the retained config message so any
    // timezone change made during the outage takes effect immediately.
    // Blocks for up to several seconds on TLS handshake; timestamp is taken
    // AFTER so the retry gate counts idle time between attempts.
    mqttMgr.begin(mqttWifiClient, timeClient);
    lastMqttAttemptMs = millis();
    if (!mqttMgr.connected()) {
        LOG("[Init] MQTT: begin() did not leave client connected; will retry");
        return;
    }

    // "Transmit initial message" -- the first status publish after every
    // successful (re)connect. Boot-time battery values from batteryMon.begin()
    // are used on cold boot; post-demotion uses the last READY-period sample.
    mqttMgr.publishStatus(batteryMon.getPercentage(),
                          batteryMon.getVoltage(),
                          wifiMgr.getRSSI());
    lastStatusTime = now;

    if (cameraStarted) {
        LOG("[Init] MQTT -> READY (camera already running)");
        initState        = READY;
        reachedReadyOnce = true;
    } else {
        LOG("[Init] MQTT -> CAMERA_START");
        initState = INIT_CAMERA_START;
    }
}

static void tickInitCameraStart(unsigned long now) {
    // One-shot synchronous bring-up. Matches the old setup() Phase B code
    // exactly -- no changes to camera config or pipeline wiring.
    LOG("[Init] Starting Ch0 (H264 FHD) + Ch3 (RGB) + RTSP + motion detection");

    configStream.setBitrate(RTSP_BITRATE);
    Camera.configVideoChannel(RTSP_CHANNEL, configStream);
    Camera.configVideoChannel(DETECT_CHANNEL, configMD);
    Camera.videoInit();

    rtsp.configVideo(configStream);
    rtsp.begin();
    videoToRtsp.registerInput(Camera.getStream(RTSP_CHANNEL));
    videoToRtsp.registerOutput(rtsp);
    if (videoToRtsp.begin() != 0) {
        LOG("[Pipeline] ERROR: videoToRtsp connection failed");
    }
    Camera.channelBegin(RTSP_CHANNEL);

    motionDet.configVideo(configMD);
    motionDet.begin();
    videoToMotion.registerInput(Camera.getStream(DETECT_CHANNEL));
    videoToMotion.setStackSize();
    videoToMotion.registerOutput(motionDet);
    if (videoToMotion.begin() != 0) {
        LOG("[Pipeline] ERROR: videoToMotion connection failed");
    }
    Camera.channelBegin(DETECT_CHANNEL);

    cameraStarted   = true;
    warmupStartMs   = now;
    initState       = INIT_CAMERA_WARMUP;
    LOG("[Init] CAMERA_START -> CAMERA_WARMUP (non-blocking 8s)");
}

static void tickInitCameraWarmup(unsigned long now) {
    if (now - warmupStartMs < INIT_CAMERA_WARMUP_MS) return;
    LOGF("[Init] Warm-up done (MD count: %u) -> READY\n", motionDet.getResultCount());
    initState        = READY;
    reachedReadyOnce = true;
}

static void tickReady(unsigned long now) {
    // Motion detection first -- highest priority work, never blocked.
    bool motionDetected = checkMotion();
    handleMotionRecordState(motionDetected, now);

    // Battery sampling is strictly confined to READY so hasNewAlert()'s
    // edge-detection doesn't get clobbered while we can't publish.
    batteryMon.update();
    if (batteryMon.hasNewAlert()) {
        mqttMgr.publishBatteryAlert(
            batteryMon.getAlertLevel(),
            batteryMon.getPercentage(),
            batteryMon.getVoltage()
        );
    }

    // Periodic status publish (every MQTT_STATUS_INTERVAL_MS = 1 hour)
    if (now - lastStatusTime >= MQTT_STATUS_INTERVAL_MS) {
        lastStatusTime = now;
        mqttMgr.publishStatus(
            batteryMon.getPercentage(),
            batteryMon.getVoltage(),
            wifiMgr.getRSSI()
        );
    }

    // IMPORTANT: there is no mqttMgr.loop() call (and MqttManager no longer
    // exposes one). On the Ameba-patched PubSubClient / WiFiClient stack,
    // calling loop() blocks the main task for tens of seconds at a time
    // which freezes motion detection.
    //   - MqttManager::begin() calls setKeepAlive(0), so the broker never
    //     disconnects us for keepalive timeout.
    //   - publishMotionEvent() has its own force-reconnect path
    //     (mqtt_manager.cpp) and is the only code that touches MQTT state
    //     from the loop. Motion start/stop events reconnect on demand.
    if (streamState == STATE_IDLE) {
        wifiMgr.ensureConnected();   // non-blocking
        timeClient.update();         // self-throttled hourly re-sync
        ensureRTC();                 // no-op if already set
        mqttMgr.checkConfig();       // retained config poll
    }

    // Demotion trigger: WiFi loss knocks us back to the init state machine.
    // The NTP + MQTT handshake re-runs on recovery (re-pull retained config
    // + fresh status publish). Camera is left running -- see "Runtime
    // WiFi-loss behavior" in CLAUDE.md.
    if (WiFi.status() != WL_CONNECTED) {
        LOG("[Init] READY -> INIT_WIFI (WiFi dropped; camera left running)");
        initState         = INIT_WIFI;
        lastWifiAttemptMs = 0;   // allow immediate reconnect attempt
    }
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
    unsigned long now = millis();

    switch (initState) {
        case INIT_WIFI:           tickInitWifi(now);          break;
        case INIT_NTP:            tickInitNtp(now);           break;
        case INIT_MQTT:           tickInitMqtt(now);          break;
        case INIT_CAMERA_START:   tickInitCameraStart(now);   break;
        case INIT_CAMERA_WARMUP:  tickInitCameraWarmup(now);  break;
        case READY:               tickReady(now);             break;
    }

#if WDT_ENABLED
    // Cold-boot watchdog escalation: if we've never reached READY and the
    // init states have been running longer than BOOT_INIT_TIMEOUT_MS, stop
    // refreshing so the WDT reboots us. Runtime demotions (after at least
    // one READY) never self-reboot -- we just wait for the AP to come back.
    bool stuck_at_boot = !reachedReadyOnce && (now - bootStartMs) > BOOT_INIT_TIMEOUT_MS;
    if (!stuck_at_boot) {
        wdt.refresh();
    } else {
        static bool logged = false;
        if (!logged) {
            LOG("[WDT] Boot init exceeded BOOT_INIT_TIMEOUT_MS -- ceasing refresh, board will reset");
            logged = true;
        }
    }
#endif

    updateLeds();

    // Adaptive delay: fastest during active streaming, idle otherwise.
    bool active = (initState == READY && streamState != STATE_IDLE);
    delay(active ? LOOP_DELAY_ACTIVE_MS : LOOP_DELAY_IDLE_MS);
}

// ============================================================
// LEDs
// ============================================================
static void updateLeds() {
    // Green: motion/record. Only meaningful once READY.
    LedManager::Mode green = LedManager::LED_OFF;
    if (initState == READY && streamState != STATE_IDLE) {
        green = startPublished ? LedManager::LED_ON : LedManager::LED_BLINK_FAST;
    }
    ledMgr.setGreen(green);

    // Blue: system health ladder (same semantics as before; the init state
    // machine's gates line up with these checks naturally).
    LedManager::Mode blue;
    if (WiFi.status() != WL_CONNECTED)      blue = LedManager::LED_ON;
    else if (!rtcInitialized)               blue = LedManager::LED_BLINK_FAST;
    else if (!mqttMgr.connected())          blue = LedManager::LED_BLINK_SLOW;
    else                                    blue = LedManager::LED_OFF;
    ledMgr.setBlue(blue);

    ledMgr.update();
}

// ============================================================
// Motion Detection
// ============================================================
bool checkMotion() {
    uint16_t count = motionDet.getResultCount();
#ifndef BUILD_RELEASE
    // Log only on count change so the console isn't drowned by idle heartbeats.
    static uint16_t lastCount = 0xFFFF;
    if (count != lastCount) {
        LOGF("[MD] regions=%u\n", count);
        lastCount = count;
    }
#endif
    return count >= MOTION_DETECT_SENSITIVITY;
}

// ============================================================
// Streaming State Machine
// ============================================================
void handleMotionRecordState(bool motionDetected, unsigned long now) {
    switch (streamState) {
        case STATE_IDLE:
            if (motionDetected) {
                // Transition on motion detection, NOT on publish success.
                // Two reasons:
                //   1. The green LED reads streamState to decide whether to
                //      light at all (off in IDLE, blink-fast or solid in
                //      MOTION_ACTIVE depending on startPublished). Gating the
                //      transition on publish meant a broken broker pinned us
                //      in IDLE and the LED never turned on.
                //   2. Gating the transition on publish also caused a log
                //      storm: every ~100 ms loop tick we'd re-attempt publish
                //      and re-log "[RTSP] Motion-start" + "publish=FAIL".
                // Now MOTION_ACTIVE does the retrying at a rate-limited cadence
                // and startPublished drives the LED distinction.
                streamState              = STATE_MOTION_ACTIVE;
                motionLastSeenMs         = now;   // anchor the post-roll timer
                streamStartTime          = now;   // for motion-stop duration log
                startPublished           = false;
                lastMotionStartPublishMs = 0;     // 0 -> attempt on first MOTION_ACTIVE tick
                // Build the URL once per motion burst; reused by the retry path.
                snprintf(rtspUrl, sizeof(rtspUrl), "rtsp://%s:%d",
                         WiFi.localIP().get_address(), rtsp.getPort());
                LOGF("[RTSP] Motion-start: %s\n", rtspUrl);
                LOG("[State] IDLE -> MOTION_ACTIVE");
            }
            break;

        case STATE_MOTION_ACTIVE:
            if (motionDetected) {
                // Every positive tick pushes the stop deadline forward by
                // MOTION_POST_ROLL_MS.
                motionLastSeenMs = now;
            }

            // Keep retrying the motion-start publish until it lands. Rate
            // limit matches MqttManager's motion-reconnect gate so we don't
            // double up on the attempt cadence when the failure mode is a
            // dead TCP that still reports connected().
            if (!startPublished &&
                (lastMotionStartPublishMs == 0 ||
                 now - lastMotionStartPublishMs >= MQTT_MOTION_RECONNECT_INTERVAL_MS)) {
                lastMotionStartPublishMs = now;
                onMotionStart();  // sets startPublished on success
            }

            if (!motionDetected && now - motionLastSeenMs >= MOTION_POST_ROLL_MS) {
                // Post-roll expired. Exit unconditionally -- a retry loop on
                // motion-end would just reintroduce the storm, and if start
                // never landed the server has no open recording to close.
                if (startPublished) {
                    onMotionEnd();  // best effort; return value ignored
                }
                LOGF("[State] MOTION_ACTIVE -> IDLE (%lu ms since last motion)\n",
                     now - motionLastSeenMs);
                streamState    = STATE_IDLE;
                startPublished = false;
            }
            break;
    }
}

// ============================================================
// Motion event notification
// ============================================================
// Ch0, RTSP server, and Ch0->RTSP pipeline run continuously once brought up
// in INIT_CAMERA_START. These helpers just publish the MQTT event; rtspUrl
// and streamStartTime are owned by the state machine (set on IDLE ->
// MOTION_ACTIVE), and retry cadence is driven there too. Return value
// reflects whether the publish was accepted by the TCP stack.
bool onMotionStart() {
    bool ok = mqttMgr.publishMotionEvent(true, rtspUrl);
    if (ok) startPublished = true;
    return ok;
}

// Publishes the motion-stop MQTT event. Called once on MOTION_ACTIVE->IDLE
// when startPublished was true; return value is informational.
bool onMotionEnd() {
    unsigned long durationMs = millis() - streamStartTime;
    LOGF("[RTSP] Motion-stop after %.1f seconds\n", durationMs / 1000.0f);

    bool ok = mqttMgr.publishMotionEvent(false, NULL);
    if (!ok) return false;

    mqttMgr.publishStatus(batteryMon.getPercentage(), batteryMon.getVoltage(), wifiMgr.getRSSI());
    lastStatusTime = millis();  // Reset hourly timer so we don't double-publish
    return true;
}
