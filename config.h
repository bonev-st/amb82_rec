#ifndef CONFIG_H
#define CONFIG_H

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
// WiFi Configuration
// ============================================================
#define WIFI_SSID           "xxxx956"
#define WIFI_PASSWORD       "1RootPass0"
#define WIFI_CONNECT_TIMEOUT_MS   15000
#define WIFI_RECONNECT_INTERVAL_MS 10000

// ============================================================
// MQTT Configuration
// ============================================================
#define MQTT_BROKER         "192.168.2.143"
#define MQTT_USER           "amb82_cam_01"
#define MQTT_PASSWORD       "DcU9EOHGPWxSH2T6"

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
// Recording Indicator LED
// ============================================================
// Use LED_G (on-board green LED on AMB82 Mini, Arduino pin 24 = PE_6).
// Passing PE_6 directly does not work — pinMode/digitalWrite expect the
// Arduino pin index from the variant table, not the raw mbed pin enum.
#define REC_LED_PIN         LED_G

// ============================================================
// NTP
// ============================================================
#define NTP_SERVER           "pool.ntp.org"
#define NTP_TIMEZONE_OFFSET  0      // Offset in seconds from UTC (e.g., 3600 for UTC+1)
#define NTP_UPDATE_INTERVAL  3600000 // Sync every hour (ms)

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
  #define WDT_TIMEOUT_MS     30000    // 30s — reboot if loop hangs
#else
  #define WDT_ENABLED        0
#endif

// ============================================================
// Serial Debug
// ============================================================
#define SERIAL_BAUD          115200

#ifdef BUILD_RELEASE
  // Release: no serial output at all — UART peripheral stays off
  #define LOG(msg)           ((void)0)
  #define LOGF(fmt, ...)     ((void)0)
#else
  // Debug: full serial logging
  #define LOG(msg)           Serial.println(msg)
  #define LOGF(fmt, ...)     printf(fmt, ##__VA_ARGS__)
#endif

#endif // CONFIG_H
