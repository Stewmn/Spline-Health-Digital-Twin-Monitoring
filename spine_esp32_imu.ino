// ============================================================
//  SpineGuard IoT — ESP32 + IMU (MPU6050 / MPU9250 / GY-521)
//  Sends:  BEND:<pitch>,<roll>  over Serial at 115200 baud
//  Bridge: bridge.py reads this and forwards to WebSocket
//  Dashboard: spine_dashboard_v2.html reads via WebSocket
// ============================================================

#include <Wire.h>
#include <MPU6050_light.h>   // Install: "MPU6050_light" by rfetick in Library Manager

MPU6050 mpu(Wire);

// ── Config ──────────────────────────────────────────────────
#define SERIAL_BAUD     115200      // Must match bridge.py BAUD_RATE
#define SEND_INTERVAL   50          // ms between readings (~20 Hz)
#define CALIBRATE_COUNT 200         // samples for auto-calibration at boot
#define SDA_PIN         21          // Default ESP32 SDA (change if needed)
#define SCL_PIN         22          // Default ESP32 SCL (change if needed)

// ── Thresholds (mirrors dashboard logic) ────────────────────
#define THRESH_WARN     20.0f
#define THRESH_HIGH     35.0f
#define THRESH_DANGER   45.0f

// ── LED feedback pins (optional, comment out if unused) ─────
#define LED_OK          2           // Built-in LED on most ESP32 boards
// #define LED_WARN     4
// #define LED_DANGER   5

unsigned long lastSend = 0;

// ── I2C Scanner helper (runs once at boot) ──────────────────
void i2cScan() {
  Serial.println(F("[I2C] Scanning..."));
  int found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("[I2C] Device found at 0x"));
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println(F("[I2C] No devices found! Check wiring."));
  else Serial.println(F("[I2C] Scan done."));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  Serial.println(F("============================================"));
  Serial.println(F("  SpineGuard IoT — ESP32 IMU Bridge"));
  Serial.println(F("============================================"));

  // ── Start I2C ───────────────────────────────────────────
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);            // 400 kHz fast mode

  // ── I2C scan (helps confirm sensor is wired correctly) ──
  i2cScan();

  // ── Init MPU6050 ────────────────────────────────────────
  byte status = mpu.begin();
  if (status != 0) {
    Serial.print(F("[ERROR] MPU6050 init failed. Code: "));
    Serial.println(status);
    Serial.println(F("  Check: SDA->GPIO21, SCL->GPIO22, VCC->3.3V, AD0->GND"));
    while (true) {
      digitalWrite(LED_OK, !digitalRead(LED_OK));   // blink forever
      delay(200);
    }
  }
  Serial.println(F("[OK] MPU6050 found!"));

  // ── Auto-calibration ────────────────────────────────────
  Serial.println(F("[CAL] Keep sensor FLAT and STILL..."));
  delay(1000);
  mpu.calcOffsets(true, true);      // calibrate gyro + accel
  Serial.println(F("[CAL] Calibration complete!"));

  pinMode(LED_OK, OUTPUT);
  digitalWrite(LED_OK, HIGH);

  Serial.println(F("[READY] Streaming BEND data..."));
  Serial.println(F("--------------------------------------------"));
}

void loop() {
  mpu.update();                     // read latest IMU data

  unsigned long now = millis();
  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;

    float pitch = mpu.getAngleX();  // forward/backward bend
    float roll  = mpu.getAngleY();  // left/right bend

    // ── Send in format expected by bridge.py + dashboard ──
    // Format: BEND:<pitch>,<roll>
    Serial.print(F("BEND:"));
    Serial.print(pitch, 1);
    Serial.print(F(","));
    Serial.println(roll, 1);

    // ── Optional LED feedback based on severity ──────────
    float total = sqrt(pitch * pitch + roll * roll);
    if (total > THRESH_DANGER) {
      // Rapid blink = DANGER
      digitalWrite(LED_OK, (now / 100) % 2);
    } else if (total > THRESH_WARN) {
      // Slow blink = WARNING
      digitalWrite(LED_OK, (now / 400) % 2);
    } else {
      // Solid ON = OK
      digitalWrite(LED_OK, HIGH);
    }
  }
}

// ============================================================
//  WIRING GUIDE
//  ─────────────────────────────────────────────────────────
//  MPU6050 (GY-521)     ESP32
//  ─────────────────    ─────────────
//  VCC              →   3.3V   (NOT 5V!)
//  GND              →   GND
//  SDA              →   GPIO 21
//  SCL              →   GPIO 22
//  AD0              →   GND    (sets I2C address to 0x68)
//  INT              →   (not needed for polling mode)
//
//  REQUIRED LIBRARY
//  ─────────────────────────────────────────────────────────
//  Arduino IDE → Tools → Manage Libraries
//  Search: "MPU6050_light"  by rfetick  → Install
//
//  HOW TO RUN THE FULL SYSTEM
//  ─────────────────────────────────────────────────────────
//  1. Flash this code to your ESP32
//  2. Open Serial Monitor (115200) to verify BEND: lines appear
//  3. Close Serial Monitor (bridge.py needs the COM port!)
//  4. Run:  python bridge.py
//  5. Open spine_dashboard_v2.html in browser
//  6. Enter IP: 127.0.0.1  Port: 8765  → Click Connect
//
//  TROUBLESHOOTING
//  ─────────────────────────────────────────────────────────
//  • "MPU6050 init failed" → Check wiring, use I2C scanner output
//  • COM port busy         → Close Arduino Serial Monitor first
//  • No data in dashboard  → Confirm bridge.py shows "Broadcasting: BEND:..."
//  • Wrong COM port        → Change SERIAL_PORT in bridge.py (e.g. COM3, COM5)
// ============================================================
