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
    MQTT_TLS        - Enable TLS: "1" to enable (optional)
    MQTT_CA_CERT    - Path to CA certificate PEM file (required if TLS)
    MQTT_CLIENT_CERT - Path to client certificate PEM file (for mTLS)
    MQTT_CLIENT_KEY  - Path to client private key PEM file (for mTLS)
    MQTT_TOPIC      - Topic pattern (default: camera/+/motion)
    CLIPS_DIR       - Output directory (default: /clips)
    MAX_DURATION    - Max recording seconds (default: 300)
    RTSP_TIMEOUT_US - RTSP socket I/O timeout in microseconds (default: 5000000).
                      Legacy name RTSP_STIMEOUT_US still honored.
    MIN_CLIP_BYTES  - Drop clips smaller than this as failed (default: 4096)
"""

import json
import logging
import os
import re
import signal
import ssl
import subprocess
import sys
import threading
from datetime import datetime, timezone
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
MQTT_TLS = os.getenv("MQTT_TLS", "") == "1"
MQTT_CA_CERT = os.getenv("MQTT_CA_CERT", "")
MQTT_CLIENT_CERT = os.getenv("MQTT_CLIENT_CERT", "")
MQTT_CLIENT_KEY = os.getenv("MQTT_CLIENT_KEY", "")
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "camera/+/motion")
CLIPS_DIR = Path(os.getenv("CLIPS_DIR", "/clips"))
MAX_DURATION = int(os.getenv("MAX_DURATION", "300"))
RTSP_TIMEOUT_US = int(os.getenv("RTSP_TIMEOUT_US",
                                os.getenv("RTSP_STIMEOUT_US", "5000000")))  # 5s
MIN_CLIP_BYTES = int(os.getenv("MIN_CLIP_BYTES", "4096"))

DEVICE_ID_RE = re.compile(r"^[A-Za-z0-9_-]+$")

# active_recordings: device_id -> dict. Touched from the paho network
# thread (on_message), the reaper thread, and the main thread (shutdown),
# so always acquire _recordings_lock before reading/writing.
active_recordings = {}
_recordings_lock = threading.Lock()


def _drain_stderr_to_file(pipe, log_path: Path):
    """Consume FFmpeg stderr line-by-line and write to a sidecar .log file.

    Running in a dedicated thread. Without this, a full stderr pipe buffer
    will block FFmpeg mid-recording.

    Lazy-opens the file on the first byte so ffmpeg invocations that exit
    before printing anything (common when a duplicate motion-start kills
    the previous ffmpeg within milliseconds) don't leave 0-byte log files
    behind. The parent directory is also created lazily.
    """
    f = None
    try:
        for line in iter(pipe.readline, b""):
            if f is None:
                log_path.parent.mkdir(parents=True, exist_ok=True)
                f = open(log_path, "ab")
            f.write(line)
    except Exception as e:  # best-effort
        log.warning("stderr drain failed for %s: %s", log_path, e)
    finally:
        if f is not None:
            try:
                f.close()
            except Exception:
                pass
        try:
            pipe.close()
        except Exception:
            pass


def start_recording(device_id: str, rtsp_url: str, mqtt_client):
    """Spawn FFmpeg to record from RTSP stream."""
    with _recordings_lock:
        existing = active_recordings.get(device_id)
    if existing is not None:
        # Duplicate motion-start: the previous FFmpeg may be dead or may
        # be recording from a stale URL (camera IP changed). Stop it
        # first AND publish its clip metadata so we don't silently lose
        # the previous recording.
        log.warning("Duplicate motion-start for %s -- finalizing previous recording first",
                    device_id)
        _stop_recording_internal(device_id, publish_clip=True, mqtt_client=mqtt_client)

    today = datetime.now().strftime("%Y-%m-%d")
    out_dir = CLIPS_DIR / device_id / today
    out_dir.mkdir(parents=True, exist_ok=True)

    timestamp = datetime.now().strftime("%H-%M-%S")
    out_path = out_dir / f"{device_id}_{timestamp}.mp4"
    # FFmpeg stderr logs go into a per-date logs/ subfolder to keep the
    # clip directory clean. The folder itself is created lazily by the
    # drain thread if (and only if) ffmpeg actually produces output.
    log_path = out_dir / "logs" / f"{device_id}_{timestamp}.ffmpeg.log"

    cmd = [
        "ffmpeg",
        "-hide_banner",
        "-loglevel", "warning",
        "-rtsp_transport", "tcp",
        # FFmpeg 5+ renamed the RTSP socket timeout from -stimeout to -timeout
        # (still microseconds). The old name was removed in 7.x and causes
        # "Unrecognized option 'stimeout'" + exit code 8 before any I/O.
        "-timeout", str(RTSP_TIMEOUT_US),
        "-i", rtsp_url,
        "-c", "copy",                         # passthrough H264, no re-encode
        "-t", str(MAX_DURATION),              # safety cap
        "-y",
        str(out_path),
    ]

    log.info("Starting recording: %s -> %s", rtsp_url, out_path)
    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
    except FileNotFoundError:
        log.error("ffmpeg not found -- is it installed?")
        return
    except Exception as e:
        log.error("Failed to start FFmpeg: %s", e)
        return

    drain_thread = threading.Thread(
        target=_drain_stderr_to_file,
        args=(proc.stderr, log_path),
        daemon=True,
    )
    drain_thread.start()

    with _recordings_lock:
        active_recordings[device_id] = {
            "process": proc,
            "path": str(out_path),
            "log_path": str(log_path),
            "start_time": datetime.now(timezone.utc),
            "drain_thread": drain_thread,
        }
    log.info("FFmpeg started (PID %d) for %s", proc.pid, device_id)


def _stop_recording_internal(device_id: str, publish_clip: bool, mqtt_client):
    """Stop the active recording and optionally publish clip metadata.

    Returns the recording dict (popped) or None if nothing was active.
    """
    with _recordings_lock:
        rec = active_recordings.pop(device_id, None)
    if rec is None:
        return None

    proc = rec["process"]
    log.info("Stopping recording for %s (PID %d)", device_id, proc.pid)

    if proc.poll() is None:
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            log.warning("FFmpeg didn't stop in 10s, killing")
            proc.kill()
            proc.wait()

    # Give the drain thread a brief moment to flush remaining stderr.
    rec["drain_thread"].join(timeout=1.0)

    clip_path = rec["path"]
    clip_size = os.path.getsize(clip_path) if os.path.exists(clip_path) else 0

    # Drop tiny/empty clips -- usually means FFmpeg never connected or the
    # stream died immediately. Leaving them around silently fills the disk.
    if clip_size < MIN_CLIP_BYTES:
        log.warning("Clip %s is %d bytes (< %d); deleting as failed",
                    clip_path, clip_size, MIN_CLIP_BYTES)
        try:
            os.remove(clip_path)
        except OSError:
            pass
        if publish_clip and mqtt_client is not None:
            mqtt_client.publish(
                f"camera/{device_id}/alert",
                json.dumps({
                    "device": device_id,
                    "error": "recording_failed",
                    "size_bytes": clip_size,
                    "start_time": rec["start_time"].isoformat(),
                }),
            )
        return rec

    end_time = datetime.now(timezone.utc)
    duration_s = round((end_time - rec["start_time"]).total_seconds(), 1)
    log.info("Recording saved: %s (%d bytes, %.1fs)", clip_path, clip_size, duration_s)

    if publish_clip and mqtt_client is not None:
        clip_topic = f"camera/{device_id}/clip"
        clip_payload = json.dumps({
            "device": device_id,
            "filename": os.path.basename(clip_path),
            "path": clip_path,
            "size_bytes": clip_size,
            "duration_s": duration_s,
            "start_time": rec["start_time"].isoformat(),
            "end_time": end_time.isoformat(),
        })
        mqtt_client.publish(clip_topic, clip_payload)
        log.info("Published clip metadata to %s", clip_topic)

    return rec


def stop_recording(device_id: str, mqtt_client):
    """Public wrapper that logs when there's nothing to stop."""
    rec = _stop_recording_internal(device_id, publish_clip=True, mqtt_client=mqtt_client)
    if rec is None:
        log.warning("No active recording for %s", device_id)


