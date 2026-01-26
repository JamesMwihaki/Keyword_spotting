#ifndef VOICE_CONTROL_H
#define VOICE_CONTROL_H

#include <Arduino.h>

//Thresh for the confidence scores to move or not move the motors
#define CONFIDENCE_THRESHOLD 0.90f

void setupVoice();
void checkVoice();
void start_motor_sequence(int index, const char* cmd);

#endif