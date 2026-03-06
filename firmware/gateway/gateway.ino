/*
 * ============================================================
 * LANDSLIDE EARLY WARNING SYSTEM - GATEWAY NODE
 * OPTIMIZED FOR 195mm ANTENNA
 * ============================================================
 * LoRa: 433 MHz, SF7 (fast), optimized for proper antenna
 * ============================================================
 */

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>

#define SCK   18
#define MISO  19
#define MOSI  23
#define SS    5
#define RST   14
#define DIO0  2

#define SIM800L_RX 16
#define SIM800L_TX 17
HardwareSerial sim800l(1);

// ⚠️  UPDATE THESE FOR YOUR LOCATION!
const char* ssid          = "vivoV60";
const char* password      = "12345678";
const char* serverUrl     = "http://10.39.11.65:3000/api/data";

const char* alertPhoneNumber = "+639275196190";
unsigned long lastSMSTime = 0;
const unsigned long SMS_COOLDOWN = 300000;

void setup() {
  Serial.begin(115200);
  delay(1000);
  sim800l.begin(9600, SERIAL_8N1, SIM800L_RX, SIM800L_TX);

  Serial.println("============================================");
  Serial.println("LANDSLIDE EWS - GATEWAY NODE");
  Serial.println("============================================");

  // Connect WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi Connected!");
    Serial.print("  IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("  Backend: ");
    Serial.println(serverUrl);
  } else {
    Serial.println("\n✗ WiFi Failed! SMS-only mode.");
  }

  // Initialize LoRa
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("✗ LoRa 433 MHz failed!");
    Serial.println("  Check wiring and antenna!");
    while (1) delay(1000);
  }

  // Configure LoRa - OPTIMIZED FOR 195mm ANTENNA (MUST MATCH TRANSMITTER!)
  LoRa.setTxPower(20);            // Maximum power
  LoRa.setSpreadingFactor(7);     // SF7 - Fast with good antenna
  LoRa.setSignalBandwidth(125E3); // 125 kHz
  LoRa.setCodingRate4(5);         // 4/5 coding rate
  LoRa.enableCrc();               // Data validation
  LoRa.setSyncWord(0x34);         // Private network

  Serial.println("✓ LoRa Gateway Initialized (433 MHz)");
  Serial.println("  Antenna: 195mm");
  Serial.println("  TX Power: 20 dBm");
  Serial.println("  Spreading Factor: 7 (Fast mode)");
  Serial.println("  Coding Rate: 4/5");
  Serial.println("  Bandwidth: 125 kHz");

  // Initialize SIM800L GSM
  delay(3000);
  Serial.println("Initializing SIM800L GSM...");
  initializeSIM800L();

  Serial.println("============================================");
  Serial.println("SYSTEM READY - Listening for LoRa packets");
  Serial.println("============================================\n");
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    Serial.println("┌────────────────────────────────────────┐");
    Serial.println("│    LORA PACKET RECEIVED                │");
    Serial.println("└────────────────────────────────────────┘");
    Serial.print("RSSI: "); Serial.print(rssi); Serial.println(" dBm");
    Serial.print("SNR:  "); Serial.print(snr); Serial.println(" dB");
    Serial.print("Raw Data: "); Serial.println(incoming);
    Serial.println("─────────────────────────────────────────");

    // Parse CSV
    float values[6] = {0, 0, 0, 0, 0, 0};
    int valueIndex = 0;
    String token = "";

    for (int i = 0; i <= incoming.length(); i++) {
      char c = (i < incoming.length()) ? incoming.charAt(i) : ',';
      if (c == ',') {
        if (valueIndex < 6) {
          values[valueIndex] = token.toFloat();
          valueIndex++;
        }
        token = "";
      } else {
        token += c;
      }
    }

    int   soilMoisture = (int)values[0];
    int   rainValue    = (int)values[1];
    float tiltX        = values[2];
    float tiltY        = values[3];
    float tiltZ        = values[4];
    int   alertLevel   = (int)values[5];

    // Validate
    if (valueIndex < 6 || alertLevel < 0 || alertLevel > 3) {
      Serial.println("⚠ Corrupted packet - skipping!");
      Serial.print("   Parsed values: ");
      Serial.println(valueIndex);
      Serial.println();
      return;
    }

    // Display parsed data
    Serial.println("PARSED DATA:");
    Serial.print("Soil Moisture:   "); Serial.print(soilMoisture); Serial.println(" %");
    Serial.print("Rain Value:      "); Serial.println(rainValue);
    Serial.print("Tilt (X,Y,Z):    ");
    Serial.print(tiltX, 1); Serial.print(", ");
    Serial.print(tiltY, 1); Serial.print(", ");
    Serial.println(tiltZ, 1);
    Serial.print("Alert Level:     "); Serial.println(alertLevel);
    Serial.println("═════════════════════════════════════════\n");

    // Send to backend
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");
      http.setTimeout(10000);

      String csvData = String(soilMoisture) + "," +
                       String(rainValue)    + "," +
                       String(tiltX, 1)    + "," +
                       String(tiltY, 1)    + "," +
                       String(tiltZ, 1)    + "," +
                       String(alertLevel);

      String jsonPayload = "{\"data\":\"" + csvData + "\"}";

      Serial.print("Sending to backend... ");
      int httpCode = http.POST(jsonPayload);

      if (httpCode > 0) {
        Serial.print("SUCCESS - HTTP ");
        Serial.println(httpCode);
        Serial.print("Response: ");
        Serial.println(http.getString());
      } else {
        Serial.print("FAILED - ");
        Serial.println(http.errorToString(httpCode).c_str());
        sendSMS("ALERT: Backend offline. Level: " + String(alertLevel));
      }

      http.end();
      Serial.println();

    } else {
      Serial.println("⚠ WiFi Disconnected! SMS backup...");
      sendSMS("ALERT: No WiFi. Level: " + String(alertLevel));
    }

    // SMS for high alerts
    if (alertLevel == 3) {
      String msg = "EMERGENCY: LANDSLIDE DANGER!\n";
      msg += "Alert: Level 3 - DANGER (Very High Risk)\n";
      msg += "Soil: " + String(soilMoisture) + "%\n";
      msg += "EVACUATE IMMEDIATELY!";
      sendSMS(msg);
    } else if (alertLevel == 2) {
      String msg = "WARNING: Landslide Risk Detected!\n";
      msg += "Alert: Level 2 - WARNING (High Risk)\n";
      msg += "Soil: " + String(soilMoisture) + "%\n";
      msg += "Prepare to evacuate!";
      sendSMS(msg);
    }
  }

  delay(100);
}

