// ================= UART2 RECEIVER + OLED + WIFI + SPREADSHEET LOGGING =================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ================= WIFI CONFIG =================
const char* ssid = "StPeters-PSK";
const char* password = "4OddDevices";
const char* scriptURL = "https://script.google.com/macros/s/AKfycbyU1M2sU_9DnGU3o2FY5jMvl5DK1ZMy7NqgRv9iOBJZJeY12tylTrnuCzskVUp1Y0OLrA/exec";

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================= UART =================
#define UART2_TX 17
#define UART2_RX 16
#define BAUD 9600

// ================= TIMINGS =================
#define DATA_LOG_INTERVAL 1800000  // 30 minutes in ms
#define STATUS_INTERVAL 30000      // 30 seconds in ms

// ================= VARIABLES =================
String currentLine = "";
String lastMessage = "";
unsigned long lastMessageTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastDataLog = 0;
unsigned long lastStatusUpdate = 0;
uint32_t totalMessages = 0;
unsigned long timeSinceLastContact = 0;

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  
  // UART2
  Serial2.begin(BAUD, SERIAL_8N1, UART2_RX, UART2_TX);
  
  // OLED
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();
  
  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    delay(500);
    Serial.print(".");
  }
  
  display.clearDisplay();
  display.setCursor(0, 0);
  if (WiFi.status() == WL_CONNECTED) {
    display.println("WiFi Connected");
    display.setCursor(0, 10);
    display.println(WiFi.localIP());
    Serial.println("\nWiFi Connected");
    Serial.println(WiFi.localIP());
  } else {
    display.println("WiFi Failed");
    Serial.println("\nWiFi Connection Failed");
  }
  display.display();
  delay(2000);
  
  lastMessageTime = millis();
  lastDataLog = millis();
  lastStatusUpdate = millis();
}

// ================= LOOP =================
void loop() {
  // ================= CHECK WIFI CONNECTION =================
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Reconnecting...");
    WiFi.reconnect();
  }
  
  // ================= RECEIVE UART =================
  while (Serial2.available()) {
    char c = Serial2.read();
    Serial.write(c);
    
    if (c == '\n') {
      currentLine.trim();
      if (currentLine.length() > 0) {
        lastMessage = currentLine;
        totalMessages++;
        lastMessageTime = millis();
        updateDisplay();
      }
      currentLine = "";
    } else if (c != '\r') {
      currentLine += c;
    }
  }
  
  // ================= DATA LOG (30 MINUTES) =================
  if (millis() - lastDataLog >= DATA_LOG_INTERVAL) {
    lastDataLog = millis();
    if (WiFi.status() == WL_CONNECTED && lastMessage.length() > 0) {
      logDataToSheet(lastMessage);
    }
  }
  
  // ================= SIGN OF LIFE (30 SECONDS) =================
  if (millis() - lastStatusUpdate >= STATUS_INTERVAL) {
    lastStatusUpdate = millis();
    if (WiFi.status() == WL_CONNECTED) {
      sendHeartbeat();
    }
  }
  
  // ================= UPDATE DISPLAY TIMER (1 SECOND) =================
  if (millis() - lastDisplayUpdate >= 1000) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}

// ================= LOG DATA TO SPREADSHEET =================
void logDataToSheet(String dataLine) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LOG] WiFi not connected");
    return;
  }
  
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();
  
  String url = String(scriptURL) + "?type=log&data=" + urlEncode(dataLine);
  
  Serial.println("[LOG] Sending: " + dataLine);
  
  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      Serial.println("[LOG] Success");
    } else {
      Serial.println("[LOG] HTTP Error: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("[LOG] Connection failed");
  }
}

// ================= SEND HEARTBEAT (SIGN OF LIFE) =================
void sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();
  
  String url = String(scriptURL) + "?type=status";
  
  if (http.begin(client, url)) {
    int httpCode = http.GET();
    Serial.println("[HEARTBEAT] HTTP " + String(httpCode));
    http.end();
  } else {
    Serial.println("[HEARTBEAT] Connection failed");
  }
}

// ================= URL ENCODE =================
String urlEncode(String str) {
  String encoded = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || 
        (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      encoded += '%';
      if (c < 0x10) encoded += '0';
      encoded += String(c, HEX);
    }
  }
  return encoded;
}

// ================= OLED UPDATE =================
void updateDisplay() {
  unsigned long secondsSince = (millis() - lastMessageTime) / 1000;
  unsigned long secondsSinceContact = (millis() - lastStatusUpdate) / 1000;
  
  display.clearDisplay();
  display.setTextSize(1);
  
  // ===== HEADER =====
  display.setCursor(0, 0);
  display.print("Msgs: ");
  display.print(totalMessages);
  display.print(" | WiFi: ");
  display.println(WiFi.status() == WL_CONNECTED ? "OK" : "NO");
  
  display.setCursor(0, 10);
  display.print("Last RX: ");
  display.print(secondsSince);
  display.println("s");
  
  display.setCursor(0, 20);
  display.print("Last Contact: ");
  display.print(secondsSinceContact);
  display.println("s");
  
  // ===== MESSAGE =====
  display.setCursor(0, 34);
  display.println("Last Msg:");
  display.setCursor(0, 46);
  display.println(lastMessage);
  
  display.display();
}
