// ================= UART2 RECEIVER + OLED + WIFI + SPREADSHEET LOGGING =================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ================= WIFI CONFIG =================
const char* ssid = "";
const char* password = "";
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
#define STATUS_INTERVAL 30000       // 30 seconds heartbeat
#define DISPLAY_INTERVAL 1000       // 1 second display update
#define LOG_TIMEOUT 5000            // 5 second timeout for HTTP

// ================= VARIABLES =================
String currentLine = "";
String lastMessage = "";
unsigned long lastMessageTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastSuccessfulContact = 0;
uint32_t totalMessages = 0;
String pendingLogData = "";
unsigned long logAttemptTime = 0;
bool isLoggingData = false;
bool httpInProgress = false;

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  delay(500);
  
  // UART2
  Serial2.begin(BAUD, SERIAL_8N1, UART2_RX, UART2_TX);
  
  // OLED
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    while (1);
  }
  
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
  lastHeartbeatTime = millis();
  lastSuccessfulContact = millis();
  lastDisplayUpdate = millis();
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
        pendingLogData = currentLine;
        logAttemptTime = millis();
        isLoggingData = true;
        Serial.println("[RX] Message queued for logging: " + currentLine);
      }
      currentLine = "";
    } else if (c != '\r') {
      currentLine += c;
    }
  }
  
  // ================= LOG DATA (ASYNC, NON-BLOCKING) =================
  if (isLoggingData && !httpInProgress && WiFi.status() == WL_CONNECTED && pendingLogData.length() > 0) {
    if (millis() - logAttemptTime > 100) {
      Serial.println("[LOG] Starting HTTP request...");
      logDataToSheet(pendingLogData);
      pendingLogData = "";
    }
  }
  
  // ================= CHECK FOR HTTP TIMEOUT =================
  if (httpInProgress && millis() - logAttemptTime > LOG_TIMEOUT) {
    Serial.println("[LOG] HTTP timeout, clearing flags");
    httpInProgress = false;
    isLoggingData = false;
  }
  
  // ================= HEARTBEAT (30 SECONDS) =================
  if (millis() - lastHeartbeatTime >= STATUS_INTERVAL) {
    lastHeartbeatTime = millis();
    if (WiFi.status() == WL_CONNECTED) {
      sendHeartbeat();
    }
  }
  
  // ================= UPDATE DISPLAY (1 SECOND) =================
  if (millis() - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}

// ================= LOG DATA TO SPREADSHEET =================
void logDataToSheet(String dataLine) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LOG] WiFi not connected, abandoning");
    isLoggingData = false;
    return;
  }
  
  httpInProgress = true;
  
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();
  client.setTimeout(5000);
  
  String url = String(scriptURL) + "?type=log&data=" + urlEncode(dataLine);
  
  Serial.println("[LOG] URL: " + url);
  
  if (!http.begin(client, url)) {
    Serial.println("[LOG] http.begin() failed");
    httpInProgress = false;
    isLoggingData = false;
    client.stop();
    return;
  }
  
  http.setConnectTimeout(2000);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    Serial.println("[LOG] HTTP 200 OK - data logged");
    lastSuccessfulContact = millis();
  } else if (httpCode == 302 || httpCode == 301) {
    Serial.println("[LOG] HTTP " + String(httpCode) + " redirect - check deployment URL");
  } else if (httpCode < 0) {
    Serial.println("[LOG] HTTP error: " + String(http.errorToString(httpCode)));
  } else {
    Serial.println("[LOG] HTTP " + String(httpCode));
  }
  
  http.end();
  client.stop();
  
  // ALWAYS clear flags after HTTP completes
  httpInProgress = false;
  isLoggingData = false;
}

// ================= SEND HEARTBEAT =================
void sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();
  client.setTimeout(5000);
  
  String url = String(scriptURL) + "?type=status";
  
  if (!http.begin(client, url)) {
    Serial.println("[HEARTBEAT] http.begin() failed");
    client.stop();
    return;
  }
  
  http.setConnectTimeout(2000);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    Serial.println("[HEARTBEAT] HTTP 200 OK");
    lastSuccessfulContact = millis();
  } else if (httpCode < 0) {
    Serial.println("[HEARTBEAT] Error: " + String(http.errorToString(httpCode)));
  } else {
    Serial.println("[HEARTBEAT] HTTP " + String(httpCode));
  }
  
  http.end();
  client.stop();
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
  unsigned long secondsSinceLastMsg = (millis() - lastMessageTime) / 1000;
  unsigned long secondsSinceContact = (millis() - lastSuccessfulContact) / 1000;
  
  display.clearDisplay();
  display.setTextSize(1);
  
  // ===== HEADER =====
  display.setCursor(0, 0);
  display.print("Msgs: ");
  display.print(totalMessages);
  display.print(" | WiFi: ");
  display.println(WiFi.status() == WL_CONNECTED ? "OK" : "NO");
  
  // ===== LAST MESSAGE RX =====
  display.setCursor(0, 10);
  display.print("Last RX: ");
  display.print(secondsSinceLastMsg);
  display.println("s");
  
  // ===== LAST SUCCESSFUL CONTACT =====
  display.setCursor(0, 20);
  display.print("Last OK: ");
  display.print(secondsSinceContact);
  display.println("s");
  
  // ===== HTTP STATUS =====
  display.setCursor(0, 30);
  display.print("HTTP: ");
  if (httpInProgress) {
    display.println("IN PROGRESS");
  } else if (isLoggingData) {
    display.println("PENDING");
  } else {
    display.println("IDLE");
  }
  
  // ===== MESSAGE CONTENT =====
  display.setCursor(0, 44);
  display.println("Last Msg:");
  display.setCursor(0, 54);
  String displayMsg = lastMessage;
  if (displayMsg.length() > 20) {
    displayMsg = displayMsg.substring(0, 20);
  }
  display.println(displayMsg);
  
  display.display();
}
