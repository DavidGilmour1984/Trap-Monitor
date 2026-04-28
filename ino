// ================= CONFIG =================
#define RADIO_POWER_PIN 4

#define UART2_TX 17
#define UART2_RX 16
#define BAUD 9600

#define TRAP_PIN 23
#define VBAT_PIN 36

#define WAKE_INTERVAL_MIN 3

#define RADIO_BOOT_DELAY_MS 2000
#define POST_TX_DELAY_MS 1000
#define TX_RETRIES 3

// ================= PERSISTENT =================
RTC_DATA_ATTR uint32_t bootCount = 0;

// ================= HELPERS =================

// -------- Read battery voltage --------
float readVoltage() {
  analogSetAttenuation(ADC_11db);
  delay(50);

  int raw = analogRead(VBAT_PIN);

  Serial.print("RAW ADC: ");
  Serial.println(raw);

  float voltage = (raw / 4095.0) * 3.3 * 2.0;  // adjust if divider differs

  return voltage;
}

// -------- Control radio power --------
void powerRadio(bool state) {
  digitalWrite(RADIO_POWER_PIN, state ? HIGH : LOW);
}

// ================= SETUP =================
void setup() {

  Serial.begin(9600);
  Serial2.begin(BAUD, SERIAL_8N1, UART2_RX, UART2_TX);

  pinMode(RADIO_POWER_PIN, OUTPUT);
  pinMode(TRAP_PIN, INPUT_PULLUP);

  bootCount++;

  Serial.println("\n====================");
  Serial.print("Boot #: ");
  Serial.println(bootCount);

  // ================= READ INPUTS =================
  int trapState = digitalRead(TRAP_PIN);
  float voltage = readVoltage();

  Serial.print("Trap state: ");
  Serial.println(trapState == LOW ? "TRIGGERED" : "OK");

  Serial.print("Voltage: ");
  Serial.println(voltage, 2);

  // ================= TRANSMIT =================
  Serial.println("Powering radio ON...");
  powerRadio(true);

  delay(RADIO_BOOT_DELAY_MS);

  for (int i = 0; i < TX_RETRIES; i++) {

    // CLEAN CSV FORMAT
    Serial2.print("COUNT,");
    Serial2.print(bootCount);
    Serial2.print(",");

    Serial2.print(trapState == LOW ? "TRIGGERED" : "OK");
    Serial2.print(",");

    Serial2.println(voltage, 2);

    delay(300);
  }

  delay(POST_TX_DELAY_MS);

  Serial.println("Powering radio OFF...");
  powerRadio(false);

  Serial.println("Message sent");

  // ================= SLEEP =================
  Serial.println("Sleeping...");

  esp_sleep_enable_timer_wakeup(
    (uint64_t)WAKE_INTERVAL_MIN * 60 * 1000000ULL
  );

  delay(200);
  esp_deep_sleep_start();
}

// ================= LOOP =================
void loop() {}
