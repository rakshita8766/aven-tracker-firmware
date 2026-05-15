#define BUTTON_PIN 4 // button is connected to gpio 4

volatile bool pressed = false; //

// Interrupt function
void IRAM_ATTR isr() {
  pressed = true;
}

void setup() {
  Serial.begin(115200); // starts serial communication
  delay(1000); // Delay gives time for serial monitor to open

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), isr, FALLING);
  //command sets an interrupt on pin 4 so that when the button is pressed (signal goes from HIGH to LOW),the fun isr is executed immediately
  Serial.println("System Ready");
}

void loop() {
  if (pressed) {
    Serial.println("Welcome to AI TECH");
    pressed = false;
  }
}