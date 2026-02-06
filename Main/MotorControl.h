#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

enum ScreenState { AT_TOP, AT_BOTTOM, TRANSIT };

struct MotorLogic {
  int id;
  unsigned long startTime;
  bool isRunning;
  const long duration;
  ScreenState currentState;
  long currentPosition; // 0 to duration
};

// Hardware Constants
extern const int numMotors;
extern bool isEmergencyStopActive;
extern MotorLogic motors[];

// Function Prototypes
void setupMotors();
void moveMotor(int id, String direction);
void updateMotors();
void allStop();

#endif