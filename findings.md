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

## Cert Gotchas (learned the hard way)

### Gotcha A: `-extensions v3_ca` alone does NOT create a valid CA
Running `openssl req -new -x509 -extensions v3_ca ...` without a config file
that defines the `[v3_ca]` section produces a cert with **no extensions**.
OpenSSL accepts this silently but mbedTLS on the AMB82 rejects it as a CA
(error `-0x2700` / `MBEDTLS_ERR_X509_CERT_VERIFY_FAILED`).

**Fix:** always generate the CA with `basicConstraints = critical, CA:TRUE`
and `keyUsage = critical, keyCertSign, cRLSign`. See README for working commands.

### Gotcha D: mbedTLS 2.28 does NOT verify IP SAN against hostname string
This is the most subtle one. The SDK's `ard_ssl.c` calls:
```c
mbedtls_ssl_set_hostname(ssl, SNI_hostname);
```
where `SNI_hostname` is whatever string was passed to `WiFiSSLClient::connect(const char*, uint16_t)`. mbedTLS 2.28 then checks that string against the cert's **DNS SAN** entries — it does **not** check IP-literal strings against IP SAN entries.

So even if the server cert has `subjectAltName = IP:192.168.2.143`, a connection to `"192.168.2.143"` as a hostname will fail with `-0x2700` because mbedTLS tries to match the literal string "192.168.2.143" against `sbbu01.local`/`localhost` (the DNS entries) and nothing matches.

The SDK's `WiFiSSLClient::connect(IPAddress, uint16_t)` overload bypasses this — it never sets `_sni_hostname`, so `mbedtls_ssl_set_hostname(ssl, NULL)` is called and mbedTLS skips hostname matching entirely (chain-of-trust verification still runs).

**Fix:** when connecting by IP with TLS, configure PubSubClient with `setServer(IPAddress, port)` instead of `setServer(const char*, port)`. PubSubClient routes IPAddress-based setServer to `Client::connect(IPAddress, port)`, which takes the non-SNI path through WiFiSSLClient.

### Gotcha C: NTPClient does NOT set the hardware RTC
mbedTLS calls `time()` to check cert `notBefore`/`notAfter` during handshake.
`time()` on the AMB82 reads the **hardware RTC**, not `NTPClient`'s internal
counter. `NTPClient::forceUpdate()` only updates its own epoch — the RTC stays
at boot-zero (e.g., year 1970), so every valid cert appears "not yet valid"
and handshake fails with `-0x2700`.

**Fix:** after `NTPClient::forceUpdate()` succeeds, call:
```cpp
extern "C" void rtc_init(void);
extern "C" void rtc_write(time_t t);
rtc_init();
rtc_write((time_t)timeClient.getEpochTime());
```
`rtc_write()` is the SDK-linkable symbol (in `liboutsrc.a`) that writes the
hardware RTC. **Do NOT use `set_time()` from `rtc_time.h`** — it's declared
but not compiled into the platform libraries, so the linker fails with
`undefined reference to set_time`. All subsequent `time()` calls (including
those inside mbedTLS) return correct wall-clock time after `rtc_write()`.

This must be done **before** any TLS operation (including `mqttMgr.begin()`
which calls `pullConfig()` over TLS).

### Gotcha B: Server cert must have IP in Subject Alternative Name
mbedTLS validates the connection target (IP or hostname) against the cert's
SAN, not against the CN. If the server cert only has `CN=sbbu01.local` but
the firmware connects to `192.168.2.143`, verification fails with `-0x2700`.

**Fix:** add SAN entries covering all names/IPs clients might use:
```
subjectAltName = DNS:sbbu01.local, IP:192.168.2.143, IP:127.0.0.1
```

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
