#include <WiFi.h>

// ===== CHANGE THESE (keep simple) =====
const char* ssid = "ESP32_TEST";     // Your hotspot name
const char* password = "12345678";   // Your hotspot password

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Set WiFi mode
  WiFi.mode(WIFI_STA);

  Serial.println("Scanning WiFi...");

  // Scan available networks
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found!");
  } else {
    Serial.println("Networks found:");
    for (int i = 0; i < n; i++) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(WiFi.SSID(i));
    }
  }

  Serial.println("----------------------");
  Serial.println("Connecting to WiFi...");

  // Start connection
  WiFi.begin(ssid, password);

  int retry = 0;

  // Try for 20 seconds
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
    retry++;

    if (retry > 20) {
      Serial.println("❌ Failed to connect!");
      return;
    }
  }

  // If connected
  Serial.println("✅ WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Nothing here
}