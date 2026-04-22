#ifndef MQTT_CERTS_H
#define MQTT_CERTS_H

// ============================================================
// MQTT TLS Certificates (PEM format)
//
// Generated with OpenSSL for LAN-only mTLS setup.
// CA: self-signed "AMB82 MQTT CA" (10-year validity)
// Server CN: sbbu01.local
// Client CN: amb82_cam_01
//
// To regenerate, see README.md "MQTT Security" section.
// ============================================================

// CA certificate -- used to verify the broker's server certificate
// Regenerated 2026-04-14 with basicConstraints=CA:TRUE (mbedTLS requirement)
static const char* mqtt_ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDADCCAeigAwIBAgIUODyAfZlwfPM3CRtBriWtj62r7b4wDQYJKoZIhvcNAQEL\n" \
"BQAwGDEWMBQGA1UEAwwNQU1CODIgTVFUVCBDQTAeFw0yNjA0MjIwNzAxNTBaFw0z\n" \
"NjA0MTkwNzAxNTBaMBgxFjAUBgNVBAMMDUFNQjgyIE1RVFQgQ0EwggEiMA0GCSqG\n" \
"SIb3DQEBAQUAA4IBDwAwggEKAoIBAQDZIdIcU9GmDwNe6ZBb3bRQqoSNR7Xf+jD/\n" \
"wdxN5kcpSmWhyqq/DRQWOdaugXG6GlIZ5Ps+LkMQ2t13j/6yTzGMVGcrSsEo605U\n" \
"Ul20fc5x6byimJLMDMVfpeH/0BVhfAj2mnwN9HX3BqN/aBDVNycCCiSxXi4Cp0dt\n" \
"uZwt89xtvcXrPoZFm1HxYQrcghjExmuLdVjv1omiHgABEZ4/tTgtEkXEtcjvgy2m\n" \
"wkyJJ3ECsCjBofFXdm4GdEFhGHcq1PRi9/stMQJAkVjBSmXDqIamOTv01R3h41wg\n" \
"8YaMPJQArHhSLGgxxeQhmxdB4eZYm8Om9SBBdkPapj9/xJUS1BM3AgMBAAGjQjBA\n" \
"MA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/BAQDAgEGMB0GA1UdDgQWBBQD/jCn\n" \
"ycwcwSON7g5YYdxFSoVHnjANBgkqhkiG9w0BAQsFAAOCAQEAjgbaI2hoO21CIBE8\n" \
"HjOly/7K7WE6eNR5lAIJP+8ISl3vjOhF5hqll2NrUCX/SDRox4BtPwPA0otXgE9R\n" \
"O+fQgaHN8kTcQYb9YV4UOJQQPhe12VfR2WHhoiqJzrHBrYcqam5/wBUh84P8X8jK\n" \
"gWP55/JA3YzBqCyA9BcWZ3awb2pkXevG+ERNy+Xjx2Qhb+lK4tQMkOrpO2ePWoao\n" \
"otF0uDc6tPwA7JR3T4TbTxwbb49Yv6zRYzWZj1Dq5Gf9XlEX1/8u83tXLVeD9L/F\n" \
"W0yhdrxmW3irQXHjBCp6qlPYOA0/qV5J816/YZD+0fxNImOZpV8BjAXkIQ08/R0S\n" \
"kEQVFw==\n" \
"-----END CERTIFICATE-----\n";

// Client certificate -- presented to broker for mTLS authentication
// Re-signed 2026-04-14 by regenerated CA (same client key)
static const char* mqtt_client_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIC/zCCAeegAwIBAgIUfOPBFYpbZ4wYJU3NNNTdqzWFemcwDQYJKoZIhvcNAQEL\n" \
"BQAwGDEWMBQGA1UEAwwNQU1CODIgTVFUVCBDQTAeFw0yNjA0MjIwNzAxNTFaFw0y\n" \
"NzA0MjIwNzAxNTFaMBcxFTATBgNVBAMMDGFtYjgyX2NhbV8wMTCCASIwDQYJKoZI\n" \
"hvcNAQEBBQADggEPADCCAQoCggEBAOAVJiXNMVaG6a5zbf7l4KqMt9x/XNYdJCcz\n" \
"RW1P8Q8UMZe/vwYrgG6st3CHdhB7WuIb0+cNHH3645dd7qWo1+2S/4IJ+QQFKoPh\n" \
"Xgdj+mFhWruykFByJg44i7w67d+4fM0i7tMp0+d7y6QrCok55THCZYUjbhJru6KE\n" \
"Iq5QyC3+3Bs5ZPXscpj5pDcaA0LnAz6Iowu4liJU57mgQjNhbSSUD3zAuLg0/0Nu\n" \
"uzLT7Zdj9IYlzvs3FeUs0YV9AUfAJjzsojGOTrrntVZgxnxPc0pdwDI/WQ4MXy1u\n" \
"ZNeSZ2D2Lx9n5aSETKhocLtp3WCwkeaWU77C9V3iMIw/fSBczEcCAwEAAaNCMEAw\n" \
"HQYDVR0OBBYEFCpFLfQeqlHiTf0GEQk0yzlvHPe0MB8GA1UdIwQYMBaAFAP+MKfJ\n" \
"zBzBI43uDlhh3EVKhUeeMA0GCSqGSIb3DQEBCwUAA4IBAQBI00NXuja2rVWFVYIN\n" \
"HrxgrX8DJFjMPRlql/e4szDLP9dQyd792Lsw9hhLSusNl2faBwHNDixF8SC66TYD\n" \
"BhuFZmFbFkN9gmv01ohwV66BJq2pwJfX54EZB0DC7OAL+5bManPlJAjpI/2qXFsu\n" \
"BbYjBaqR5W6H+v5n9WvWIKk808qH1iBBVSg0CHe4aX5ECIYJQfpZRVVM1vg719UG\n" \
"BIKepq/UiQKs+QMb33h6BI+iVd73cJng8QroP4/CGWveTG3SrzBnx8v8rMi7q0nr\n" \
"wvqL6Cv37wGvO2JbpEemMhAtV98Ut5JgENFdLcsPchn/7X+MYKs8k41TPjcQcrr9\n" \
"65nl\n" \
"-----END CERTIFICATE-----\n";

