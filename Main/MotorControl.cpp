#include "MotorControl.h"

bool isEmergencyStopActive = false;

// Pin Definitions
int dir1[6] = {27, 19, 0, 0, 0, 0};
int dir2[6] = {26, 21, 0, 0, 0, 0};
int enPins[6] = {14, 18, 0, 0, 0, 0};
const int numMotors = 6;

int dutyCycle = 200;
const int freq = 5000;
const int resolution = 8;

MotorLogic motors[6] = {
    {0, 0, false, 5000, AT_TOP, 0}, {1, 0, false, 5000, AT_TOP, 0},
    {2, 0, false, 5000, AT_TOP, 0}, {3, 0, false, 5000, AT_TOP, 0},
    {4, 0, false, 5000, AT_TOP, 0}, {5, 0, false, 5000, AT_TOP, 0}};

void setupMotors() {
  for (int i = 0; i < numMotors; i++) {
    pinMode(dir1[i], OUTPUT);
    pinMode(dir2[i], OUTPUT);
    digitalWrite(dir1[i], LOW);
    digitalWrite(dir2[i], LOW);
    ledcAttach(enPins[i], freq, resolution);
  }
}

void moveMotor(int id, String direction) {
  if (isEmergencyStopActive && direction != "STOP")
    return;

  if (direction == "UP" && motors[id].currentState == AT_TOP) {
    Serial.println("Ignored UP: Already AT_TOP");
    return;
  }
  if (direction == "DOWN" && motors[id].currentState == AT_BOTTOM) {
    Serial.println("Ignored DOWN: Already AT_BOTTOM");
    return;
  }

  if (direction == "UP") {
    if (motors[id].currentPosition <= 0) {
      Serial.println("Ignored UP: Position is 0");
      return;
    }
    digitalWrite(dir1[id], HIGH);
    digitalWrite(dir2[id], LOW);
    ledcWrite(enPins[id], dutyCycle);
    motors[id].currentState = TRANSIT;
    motors[id].startTime = millis(); // Used as 'lastUpdate' time
    motors[id].isRunning = true;
    // Assume moving UP takes us towards TOP.
    // We will set AT_TOP when timer expires?
    // Or we just stay in TRANSIT until timer expires, then we "assume" we are
    // at the new state? Let's assume operation completes successfully.
  } else if (direction == "DOWN") {
    if (motors[id].currentPosition >= motors[id].duration) {
      Serial.println("Ignored DOWN: Max duration reached");
      return;
    }
    digitalWrite(dir1[id], LOW);
    digitalWrite(dir2[id], HIGH);
    ledcWrite(enPins[id], dutyCycle);
    motors[id].currentState = TRANSIT;
    motors[id].startTime = millis(); // Used as 'lastUpdate' time
    motors[id].isRunning = true;
  } else {
    digitalWrite(dir1[id], LOW);
    digitalWrite(dir2[id], LOW);
    ledcWrite(enPins[id], 0);
    motors[id].isRunning = false;
    // State update happens in updateMotors or here?
    // If we stop manually, we might be in TRANSIT (undefined position).
    // Let's leave it as TRANSIT if stopped mid-way, or user defined?
    // But the 5s timer is what defines "AT_BOTTOM".
  }
}

void updateMotors() {
  unsigned long now = millis();
  for (int i = 0; i < numMotors; i++) {
    if (motors[i].isRunning) {
      unsigned long delta = now - motors[i].startTime;
      motors[i].startTime = now; // reset for next delta

      // Determine direction from pins
      bool goingUp = digitalRead(dir1[i]) == HIGH;

      if (goingUp) {
        motors[i].currentPosition -= delta;
        if (motors[i].currentPosition <= 0) {
          motors[i].currentPosition = 0;
          moveMotor(motors[i].id, "STOP");
          motors[i].currentState = AT_TOP;
          Serial.println("Auto-Stop: Top Reached");
        }
      } else {
        // Going Down
        motors[i].currentPosition += delta;
        if (motors[i].currentPosition >= motors[i].duration) {
          motors[i].currentPosition = motors[i].duration;
          moveMotor(motors[i].id, "STOP");
          motors[i].currentState = AT_BOTTOM;
          Serial.println("Auto-Stop: Bottom Reached");
        }
      }

      if (isEmergencyStopActive) {
        moveMotor(motors[i].id, "STOP");
      }
    }
  }
}

void allStop() {
  for (int i = 0; i < numMotors; i++)
    moveMotor(i, "STOP");
}