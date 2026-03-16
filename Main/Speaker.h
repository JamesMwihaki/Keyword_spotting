#ifndef SPEAKER_H
#define SPEAKER_H

#include "driver/i2s.h"
#include <Arduino.h>

#define SPEAKER_I2S_NUM I2S_NUM_1
#define SPEAKER_BCLK 22
#define SPEAKER_LRC 23
#define SPEAKER_DIN 5

class Speaker {
public:
  void begin();
  void playChunk(const uint8_t *data, size_t size);
  void stop();
  void test();
};

#endif
