#define RXD2 18
#define TXD2 17

void setup() {
  Serial.begin(115200);     // PC ↔ ESP32
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); // ESP32 ↔ EC200U

  Serial.println("Type AT commands:");
}

void loop() {

  // 🔹 From PC → Modem
  if (Serial.available()) {
    Serial2.write(Serial.read());
  }

  // 🔹 From Modem → PC
  if (Serial2.available()) {
    Serial.write(Serial2.read());
  }
}