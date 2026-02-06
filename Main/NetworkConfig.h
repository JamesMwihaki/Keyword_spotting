#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include "MotorControl.h"
#include <WebServer.h>
#include <WiFi.h>

// 1. Create the server object on port 80
extern WebServer server;

void setupWiFi();
void handleServer();

#endif