#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <WiFi.h>
#include <WebServer.h>
#include "MotorControl.h" // So we can call moveMotor()


// 1. Create the server object on port 80
extern WebServer server;

void setupWiFi();
void handleServer();

#endif