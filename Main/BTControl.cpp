#include "BluetoothSerial.h"
#include "MotorControl.h"

BluetoothSerial SerialBT;

void setupBluetooth() {
  if (!SerialBT.begin("ESP32_Control")) {
    Serial.println("An error occurred initializing Bluetooth");
  } else {
    Serial.println("Bluetooth initialized. Ready to pair.");
  }
}

void checkBluetooth() {
  //  handle the incoming messages
  if (SerialBT.available()) {
    String data = SerialBT.readStringUntil('\n');
    data.trim();

    int commaIndex = data.indexOf(',');
    if (commaIndex != -1) {
      // Parse the ID and the String command
      int id = data.substring(0, commaIndex).toInt();
      String command = data.substring(commaIndex + 1);

      Serial.println("Motor ID:");
      Serial.println(id);
      Serial.println("");
      Serial.println(id);

      // Safety check: ensure ID is within your array range (0-5)
      if (id >= 0 && id <= 5) {
        int dir = DIR_STOP;
        if (command == "UP")
          dir = DIR_UP;
        else if (command == "DOWN")
          dir = DIR_DOWN;

        moveMotor(id, dir);
      }
    }
  }
}