#include "MotorControl.h"

bool isEmergencyStopActive = false;

// Pin definitions — set step/dir/en.
// -1 for any motor not yet connected.
MotorPins motorPins[6] = {
  {18, 14, 27}, // Motor 0: Green Screen  — WIRED
  {-1, -1, -1}, // Motor 1: Blue Screen   — not yet wired
  {-1, -1, -1}, // Motor 2: White Screen  — not yet wired
  {-1, -1, -1}, // Motor 3: Black Screen  — not yet wired
  {-1, -1, -1}, // Motor 4: Grey Screen   — not yet wired
  {-1, -1, -1}, // Motor 5: Rose Screen   — not yet wired
};

const int numMotors = 6;

// Stepper Settings
const int freq = 5000;
const int resolution = 12;
const int stepDuty = 1024;

MotorLogic motors[6] = {
    {0, 0, false, 10000, AT_TOP, 0}, {1, 0, false, 5000, AT_TOP, 0},
    {2, 0, false, 10000, AT_TOP, 0}, {3, 0, false, 5000, AT_TOP, 0},
    {4, 0, false, 5000, AT_TOP, 0},  {5, 0, false, 5000, AT_TOP, 0}};

void setupMotors() {
  for (int i = 0; i < numMotors; i++) {
    if (motorPins[i].step == -1) continue;
    pinMode(motorPins[i].dir, OUTPUT);
    pinMode(motorPins[i].en, OUTPUT);

    // TMC2209 Enable is Active LOW. HIGH = disabled at boot.
    digitalWrite(motorPins[i].en, HIGH);

    // Attach PWM to the STEP pin to create movement pulses
    ledcAttach(motorPins[i].step, freq, resolution);
    ledcWrite(motorPins[i].step, 0);
  }
}

void moveMotor(int id, int direction) {
  if (isEmergencyStopActive && direction != DIR_STOP)
    return;

  // Boundary Checks
  if (direction == DIR_UP && motors[id].currentState == AT_TOP)
    return;
  if (direction == DIR_DOWN && motors[id].currentState == AT_BOTTOM)
    return;

  if (motorPins[id].step == -1) return; // Not yet wired

  if (direction == DIR_UP) {
    digitalWrite(motorPins[id].dir, HIGH);   // Set Direction
    digitalWrite(motorPins[id].en, LOW);     // Enable Driver (Active LOW)
    ledcWrite(motorPins[id].step, stepDuty); // Start Pulsing

    motors[id].isRunning = true;
    motors[id].startTime = millis();
    motors[id].currentState = TRANSIT;

  } else if (direction == DIR_DOWN) {
    digitalWrite(motorPins[id].dir, LOW);    // Opposite Direction
    digitalWrite(motorPins[id].en, LOW);     // Enable Driver
    ledcWrite(motorPins[id].step, stepDuty); // Start Pulsing

    motors[id].isRunning = true;
    motors[id].startTime = millis();
    motors[id].currentState = TRANSIT;

  } else {                                  // STOP
    ledcWrite(motorPins[id].step, 0);       // Stop Pulsing
    digitalWrite(motorPins[id].en, HIGH);   // Disable Driver to save power/heat
    motors[id].isRunning = false;
  }
}

void updateMotors() {
  unsigned long now = millis();
  for (int i = 0; i < numMotors; i++) {
    if (motors[i].isRunning) {
      unsigned long delta = now - motors[i].startTime;
      motors[i].startTime = now;

      bool goingUp = (digitalRead(motorPins[i].dir) == HIGH);

      if (goingUp) {
        motors[i].currentPosition -= delta;
        if (motors[i].currentPosition <= 0) {
          motors[i].currentPosition = 0;
          moveMotor(i, DIR_STOP);
          motors[i].currentState = AT_TOP;
        }
      } else {
        motors[i].currentPosition += delta;
        if (motors[i].currentPosition >= motors[i].duration) {
          motors[i].currentPosition = motors[i].duration;
          moveMotor(i, DIR_STOP);
          motors[i].currentState = AT_BOTTOM;
        }
      }
    }
  }

  if (isEmergencyStopActive)
    allStop();
}

void allStop() {
  for (int i = 0; i < numMotors; i++) {
    if (motorPins[i].step != -1)
      moveMotor(i, DIR_STOP);
  }
}