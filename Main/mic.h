#ifndef VOICE_CONTROL_H
#define VOICE_CONTROL_H

#include <Arduino.h>

#define CONFIDENCE_THRESHOLD 0.95f

void setupVoice();
void checkVoice();
void start_motor_sequence(int index, const char *cmd);


#endif