#include <WiFi.h>
#include <WebServer.h>
#include "esp_wifi.h"
#include "esp_sleep.h"

// CONFIG :
#define DEEP_SLEEP_DURATION_NO_WIFI 10
#define DEEP_SLEEP_DURATION 10

#define CONNECTION_TIMEOUT 5

const char *ssid = ""; // Your WiFi ssid (name)
const char *password = ""; // Your WiFi passowrd

IPAddress local_IP(192, 168, 0, 31); // Board ip address
IPAddress gateway(192, 168, 0, 1); // Gateway
IPAddress subnet(255, 255, 255, 0); // Mask
IPAddress primaryDNS(1, 1, 1, 1); // Primary DNS
IPAddress secondaryDNS(1, 0, 0, 1); // Secondary DNS

WebServer server(80); // HTTP server port, default 80

const int PIN_COUNT = 5; // Amount of output pins

// {0, 1, 2, 3, 18, 19, 20} ESP8684 usable pin numbers, use as many as you need. Other boards might have other usable pins
const int OUTPUT_PINS[PIN_COUNT] = {2, 3, 18, 19, 20}; // Gpio numbers used as output, their amount must be equal to `PIN_COUNT`
bool OUTPUT_STATUS[PIN_COUNT] = {false, false, false, false, false}; // Default output pins status, amount must be equal to `PIN_COUNT`

const int SWITCH_COUNT = 2; // Amount of physical buttons
const int SWITCH_PINS[SWITCH_COUNT] = {6, 7}; // Switch pins - ORDER MATTERS - FIRST SWITCH = FIRST OUTPUT_PIN

const int CONFIRM_THRESHOLD = 3; // Amount of button checks before activation - every check happens every 25ms - In case of error like when the high voltage wires induce switch wires.

// END of config




unsigned long lastActivitySwitch[SWITCH_COUNT] = {0};
int switchConfirmCount[SWITCH_COUNT] = {0};

unsigned long lastWifiCheck = 0;
unsigned long lastSwitchCheck = 0;

String get_json() {
  String json = "{";
    json += "\"status\": {";
    for (int i = 0; i < PIN_COUNT; i++) {
      json += "\"" + String(OUTPUT_PINS[i]) + "\": \"" + String(OUTPUT_STATUS[i]) + "\"";
      if (i != PIN_COUNT -1) {
        json += ",";
      }
    }
    json += "},";
    json += "\"uptime\": " + String(millis()) + "";
    json += "}";
    return json;
}

// ===== handlers =====
void handleRoot() {
  
    String json = get_json();
    server.send(200, "application/json", json);
    Serial.println("ask");
}

void handleLamp() {

  if (!server.hasArg("state") || !server.hasArg("number")) {
    server.send(400, "text/plain", "Bad request.");
    return;
  }

  String state = server.arg("state");
  int pin_number = server.arg("number").toInt();



  Serial.println("changing" + String(OUTPUT_PINS[pin_number]));

  if (state == "on") {
    digitalWrite(OUTPUT_PINS[pin_number], HIGH);
    OUTPUT_STATUS[pin_number] = true;
    String json = get_json();
    server.send(200, "application/json", json);
  } 
  else if (state == "off") {
    digitalWrite(OUTPUT_PINS[pin_number], LOW);
    OUTPUT_STATUS[pin_number] = false;
    String json = get_json();
    server.send(200, "application/json", json);
  } 
  else {
    server.send(400, "text/plain", "Nieznany stan");
  }
}


void setup()
{
  
  for (int pin_number: OUTPUT_PINS) {
    pinMode(pin_number, OUTPUT);
  }
  for (int pin_number: SWITCH_PINS) {
    pinMode(pin_number, INPUT_PULLUP);
  }
  Serial.begin(115200);
  delay(50);

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Błąd konfiguracji IP");
  }

  WiFi.mode(WIFI_STA); // Optional
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");

  // Wait for connection to establish before checking status
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < CONNECTION_TIMEOUT * 1000) {
    delay(100);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(WiFi.localIP());
  }

  delay(100);
  server.enableCORS(true);

  server.on("/", handleRoot);
  server.on("/lamp", handleLamp);

  server.begin();
}

void switch_check() {
  for (int i = 0; i < SWITCH_COUNT; i++) {
    if (digitalRead(SWITCH_PINS[i]) == LOW) {
      switchConfirmCount[i]++;
      if (switchConfirmCount[i] == CONFIRM_THRESHOLD && (millis() - lastActivitySwitch[i] > 500)) {
        lastActivitySwitch[i] = millis();
        if (OUTPUT_STATUS[i]) {
          digitalWrite(OUTPUT_PINS[i], LOW);
        } else {
          digitalWrite(OUTPUT_PINS[i], HIGH);
        }
        OUTPUT_STATUS[i] = !OUTPUT_STATUS[i];
        Serial.println("Changed by button");
      }
    } else {
      switchConfirmCount[i] = 0;
    }
  }
}

void loop()
{
  
  if (millis() - lastWifiCheck >= 5000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost, reconnecting...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
            
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nReconnected!");
        Serial.println(WiFi.localIP());
      }
    }
  }
 
  if (millis() - lastSwitchCheck >= 25) {
    lastSwitchCheck = millis();
    switch_check();
  }
  server.handleClient();

  delay(30);
}