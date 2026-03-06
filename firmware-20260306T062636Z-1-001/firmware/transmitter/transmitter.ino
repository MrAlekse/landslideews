/*
 * ============================================================
 * LANDSLIDE EARLY WARNING SYSTEM - TRANSMITTER NODE
 * OPTIMIZED FOR 195mm ANTENNA
 * ============================================================
 * LoRa: 433 MHz, SF7 (fast), optimized for proper antenna
 * ============================================================
 */

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_Sensor.h>

// LoRa Pin Configuration
#define SCK   18
#define MISO  19
#define MOSI  23
#define SS    5
#define RST   14
#define DIO0  2

// Sensor Pin Configuration
#define SOIL_MOISTURE_PIN  34
#define RAIN_SENSOR_PIN    35

// Alert Thresholds - ALIGNED WITH THESIS TABLE 1
#define SOIL_CAUTION    60       // > 60% = Caution Level
#define SOIL_ALERT      80       // > 80% = Alert Level  
#define SOIL_WARNING    90       // > 90% = Warning Level
#define RAIN_CAUTION    500      // < 500 = Caution
#define RAIN_ALERT      250      // < 250 = Alert
#define RAIN_WARNING    50       // < 50 = Warning
#define TILT_CAUTION    5.0      // > 5° = Caution start
#define TILT_ALERT      10.0     // > 10° = Alert
#define TILT_WARNING    20.0     // > 20° = Warning

#define NODE_ID "NODE_001"

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Baseline calibration values
float baselineX = 0;
float baselineY = 0;
float baselineZ = 0;
bool calibrated = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("============================================");
  Serial.println("LANDSLIDE EWS - TRANSMITTER NODE");
  Serial.println("============================================");

  // Initialize LoRa
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("✗ LoRa 433 MHz failed!");
    Serial.println("  Check wiring and antenna!");
    while (1) delay(1000);
  }

  // Configure LoRa - OPTIMIZED FOR 195mm ANTENNA
  LoRa.setTxPower(20);            // Maximum power
  LoRa.setSpreadingFactor(7);     // SF7 - Fast with good antenna
  LoRa.setSignalBandwidth(125E3); // 125 kHz
  LoRa.setCodingRate4(5);         // 4/5 coding rate
  LoRa.enableCrc();               // Data validation
  LoRa.setSyncWord(0x34);         // Private network

  Serial.println("✓ LoRa Initialized (433 MHz)");
  Serial.println("  Antenna: 195mm");
  Serial.println("  TX Power: 20 dBm");
  Serial.println("  Spreading Factor: 7 (Fast mode)");
  Serial.println("  Coding Rate: 4/5");
  Serial.println("  Bandwidth: 125 kHz");

  // Initialize ADXL345
  if (!accel.begin()) {
    Serial.println("✗ ADXL345 not detected!");
    Serial.println("  Check I2C wiring (SDA=21, SCL=22)");
    Serial.println("  Continuing without tilt sensor...");
  } else {
    accel.setRange(ADXL345_RANGE_16_G);
    Serial.println("✓ ADXL345 Initialized");
  }

  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(RAIN_SENSOR_PIN, INPUT);
  Serial.println("✓ Sensor pins initialized");

  // Calibrate tilt sensor baseline
  Serial.println("\nCalibrating tilt sensor...");
  Serial.println("Keep device STABLE for 3 seconds...");
  delay(3000);
  
  // Take 10 readings and average
  float sumX = 0, sumY = 0, sumZ = 0;
  for (int i = 0; i < 10; i++) {
    sensors_event_t event;
    accel.getEvent(&event);
    sumX += event.acceleration.x;
    sumY += event.acceleration.y;
    sumZ += event.acceleration.z;
    delay(100);
  }
  
  baselineX = sumX / 10.0;
  baselineY = sumY / 10.0;
  baselineZ = sumZ / 10.0;
  calibrated = true;
  
  Serial.println("✓ Tilt sensor calibrated!");
  Serial.print("  Baseline X: "); Serial.println(baselineX, 2);
  Serial.print("  Baseline Y: "); Serial.println(baselineY, 2);
  Serial.print("  Baseline Z: "); Serial.println(baselineZ, 2);

  Serial.println("============================================");
  Serial.println("SYSTEM READY - Starting transmission...");
  Serial.println("============================================\n");
  delay(2000);
}

