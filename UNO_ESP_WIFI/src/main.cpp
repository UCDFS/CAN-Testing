#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

const char* ssid = "FS_Dashboard";
const char* password = "12345678";

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

String motorStatus = "Unknown";
int rpm = 0;
int torque = 0;

// Conversion: 1 rpm = 0.01777 km/h
float rpmToKmh(float rpmValue) {
  return rpmValue * 0.01777;
}

// HTML with speed display
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>FS Motor Dashboard</title>
  <style>
    body { font-family: Arial; background:#0e1111; color:white; margin:0; padding:20px; }
    h1 { color:#2e5786; text-align:center; }
    .flex { display:flex; justify-content:center; gap:20px; margin-bottom:20px; }
    .box { border:1px solid #2e5786; border-radius:8px; padding:15px; width:150px; text-align:center; }
    #log { background:#111; color:#ccc; border:1px solid #333; padding:10px; height:300px; overflow-y:scroll; font-family:monospace; font-size:13px; }
    #speed { font-size:64px; color:#00ff88; text-align:center; margin-top:30px; }
  </style>
</head>
<body>
  <h1>Formula Student Motor Dashboard</h1>
  <div class="flex">
    <div class="box"><b>Status:</b><br><span id="status">Unknown</span></div>
    <div class="box"><b>RPM:</b><br><span id="rpm">0</span></div>
    <div class="box"><b>Torque:</b><br><span id="torque">0</span></div>
  </div>

  <div><b>Live CAN Frames:</b></div>
  <div id="log"></div>

  <div id="speed">0.0 km/h</div>

<script>
  function updateSpeed(rpm) {
    const kmh = rpm * 0.01777;
    document.getElementById('speed').textContent = kmh.toFixed(1) + " km/h";
  }

  var ws = new WebSocket('ws://' + location.hostname + ':81/');
  ws.onmessage = function(event){
    var data = JSON.parse(event.data);

    if(data.type === "values") {
      document.getElementById('status').textContent = data.status;
      document.getElementById('rpm').textContent = data.rpm;
      document.getElementById('torque').textContent = data.torque;
      updateSpeed(data.rpm);
    } else if(data.type === "can") {
      var log = document.getElementById('log');
      log.innerHTML += data.frame + "<br>";
      log.scrollTop = log.scrollHeight;
    }
  }
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", webpage);
}

void sendValues() {
  String msg = "{\"type\":\"values\",\"status\":\"" + motorStatus + "\",\"rpm\":" + rpm + ",\"torque\":" + torque + "}";
  webSocket.broadcastTXT(msg);
}

void sendCANFrame(const String& frameLine) {
  String msg = "{\"type\":\"can\",\"frame\":\"" + frameLine + "\"}";
  webSocket.broadcastTXT(msg);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println("\nWiFi AP started");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.begin();
  webSocket.begin();
}

void loop() {
  server.handleClient();
  webSocket.loop();

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("RPM:")) rpm = line.substring(4).toInt();
    else if (line.startsWith("TORQUE:")) torque = line.substring(7).toInt();
    else if (line.startsWith("STATUS:")) {
      motorStatus = line.substring(7);
    }

    else if (line.startsWith("CAN:")) sendCANFrame(line);

    sendValues();
  }
}
