
const int ledPin = 2;   // GPIO 2
const int pwmChannel = 0;
const int freq = 5000;
const int resolution = 8;

void setup() {
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(ledPin, pwmChannel);
}

void loop() {
  ledcWrite(pwmChannel, 25);   // 10%
  delay(2000);

  ledcWrite(pwmChannel, 128);  // 50%
  delay(2000);

  ledcWrite(pwmChannel, 230);  // 90%
  delay(2000);
}