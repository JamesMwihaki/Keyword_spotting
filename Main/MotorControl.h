#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

enum ScreenState { AT_TOP, AT_BOTTOM, TRANSIT };

struct MotorPins {
  int step; // -1 = not wired yet
  int dir;
  int en;
};

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
extern MotorPins motorPins[];

enum MotorDirection { DIR_STOP = 0, DIR_UP = 1, DIR_DOWN = 2 };

// Function Prototypes
void setupMotors();
void moveMotor(int id, int direction);
void updateMotors();
void allStop();

#endif