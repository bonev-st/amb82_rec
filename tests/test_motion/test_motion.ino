/*
 * Test 7: Motion Detection (RTSP Architecture)
 * Verifies: MotionDetection on Ch3 (RGB), configVideo, StreamIO, getResult
 * Needs: Board only -- wave hand in front of camera to trigger
 *
 * Based on SDK example: MotionDetection/LoopPostProcessing
 */

#include <VideoStream.h>
#include <MotionDetection.h>
#include <StreamIO.h>

#define SERIAL_BAUD     115200
#define CH_VIDEO        0           // H264 channel (needed for sensor init)
#define CH_DETECT       3           // RGB channel for motion detection
#define TRIGGER_COUNT   3           // Motion regions to consider triggered

#define REC_LED_PIN     LED_G

// Match SDK example: both channels configured, globals for all objects
VideoSetting configV(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configMD(VIDEO_VGA, 10, VIDEO_RGB, 0);
MotionDetection MD;
StreamIO videoStreamerMD(1, 1);

int passCount = 0;
int failCount = 0;

void reportTest(const char* name, bool passed) {
    if (passed) {
        passCount++;
        printf("[PASS] %s\n", name);
    } else {
        failCount++;
        printf("[FAIL] %s\n", name);
    }
}

void setup() {
    pinMode(REC_LED_PIN, OUTPUT);
    digitalWrite(REC_LED_PIN, LOW);

    Serial.begin(SERIAL_BAUD);
    delay(2000);

    printf("\n========================================\n");
    printf(" Test 7: Motion Detection (Ch3 RGB)\n");
    printf("========================================\n\n");

    // Configure both channels (Ch0 needed for sensor init)
    Camera.configVideoChannel(CH_VIDEO, configV);
    Camera.configVideoChannel(CH_DETECT, configMD);
    Camera.videoInit();
    reportTest("Camera videoInit()", true);

    // Configure motion detection -- no setTriggerBlockCount (matches SDK example)
    MD.configVideo(configMD);
    MD.begin();
    reportTest("MD.configVideo() + begin()", true);

    // Connect StreamIO: Camera Ch3 -> MD
    videoStreamerMD.registerInput(Camera.getStream(CH_DETECT));
    videoStreamerMD.setStackSize();
    videoStreamerMD.registerOutput(MD);
    int pipeResult = videoStreamerMD.begin();
    printf("[TEST] StreamIO.begin(): %d (0 = success)\n", pipeResult);
    reportTest("StreamIO pipeline connected", pipeResult == 0);

    // Start detection channel
    Camera.channelBegin(CH_DETECT);
    reportTest("channelBegin(Ch3)", true);

    // Wait for AE to stabilize and MD background model to build
    printf("[TEST] Waiting for AE + background model (8s)...\n");
    delay(8000);

    // Initial result check
    uint16_t count = MD.getResultCount();
    printf("[TEST] Initial getResultCount(): %d\n", count);
    reportTest("getResultCount() works", true);

    // Summary
    printf("\n========================================\n");
    printf(" Results: %d PASS, %d FAIL\n", passCount, failCount);
    printf("========================================\n");
    printf("\n[TEST] Monitoring motion (wave hand in front of camera):\n");
    printf("  Trigger threshold: %d regions\n\n", TRIGGER_COUNT);
}

void loop() {
    uint16_t count = MD.getResultCount();

    if (count > 0) {
		digitalWrite(REC_LED_PIN, HIGH);

        std::vector<MotionDetectionResult> results = MD.getResult();
        printf("  MOTION: %d regions", count);
        if (count >= TRIGGER_COUNT) {
            printf(" [TRIGGERED]");
        }
        printf("\n");
        for (int i = 0; i < count && i < 3; i++) {
            printf("    Region %d: x=[%.2f,%.2f] y=[%.2f,%.2f]\n", i,
                   results[i].xMin(), results[i].xMax(),
                   results[i].yMin(), results[i].yMax());
        }
    } else {
		digitalWrite(REC_LED_PIN, LOW);

		printf("  . (no motion)\n");
    }

    delay(500);
}
