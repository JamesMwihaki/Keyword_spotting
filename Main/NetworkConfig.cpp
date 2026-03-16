#include "NetworkConfig.h"

extern bool isEmergencyStopActive;

#include "Secrets.h"

// Set your own Network Name and Password
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

WebServer server(80);

// 1. Store the HTML as a string (use R"rawliteral(...)rawliteral" for
// multi-line)
const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Studio Backdrop Control</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background-color: #1a1a1a; color: #fff; padding: 20px; }
        h1 { color: #f4f4f4; margin-bottom: 30px; }
        .grid-container { display: flex; flex-wrap: wrap; justify-content: center; gap: 15px; max-width: 1000px; margin: 0 auto; }
        .section { background: #2d2d2d; padding: 15px; border-radius: 12px; width: 280px; box-shadow: 0 10px 20px rgba(0,0,0,0.3); border-top: 8px solid; }
        
        /* Backdrop Specific Colors */
        .color-green { border-color: #00FF00; }
        .color-blue { border-color: #0000FF; }
        .color-white { border-color: #FFFFFF; }
        .color-black { border-color: #000000; }
        .color-grey { border-color: #808080; }
        .color-rose { border-color: #FF66CC; }

        .btn { display: inline-block; width: 45%; padding: 12px; margin: 5px; font-size: 16px; font-weight: bold; color: white; border-radius: 6px; border: none; cursor: pointer; transition: 0.2s; text-transform: uppercase; }
        .btn-move { background-color: #444; border: 1px solid #666; }
        .btn-move:active { background-color: #666; transform: translateY(2px); }
        
        .controls-footer { margin-top: 40px; padding: 20px; }
        .btn-off { background-color: #d32f2f; width: 80%; max-width: 400px; margin-bottom: 10px; }
        .btn-reset { background-color: #388E3C; width: 80%; max-width: 400px; font-size: 14px; }
    </style>
</head>
<body>

    <h1>Studio Backdrop System</h1>

    <div class="grid-container" id="controls">
        </div>

    <div class="controls-footer">
        <button class="btn btn-off" onclick="stopAll()">⚠️ EMERGENCY STOP ALL</button> <br>
        <button class="btn btn-reset" onclick="resetSystem()">SYSTEM RESET / RE-ENABLE</button>
    </div>

    <script>
        const espIP = window.location.origin; 

        // Professional Photography Backdrop Configuration
        const backdrops = [
            { name: "Green Screen", class: "color-green" },
            { name: "Chroma Blue",  class: "color-blue" },
            { name: "Arctic White", class: "color-white" },
            { name: "Jet Black",    class: "color-black" },
            { name: "Neutral Grey", class: "color-grey" },
            { name: "Rose Petal",   class: "color-rose" }
        ];

        const controlsDiv = document.getElementById('controls');
        
        // Generate UI based on the backdrops array
        backdrops.forEach((backdrop, i) => {
            controlsDiv.innerHTML += `
                <div class="section ${backdrop.class}">
                    <h3>${backdrop.name}</h3>
                    <button class="btn btn-move" 
                        onmousedown="sendCommand(${i}, 'UP')" onmouseup="sendCommand(${i}, 'STOP')"
                        ontouchstart="sendCommand(${i}, 'UP')" ontouchend="sendCommand(${i}, 'STOP')">UP ▲</button>
                    <button class="btn btn-move" 
                        onmousedown="sendCommand(${i}, 'DOWN')" onmouseup="sendCommand(${i}, 'STOP')"
                        ontouchstart="sendCommand(${i}, 'DOWN')" ontouchend="sendCommand(${i}, 'STOP')">DOWN ▼</button>
                </div>`;
        });

        function sendCommand(motorId, direction) {
            let path = `${espIP}/move?id=${motorId}&dir=${direction}`;
            fetch(path)
            .then(response => response.text())
            .then(data => {
                if (data === "LIMIT_TOP") alert("Screen at the top");
                if (data === "LIMIT_BOTTOM") alert("Screen at the bottom");
            })
            .catch(err => console.log("Request failed"));
        }

        function stopAll() {
            fetch(`${espIP}/O`, { mode: 'no-cors' })
            .then(() => alert("ALL MOTORS STOPPED"))
            .catch(err => console.log("Stop failed"));
        }

        function resetSystem() {
            fetch(`${espIP}/reset`, { mode: 'no-cors' })
            .then(() => alert("System Online"))
            .catch(err => console.log("Reset failed"));
        }

        window.oncontextmenu = function(event) {
            event.preventDefault();
            event.stopPropagation();
            return false;
        };
    </script>
</body>
</html>
)rawliteral";

void setupWiFi() {
  Serial.println("Connecting to WiFi...");

  // 1. Set to Station Mode and start connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // 2. Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // 3. Print the IP address assigned by your router
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // --- Routes remain the same ---
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });

  server.on("/O", HTTP_GET, []() {
    isEmergencyStopActive = true;
    // allStop(); // Ensure this function is defined in your main sketch
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "HALTED");
  });

  server.on("/move", HTTP_GET, []() {
    if (server.hasArg("id") && server.hasArg("dir")) {
      int id = server.arg("id").toInt();
      String dir = server.arg("dir");
      Serial.printf("HTTP Command: Motor %d -> %s\n", id, dir.c_str());

      if (dir == "UP" && motors[id].currentState == AT_TOP) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/plain", "LIMIT_TOP");
        return;
      }
      if (dir == "DOWN" && motors[id].currentState == AT_BOTTOM) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/plain", "LIMIT_BOTTOM");
        return;
      }

      int direction = DIR_STOP;
      if (dir == "UP")
        direction = DIR_UP;
      else if (dir == "DOWN")
        direction = DIR_DOWN;

      moveMotor(id, direction);
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/plain", "OK");
    }
  });

  server.on("/reset", HTTP_GET, []() {
    isEmergencyStopActive = false;
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "SYSTEM_READY");
  });

  server.begin();
}

void handleServer() { server.handleClient(); }