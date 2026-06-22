/*
 * Mouse Jiggler — Raw BLE HID Mouse
 * ESP32 WROOM / D0WD-V3
 * No external parts — onboard LED only (GPIO 2)
 *
 * LED patterns:
 *   Slow blink (1s)  = advertising
 *   Solid on         = connected + jiggling
 *   Fast blink 200ms = paused/killed
 *
 * Serial commands:
 *   kill   → toggle jiggler on/off
 *   status → print current state
 *   count  → show jiggle counter
 *   reset  → zero the counter
 *
 * No libraries to install — all built into ESP32 core.
 * Board: ESP32 Dev Module
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include <HIDTypes.h>

// ─── Built-in LED ─────────────────────────────────────────
#define LED_PIN  2   // Onboard blue LED (most ESP32 dev boards)

// ─── Jiggle config ────────────────────────────────────────
#define JIGGLE_MIN 1000
#define JIGGLE_MAX 4000
#define JIGGLE_AMT 8

// ─── HID Report Descriptor (standard 3-byte mouse) ────────
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
bool               connected = false;

// ─── State ────────────────────────────────────────────────
bool          killed      = false;
int           jiggleCount = 0;
unsigned long lastJiggle  = 0;
unsigned long lastLED     = 0;
bool          ledState    = false;
String        serialBuf   = "";

// ─── BLE callbacks ────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    connected = true;
    digitalWrite(LED_PIN, HIGH);  // Solid on = connected
    Serial.println("[BLE] Connected! Jiggling...");
  }
  void onDisconnect(BLEServer* s) override {
    connected = false;
    Serial.println("[BLE] Disconnected. Re-advertising...");
    BLEDevice::startAdvertising();
  }
};

// ─── Prototypes ───────────────────────────────────────────
void setupBLE();
void updateLED();
void doJiggle();
void handleSerialCommand(String cmd);
void printStatus();

// ═════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n╔═══════════════════════════════════╗");
  Serial.println("║   Mouse Jiggler — Raw BLE Mouse  ║");
  Serial.println("║   kill | status | count | reset  ║");
  Serial.println("║                                  ║");
  Serial.println("║ LED: slow=advertising            ║");
  Serial.println("║      solid=connected             ║");
  Serial.println("║      fast=paused                 ║");
  Serial.println("╚═══════════════════════════════════╝\n");

  setupBLE();
  randomSeed(analogRead(0));
  printStatus();
}

void loop() {
  unsigned long now = millis();

  // ─── Serial input ───────────────────────────────────────
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

  // ─── LED blink logic ────────────────────────────────────
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

  // ─── Jiggle ─────────────────────────────────────────────
  if (!killed && connected &&
      now - lastJiggle > (unsigned long)random(JIGGLE_MIN, JIGGLE_MAX)) {
    doJiggle();
    lastJiggle = now;
  }
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
  adv->setAppearance(0x03C2);  // HID Mouse
  adv->addServiceUUID(hid->hidService()->getUUID());
  adv->setScanResponse(false);
  adv->start();

  Serial.println("[BLE] Advertising as 'MouseJiggler'...");
}

void doJiggle() {
  int8_t x = random(-JIGGLE_AMT, JIGGLE_AMT + 1);
  int8_t y = random(-JIGGLE_AMT, JIGGLE_AMT + 1);
  if (x == 0 && y == 0) x = 3;

  uint8_t report[3] = { 0x00, (uint8_t)x, (uint8_t)y };
  inputReport->setValue(report, sizeof(report));
  inputReport->notify();
  jiggleCount++;
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
  Serial.println("\n╔═══════════════════════════════════╗");
  Serial.print("║ BLE:   ");
  Serial.print(connected ? "Connected        " : "Advertising...   ");
  Serial.println("║");
  Serial.print("║ State: ");
  Serial.print(killed ? "PAUSED       " : "RUNNING      ");
  Serial.println("║");
  Serial.print("║ Count: ");
  Serial.print(jiggleCount);
  if      (jiggleCount < 10)  Serial.print("        ");
  else if (jiggleCount < 100) Serial.print("       ");
  else                        Serial.print("      ");
  Serial.println("║");
  Serial.println("╚═══════════════════════════════════╝\n");
}