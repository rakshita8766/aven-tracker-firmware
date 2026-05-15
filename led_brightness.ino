#define LED_PIN 5
#define BTN_INC 12
#define BTN_DEC 13

volatile bool incFlag = false;
volatile bool decFlag = false;

int brightness = 0; //stores LED brightness (0–255)

unsigned long lastIncTime = 0; //Stores last time (in millisec)when increase button was accepted
unsigned long lastDecTime = 0;
const int debounceDelay = 200;//after 1press,wait 200ms before accepting next press”

// Interrupts (ONLY set flags)
void IRAM_ATTR incISR() {
  incFlag = true;
}

void IRAM_ATTR decISR() {
  decFlag = true;
}

void setup() {
  Serial.begin(115200);

  pinMode(BTN_INC, INPUT_PULLUP);
  pinMode(BTN_DEC, INPUT_PULLUP);

  // PWM setup(to control brightness of LED)
  ledcAttach(LED_PIN, 5000, 8);

  // Start with LED OFF
  ledcWrite(LED_PIN, 0);

  Serial.println("System Ready");

  // Attach interrupts
  attachInterrupt(digitalPinToInterrupt(BTN_INC), incISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_DEC), decISR, FALLING);
}

void loop() {

  // Increase brightness
  if (incFlag) { // this flag is set by interrupt
    incFlag = false;

    if (millis() - lastIncTime > debounceDelay) {
      lastIncTime = millis();

      brightness += 25; //Increase brightness step-by-step
      if (brightness > 255) brightness = 255;

      ledcWrite(LED_PIN, brightness);

      Serial.print("Brightness Increased: ");
      Serial.println(brightness);
    }
  }

  // Decrease brightness
  if (decFlag) {
    decFlag = false;

    if (millis() - lastDecTime > debounceDelay) {
      lastDecTime = millis();

      brightness -= 25;
      if (brightness < 0) brightness = 0;

      ledcWrite(LED_PIN, brightness);

      Serial.print("Brightness Decreased: ");
      Serial.println(brightness);
    }
  }
}