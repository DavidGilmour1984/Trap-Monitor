// ================= CONFIG =================
#define TRIGGER_PIN 15
#define RADIO_M0    4
#define RADIO_M1    5
#define VP_PIN      36

#define RXD2 16
#define TXD2 17

#define TRAP_ID "Trap 001"

// ================= RTC MEMORY =================
RTC_DATA_ATTR uint32_t minuteCounter = 0;
RTC_DATA_ATTR uint32_t lastTriggerSend = 0;  // last minute we sent trigger

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  delay(100);

  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(RADIO_M0, OUTPUT);
  pinMode(RADIO_M1, OUTPUT);

  // Increment "time" (1 count per minute wake)
  minuteCounter++;

  bool triggered = (digitalRead(TRIGGER_PIN) == HIGH);  // <-- YOUR LOGIC: HIGH = triggered

  // ---- WAKE RADIO ----
  radioWake();
  delay(100);

  // =====================================================
  // ================= TRIGGER SEND (MAX 1/MIN) ===========
  // =====================================================
  if (triggered && (minuteCounter - lastTriggerSend >= 1)) {

    Serial.println("DEBUG: Sending Trigger");

    Serial2.print(TRAP_ID);
    Serial2.println(" Triggered");

    delay(200);

    lastTriggerSend = minuteCounter;
  }

  // =====================================================
  // ================= VOLTAGE EVERY 5 MIN ===============
  // =====================================================
  if (minuteCounter % 5 == 0) {

    int raw = analogRead(VP_PIN);
    float voltage = raw * (3.3 / 4095.0) * 2.0;

    Serial.print("Voltage: ");
    Serial.println(voltage, 2);

    Serial2.print("Voltage:");
    Serial2.println(voltage, 2);

    delay(200);
  }

  // ---- SLEEP RADIO ----
  radioSleep();

  // =====================================================
  // ================= SLEEP ==============================
  // =====================================================
  esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL); // 1 min wake
  esp_deep_sleep_start();
}

// ================= LOOP =================
void loop() {}


// ================= RADIO CONTROL =================
void radioWake() {
  digitalWrite(RADIO_M0, LOW);
  digitalWrite(RADIO_M1, LOW);
}

void radioSleep() {
  digitalWrite(RADIO_M0, HIGH);
  digitalWrite(RADIO_M1, HIGH);
}
