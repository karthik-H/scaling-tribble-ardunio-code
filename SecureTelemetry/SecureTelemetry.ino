#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>

// -------------------------------------------------------------------------
// CONFIGURATION
// -------------------------------------------------------------------------
const char *WIFI_SSID = "YOUR_WIFI_SSID";     // REPLACE THIS
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD"; // REPLACE THIS

const char *SERVER_URL = "https://api.example-iot.com:8443/v1/telemetry/stream";
const int SENSOR_PIN = 34; // standard analog pin on ESP32

// If you have the Root CA cert, put it here.
// For prototype, we will use setInsecure() (NOT RECOMMENDED FOR PRODUCTION).
const char *ROOT_CA_CERT =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBh\n"
    "... (Place your Root CA Certificate Here) ...\n"
    "-----END CERTIFICATE-----\n";

// -------------------------------------------------------------------------
// GLOBALS
// -------------------------------------------------------------------------
uint8_t device_private_key[32]; // Derived from MAC address
String device_mac_str;

// -------------------------------------------------------------------------
// HELPER FUNCTIONS
// -------------------------------------------------------------------------

// Convert byte array to hex string
String bytesToHex(const uint8_t *data, size_t len) {
  String hexStr = "";
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 16)
      hexStr += "0";
    hexStr += String(data[i], HEX);
  }
  return hexStr;
}

// Generate Device Private Key from MAC Address using SHA-256
void generateDeviceKey() {
  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);

  // Create a string representation for logging/ID
  char macBuffer[18];
  snprintf(macBuffer, sizeof(macBuffer), "%02X:%02X:%02X:%02X:%02X:%02X",
           baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4],
           baseMac[5]);
  device_mac_str = String(macBuffer);

  // Derive private key: SHA256(MAC_BYTES)
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256 (not 224)
  mbedtls_sha256_update(&ctx, baseMac, 6);
  mbedtls_sha256_finish(&ctx, device_private_key);
  mbedtls_sha256_free(&ctx);

  Serial.println("Device ID (MAC): " + device_mac_str);
  Serial.println("Private Key Derived (In-Memory Only).");
}

// HMAC-SHA256 Signing
String signData(String data) {
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, device_private_key, 32);
  mbedtls_md_hmac_update(&ctx, (const unsigned char *)data.c_str(),
                         data.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  return bytesToHex(hmacResult, 32);
}

// -------------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }            // Wait for serial
  delay(1000); // Settle

  // 1. Initialize Security
  generateDeviceKey();

  // 2. Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // 3. Configure Time (NTP)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for time");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTime Synced");

  // 4. Configure Inputs
  pinMode(SENSOR_PIN, INPUT);
}

// -------------------------------------------------------------------------
// LOOP
// -------------------------------------------------------------------------
void loop() {
  // Check WiFi Connection
  if (WiFi.status() == WL_CONNECTED) {

    // 1. Read Sensor
    int sensorValue = analogRead(SENSOR_PIN);

    // Validate Range
    if (sensorValue < 0 || sensorValue > 4095) {
      Serial.printf("Error: Sensor value %d out of range (0-4095)\n",
                    sensorValue);
    } else {

      // 2. Prepare Payload Data
      unsigned long timestamp = time(
          nullptr); // NOTE: Requires NTP Sync for real time, but for simpler
                    // telemetry we act as if time is handled or ignored. Since
                    // user requested 'epoch seconds', we strictly need NTP.
                    // Let's add simple NTP config if needed, or just send
                    // 0/millis if NTP not set up. Re-reading requirements:
                    // "timestamp (epoch seconds)". I should add NTP.

      // Quick NTP sync attempt (blocking first time only if needed, or just
      // standard config) Usually configTime() is called in setup.

      // 3. Construct Signing String and JSON
      // Signature payload: "device_id|timestamp|sensor_value"
      // Note: time(nullptr) returns 0 if not synced.
      // User didn't explicitly ask for NTP code but implied epoch. I'll stick
      // to a placeholder "0" if not synced, but to be "Correct" I should
      // probably add `configTime` in setup.

      // Let's add NTP sync in setup so this is valid.

      String payloadData =
          device_mac_str + "|" + String(timestamp) + "|" + String(sensorValue);
      String signature = signData(payloadData);

      // Construct JSON
      String jsonPayload = "{";
      jsonPayload += "\"device_id\":\"" + device_mac_str + "\",";
      jsonPayload += "\"timestamp\":" + String(timestamp) + ",";
      jsonPayload += "\"sensor_value\":" + String(sensorValue) + ",";
      jsonPayload += "\"signature\":\"" + signature + "\"";
      jsonPayload += "}";

      Serial.println("Sending Payload: " + jsonPayload);

      // 4. Send HTTPS POST
      NetworkClientSecure client;
      client.setInsecure(); // Skip certificate validation for prototype
      // client.setCACert(ROOT_CA_CERT); // Use this for production

      HTTPClient https;
      if (https.begin(client, SERVER_URL)) {
        https.addHeader("Content-Type", "application/json");

        int httpCode = https.POST(jsonPayload);

        if (httpCode > 0) {
          Serial.printf("HTTPS POST Code: %d\n", httpCode);
          if (httpCode == HTTP_CODE_OK || httpCode == 201) {
            String payload = https.getString();
            Serial.println("Response: " + payload);
          }
        } else {
          Serial.printf("HTTPS POST Failed, error: %s\n",
                        https.errorToString(httpCode).c_str());
        }
        https.end();
      } else {
        Serial.println("Unable to connect to server");
      }
    }
  } else {
    Serial.println("WiFi Disconnected. Reconnecting...");
    WiFi.reconnect();
  }

  // 5. Wait 5 seconds
  delay(5000);
}
