#ifndef VOICE_CONTROL_H
#define VOICE_CONTROL_H

#include <Arduino.h>

// 0.80f is 80% confidence. 1.00f is usually too strict.
#define CONFIDENCE_THRESHOLD 0.99f

void setupVoice();
void checkVoice();
void start_motor_sequence(int index, const char* cmd);

#endif