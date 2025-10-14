#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32_Hotspot";
const char* password = "12345678"; // must be at least 8 chars

// Create a web server on port 80
WebServer server(80);

// Define what happens when someone visits the root page
void handleRoot() {
  server.send(200, "text/html", "<h1>Hello from ESP32!</h1><p>You’re connected to my Wi-Fi network.</p>");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting Access Point...");
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Define route and start server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Listen for client requests
  server.handleClient();
}
