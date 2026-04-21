/*
 * AMB82 Mini Motion-Triggered RTSP Streamer
 *
 * Detects motion via on-board camera pipeline (Channel 3, RGB).
 *
 * RTSP streaming model: the Ch0 H264 FHD encoder and the RTSP server run
 * CONTINUOUSLY from boot. Dynamically re-starting them per motion event
 * produced decodable-but-static streams (cached IDR), so we leave them
 * always-on. Motion events become MQTT notifications that tell the server
 * "now is an interesting moment to record from :554 for a while"; the
 * recorder pulls the stream on demand.
 *
 * Tradeoff: Ch0 encoder power is spent even when no clip is being recorded.
 * Acceptable for the current battery target; reconsider if a working
 * start/stop sequence is found.
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

// Channel 0: High-res H264 for RTSP streaming (started on demand)
VideoSetting configStream(VIDEO_FHD, CAM_FPS, VIDEO_H264, 0);

// Channel 3: Low-res RGB for motion detection (always on)
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
// State Machine
// ============================================================
enum MotionRecordState {
    STATE_IDLE,          // no motion; server is not recording
    STATE_MOTION_ACTIVE, // motion detected, server has been told to record
    STATE_POST_ROLL      // motion stopped, server still recording the tail
};

MotionRecordState streamState = STATE_IDLE;
unsigned long motionEndTime = 0;
unsigned long streamStartTime = 0;
unsigned long lastStatusTime = 0;

// true after a successful motion-start publish, false after a successful
// motion-end publish. Drives the green LED (solid = server was notified,
// blink = motion detected but notification didn't get through).
bool startPublished = false;

// Set once, when a valid NTP epoch is first written to the hardware RTC.
// Used to keep the RTC write idempotent and to drive the blue status LED.
bool rtcInitialized = false;

// Write the NTP epoch to the hardware RTC the first time we see a valid
// value. mbedTLS reads the RTC (via time()) to check cert notBefore/notAfter
// during TLS handshake -- without this, every cert looks "not yet valid"
// and handshake fails with -0x2700. Called from setup() and from loop()
// after each successful timeClient.update() so recovery works even when
// boot-time NTP fails.
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
// Setup
// ============================================================
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

    // ----------------------------------------------------------
    // Phase A0 -- Watchdog (RELEASE only)
    // Start the WDT BEFORE WiFi so a hung boot can reset itself. Refresh
    // it inside the WiFi retry loop so a normal retry doesn't trip it.
    // ----------------------------------------------------------
#if WDT_ENABLED
    wdt.init(WDT_TIMEOUT_MS);
    wdt.start();
    LOG("[WDT] Watchdog started (30s timeout)");
#endif

    // ----------------------------------------------------------
    // Phase A -- WiFi, with bounded retry
    // rtsp.begin() below binds a socket and hangs forever if WiFi isn't
    // up, so we don't proceed until association succeeds. In RELEASE the
    // WDT reboots the board if retries are exhausted; in DEBUG we warn
    // and proceed so the developer gets a log instead of a silent hang.
    // ----------------------------------------------------------
    const int WIFI_BOOT_MAX_ATTEMPTS = 12;  // ~12 x (8s timeout + 5s delay) ~= 156s
    bool wifiOk = false;
    for (int attempt = 1; attempt <= WIFI_BOOT_MAX_ATTEMPTS; attempt++) {
        if (wifiMgr.begin()) { wifiOk = true; break; }
        LOGF("[WiFi] Boot attempt %d/%d failed; retrying in 5 s...\n",
             attempt, WIFI_BOOT_MAX_ATTEMPTS);
#if WDT_ENABLED
        wdt.refresh();
#endif
        delay(5000);
    }
    if (!wifiOk) {
#if WDT_ENABLED
        LOG("[WiFi] FATAL: no WiFi after all boot attempts; waiting for WDT reset");
        while (1) { delay(1000); }  // don't refresh; WDT fires within 30 s
#else
        LOG("[WiFi] WARNING: no WiFi after all boot attempts; proceeding (RTSP may hang)");
#endif
    }

    timeClient.begin();
    // Two snappy attempts at boot. If both fail, RTC stays unset and
    // ensureRTC() in the main loop writes it as soon as NTP recovers.
    for (int attempt = 1; attempt <= 2; attempt++) {
        if (timeClient.forceUpdate()) {
            LOGF("[NTP] Synced: %s (attempt %d)\n",
                 timeClient.getFormattedTime().c_str(), attempt);
            break;
        }
        LOGF("[NTP] Sync failed (attempt %d/2)\n", attempt);
        if (attempt < 2) delay(500);
    }
    ensureRTC();
    if (!rtcInitialized) {
        LOG("[RTC] WARNING: NTP epoch invalid at boot; will retry from loop -- TLS unavailable until then");
    }

    // ----------------------------------------------------------
    // Phase B -- Camera + RTSP + Motion Detection
    // BOTH Ch0 (H264) and Ch3 (RGB) are configured and started once at
    // boot and run continuously. Dynamically toggling Ch0/RTSP between
    // motion events produced RTSP clips that decoded as a static image
    // (the re-started Ch0 encoder emitted one cached sample). Keeping
    // Ch0 + RTSP + Ch0->RTSP pipeline alive permanently matches the SDK
    // example and gives a clean H264 stream on every motion event.
    // ----------------------------------------------------------
    configStream.setBitrate(RTSP_BITRATE);
    Camera.configVideoChannel(RTSP_CHANNEL, configStream);
    Camera.configVideoChannel(DETECT_CHANNEL, configMD);
    Camera.videoInit();

    // RTSP server + Ch0 -> RTSP pipeline (WiFi is already up).
    rtsp.configVideo(configStream);
    rtsp.begin();
    videoToRtsp.registerInput(Camera.getStream(RTSP_CHANNEL));
    videoToRtsp.registerOutput(rtsp);
    if (videoToRtsp.begin() != 0) {
        LOG("[Pipeline] ERROR: videoToRtsp connection failed");
    }
    Camera.channelBegin(RTSP_CHANNEL);

    // Motion detection pipeline on Ch3.
    motionDet.configVideo(configMD);
    motionDet.begin();
    videoToMotion.registerInput(Camera.getStream(DETECT_CHANNEL));
    videoToMotion.setStackSize();
    videoToMotion.registerOutput(motionDet);
    if (videoToMotion.begin() != 0) {
        LOG("[Pipeline] ERROR: videoToMotion connection failed");
    }
    Camera.channelBegin(DETECT_CHANNEL);

    LOG("[Setup] Warming up motion detector (8s)...");
    delay(8000);
    LOGF("[Setup] Initial MD count: %u\n", motionDet.getResultCount());

    // ----------------------------------------------------------
    // Phase C -- Battery + MQTT (after camera/RTSP are live)
    // ----------------------------------------------------------
    batteryMon.begin();

#if MQTT_USE_TLS
    mqttWifiClient.setRootCA((unsigned char*)mqtt_ca_cert);
    mqttWifiClient.setClientCertificate((unsigned char*)mqtt_client_cert,
                                        (unsigned char*)mqtt_client_key);
    LOG("[MQTT] TLS + mTLS enabled (port 8883)");
#else
    LOG("[MQTT] Plain connection (port 1883)");
#endif
    mqttMgr.begin(mqttWifiClient, timeClient);

    LOG("[Setup] Complete -- entering motion detection loop");
    LOG("========================================\n");

    mqttMgr.publishStatus(batteryMon.getPercentage(), batteryMon.getVoltage(), wifiMgr.getRSSI());
    lastStatusTime = millis();
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
    unsigned long now = millis();

    // Motion detection state machine -- always first, never blocked.
    bool motionDetected = checkMotion();
    handleMotionRecordState(motionDetected, now);

    // Battery monitoring -- update() has its own 60s throttle
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
    // Instead:
    //   - MqttManager::begin() calls setKeepAlive(0), so the broker never
    //     disconnects us for keepalive timeout.
    //   - publishMotionEvent() has its own force-reconnect path
    //     (mqtt_manager.cpp) and is the only code that touches MQTT state
    //     from the loop. Motion start/stop events reconnect on demand.
    //
    // WiFi maintenance is safe here because WifiManager::ensureConnected()
    // is a non-blocking state machine (wifi_manager.cpp Edit 3).
    if (streamState == STATE_IDLE) {
        wifiMgr.ensureConnected();
        timeClient.update();    // re-sync NTP (self-throttled to once per hour)
        ensureRTC();            // write RTC the first time NTP produces a valid epoch
        mqttMgr.checkConfig();  // poll for timezone/config updates
    }

#if WDT_ENABLED
    wdt.refresh();
#endif

    // LED indicators (non-blocking). Green: motion/record. Blue: system health.
    ledMgr.setGreen(
        streamState == STATE_IDLE
            ? LedManager::LED_OFF
            : (startPublished ? LedManager::LED_ON : LedManager::LED_BLINK_FAST)
    );
    if (WiFi.status() != WL_CONNECTED) {
        ledMgr.setBlue(LedManager::LED_ON);
    } else if (!rtcInitialized) {
        ledMgr.setBlue(LedManager::LED_BLINK_FAST);
    } else if (!mqttMgr.connected()) {
        ledMgr.setBlue(LedManager::LED_BLINK_SLOW);
    } else {
        ledMgr.setBlue(LedManager::LED_OFF);
    }
    ledMgr.update();

    // Adaptive delay: save power when idle, stay responsive during streaming
    delay(streamState == STATE_IDLE ? LOOP_DELAY_IDLE_MS : LOOP_DELAY_ACTIVE_MS);
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
                // Transition only when the server was successfully notified,
                // otherwise stay in IDLE and retry next tick. The motion-
                // reconnect gate in MqttManager rate-limits the retries.
                if (onMotionStart()) {
                    streamState = STATE_MOTION_ACTIVE;
                    LOG("[State] IDLE -> MOTION_ACTIVE");
                }
            }
            break;

        case STATE_MOTION_ACTIVE:
            if (!motionDetected) {
                motionEndTime = now;
                streamState = STATE_POST_ROLL;
                LOG("[State] MOTION_ACTIVE -> POST_ROLL (10s countdown)");
            }
            break;

        case STATE_POST_ROLL:
            if (motionDetected) {
                streamState = STATE_MOTION_ACTIVE;
                LOG("[State] POST_ROLL -> MOTION_ACTIVE (motion resumed)");
            } else if (now - motionEndTime >= MOTION_POST_ROLL_MS) {
                // Same gating as STATE_IDLE -> STATE_MOTION_ACTIVE: only return
                // to IDLE once we've confirmed the recorder knows to stop.
                if (onMotionEnd()) {
                    streamState = STATE_IDLE;
                    LOG("[State] POST_ROLL -> IDLE");
                }
            }
            break;
    }
}

// ============================================================
// Motion event notification
// ============================================================
// Ch0, RTSP server, and Ch0->RTSP pipeline run continuously from boot
// (see setup()). onMotionStart / onMotionEnd just publish the MQTT event
// and track duration; the RTSP stream is *always* available on :554.
// Return value reflects whether the publish was accepted by the TCP
// stack -- the state machine uses it to decide whether to advance.
bool onMotionStart() {
    streamStartTime = millis();

    // Build RTSP URL (IP may have changed after reconnect).
    snprintf(rtspUrl, sizeof(rtspUrl), "rtsp://%s:%d",
             WiFi.localIP().get_address(), rtsp.getPort());
    LOGF("[RTSP] Motion-start: %s\n", rtspUrl);

    bool ok = mqttMgr.publishMotionEvent(true, rtspUrl);
    if (ok) startPublished = true;
    return ok;
}

bool onMotionEnd() {
    unsigned long durationMs = millis() - streamStartTime;
    LOGF("[RTSP] Motion-stop after %.1f seconds\n", durationMs / 1000.0f);

    bool ok = mqttMgr.publishMotionEvent(false, NULL);
    if (!ok) return false;

    startPublished = false;
    mqttMgr.publishStatus(batteryMon.getPercentage(), batteryMon.getVoltage(), wifiMgr.getRSSI());
    lastStatusTime = millis();  // Reset hourly timer so we don't double-publish
    return true;
}
