#define RX_PIN 16
#define BAUD 9600

String currentDelay = "";
int good = 0;
int bad = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(BAUD, SERIAL_8N1, RX_PIN, -1);
}

void loop() {

  static String line = "";

  while (Serial2.available()) {

    char c = Serial2.read();

    if (c == '\n') {

      // Check if line contains HELLO
      if (line.indexOf("HELLO") >= 0) {
        good++;
      } else {
        bad++;
      }

      // Detect delay marker
      if (line.startsWith("D")) {

        if (currentDelay.length() > 0) {
          Serial.print("Delay ");
          Serial.print(currentDelay);
          Serial.print(" us  GOOD=");
          Serial.print(good);
          Serial.print("  BAD=");
          Serial.print(bad);
          Serial.print("  SCORE=");
          Serial.println(good - bad);
        }

        // Reset counters
        good = 0;
        bad = 0;

        currentDelay = line.substring(1, 4);
      }

      line = "";
    }
    else {
      line += c;
    }
  }
}
