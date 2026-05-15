#include <WiFi.h>
#include <WebServer.h>

const char* ssid     = "iphone";
const char* password = "raksh@24";

#define LED_PIN    2
#define BUTTON_PIN 4

WebServer server(80);
bool ledState = false;
bool lastBtnState = HIGH;
int pressCount = 0;
String lastReceived = "Nothing yet";
String lastSent = "Nothing yet";

String getHTML() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32 Control</title>"; //page lab title
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='3'>";
  html += "<style>";
  html += "body{font-family:Arial;background:#0a0e1a;color:#c8d8f0;padding:20px;text-align:center;}";
  html += "h1{color:#00d4ff;}";
  html += ".box{background:#0f1628;border:1px solid #1a2a4a;border-radius:8px;padding:20px;margin:15px auto;max-width:500px;}";
  html += "input[type=text]{width:80%;padding:10px;background:#070c18;color:#00d4ff;border:1px solid #1a2a4a;border-radius:4px;font-size:16px;}";
  html += "input[type=submit]{padding:10px 20px;background:#00d4ff;color:#000;border:none;border-radius:4px;font-size:16px;cursor:pointer;margin-top:10px;}";
  html += ".led-on{color:#ffee44;font-size:24px;font-weight:bold;}";
  html += ".led-off{color:#555;font-size:24px;}";
  html += ".data{color:#00ff88;font-size:18px;padding:10px;}";
  html += ".label{color:#4a6080;font-size:13px;margin-bottom:5px;}";
  html += "</style></head><body>";
  html += "<h1>ESP32 Control Panel</h1>"; // main heading
  html += "<div class='box'>";
  html += "<div class='label'>INPUT SECTION - Send data to ESP32</div>"; // input section send data to esp32
  html += "<form action='/send' method='GET'>";
  html += "<input type='text' name='msg' placeholder='Type your message here...' required><br>";
  html += "<input type='submit' value='SEND DATA'>";
  html += "</form></div>";
  html += "<div class='box'>";
  html += "<div class='label'>OUTPUT SECTION - Data from ESP32</div>"; //o/p sec data from esp32
  if (ledState) {
    html += "<div class='led-on'>LED is ON</div>"; //led on 
  } else {
    html += "<div class='led-off'>LED is OFF</div>"; // led off
  }
  html += "<div class='label' style='margin-top:15px'>Last message received by ESP32:</div>";
  html += "<div class='data'>" + lastReceived + "</div>";
  html += "<div class='label' style='margin-top:10px'>Last data sent from button:</div>";
  html += "<div class='data'>" + lastSent + "</div>";
  html += "<div class='label'>Button press count: " + String(pressCount) + "</div>";
  html += "</div>";
  html += "<div style='color:#4a6080;font-size:12px'>Auto-refreshes every 3 seconds</div>";
  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleSend() {
  if (server.hasArg("msg")) {
    String msg = server.arg("msg");
    lastReceived = msg;
    ledState = true;
    digitalWrite(LED_PIN, HIGH);
    Serial.print("Received: ");
    Serial.println(msg);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("==============================");
  Serial.println("WiFi Connected!");
  Serial.print("Open this in Chrome: http://");
  Serial.println(WiFi.localIP());
  Serial.println("==============================");

  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(200);
    digitalWrite(LED_PIN, LOW);  delay(200);
  }

  server.on("/", handleRoot);
  server.on("/send", handleSend);
  server.begin();
  Serial.println("Server started!");
}

void loop() {
  server.handleClient();

  bool currentBtnState = digitalRead(BUTTON_PIN);
  if (lastBtnState == HIGH && currentBtnState == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      pressCount++;
      lastSent = "Button pressed! Count: " + String(pressCount);
      ledState = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println(lastSent);
    }
  }
  lastBtnState = currentBtnState;
  delay(10);
}