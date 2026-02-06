#include "MotorControl.h"
#include "mic.h"
#include "NetworkConfig.h"

void setup() {
    Serial.begin(921600);
    setupMotors();
    setupVoice();
    setupWiFi();
    Serial.println("System Ready!");
}

void loop() {    
    checkVoice();     // Listen for commands
    updateMotors();    // Handle motor timers
    handleServer();

}