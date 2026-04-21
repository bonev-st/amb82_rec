#ifndef CONFIG_H
#define CONFIG_H

// WIFI_* and MQTT_BROKER/USER/PASSWORD live in secrets.h (gitignored).
// Copy secrets.h.example -> secrets.h and fill in real values.
#include "secrets.h"

// ============================================================
// Build Mode: uncomment ONE of the following lines
// ============================================================
// #define BUILD_RELEASE    // Production: no serial, WDT, power-optimized
#define BUILD_DEBUG         // Development: serial logging, faster polling

// ============================================================
// Firmware Version
// ============================================================
#define FIRMWARE_VERSION    "1.0.0"

#ifdef BUILD_RELEASE
  #define FIRMWARE_BUILD    "RELEASE"
#else
  #define FIRMWARE_BUILD    "DEBUG"
#endif

// ============================================================
// Device Identity
// ============================================================
#define DEVICE_ID           "amb82_cam_01"
#define DEVICE_NAME         "AMB82 Motion Camera"

// ============================================================
// WiFi Configuration (SSID/password in secrets.h)
// ============================================================
#define WIFI_BOOT_TIMEOUT_MS      8000   // shorter: proceed to loop after 8s
#define WIFI_CONNECT_TIMEOUT_MS   15000
#define WIFI_RECONNECT_INTERVAL_MS 10000

// ============================================================
// MQTT Configuration (broker/user/password in secrets.h)
// ============================================================

// MQTT Security: set to 1 for TLS + mTLS on port 8883, 0 for plain on 1883
#define MQTT_USE_TLS        1
#if MQTT_USE_TLS
  #define MQTT_PORT         8883
#else
  #define MQTT_PORT         1883
#endif

// MQTT Topics
#define MQTT_TOPIC_STATUS   "camera/" DEVICE_ID "/status"
#define MQTT_TOPIC_BATTERY  "camera/" DEVICE_ID "/battery"
#define MQTT_TOPIC_MOTION   "camera/" DEVICE_ID "/motion"
#define MQTT_TOPIC_LWT      "camera/" DEVICE_ID "/availability"
#define MQTT_TOPIC_CONFIG   "camera/" DEVICE_ID "/config"

#define MQTT_RECONNECT_INTERVAL_MS 5000
#define MQTT_MOTION_RECONNECT_INTERVAL_MS 2000  // motion-event forced reconnect rate limit
#define MQTT_STATUS_INTERVAL_MS    3600000 // 1 hour

// ============================================================
// Battery Monitoring
// ============================================================
#define BATTERY_ADC_PIN     A0
#ifdef BUILD_RELEASE
  #define BATTERY_CHECK_INTERVAL_MS  120000  // Check every 120 seconds (release)
#else
  #define BATTERY_CHECK_INTERVAL_MS  60000   // Check every 60 seconds (debug)
#endif
#define BATTERY_FULL_VOLTAGE       4.2f    // Fully charged LiPo
#define BATTERY_EMPTY_VOLTAGE      3.3f    // Cutoff voltage
#define BATTERY_LOW_THRESHOLD_PCT  20      // Low battery alert at 20%
#define BATTERY_CRITICAL_PCT       10      // Critical battery alert
#define BATTERY_VOLTAGE_DIVIDER    2.0f    // Resistor divider ratio (adjust to your circuit)
#define BATTERY_ADC_RESOLUTION     1024    // 10-bit ADC
#define BATTERY_ADC_REF_VOLTAGE    3.3f    // ADC reference voltage
#define BATTERY_SAMPLES            10      // Averaging samples

// ============================================================
// Motion Detection (Channel 3, RGB format required by SDK)
// ============================================================
#define DETECT_CHANNEL      3              // SDK requires channel 3 for RGB motion detection
#ifdef BUILD_RELEASE
  #define DETECT_FPS        5              // 5fps saves power; ~200ms detection latency
#else
  #define DETECT_FPS        10             // 10fps for responsive debugging
#endif
#define DETECT_CODEC        VIDEO_RGB      // Must be RGB for MotionDetection
#define MOTION_DETECT_SENSITIVITY  2       // Trigger when >= N connected motion regions detected
#define MOTION_POST_ROLL_MS        10000   // Stream 10 seconds after motion ends

// ============================================================
// RTSP Streaming (Channel 0, H264)
// ============================================================
#define RTSP_CHANNEL        0
#define RTSP_BITRATE        (2 * 1024 * 1024)  // 2 Mbps recommended for WiFi

// ============================================================
// Indicator LEDs
// ============================================================
// Green LED (LED_G, Arduino pin 24 = PE_6): motion/record state.
// Blue LED  (LED_BUILTIN, PF_9): system health (WiFi/NTP/MQTT).
// Passing the raw mbed pin enum (PE_6, PF_9) does not work -- pinMode /
// digitalWrite want the Arduino pin index from the variant table.
#define REC_LED_PIN         LED_G
#define STATUS_LED_PIN      LED_BUILTIN

// ============================================================
// NTP
// ============================================================
#define NTP_SERVER           "pool.ntp.org"
// Timezone is server-authoritative at runtime via MQTT config message.
// Until the broker delivers one, epochs/timestamps are in UTC.
#define NTP_UPDATE_INTERVAL  3600000  // Sync every hour (ms)
#define NTP_VALID_EPOCH_MIN  1704067200UL  // 2024-01-01T00:00:00Z -- sanity floor

// ============================================================
// Power Management
// ============================================================
#ifdef BUILD_RELEASE
  #define LOOP_DELAY_IDLE_MS    500    // 500ms between polls when idle (power saving)
  #define LOOP_DELAY_ACTIVE_MS  100    // 100ms during streaming/post-roll
#else
  #define LOOP_DELAY_IDLE_MS    100    // Fast polling for debug
  #define LOOP_DELAY_ACTIVE_MS  100
#endif

// ============================================================
// Config Polling
// ============================================================
#ifdef BUILD_RELEASE
  #define CONFIG_CHECK_INTERVAL_MS  300000  // 5 minutes (release)
#else
  #define CONFIG_CHECK_INTERVAL_MS  60000   // 1 minute (debug)
#endif

// ============================================================
// Watchdog Timer (release only)
// ============================================================
#ifdef BUILD_RELEASE
  #define WDT_ENABLED        1
  #define WDT_TIMEOUT_MS     30000    // 30s -- reboot if loop hangs
#else
  #define WDT_ENABLED        0
#endif

// ============================================================
// Serial Debug
// ============================================================
#define SERIAL_BAUD          115200

#ifdef BUILD_RELEASE
  // Release: no serial output at all -- UART peripheral stays off
  #define LOG(msg)           ((void)0)
  #define LOGF(fmt, ...)     ((void)0)
#else
  // Debug: full serial logging
  #define LOG(msg)           Serial.println(msg)
  #define LOGF(fmt, ...)     printf(fmt, ##__VA_ARGS__)
#endif

#endif // CONFIG_H
