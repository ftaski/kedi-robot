// mBot (mCore, ATmega328P) cat toy.
// WANDER: moves like prey (dart, creep, spin, freeze, curve).
// Something within 40 cm: robot stops and samples distance. Changing distance = moving thing (the cat)
// -> PLAY: teases (wiggle, lunge, freeze), escapes when the cat gets within escapeCm, searches when it loses it.
// Steady distance = wall/furniture -> turns away. Ultrasonic only measures distance; it cannot tell a cat
// from a moving person or another pet.
// Onboard button toggles play; auto-stops after playMin minutes.
// No library: mCore pins taken from Makeblock's MeMCore port map.
//
// Serial protocol, 115200, one command per line (same over USB and the Bluetooth module):
//   app -> robot:  S            start play
//                  X            stop
//                  M,<l>,<r>    manual drive, -255..255; must repeat <1 s apart, else robot returns to auto
//                  C,<maxPwm>,<escapeCm>,<playMin>   settings (not saved across power-off)
//   robot -> app:  D,<cm>,<mode>,<maxPwm>,<escapeCm>,<playMin>   every 300 ms; mode: I idle, W wander, P play, M manual

const int M1_DIR = 7, M1_PWM = 6;   // left motor (mounted mirrored)
const int M2_DIR = 4, M2_PWM = 5;   // right motor
const int SONAR = A1;               // ultrasonic on port 4 (single-wire; port 3 would be A3)
const int BUZZER = 8;
const int BUTTON = A7;              // analog-only pin, reads ~0 when pressed

const int NOTICE_CM = 40, TEASE_CM = 60;
const unsigned long LOST_MS = 4000, MANUAL_TIMEOUT_MS = 1000;

// Left motor power in %. Lower it (e.g. 80) if the robot pulls toward the left side.
const int LEFT_TRIM = 100;

// Weak AA cells brown out the board on sudden motor current, so speed is capped and ramped.
const int RAMP_STEP = 30, RAMP_MS = 8;
int maxPwm = 200, escapeCm = 15, playMin = 10;

enum Mode { WANDER, PLAY };
Mode mode = WANDER;
bool playing = false, manual = false;
unsigned long playStart, actionEnd, lastSeen, lastManual;
int curL = 0, curR = 0;

void setMotors(int l, int r) {
  l = -(long)l * LEFT_TRIM / 100;  // left motor is mirrored
  digitalWrite(M1_DIR, l >= 0 ? HIGH : LOW); analogWrite(M1_PWM, min(abs(l), 255));
  digitalWrite(M2_DIR, r >= 0 ? HIGH : LOW); analogWrite(M2_PWM, min(abs(r), 255));
}

void drive(int l, int r) {
  l = constrain(l, -maxPwm, maxPwm);
  r = constrain(r, -maxPwm, maxPwm);
  while (curL != l || curR != r) {
    curL += constrain(l - curL, -RAMP_STEP, RAMP_STEP);
    curR += constrain(r - curR, -RAMP_STEP, RAMP_STEP);
    setMotors(curL, curR);
    delay(RAMP_MS);
  }
}

int distanceCm() {
  pinMode(SONAR, OUTPUT);
  digitalWrite(SONAR, LOW); delayMicroseconds(2);
  digitalWrite(SONAR, HIGH); delayMicroseconds(10);
  digitalWrite(SONAR, LOW);
  pinMode(SONAR, INPUT);
  long t = pulseIn(SONAR, HIGH, 25000);
  return t == 0 ? 400 : t / 58;  // timeout = nothing in front
}

bool buttonPressed() {
  if (analogRead(BUTTON) > 100) return false;
  delay(30);
  bool down = analogRead(BUTTON) < 100;
  while (analogRead(BUTTON) < 100) {}  // wait for release
  return down;
}

void chirp(int f1, int f2) { tone(BUZZER, f1, 60); delay(80); tone(BUZZER, f2, 60); delay(80); }

// Robot holds still and watches: a cat moves, a wall doesn't.
bool somethingMoving() {
  drive(0, 0); delay(100);
  int lo = 400, hi = 0;
  for (int i = 0; i < 6; i++) {
    int d = distanceCm();
    lo = min(lo, d); hi = max(hi, d);
    delay(80);
  }
  return hi - lo > 4;
}