void initializeSIM800L() {
  Serial.println("Waiting for SIM800L to boot...");
  delay(5000);  // SIM800L needs time to boot up
  
  // Test basic communication
  Serial.print("Testing AT... ");
  sim800l.println("AT");
  delay(1000);
  
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 2000) {  // Wait up to 2 seconds
    if (sim800l.available()) {
      char c = sim800l.read();
      response += c;
      Serial.write(c);
    }
  }
  
  if (response.indexOf("OK") >= 0) {
    Serial.println("✓ AT OK");
  } else {
    Serial.println("⚠ AT Failed (continuing anyway)");
  }
  
  // Set SMS text mode
  Serial.print("Setting SMS mode... ");
  sim800l.println("AT+CMGF=1");
  delay(1000);
  
  response = "";
  startTime = millis();
  while (millis() - startTime < 2000) {
    if (sim800l.available()) {
      char c = sim800l.read();
      response += c;
      Serial.write(c);
    }
  }
  
  if (response.indexOf("OK") >= 0) {
    Serial.println("✓ SMS Mode OK");
  } else {
    Serial.println("⚠ SMS Mode Failed");
  }
  
  // Check network registration
  Serial.print("Checking network... ");
  sim800l.println("AT+CREG?");
  delay(1000);
  
  startTime = millis();
  while (millis() - startTime < 2000) {
    if (sim800l.available()) {
      Serial.write(sim800l.read());
    }
  }
  Serial.println();
  
  Serial.println("✓ SIM800L Initialized");
  Serial.println("  Note: SMS will only work with good cellular signal");
}

void sendSMS(String message) {
  if (millis() - lastSMSTime < SMS_COOLDOWN) {
    Serial.println("⚠ SMS Cooldown active");
    return;
  }
  Serial.println("Sending SMS...");
  sim800l.println("AT+CMGF=1");
  delay(100);
  sim800l.println("AT+CMGS=\"" + String(alertPhoneNumber) + "\"");
  delay(100);
  sim800l.print(message);
  delay(100);
  sim800l.write(26);
  delay(5000);
  while (sim800l.available()) {
    Serial.write(sim800l.read());
  }
  lastSMSTime = millis();
  Serial.println("✓ SMS Sent!");
}