def _reaper_loop(mqtt_client, stop_event: threading.Event):
    """Periodically check for FFmpeg processes that have exited on their own.

    Without this, clips that finish because -t MAX_DURATION was reached --
    or because ffmpeg crashed / RTSP died -- would sit in active_recordings
    until the firmware happens to publish motion=false, which may never
    happen if the firmware rebooted mid-stream.
    """
    while not stop_event.wait(timeout=5.0):
        with _recordings_lock:
            dead = [(dev, rec["process"].returncode)
                    for dev, rec in active_recordings.items()
                    if rec["process"].poll() is not None]
        for device_id, rc in dead:
            log.info("FFmpeg for %s exited on its own (rc=%s) -- finalizing",
                     device_id, rc)
            try:
                _stop_recording_internal(device_id, publish_clip=True,
                                         mqtt_client=mqtt_client)
            except Exception as e:
                log.error("Reaper: error finalizing %s: %s", device_id, e)


def on_connect(client, userdata, flags, reason_code, properties=None):
    log.info("Connected to MQTT broker (rc=%s)", reason_code)
    client.subscribe(MQTT_TOPIC)
    log.info("Subscribed to %s", MQTT_TOPIC)


def on_message(client, userdata, msg):
    # Retained motion-start messages would re-trigger recording on every
    # reconnect with a possibly-stale RTSP URL. Firmware publishes these
    # non-retained today; this guard keeps us safe if that ever changes.
    if msg.retain:
        log.debug("Ignoring retained message on %s", msg.topic)
        return

    try:
        payload = json.loads(msg.payload.decode())
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        log.warning("Invalid payload on %s: %s", msg.topic, e)
        return

    parts = msg.topic.split("/")
    if len(parts) < 3:
        log.warning("Unexpected topic format: %s", msg.topic)
        return
    device_id = parts[1]
    if not DEVICE_ID_RE.fullmatch(device_id):
        log.warning("Rejected device_id %r from topic %s", device_id, msg.topic)
        return

    motion = payload.get("motion", False)
    rtsp_url = payload.get("rtsp", "")

    log.info("Motion event: device=%s motion=%s rtsp=%s", device_id, motion, rtsp_url)

    if motion:
        # Only accept plain rtsp:// URLs. FFmpeg accepts many other schemes
        # (concat:, file:, ...) that could be abused to read server-local
        # files if a broker-authenticated attacker publishes.
        if not rtsp_url.startswith("rtsp://"):
            log.warning("Rejecting non-rtsp URL for %s: %r", device_id, rtsp_url)
            return
        start_recording(device_id, rtsp_url, client)
    else:
        stop_recording(device_id, client)


