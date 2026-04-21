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
"MIIDITCCAgmgAwIBAgIUKpgrrLcdQUlFCJVIq2bHNISFJSwwDQYJKoZIhvcNAQEL\n" \
"BQAwGDEWMBQGA1UEAwwNQU1CODIgTVFUVCBDQTAeFw0yNjA0MTQwNjA0MzZaFw0z\n" \
"NjA0MTEwNjA0MzZaMBgxFjAUBgNVBAMMDUFNQjgyIE1RVFQgQ0EwggEiMA0GCSqG\n" \
"SIb3DQEBAQUAA4IBDwAwggEKAoIBAQDAwEmbnZejtRkznR3gWGkHGew7yjN2PKUA\n" \
"7/HalmqWbEhkJbDq5sVYp8VLoZ+ckG3EiTeCf/yfFGThw+8b/3eLvJplnGBRrzNU\n" \
"sVaHcusAvKd3VLiD39cbQrtnnIo94KxO1h5LZjots4MJP19B2GBOjFlOFSeTsTOB\n" \
"1GVmP42aonteRXTZ50m90FCfLK9Ce23mC4MHzwCVMjNoHwEllceckrAtxgA6Wm/I\n" \
"Deh5KOUFpJkmsVr7fWPYMoNOqyTVunXPlTQbTxh9In/jkRPS49H4vwzpFKpe2hOl\n" \
"3ahoLrvXoTYW/DzK93apFHBn6ZJOWQqW/pFxd4/cPohWQukbvANpAgMBAAGjYzBh\n" \
"MB0GA1UdDgQWBBRG+1ZZ/t89XOkFJIaH8nmExWr8BzAPBgNVHRMBAf8EBTADAQH/\n" \
"MA4GA1UdDwEB/wQEAwIBBjAfBgNVHSMEGDAWgBRG+1ZZ/t89XOkFJIaH8nmExWr8\n" \
"BzANBgkqhkiG9w0BAQsFAAOCAQEAVjdhTWu9NA8FVKzNVnjLbiX48xm6Q3JAH8+W\n" \
"pTk9ycKjrV4q/WemqPWk4xnVnUDUHJlF0soGarChIZva05khSqcg2hTHqfXBGkIk\n" \
"GFCfLczsteUq2soCYe7pYhplk2rYSvI93Pui1FXYYDZNn8AJXOFDfJ5beTOufL+A\n" \
"7JKVu4MwFaKopUsXW75EEYHO4+zUrvxPKrS46w3gsRH/KMzRKpHZq72XpVlsvRs+\n" \
"eRWeejnVjQ56ZTy1/SRj68rLcpT/HIeRY2wOzfjVTwQsdggH+iSEsHXX63WyHIPa\n" \
"XwU7PoWKCdw2AEfK2D/C19EqY9uV1zlR8oVy/afGgtptHz+CBw==\n" \
"-----END CERTIFICATE-----\n";

// Client certificate -- presented to broker for mTLS authentication
// Re-signed 2026-04-14 by regenerated CA (same client key)
static const char* mqtt_client_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIC/zCCAeegAwIBAgIUDs6ggp1GeYL374TtZhL6yOq7IUcwDQYJKoZIhvcNAQEL\n" \
"BQAwGDEWMBQGA1UEAwwNQU1CODIgTVFUVCBDQTAeFw0yNjA0MTQwNjA0MzdaFw0z\n" \
"NjA0MTEwNjA0MzdaMBcxFTATBgNVBAMMDGFtYjgyX2NhbV8wMTCCASIwDQYJKoZI\n" \
"hvcNAQEBBQADggEPADCCAQoCggEBANBfEXlaPty8RpqgbsF7BLdS1kIWITQ2ipf6\n" \
"wDHLBgATbOwRkBbnGYrdHBnBI2BYn7UUbN33sXnLNYqOLdGASa1wtHI1o+WkEvEn\n" \
"Qi3XP9xy5vWcQ+qDYVUvlxHc5E+AV7Xh58o3LBTTz3oSdqey0mW48VeVYCdtjtJP\n" \
"owuD1Fd37+NzA0SuEuN13b66lLh7cYWZ/+K5IKxlmF96AE3ExDSaHIobLwp83t07\n" \
"5a+dSygxuFocalymz9Gff5LMfAJkMdaM8oXMOhkTNrGknCEbPhldVrEByv+D9qXo\n" \
"NCDrMFs0CT1+cNiHhgnH8e4iNaZ59ECz5comDv2CIDhy57lCVcMCAwEAAaNCMEAw\n" \
"HQYDVR0OBBYEFALBdPKB9xeRn3ghbd3AV0yttLB+MB8GA1UdIwQYMBaAFEb7Vln+\n" \
"3z1c6QUkhofyeYTFavwHMA0GCSqGSIb3DQEBCwUAA4IBAQB2ahWToN3lZOET0Wgi\n" \
"Ec545wZ+tM1BhlqalFVmGKwUclcD5yEYQ7yZPPQV48Sg5yEmRQcPqSZvhwNJLGke\n" \
"Su6G1ckNjn6CP7729fk7nJcPXsuXK9qKUelH4oD/fKd3Q2bExO9WmXza+3L9m1SL\n" \
"iRwuA/Al8r5RlxWkBDWiWyVGMVbyme44WLN9uqaOEFCctNbcCDRgumTb7sW9V3yS\n" \
"7rDsCoTEfLNDUMEznpyi/C4irgFRBcuFAaNzdAZKjybxfoc9qfPJTi0YvgVku0zA\n" \
"R5coIkc59J6U3VOtwbTg98Ts9SMbTRqS6gYiEeV5Cqa3D6Omp+S0lDdtNSJaE4SD\n" \
"ERQ1\n" \
"-----END CERTIFICATE-----\n";

// Client private key -- keep secret, used for mTLS handshake
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
