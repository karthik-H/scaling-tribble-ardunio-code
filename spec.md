# ESP32 Secure Telemetry Specification

## 1. Overview
This project implements a secure telemetry system for an ESP32 microcontroller. It reads analog sensor data, signs it using a device-unique key derived from the MAC address, and streams it securely via HTTPS.

## 2. Requirements

### 2.1. Sensor Data Acquisition
- **Source**: Analog input pin GPIO 34.
- **Sampling Interval**: Every 5 seconds.
- **Validation**:
  - Value range: 0 to 4095 (12-bit resolution).
  - Invalid readings should be handled (logged or ignored).

### 2.2. Security & Identity
- **Device Identity**: Unique `device_id` based on the ESP32 MAC address.
- **Key Generation**:
  - **Input**: ESP32 MAC address.
  - **Algorithm**: SHA-256.
  - **Storage**: In-memory only. **NO hardcoded secrets**.
  - **Timing**: Generated at runtime during initialization.

### 2.3. Data Integrity & Authentication
- **Signing Algorithm**: HMAC-SHA256.
- **Key**: The runtime-derived private hash key.
- **Payload Structure**:
  ```json
  {
    "device_id": "string",
    "timestamp": number (epoch seconds),
    "sensor_value": number (integer),
    "signature": "string (hex encoded HMAC)"
  }
  ```

### 2.4. Data Transmission
- **Network**: WiFi (station mode).
- **Protocol**: HTTPS (TLS/SSL).
- **Method**: HTTP POST.
- **Endpoint**: `https://api.example-iot.com:8443/v1/telemetry/stream`
- **Port**: 8443.
- **Content-Type**: `application/json`.
- **Error Handling**: Basic connection retry logic.

## 3. constraints
- **Platform**: ESP32 (Arduino framework).
- **Libraries**:
  - `WiFi` (Standard ESP32)
  - `HTTPClient` (Standard ESP32)
  - `NetworkClientSecure` (Standard ESP32 for HTTPS)
  - `mbedtls` (Standard ESP32 SDK for crypto) OR `ArduinoJson` (if needed for JSON, though manual string formatting is acceptable for simple payloads).
