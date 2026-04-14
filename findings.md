# Findings — MQTT Security (mTLS + Auth)

## SDK Research: WiFiSSLClient

**Header:** `libraries/WiFi/src/WiFiSSLClient.h`
- Extends `Client` (same base as `WiFiClient`) — drop-in for PubSubClient
- Uses mbedTLS internally
- Key methods:
  ```cpp
  void setRootCA(unsigned char *rootCA);           // PEM string — verify server cert
  void setClientCertificate(unsigned char *cert, unsigned char *key);  // mTLS
  void setPreSharedKey(unsigned char *pskIdent, unsigned char *psKey); // PSK (hex)
  ```
- SDK has working example: `libraries/MQTTClient/examples/MQTT_TLS/MQTT_TLS.ino`
- PubSubClient takes `Client&` — WiFiSSLClient is transparent replacement

## Implementation Decisions

| Decision | Rationale |
|----------|-----------|
| Self-signed CA (10-year) | LAN-only setup, no public DNS needed |
| mTLS (client certs) | User explicitly requested full security stack |
| Server-side TLS + client certs + password | Belt-and-suspenders: cert authenticates device, password authorizes it |
| Keep anonymous listener on 1883 | Backward compatibility per requirements |
| `mqtt_certs.h` in git repo | User decision — demo project, not production secrets |
| `per_listener_settings true` | Allows different auth rules per Mosquitto listener |
| `use_identity_as_username false` | Use password-file username, not cert CN, for MQTT auth |
| Recorder uses separate client cert (CN=recorder) | Different identity per service, proper cert management |
| `begin(Client&)` in mqtt_manager | Works with both WiFiClient and WiFiSSLClient without templates |

## Security Architecture

```
Camera (amb82_cam_01)              Broker (sbbu01.local)
+----------------------------+     +----------------------------+
| WiFiSSLClient              |     | Mosquitto                  |
|   CA cert (verify server)  |---->| :8883 TLS listener         |
|   Client cert + key (mTLS) |     |   server cert + key        |
|   Username + password      |     |   CA cert (verify clients)  |
+----------------------------+     |   password_file             |
                                   |                            |
Recorder (recorder)                | :1883 plain listener       |
+----------------------------+     |   allow_anonymous true     |
| paho-mqtt + tls_set()      |---->|                            |
|   CA cert, client cert+key |     +----------------------------+
|   Username + password      |
+----------------------------+
```

## Certificates Generated (2026-04-13)

| File | CN | Purpose | Validity |
|------|----|---------|----------|
| `ca.crt` / `ca.key` | AMB82 MQTT CA | Root CA — signs all other certs | 10 years |
| `server.crt` / `server.key` | sbbu01.local | Broker identity | 10 years |
| `client_camera.crt` / `client_camera.key` | amb82_cam_01 | Camera mTLS identity | 10 years |
| `client_recorder.crt` / `client_recorder.key` | recorder | Recorder mTLS identity | 10 years |

## Key Gotchas

1. **pullConfig() needs TLS too** — it creates a disposable WiFiClient/PubSubClient.
   Must use WiFiSSLClient with the same certs, or the throwaway connection fails on 8883.
   Also must pass MQTT_USER/MQTT_PASSWORD (previously connected anonymously).

2. **Mosquitto `per_listener_settings true`** — required when running two listeners
   with different auth rules. Without it, settings are global and the password_file
   would apply to both listeners (breaking anonymous on 1883).

3. **`use_identity_as_username false`** — if true, Mosquitto would use the cert CN
   as the MQTT username, ignoring the CONNECT packet username. We want password-file
   auth separate from cert identity.

4. **PEM format in C strings** — must have `\n` at end of each line and a trailing `\n`
   after the `-----END ...-----` line. Missing newlines cause mbedTLS parse failures.

5. **systemd `%h` in Environment=** — does NOT expand `%h` in `Environment=` directives.
   Used `%%h` in the service file which systemd expands to `%h`, then the shell expands
   `%h` at runtime. Actually for ExecStart it works, but for Environment values with paths,
   we use `%%h` which becomes literal `%h` which systemd then expands to the home directory.
