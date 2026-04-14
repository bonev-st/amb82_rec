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

// CA certificate — used to verify the broker's server certificate
static const char* mqtt_ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDETCCAfmgAwIBAgIUGgoTSoDiswqMU6a66V/i45V1TpowDQYJKoZIhvcNAQEL\n" \
"BQAwGDEWMBQGA1UEAwwNQU1CODIgTVFUVCBDQTAeFw0yNjA0MTMwNjU0MzdaFw0z\n" \
"NjA0MTAwNjU0MzdaMBgxFjAUBgNVBAMMDUFNQjgyIE1RVFQgQ0EwggEiMA0GCSqG\n" \
"SIb3DQEBAQUAA4IBDwAwggEKAoIBAQDAwEmbnZejtRkznR3gWGkHGew7yjN2PKUA\n" \
"7/HalmqWbEhkJbDq5sVYp8VLoZ+ckG3EiTeCf/yfFGThw+8b/3eLvJplnGBRrzNU\n" \
"sVaHcusAvKd3VLiD39cbQrtnnIo94KxO1h5LZjots4MJP19B2GBOjFlOFSeTsTOB\n" \
"1GVmP42aonteRXTZ50m90FCfLK9Ce23mC4MHzwCVMjNoHwEllceckrAtxgA6Wm/I\n" \
"Deh5KOUFpJkmsVr7fWPYMoNOqyTVunXPlTQbTxh9In/jkRPS49H4vwzpFKpe2hOl\n" \
"3ahoLrvXoTYW/DzK93apFHBn6ZJOWQqW/pFxd4/cPohWQukbvANpAgMBAAGjUzBR\n" \
"MB0GA1UdDgQWBBRG+1ZZ/t89XOkFJIaH8nmExWr8BzAfBgNVHSMEGDAWgBRG+1ZZ\n" \
"/t89XOkFJIaH8nmExWr8BzAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUA\n" \
"A4IBAQArRs41EZQ7Ei2RdgEM22Xl23F446R7plXxv3vWqEagwHqIe1GE3vwMSMj7\n" \
"GyWOlNJwCTR/jPonZklj5yJ2J1zP6VCvD3j3aFbpJwEFbBubLyekjngub+PiHi3j\n" \
"Mg5fZteM9jGAWjckB/qA2iyephqlw9hDQ3/Ga8lylKmw627/yFz5CQx8a88t1lps\n" \
"Kz60CTI6/nRAKaEwNfti8cQDGVM6/EFNUY9w86ZCGPdPFmPha9Yb2ENtTSaDlClp\n" \
"sABJqY9I+JBQTIysY1LfbtK/Expr0H0avMdsG5ecbIsnAZ1OUnTEnh+JJWnFfAyA\n" \
"FlNeuClOyDMFVpLZSTJR3Re3/8hR\n" \
"-----END CERTIFICATE-----\n";

// Client certificate — presented to broker for mTLS authentication
static const char* mqtt_client_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIC/zCCAeegAwIBAgIUDs6ggp1GeYL374TtZhL6yOq7IUMwDQYJKoZIhvcNAQEL\n" \
"BQAwGDEWMBQGA1UEAwwNQU1CODIgTVFUVCBDQTAeFw0yNjA0MTMwNjU0MzhaFw0z\n" \
"NjA0MTAwNjU0MzhaMBcxFTATBgNVBAMMDGFtYjgyX2NhbV8wMTCCASIwDQYJKoZI\n" \
"hvcNAQEBBQADggEPADCCAQoCggEBANBfEXlaPty8RpqgbsF7BLdS1kIWITQ2ipf6\n" \
"wDHLBgATbOwRkBbnGYrdHBnBI2BYn7UUbN33sXnLNYqOLdGASa1wtHI1o+WkEvEn\n" \
"Qi3XP9xy5vWcQ+qDYVUvlxHc5E+AV7Xh58o3LBTTz3oSdqey0mW48VeVYCdtjtJP\n" \
"owuD1Fd37+NzA0SuEuN13b66lLh7cYWZ/+K5IKxlmF96AE3ExDSaHIobLwp83t07\n" \
"5a+dSygxuFocalymz9Gff5LMfAJkMdaM8oXMOhkTNrGknCEbPhldVrEByv+D9qXo\n" \
"NCDrMFs0CT1+cNiHhgnH8e4iNaZ59ECz5comDv2CIDhy57lCVcMCAwEAAaNCMEAw\n" \
"HQYDVR0OBBYEFALBdPKB9xeRn3ghbd3AV0yttLB+MB8GA1UdIwQYMBaAFEb7Vln+\n" \
"3z1c6QUkhofyeYTFavwHMA0GCSqGSIb3DQEBCwUAA4IBAQA43InO96bJcbfpvDH/\n" \
"hvgtQE93sOa1lXN0mQ6G7CL+D/69XnMX4++l6jUP+lel+rY9rhfpZq+VTgz3OqLa\n" \
"B+PiuxrA9/F3zzDSd4LUJV+yobYRL/OhAaB120IMoMJrpgtyb2ke5ItDqiwRnxrK\n" \
"wVlZuOpcyMoEBveuoQ6zW6voopCprlkJUq86YSGiEiN18+D3l3ekrQfw/kKvoDky\n" \
"QbwqbR/TvWOnKxV5qN9LUeZS6c/2MlPxYwt4vwAlYmh9Ltfb/PNJhC8HnmeX4yN1\n" \
"NlmJptVVRL18PGep/tPpa91izI9WV5WL/pnrWaiDCw0cQFSriuvwSIdSv1bcPXFF\n" \
"Hudo\n" \
"-----END CERTIFICATE-----\n";

