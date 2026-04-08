#!/usr/bin/env python3
"""
MQTT-triggered FFmpeg recorder.

Subscribes to camera motion MQTT topics. On motion start, spawns FFmpeg
to record from the camera's RTSP stream. On motion stop, gracefully
terminates FFmpeg so the MP4 file is properly finalized.

Environment variables:
    MQTT_BROKER     - Broker hostname (default: localhost)
    MQTT_PORT       - Broker port (default: 1883)
    MQTT_USER       - Username (optional)
    MQTT_PASS       - Password (optional)
    MQTT_TOPIC      - Topic pattern (default: camera/+/motion)
    CLIPS_DIR       - Output directory (default: /clips)
    MAX_DURATION    - Max recording seconds (default: 300)
"""

import json
import logging
import os
import signal
import subprocess
import sys
from datetime import datetime
from pathlib import Path

import paho.mqtt.client as mqtt

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
log = logging.getLogger("recorder")

# Configuration from environment
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USER = os.getenv("MQTT_USER", "")
MQTT_PASS = os.getenv("MQTT_PASS", "")
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "camera/+/motion")
CLIPS_DIR = Path(os.getenv("CLIPS_DIR", "/clips"))
MAX_DURATION = int(os.getenv("MAX_DURATION", "300"))

# Track active recordings: device_id -> {"process": Popen, "path": str}
active_recordings = {}


def start_recording(device_id: str, rtsp_url: str):
    """Spawn FFmpeg to record from RTSP stream."""
    if device_id in active_recordings:
        log.warning("Already recording for %s, ignoring duplicate start", device_id)
        return

    # Create date-based output directory
    today = datetime.now().strftime("%Y-%m-%d")
    out_dir = CLIPS_DIR / device_id / today
    out_dir.mkdir(parents=True, exist_ok=True)

    timestamp = datetime.now().strftime("%H-%M-%S")
    out_path = out_dir / f"{device_id}_{timestamp}.mp4"

    cmd = [
        "ffmpeg",
        "-rtsp_transport", "tcp",
        "-i", rtsp_url,
        "-c", "copy",              # No re-encoding — passthrough H264
        "-t", str(MAX_DURATION),   # Safety cap
        "-movflags", "+faststart",
        "-y",                      # Overwrite if exists
        str(out_path),
    ]

    log.info("Starting recording: %s -> %s", rtsp_url, out_path)
    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        active_recordings[device_id] = {
            "process": proc,
            "path": str(out_path),
            "start_time": datetime.now().isoformat(),
        }
        log.info("FFmpeg started (PID %d) for %s", proc.pid, device_id)
    except FileNotFoundError:
        log.error("ffmpeg not found — is it installed?")
    except Exception as e:
        log.error("Failed to start FFmpeg: %s", e)


def stop_recording(device_id: str, mqtt_client):
    """Gracefully stop FFmpeg recording."""
    rec = active_recordings.pop(device_id, None)
    if rec is None:
        log.warning("No active recording for %s", device_id)
        return

    proc = rec["process"]
    log.info("Stopping recording for %s (PID %d)", device_id, proc.pid)

    # Send SIGTERM for graceful shutdown (FFmpeg finalizes MP4 muxing)
    proc.send_signal(signal.SIGTERM)
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        log.warning("FFmpeg didn't stop in 10s, killing")
        proc.kill()
        proc.wait()

    clip_path = rec["path"]
    clip_size = 0
    if os.path.exists(clip_path):
        clip_size = os.path.getsize(clip_path)

    log.info("Recording saved: %s (%d bytes)", clip_path, clip_size)

    # Publish clip metadata back to MQTT
    clip_topic = f"camera/{device_id}/clip"
    clip_payload = json.dumps({
        "device": device_id,
        "file": clip_path,
        "size_bytes": clip_size,
        "start_time": rec["start_time"],
        "end_time": datetime.now().isoformat(),
    })
    mqtt_client.publish(clip_topic, clip_payload)
    log.info("Published clip metadata to %s", clip_topic)


def on_connect(client, userdata, flags, reason_code, properties=None):
    log.info("Connected to MQTT broker (rc=%s)", reason_code)
    client.subscribe(MQTT_TOPIC)
    log.info("Subscribed to %s", MQTT_TOPIC)


def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        log.warning("Invalid payload on %s: %s", msg.topic, e)
        return

    # Extract device_id from topic: camera/<device_id>/motion
    parts = msg.topic.split("/")
    if len(parts) < 3:
        log.warning("Unexpected topic format: %s", msg.topic)
        return
    device_id = parts[1]

    motion = payload.get("motion", False)
    rtsp_url = payload.get("rtsp", "")

    log.info("Motion event: device=%s motion=%s rtsp=%s", device_id, motion, rtsp_url)

    if motion and rtsp_url:
        start_recording(device_id, rtsp_url)
    elif not motion:
        stop_recording(device_id, client)


def main():
    log.info("MQTT Recorder starting")
    log.info("  Broker: %s:%d", MQTT_BROKER, MQTT_PORT)
    log.info("  Topic: %s", MQTT_TOPIC)
    log.info("  Clips dir: %s", CLIPS_DIR)
    log.info("  Max duration: %ds", MAX_DURATION)

    CLIPS_DIR.mkdir(parents=True, exist_ok=True)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASS)

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)

    # Handle graceful shutdown
    def shutdown(signum, frame):
        log.info("Shutting down — stopping active recordings")
        for device_id in list(active_recordings):
            stop_recording(device_id, client)
        client.disconnect()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    client.loop_forever()


if __name__ == "__main__":
    main()
