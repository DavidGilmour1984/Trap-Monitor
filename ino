// ================= CONFIG =================
#define RADIO_POWER_PIN 4

#define UART2_TX 17
#define UART2_RX 16
#define BAUD 9600

#define TRAP_PIN 23        // external interrupt (LOW = triggered)
#define VBAT_PIN 36        // VP pin (ADC)

#define RADIO_BOOT_DELAY_MS 2000
#define POST_TX_DELAY_MS 1000
#define TX_RETRIES 3

#define HEARTBEAT_INTERVAL_MIN 10
#define TRAP_COOLDOWN_MIN 5

// ================= PERSISTENT STORAGE =================
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint64_t lastTrapTime = 0;   // microseconds since boot (RTC time)

// ================= HELPERS =================

// read battery voltage (simple scaled ADC)
float readVoltage() {
  int raw = analogRead(VBAT_PIN);

  // adjust scaling if using divider (example assumes 2:1 divider)
  float voltage = (raw / 4095.0) * 3.3 * 2.0;

  return voltage;
}

void powerRadio(bool state) {
  digitalWrite(RADIO_POWER_PIN, state ? HIGH : LOW);
}

// send message reliably
void sendMessage(String msg) {

  powerRadio(true);
  delay(RADIO_BOOT_DELAY_MS);

  for (int i = 0; i < TX_RETRIES; i++) {
    Serial2.println(msg);
    delay(200);
  }

  delay(POST_TX_DELAY_MS);
  powerRadio(false);
}

// ================= SETUP =================
void setup() {

  Serial.begin(9600);
  Serial2.begin(BAUD, SERIAL_8N1, UART2_RX, UART2_TX);

  pinMode(RADIO_POWER_PIN, OUTPUT);
  pinMode(TRAP_PIN, INPUT_PULLUP);

  bootCount++;

  esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();

  Serial.println("\n====================");
  Serial.print("Boot #: "); Serial.println(bootCount);
  Serial.print("Wake reason: "); Serial.println(wakeReason);

  uint64_t now = esp_timer_get_time(); // microseconds since boot

  // ================= TRAP TRIGGER =================
  if (wakeReason == ESP_SLEEP_WAKEUP_EXT0) {

    Serial.println("Trap triggered!");

    // enforce cooldown (5 min)
    if ((now - lastTrapTime) > (uint64_t)TRAP_COOLDOWN_MIN * 60 * 1000000ULL) {

      lastTrapTime = now;

      float v = readVoltage();

      String msg = "TRAP,COUNT:";
      msg += String(bootCount);
      msg += ",V:";
      msg += String(v, 2);

      sendMessage(msg);

      Serial.println("Trap message sent");

    } else {
      Serial.println("Trap ignored (cooldown)");
    }
  }

  // ================= HEARTBEAT =================
  else if (wakeReason == ESP_SLEEP_WAKEUP_TIMER || wakeReason == ESP_SLEEP_WAKEUP_UNDEFINED) {

    Serial.println("Heartbeat wake");

    float v = readVoltage();

    String msg = "ALIVE,COUNT:";
    msg += String(bootCount);
    msg += ",V:";
    msg += String(v, 2);

    sendMessage(msg);

    Serial.println("Heartbeat sent");
  }

  // ================= GO BACK TO SLEEP =================
  Serial.println("Going to sleep...");

  // Wake on trap (LOW)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)TRAP_PIN, 0);

  // Wake on timer (10 min)
  esp_sleep_enable_timer_wakeup(
    (uint64_t)HEARTBEAT_INTERVAL_MIN * 60 * 1000000ULL
  );

  delay(200);
  esp_deep_sleep_start();
}

// ================= LOOP =================
void loop() {}
