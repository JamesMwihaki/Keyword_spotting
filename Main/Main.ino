#include "MotorControl.h"
#include "mic.h"

void setup() {
    Serial.begin(921600);
    setupMotors();
    setupVoice();
    Serial.println("System Ready!");
}

void loop() {
    checkVoice();     // Listen for commands
    updateMotors();    // Handle motor timers
}