void pickAction() {
  unsigned long dur;
  int left, right;
  switch (random(5)) {
    case 0: left = right = random(180, 256); dur = random(200, 600); break;        // dart
    case 1: left = right = random(120, 150); dur = random(600, 1500); break;       // creep
    case 2: { int d = random(2) ? 1 : -1; left = 200 * d; right = -200 * d;        // spin
              dur = random(150, 500); break; }
    case 3: left = right = 0; dur = random(500, 2500); break;                      // freeze
    default: left = random(120, 230); right = random(120, 230); dur = random(400, 1200); // curve
  }
  actionEnd = millis() + dur;
  drive(left, right);
}

void escape() {
  drive(-230, -230); delay(350);
  int d = random(2) ? 1 : -1;
  drive(220 * d, -220 * d); delay(random(250, 500));
  drive(0, 0);
}

void tease() {
  switch (random(3)) {
    case 0: for (int i = 0; i < 3; i++) { drive(180, -180); delay(110); drive(-180, 180); delay(110); } break; // wiggle
    case 1: drive(200, 200); delay(150); drive(-200, -200); delay(150); break;                           // lunge
    default: drive(0, 0); delay(random(300, 900));                                                       // freeze
  }
  drive(0, 0);
}

void startPlaying() { playing = true; mode = WANDER; chirp(1500, 2200); playStart = millis(); pickAction(); }
void stopPlaying() { playing = false; manual = false; drive(0, 0); chirp(2200, 1500); }

// Reads one complete line from serial without blocking; returns true when cmd holds a line.
char cmd[32];
byte cmdLen = 0;
bool readCommand() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdLen == 0) continue;
      cmd[cmdLen] = 0; cmdLen = 0;
      return true;
    }
    if (cmdLen < sizeof(cmd) - 1) cmd[cmdLen++] = c;
  }
  return false;
}

void handleCommand() {
  int a, b, c;
  if (cmd[0] == 'S') { if (!playing) startPlaying(); }
  else if (cmd[0] == 'X') { if (playing || manual) stopPlaying(); }
  else if (cmd[0] == 'M' && sscanf(cmd + 1, ",%d,%d", &a, &b) == 2) {
    manual = true; lastManual = millis();
    drive(a, b);
  }
  else if (cmd[0] == 'C' && sscanf(cmd + 1, ",%d,%d,%d", &a, &b, &c) == 3) {
    maxPwm = constrain(a, 100, 255);
    escapeCm = constrain(b, 5, 35);   // must stay below NOTICE_CM
    playMin = constrain(c, 1, 60);
  }
}

void report(int d) {
  static unsigned long lastReport;
  if (millis() - lastReport < 300) return;
  lastReport = millis();
  char m = manual ? 'M' : !playing ? 'I' : mode == PLAY ? 'P' : 'W';
  Serial.print("D,"); Serial.print(d); Serial.print(','); Serial.print(m); Serial.print(',');
  Serial.print(maxPwm); Serial.print(','); Serial.print(escapeCm); Serial.print(','); Serial.println(playMin);
}

void setup() {
  pinMode(M1_DIR, OUTPUT); pinMode(M2_DIR, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  Serial.begin(115200);
  randomSeed(analogRead(A0) ^ micros());
  drive(0, 0);
}

void loop() {
  while (readCommand()) handleCommand();
  if (buttonPressed()) { if (playing) stopPlaying(); else startPlaying(); }

  int d = distanceCm();
  report(d);

  if (manual) {
    if (millis() - lastManual < MANUAL_TIMEOUT_MS) { delay(10); return; }
    manual = false;                       // app released the stick or connection dropped
    drive(0, 0);
    if (playing) { mode = WANDER; pickAction(); }
  }

  if (!playing) { delay(30); return; }
  if (millis() - playStart > (unsigned long)playMin * 60000UL) { stopPlaying(); return; }

  if (mode == WANDER) {
    if (d < NOTICE_CM) {
      if (somethingMoving()) { mode = PLAY; lastSeen = millis(); }
      else { escape(); pickAction(); }  // wall: turn away
      return;
    }
    if ((long)(millis() - actionEnd) >= 0) pickAction();
  } else {
    if (d < TEASE_CM) lastSeen = millis();
    if (millis() - lastSeen > LOST_MS) { mode = WANDER; pickAction(); return; }
    if (d < escapeCm) { escape(); delay(random(500, 1500)); }  // run, then freeze to invite a chase
    else if (d < TEASE_CM) tease();
    else { drive(130, -130); delay(150); drive(0, 0); }         // lost it: look around
  }
  delay(30);
}
