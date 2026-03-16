#include "MotorControl.h"
#include "NetworkConfig.h"
#include "mic.h"
#include "rom/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable Brownout Detector
  Serial.begin(115200);
  setupMotors();
  setupVoice();
  setupWiFi();
  Serial.println("System Ready!");
}

void loop() {
  checkVoice();   // Listen for commands
  updateMotors(); // Handle motor timers
  handleServer();
  delay(1);
}