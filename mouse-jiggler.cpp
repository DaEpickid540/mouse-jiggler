// Zigzag — NOW ACTUALLY EXTREME
void doZigzag() {
  Serial.println("[PAT] Zigzag");
  const int legs    = random(4, 9);
  const int legSize = 120;  // ← WAS 35, NOW 120 (3.5x bigger)
  int32_t accumX = 0, accumY = 0;

  for (int i = 0; i < legs; i++) {
    int dir = (i % 2 == 0) ? 1 : -1;
    for (int s = 0; s < 6; s++) {
      int8_t sx = (int8_t)(dir * legSize / 6);  // ~20px per step = 120px total
      int8_t sy = (int8_t)(dir * legSize / 6);
      sendMove(sx, sy);
      accumX += sx; accumY += sy;
      delay(18);
    }
  }
  returnToCenter(accumX, accumY);
}

// Chaos — ACTUALLY CHAOTIC
void doChaos() {
  Serial.println("[PAT] Chaos");
  const int bursts  = random(6, 13);
  int32_t accumX = 0, accumY = 0;

  for (int i = 0; i < bursts; i++) {
    int8_t dx = (int8_t)random(-127, 128);  // ← WAS -90/91, NOW -127/127 (MAX)
    int8_t dy = (int8_t)random(-127, 128);
    if (abs(dx) < 20 && abs(dy) < 20) dx = 100;
    sendMove(dx, dy);
    accumX += dx; accumY += dy;
    delay(random(25, 90));
  }
  delay(150);
  returnToCenter(accumX, accumY);
}

// Spiral — AGGRESSIVE EXPANSION
void doSpiral() {
  Serial.println("[PAT] Spiral");
  const int steps = 36;
  int32_t accumX = 0, accumY = 0;
  float prevX = 0, prevY = 0;

  for (int i = 0; i < steps; i++) {
    float t      = (2.0 * PI * i) / steps;
    float radius = 6.0 * i;  // ← WAS 3.0, NOW 6.0 (grows to ~216px)
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

// Circle — BIGGER
void doCircle() {
  Serial.println("[PAT] Circle");
  const float radius = 150.0;  // ← WAS 100, NOW 150 (50% bigger)
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

// Figure-8 — WILD
void doFigure8() {
  Serial.println("[PAT] Figure-8");
  const float A = 180.0;  // ← WAS 100, NOW 180
  const float B = 90.0;   // ← WAS 50, NOW 90
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
