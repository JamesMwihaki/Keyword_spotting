#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <WiFi.h>
#include <WebServer.h>
#include "MotorControl.h" 


// 1. Create the server object
extern WebServer server;

void setupWiFi();
void handleServer();

#endif