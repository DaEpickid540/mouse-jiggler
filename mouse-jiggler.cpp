/*
 * Mouse Jiggler — Chaotic BLE HID Mouse
 * ESP32 WROOM / D0WD-V3 — No external parts
 *
 * Fires a random pattern every ~30 seconds:
 *   Circle   — 200px diameter, returns to center
 *   Figure-8 — Lissajous curve, returns to center
 *   Zigzag   — Sharp diagonal bursts, returns to center
 *   Chaos    — Random fling, snaps back to center
 *   Spiral   — Expands outward, snaps back to center
 *
 * LED (GPIO 2):
 *   Slow blink  = advertising
 *   Solid on    = connected + running
 *   Fast blink  = paused/killed
 *
 * Serial commands: kill | status | count | reset
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include <HIDTypes.h>
#include <math.h>

// ─── Config ───────────────────────────────────────────────
#define LED_PIN   2
#define INTERVAL  30000   // 30s between jiggles
#define VARIANCE   5000   // ±5s randomness so it feels natural

// ─── HID Report Descriptor ────────────────────────────────
static const uint8_t mouseReportDesc[] = {
  USAGE_PAGE(1),       0x01,
  USAGE(1),            0x02,
  COLLECTION(1),       0x01,
    USAGE(1),          0x01,
    COLLECTION(1),     0x00,
      USAGE_PAGE(1),    0x09,
      USAGE_MINIMUM(1), 0x01,
      USAGE_MAXIMUM(1), 0x03,
      LOGICAL_MINIMUM(1), 0x00,
      LOGICAL_MAXIMUM(1), 0x01,
      REPORT_COUNT(1),  0x03,
      REPORT_SIZE(1),   0x01,
      HIDINPUT(1),      0x02,
      REPORT_COUNT(1),  0x01,
      REPORT_SIZE(1),   0x05,
      HIDINPUT(1),      0x03,
      USAGE_PAGE(1),    0x01,
      USAGE(1),         0x30,
      USAGE(1),         0x31,
      LOGICAL_MINIMUM(1), 0x81,
      LOGICAL_MAXIMUM(1), 0x7F,
      REPORT_SIZE(1),   0x08,
      REPORT_COUNT(1),  0x02,
      HIDINPUT(1),      0x06,
    END_COLLECTION(0),
  END_COLLECTION(0)
};

// ─── BLE objects ──────────────────────────────────────────
BLEHIDDevice*      hid;
BLECharacteristic* inputReport;
bool               connected  = false;

// ─── State ────────────────────────────────────────────────
bool          killed      = false;
int           jiggleCount = 0;
unsigned long nextJiggle  = 30000;
unsigned long lastLED     = 0;
bool          ledState    = false;
String        serialBuf   = "";

// ─── BLE callbacks ────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    connected = true;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[BLE] Connected! Jiggling every ~30s...");
  }
  void onDisconnect(BLEServer* s) override {
    connected = false;
    Serial.println("[BLE] Disconnected. Re-advertising...");
    BLEDevice::startAdvertising();
  }
};

// ─── Prototypes ───────────────────────────────────────────
void setupBLE();
void sendMove(int8_t x, int8_t y);
void returnToCenter(int32_t dx, int32_t dy);
void doCircle();
void doFigure8();
void doZigzag();
void doChaos();
void doSpiral();
void doPattern();
void handleSerialCommand(String cmd);
void printStatus();

// ═════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║  Chaotic Mouse Jiggler — BLE Mouse  ║");
  Serial.println("║  kill | status | count | reset      ║");
  Serial.println("║  Fires every ~30s — Teams-proof     ║");
  Serial.println("╚══════════════════════════════════════╝\n");

  setupBLE();
  randomSeed(analogRead(0));
  printStatus();
}

void loop() {
  unsigned long now = millis();

  // ─── Serial commands ──────────────────────────────────
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuf.length() > 0) {
        serialBuf.toLowerCase();
        handleSerialCommand(serialBuf);
        serialBuf = "";
      }
    } else if (c >= 32 && c < 127) {
      serialBuf += c;
      Serial.write(c);
    }
  }

  // ─── LED blink logic ──────────────────────────────────
  if (!connected) {
    // Slow blink = advertising
    if (now - lastLED > 1000) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      lastLED = now;
    }
  } else if (killed) {
    // Fast blink = paused
    if (now - lastLED > 200) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      lastLED = now;
    }
  } else {
    // Solid on = connected + running
    digitalWrite(LED_PIN, HIGH);
  }

  // ─── Pattern trigger ──────────────────────────────────
  if (!killed && connected && now >= nextJiggle) {
    doPattern();
  }
}

// ═════════════════════════════════════════════════════════
// Core mouse send — 8ms gap between HID reports
// ═════════════════════════════════════════════════════════

void sendMove(int8_t x, int8_t y) {
  if (!connected) return;
  uint8_t report[3] = { 0x00, (uint8_t)x, (uint8_t)y };
  inputReport->setValue(report, sizeof(report));
  inputReport->notify();
  delay(8);
}

// Sends mouse back to where it started — handles >127px
// by breaking into multiple packets of 100px max
void returnToCenter(int32_t dx, int32_t dy) {
  dx = -dx;
  dy = -dy;
  while (abs(dx) > 0 || abs(dy) > 0) {
    int8_t sx = (int8_t)constrain(dx, -100, 100);
    int8_t sy = (int8_t)constrain(dy, -100, 100);
    sendMove(sx, sy);
    dx -= sx;
    dy -= sy;
    delay(10);
  }
}

// ═════════════════════════════════════════════════════════
// Patterns — all return to center after moving
// ═════════════════════════════════════════════════════════

// Full circle, ~200px diameter
void doCircle() {
  Serial.println("[PAT] Circle");
  const float radius = 100.0;
  const int   steps  = 40;
  int32_t accumX = 0, accumY = 0;
  float prevX = radius, prevY = 0;

  for (int i = 1; i <= steps; i++) {
    float angle = (2.0 * PI * i) / steps;
    float currX = radius * cos(angle);
    float currY = radius * sin(angle);
    int8_t dx = (int8_t)constrain((int)(currX - prevX), -127, 127);
    int8_t dy = (int8_t)constrain((int)(currY - prevY), -127, 127);
    accumX += dx; accumY += dy;
    sendMove(dx, dy);
    delay(20);
    prevX = currX; prevY = currY;
  }
  returnToCenter(accumX, accumY);
}

// Lissajous figure-8, ~200px wide
void doFigure8() {
  Serial.println("[PAT] Figure-8");
  const float A = 100.0;
  const float B = 50.0;
  const int   steps = 48;
  int32_t accumX = 0, accumY = 0;
  float prevX = 0, prevY = 0;

  for (int i = 1; i <= steps; i++) {
    float t     = (2.0 * PI * i) / steps;
    float currX = A * sin(t);
    float currY = B * sin(2 * t);
    int8_t dx = (int8_t)constrain((int)(currX - prevX), -127, 127);
    int8_t dy = (int8_t)constrain((int)(currY - prevY), -127, 127);
    accumX += dx; accumY += dy;
    sendMove(dx, dy);
    delay(18);
    prevX = currX; prevY = currY;
  }
  returnToCenter(accumX, accumY);
}

// Sharp diagonal zigzags
void doZigzag() {
  Serial.println("[PAT] Zigzag");
  const int legs    = random(4, 9);
  const int legSize = 35;
  int32_t accumX = 0, accumY = 0;

  for (int i = 0; i < legs; i++) {
    int dir = (i % 2 == 0) ? 1 : -1;
    for (int s = 0; s < 6; s++) {
      int8_t sx = (int8_t)(dir * legSize / 6);
      int8_t sy = (int8_t)(dir * legSize / 6);
      sendMove(sx, sy);
      accumX += sx; accumY += sy;
      delay(18);
    }
  }
  returnToCenter(accumX, accumY);
}

// Random fling bursts — most chaotic
void doChaos() {
  Serial.println("[PAT] Chaos");
  const int bursts  = random(6, 13);
  int32_t accumX = 0, accumY = 0;

  for (int i = 0; i < bursts; i++) {
    int8_t dx = (int8_t)random(-90, 91);
    int8_t dy = (int8_t)random(-90, 91);
    if (abs(dx) < 20 && abs(dy) < 20) dx = 60; // avoid tiny moves
    sendMove(dx, dy);
    accumX += dx; accumY += dy;
    delay(random(25, 90)); // irregular timing = chaotic
  }
  delay(150);
  returnToCenter(accumX, accumY);
}

// Expanding spiral then snaps back
void doSpiral() {
  Serial.println("[PAT] Spiral");
  const int steps = 36;
  int32_t accumX = 0, accumY = 0;
  float prevX = 0, prevY = 0;

  for (int i = 0; i < steps; i++) {
    float t      = (2.0 * PI * i) / steps;
    float radius = 3.0 * i;         // Grows 0 → ~108px
    float currX  = radius * cos(t);
    float currY  = radius * sin(t);
    int8_t dx = (int8_t)constrain((int)(currX - prevX), -127, 127);
    int8_t dy = (int8_t)constrain((int)(currY - prevY), -127, 127);
    accumX += dx; accumY += dy;
    sendMove(dx, dy);
    delay(20);
    prevX = currX; prevY = currY;
  }
  returnToCenter(accumX, accumY);
}

// Picks a random pattern, schedules next one
void doPattern() {
  switch (random(5)) {
    case 0: doCircle();  break;
    case 1: doFigure8(); break;
    case 2: doZigzag();  break;
    case 3: doChaos();   break;
    case 4: doSpiral();  break;
  }
  jiggleCount++;
  nextJiggle = millis() + INTERVAL + random(-(long)VARIANCE, (long)VARIANCE);
  Serial.print("[JIGGLE] Done. Total patterns: ");
  Serial.print(jiggleCount);
  Serial.print("  Next in ~");
  Serial.print((nextJiggle - millis()) / 1000);
  Serial.println("s");
}

// ═════════════════════════════════════════════════════════

void setupBLE() {
  BLEDevice::init("MouseJiggler");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  hid = new BLEHIDDevice(server);
  inputReport = hid->inputReport(1);

  hid->manufacturer()->setValue("ESP32");
  hid->pnp(0x02, 0x045e, 0x0040, 0x0300);
  hid->hidInfo(0x00, 0x02);

  BLESecurity* security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_BOND);

  hid->reportMap((uint8_t*)mouseReportDesc, sizeof(mouseReportDesc));
  hid->startServices();

  BLEAdvertising* adv = server->getAdvertising();
  adv->setAppearance(0x03C2);
  adv->addServiceUUID(hid->hidService()->getUUID());
  adv->setScanResponse(false);
  adv->start();

  Serial.println("[BLE] Advertising as 'MouseJiggler'...");
}

void handleSerialCommand(String cmd) {
  Serial.println();
  if (cmd == "kill") {
    killed = !killed;
    Serial.print("[CMD] Jiggler ");
    Serial.println(killed ? "PAUSED" : "RESUMED");
    printStatus();
  } else if (cmd == "status") {
    printStatus();
  } else if (cmd == "count") {
    Serial.print("[COUNT] "); Serial.println(jiggleCount);
  } else if (cmd == "reset") {
    jiggleCount = 0;
    Serial.println("[CMD] Counter reset");
  } else {
    Serial.println("[ERR] Valid: kill | status | count | reset");
  }
}

void printStatus() {
  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.print("║ BLE:     ");
  Serial.print(connected ? "Connected          " : "Advertising...     ");
  Serial.println("║");
  Serial.print("║ State:   ");
  Serial.print(killed ? "PAUSED         " : "RUNNING        ");
  Serial.println("║");
  Serial.print("║ Patterns: ");
  Serial.print(jiggleCount);
  Serial.println("                    ║");
  if (connected && !killed) {
    Serial.print("║ Next in: ~");
    long secs = ((long)nextJiggle - (long)millis()) / 1000;
    if (secs < 0) secs = 0;
    Serial.print(secs);
    Serial.println("s                   ║");
  }
  Serial.println("╚══════════════════════════════════════╝\n");
}