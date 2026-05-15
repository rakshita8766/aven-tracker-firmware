#include <WiFi.h>

// Your hotspot credentials
const char* ssid = "RSB";
const char* password = "raksh@24";

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);   // Station mode

  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  int retry = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
    retry++;

    if (retry > 20) {
      Serial.println("Failed to connect!");
      return;
    }
  }

  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // nothing here
}