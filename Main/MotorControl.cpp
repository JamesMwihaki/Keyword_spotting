#include "MotorControl.h"


bool isEmergencyStopActive = false;

// Pin Definitions
int dir1[6] = {27, 19, 0, 0, 0, 0}; 
int dir2[6] = {26, 21, 0, 0, 0, 0}; 
int enPins[6] = {14, 18, 0, 0, 0, 0};
const int numMotors = 6;

//speed controls
int dutyCycle = 200;
const int freq = 5000;
const int resolution = 8;

MotorLogic motors[6] = {
    //set the initial states for each motor, 
    //start time is 0, isRunning is false and duration is 1 second
    {0, 0, false, 1000}, {1, 0, false, 1000},
    {2, 0, false, 1000}, {3, 0, false, 1000},
    {4, 0, false, 1000}, {5, 0, false, 1000}
};

void setupMotors() {
    //initialize the pins for each motor
    for(int i = 0; i < numMotors; i++){
        pinMode(dir1[i], OUTPUT);
        pinMode(dir2[i], OUTPUT);
        digitalWrite(dir1[i], LOW);
        digitalWrite(dir2[i], LOW);
        ledcAttach(enPins[i], freq, resolution);
    }
}

void moveMotor(int id, String direction) {
    //moves the motors depending on the motor id up or down depending on the direction recieved 
    //Supposed to not move if the emergenct stop is active and the direction is stop
    if (isEmergencyStopActive && direction != "STOP") return;

    if (direction == "UP") {
        digitalWrite(dir1[id], HIGH);
        digitalWrite(dir2[id], LOW);
        ledcWrite(enPins[id], dutyCycle);
    } else if (direction == "DOWN") {
        digitalWrite(dir1[id], LOW);
        digitalWrite(dir2[id], HIGH);
        ledcWrite(enPins[id], dutyCycle);
    } else {
        digitalWrite(dir1[id], LOW);
        digitalWrite(dir2[id], LOW);
        ledcWrite(enPins[id], 0);
    }
}


void updateMotors() {
    //update the status of the motors
    //loops through all the motors checking if the motors has reached the set duration it is supposed to spin. updates the states
    for (int i = 0; i < numMotors; i++) {
        if (motors[i].isRunning) {
            if (millis() - motors[i].startTime >= motors[i].duration || isEmergencyStopActive) {
                moveMotor(motors[i].id, "STOP");
                motors[i].isRunning = false;
            }
        }
    }
}

void allStop() {
    //If the emergency stop is presssed on the web interface 
    for(int i = 0; i < numMotors; i++) moveMotor(i, "STOP");
}