def main():
    log.info("MQTT Recorder starting")
    log.info("  Broker: %s:%d", MQTT_BROKER, MQTT_PORT)
    log.info("  TLS: %s", "enabled" if MQTT_TLS else "disabled")
    log.info("  Topic: %s", MQTT_TOPIC)
    log.info("  Clips dir: %s", CLIPS_DIR)
    log.info("  Max duration: %ds", MAX_DURATION)

    CLIPS_DIR.mkdir(parents=True, exist_ok=True)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASS)

    if MQTT_TLS:
        certfile = MQTT_CLIENT_CERT if MQTT_CLIENT_CERT else None
        keyfile = MQTT_CLIENT_KEY if MQTT_CLIENT_KEY else None
        client.tls_set(
            ca_certs=MQTT_CA_CERT,
            certfile=certfile,
            keyfile=keyfile,
            cert_reqs=ssl.CERT_REQUIRED,
            tls_version=ssl.PROTOCOL_TLS_CLIENT,
        )
        log.info("  CA cert: %s", MQTT_CA_CERT)
        if certfile:
            log.info("  Client cert: %s (mTLS)", certfile)

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)

    # Signal handlers just disconnect the MQTT loop; the real cleanup runs
    # on the main thread after loop_forever() returns. Doing MQTT publishes
    # inside a signal handler is fragile with paho's network thread.
    def _signal(signum, _frame):
        log.info("Signal %s received -- requesting shutdown", signum)
        try:
            client.disconnect()
        except Exception:
            pass

    signal.signal(signal.SIGINT, _signal)
    signal.signal(signal.SIGTERM, _signal)

    reaper_stop = threading.Event()
    reaper_thread = threading.Thread(
        target=_reaper_loop, args=(client, reaper_stop), daemon=True,
    )
    reaper_thread.start()

    try:
        client.loop_forever()
    finally:
        reaper_stop.set()
        reaper_thread.join(timeout=10.0)

    # Graceful shutdown on the main thread.
    log.info("Shutting down -- stopping active recordings")
    with _recordings_lock:
        device_ids = list(active_recordings.keys())
    for device_id in device_ids:
        try:
            stop_recording(device_id, client)
        except Exception as e:
            log.error("Error stopping %s: %s", device_id, e)
    sys.exit(0)


if __name__ == "__main__":
    main()