// Client private key -- keep secret, used for mTLS handshake
static const char* mqtt_client_key = \
"-----BEGIN PRIVATE KEY-----\n" \
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDgFSYlzTFWhumu\n" \
"c23+5eCqjLfcf1zWHSQnM0VtT/EPFDGXv78GK4BurLdwh3YQe1riG9PnDRx9+uOX\n" \
"Xe6lqNftkv+CCfkEBSqD4V4HY/phYVq7spBQciYOOIu8Ou3fuHzNIu7TKdPne8uk\n" \
"KwqJOeUxwmWFI24Sa7uihCKuUMgt/twbOWT17HKY+aQ3GgNC5wM+iKMLuJYiVOe5\n" \
"oEIzYW0klA98wLi4NP9Dbrsy0+2XY/SGJc77NxXlLNGFfQFHwCY87KIxjk6657VW\n" \
"YMZ8T3NKXcAyP1kODF8tbmTXkmdg9i8fZ+WkhEyoaHC7ad1gsJHmllO+wvVd4jCM\n" \
"P30gXMxHAgMBAAECggEAHHUzIgvcHzQFC12nc364OJCkbQFTxfFvfPb68zX61EJp\n" \
"Re9ZECyIJAYVSdo6OT+hPCPyll5I5wrsynVocGtv+MD5aAdiM4uAab0+VsvwqVtW\n" \
"zY2Q3FaS1K2DuYT2XrosR1UPZYvjWHuVUiURMnpCzD327KCiQv6hlrMXQHIL6iM9\n" \
"4y7FtNmUJgSasG2Yev+7YKQzkIhKSCTREYn1W7Z/xOQ+wGt5UcRFxdEc6ziVKlxN\n" \
"c90feS6o4zrBjEKj4MT9qOVBRWqJwvvtsEWiXo6AzTdwg9SM8iZLlLU9Bpge+TPH\n" \
"hgDf8tFBaXtcgVeP83hXQ4BShFcio8Yrwhp1tUk8qQKBgQD7P+HMjmdFLWth1Tcj\n" \
"DLBDl87DE7S6bgE/MqOi6bhMknyneQ8PNJPrvqcT6L3A/bMlk1OKGdE+WwBe0fYW\n" \
"ezm3hyrBqVtqRoao6k4iVY8C2WxF8nMkSisHk5+cmcq9TtpAlmpQ7Ikcd2R0+sRw\n" \
"b7mjDOL/JHCbss8ZnWXXuRgTUwKBgQDkUcV/YP2gWE8HkmIQRLmkybYGFhnbIy0U\n" \
"wRaN80Py6d/t+fes+4GYFWCfMJKcGbEPaZdy+j8NxQ5AGoe+T1/RWtbBuPTRJzdp\n" \
"fXjUro2GAuIbUkAj8x4JSl1yVsgjlhGyn/xCgvqJB3ABou6HVKxqCqZB+UhinyVv\n" \
"kczTZQZYvQKBgQCLHRjqCSMXA4oEdmj8CCeElYaPLtcPrp8UHfbK2nwIMcfBdt8m\n" \
"vSb2De82r9R5Cj/qG0PTIG5iB0MSPVDzq7EOdFHxPtN16azf1DQFALiZjEdOB2tH\n" \
"eaWPoWZr8B94aDOiNWaSfhWA5H6D4uFnnAT+ScFaIhQTsZLIDQV5x2uULwKBgQDS\n" \
"h/BUD4xvIV7mPX29QJHqOLFmlVSMM7hrDc0NYYaDZFK/LqLKWDPcl8G7qF9YH3yc\n" \
"GKK1O7mOqekFBTGsM6bH9jpW8YSVo9K9rBwSCU6ohtoVlVddjt1gdbkLKKht4AA7\n" \
"tLg4YGxClKAcci/+i/5b3awG46VygwtTAJ9dQieDBQKBgGlT0sL+qiDcq3kYGoKI\n" \
"8gCZDSaeKaSPWYwPPhJIIi/lpr7v4EEGN/73hTK+wjwa7LtbKAbOma8R2DNcU4rT\n" \
"3dN144Gu0P5DNIefAVyJMJYDsiHPpynHrWjoDhw23lm+2PBhNSywbNUxMU2FZS+w\n" \
"1NwmNQ2NNEgcWGDVLy3k17fC\n" \
"-----END PRIVATE KEY-----\n";

#endif // MQTT_CERTS_H
