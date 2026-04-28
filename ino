// ================= CONFIG =================
#define RADIO_POWER_PIN 4

#define UART2_TX 17
#define UART2_RX 16
#define BAUD 9600

#define ON_TIME_MS 5000
#define RADIO_BOOT_DELAY_MS 2000
#define POST_TX_DELAY_MS 1000
#define TX_RETRIES 3

#define SLEEP_TIME_S 5

// ================= PERSISTENT COUNTER =================
RTC_DATA_ATTR uint32_t counter = 0;

// ================= SETUP =================
void setup() {

  Serial.begin(9600);
  Serial2.begin(BAUD, SERIAL_8N1, UART2_RX, UART2_TX);

  pinMode(RADIO_POWER_PIN, OUTPUT);

  // ===== Boot info =====
  counter++;

  Serial.println("\n====================");
  Serial.println("ESP32 WAKE");
  Serial.print("Counter: ");
  Serial.println(counter);

  // ================= POWER RADIO =================
  Serial.println("Powering radio ON...");
  digitalWrite(RADIO_POWER_PIN, HIGH);

  delay(RADIO_BOOT_DELAY_MS);   // allow radio to initialise

  // ================= TRANSMIT =================
  Serial.println("Transmitting...");

  for (int i = 0; i < TX_RETRIES; i++) {

    Serial.print("TX attempt ");
    Serial.println(i + 1);

    Serial2.print("COUNT:");
    Serial2.print(counter);
    Serial2.print("\r\n");

    delay(200);  // spacing between packets
  }

  delay(POST_TX_DELAY_MS);  // ensure full transmission

  // ================= POWER RADIO OFF =================
  Serial.println("Powering radio OFF...");
  digitalWrite(RADIO_POWER_PIN, LOW);

  // ================= SLEEP =================
  Serial.println("Entering deep sleep...");

  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_TIME_S * 1000000ULL);

  delay(200); // allow serial to flush
  esp_deep_sleep_start();
}

// ================= LOOP =================
void loop() {}