// Client private key — keep secret, used for mTLS handshake
static const char* mqtt_client_key = \
"-----BEGIN PRIVATE KEY-----\n" \
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDQXxF5Wj7cvEaa\n" \
"oG7BewS3UtZCFiE0NoqX+sAxywYAE2zsEZAW5xmK3RwZwSNgWJ+1FGzd97F5yzWK\n" \
"ji3RgEmtcLRyNaPlpBLxJ0It1z/ccub1nEPqg2FVL5cR3ORPgFe14efKNywU0896\n" \
"EnanstJluPFXlWAnbY7ST6MLg9RXd+/jcwNErhLjdd2+upS4e3GFmf/iuSCsZZhf\n" \
"egBNxMQ0mhyKGy8KfN7dO+WvnUsoMbhaHGpcps/Rn3+SzHwCZDHWjPKFzDoZEzax\n" \
"pJwhGz4ZXVaxAcr/g/al6DQg6zBbNAk9fnDYh4YJx/HuIjWmefRAs+XKJg79giA4\n" \
"cue5QlXDAgMBAAECggEAA6pjrQFWUSn38GXTHPc6v+jZ2RBfmPYLd4OTPvnMPlMZ\n" \
"KIjHwqD7iFkkbE8pl3Sa3Qoh2I1jcC0peqloPlYl6NwUzvXvm+B7iGxOFAiRC7um\n" \
"IXr6w0gSMPfx2rNKluilYlwGtYsSU1l4is3pWKdJX6QijScuqYyAi/8xP8iCNnmr\n" \
"1UhO2RKBwF4liHKNulpA613zXfiZ5BIStfcp/tp6G8riz4CH7VZ1rgJzUFIBTyEf\n" \
"+9iKd0LKMhZn0Ofh8RR29CrtxN0t8TrcL6kOPLJcvG6nnVax6mI9ozI/65IsnqYz\n" \
"KnqYB7GW2ma8jdGRMnFvtHMJIMk3vLJ0DrQhBMOydQKBgQDuSHYE18QkCLJ/pnkO\n" \
"flhIPJx5V2uxFsJpxmEdQ/3zdH9uZeUqSP8a7sHBiCTjtfsunOd43ncFth+4psZZ\n" \
"oxtpS1rF6zdxROh0G7kqEKrAyAkU3vFpTMjtzLC9EWkDQYpTctkneGQOUc3HOS9b\n" \
"zIrZ//jXtz6R3H11rz//2CrPXwKBgQDf3UK7jHoaoED1Sf9ytSgNdTT2rIctTyrK\n" \
"lCavTAUWGl5FbQilxTPRukRKCsy1nzFLdf2EccMEd4QP0Ww+co6FelL8IdevtoXi\n" \
"pRk7iiT8VK8hmk4wHpEyJC7j7S5VJxIAyDv3h/hEzhLHnc50JpPrLoOA3kS+fzDz\n" \
"6Q9wjT4oHQKBgGSmwQtspNJfxh57kFkZ72qcB7CNx9Sm+3o4rN7y0Huc8xMMAZAq\n" \
"A3A5+CgqvQJe5XocFv6MYhRMiPuznsdQSYzhordFk0bKR7J732wwXCBQnt1tCuZi\n" \
"4+Dm/KTwjL4uWiLDuYydow4VaenEcwfAz8okANYF9m0giPJEE7GbewHbAoGBANJG\n" \
"rCMsvZj7BGki+75MaXXSrVAzGLo3jbNBW+D24CQ12m0OELxdMuKCOxjtcgH7qTr/\n" \
"doWMsikk9jhBd5Jk2niIQSCxzT4wjSwp9jyV803NYb+HiH/shmf50s2ngQjdLSzs\n" \
"6F8HKe2/P6afFUjG39ReGYnXvezN3jaNqUIIeeUFAoGBALWf3IJIN/abiYQjmYkO\n" \
"yT/VWWZ7hAIUvSb2qSjvS95VFUVsPWyGsS4UYj1Tvv8upS4gpLIQHrOmZW2EV38C\n" \
"Zf4C0eSJj1I+admxc7kSCI/4cbGRIkrwfkYJ7pPi60TyBb2AMJ/zXbypRZ2f3oY3\n" \
"m4DZdK3lZNME50uiUrQO25M7\n" \
"-----END PRIVATE KEY-----\n";

#endif // MQTT_CERTS_H