void loop() {
  // Read sensors
  int soilRaw = analogRead(SOIL_MOISTURE_PIN);
  int rainRaw = analogRead(RAIN_SENSOR_PIN);
  
  // FIXED: Inverted sensor type (low value = dry, high value = wet)
  int soilMoisture = map(soilRaw, 0, 4095, 0, 100);
  soilMoisture = constrain(soilMoisture, 0, 100);
  
  // Keep rain raw for YL-83 thresholds
  int rainValue = rainRaw;

  // Read accelerometer
  sensors_event_t event;
  accel.getEvent(&event);
  float tiltX = event.acceleration.x;
  float tiltY = event.acceleration.y;
  float tiltZ = event.acceleration.z;

  float tiltAngle = calculateTiltAngle(tiltX, tiltY, tiltZ);
  int alertLevel  = determineAlertLevel(soilMoisture, rainValue, tiltAngle);

  displayReadings(soilMoisture, rainValue, tiltAngle, alertLevel, soilRaw, rainRaw);

  // Build CSV packet
  String packet = String(soilMoisture) + "," +
                  String(rainValue)    + "," +
                  String(tiltX, 1)    + "," +
                  String(tiltY, 1)    + "," +
                  String(tiltZ, 1)    + "," +
                  String(alertLevel);

  // Send via LoRa
  Serial.print("Transmitting via LoRa... ");
  
  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();
  
  Serial.println("SENT ✓");
  Serial.print("Packet: ");
  Serial.println(packet);
  Serial.println();

  delay(5000);
}

float calculateTiltAngle(float x, float y, float z) {
  if (!calibrated) {
    // If not calibrated, use original method
    float tiltRad = atan2(sqrt(x * x + y * y), z);
    return abs(tiltRad * 180.0 / PI);
  }
  
  // Calculate deviation from baseline
  float deltaX = x - baselineX;
  float deltaY = y - baselineY;
  float deltaZ = z - baselineZ;
  
  // Total change in acceleration
  float totalChange = sqrt(deltaX*deltaX + deltaY*deltaY + deltaZ*deltaZ);
  
  // Convert to approximate degrees
  // Each 1 m/s² change ≈ ~6° tilt for small angles
  float angleDegrees = (totalChange / 9.8) * 90.0;
  
  return angleDegrees;
}

int determineAlertLevel(int soil, int rain, float tilt) {
  if (soil > SOIL_WARNING || rain < RAIN_WARNING || tilt > TILT_WARNING) return 3;
  if (soil > SOIL_ALERT || rain < RAIN_ALERT || tilt > TILT_ALERT) return 2;
  if (soil > SOIL_CAUTION || rain < RAIN_CAUTION || tilt > TILT_CAUTION) return 1;
  return 0;
}

void displayReadings(int soil, int rain, float tilt, int alert, int soilRaw, int rainRaw) {
  Serial.println("┌────────────────────────────────────────┐");
  Serial.println("│    SENSOR READINGS                     │");
  Serial.println("└────────────────────────────────────────┘");
  Serial.print("Node ID:         "); Serial.println(NODE_ID);
  Serial.print("Soil Moisture:   "); Serial.print(soil); Serial.print(" % (Raw: "); Serial.print(soilRaw); Serial.println(")");
  Serial.print("Rain Value:      "); Serial.print(rain); Serial.print(" (Raw: "); Serial.print(rainRaw); Serial.print(")");
  Serial.println(rain < RAIN_CAUTION ? " (RAIN DETECTED)" : " (No rain)");
  Serial.print("Tilt Angle:      "); Serial.print(tilt, 2); Serial.println(" °");
  Serial.println("─────────────────────────────────────────");
  Serial.print("ALERT LEVEL:     ");
  switch (alert) {
    case 0: Serial.println("0 - Normal (Low Risk) ✓");           break;
    case 1: Serial.println("1 - Caution (Moderate Risk) ⚠");     break;
    case 2: Serial.println("2 - Warning (High Risk) ⚠⚠");       break;
    case 3: Serial.println("3 - Danger (Very High Risk) ⚠⚠⚠"); break;
  }
  Serial.println("═════════════════════════════════════════");
}
