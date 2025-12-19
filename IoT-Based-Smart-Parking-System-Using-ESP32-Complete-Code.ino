//IoT Smart Parking System
#include <WiFi.h>
#include <Wire.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>

// Hardware pins
#define IR_ENTRY 18
#define IR_EXIT 19
#define SERVO_PIN 25
#define GREEN_LED 26
#define RED_LED 27

Servo barrierServo;

// WiFi credentials
const char* ssid = "Malik Daud";
const char* password = "Daud;2501492";

// Web server
WebServer server(80);

// Parking variables
int totalSlots = 4;
int availableSlots = 4;
int flagEntry = 0;
int flagExit = 0;
String gateStatus = "Closed";

// Non-blocking gate control
unsigned long gateOpenTime = 0;
const unsigned long gateOpenDuration = 2000;
bool gateMoving = false;

// ---------------- ThingSpeak ----------------
const char* thingSpeakServer = "http://api.thingspeak.com/update";
const char* thingSpeakAPIKey = "96LAD5QQE5IMQ0W6";

int prevAvailable = -1;
int prevOccupied = -1;
String prevGateStatus = "";

unsigned long lastThingSpeakTime = 0;
const unsigned long thingSpeakInterval = 15000; // 15 seconds

// ---------------- LED Control ----------------
void updateLCD() {
  if (availableSlots > 0) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  } else {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  }
}

void showFullMessage() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  delay(2000);
  updateLCD();
}

// ---------------- JSON API ----------------
void handleData() {
  String json = "{";
  json += "\"available\":" + String(availableSlots) + ",";
  json += "\"occupied\":" + String(totalSlots - availableSlots) + ",";
  json += "\"gate\":\"" + gateStatus + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ---------------- HTML ----------------
String getHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>IoT Smart Parking</title>
  <script>
    async function updateData(){
      const r = await fetch('/data');
      const d = await r.json();
      document.getElementById('a').innerText = d.available;
      document.getElementById('o').innerText = d.occupied;
      document.getElementById('g').innerText = d.gate;
    }
    setInterval(updateData,2000);
  </script>
</head>
<body onload="updateData()">
  <h2>Smart Parking System</h2>
  <p>Available: <span id="a">0</span></p>
  <p>Occupied: <span id="o">0</span></p>
  <p>Gate: <span id="g">Closed</span></p>
</body>
</html>
)rawliteral";
}

// ---------------- ThingSpeak Upload ----------------
void uploadToThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) return;

  int occupied = totalSlots - availableSlots;

  if ((availableSlots != prevAvailable ||
       occupied != prevOccupied ||
       gateStatus != prevGateStatus) &&
      millis() - lastThingSpeakTime >= thingSpeakInterval) {

    HTTPClient http;
    String url = String(thingSpeakServer) +
                 "?api_key=" + thingSpeakAPIKey +
                 "&field1=" + String(availableSlots) +
                 "&field2=" + String(occupied) +
                 "&field3=" + (gateStatus == "Open" ? "1" : "0");

    http.begin(url);
    int code = http.GET();
    http.end();

    Serial.print("ThingSpeak response: ");
    Serial.println(code);

    prevAvailable = availableSlots;
    prevOccupied = occupied;
    prevGateStatus = gateStatus;
    lastThingSpeakTime = millis();
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);

  pinMode(IR_ENTRY, INPUT);
  pinMode(IR_EXIT, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  barrierServo.attach(SERVO_PIN);
  barrierServo.write(90);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Serial.println(WiFi.localIP());

  updateLCD();

  server.on("/", []() {
    server.send(200, "text/html", getHTML());
  });

  server.on("/data", handleData);
  server.begin();
}

// ---------------- Loop ----------------
void loop() {
  server.handleClient();

  // Entry
  if (digitalRead(IR_ENTRY) == LOW && flagEntry == 0) {
    flagEntry = 1;
    if (availableSlots > 0) {
      gateStatus = "Open";
      barrierServo.write(0);
      gateOpenTime = millis();
      gateMoving = true;
      availableSlots--;
      updateLCD();
    } else {
      showFullMessage();
    }
  }

  if (digitalRead(IR_ENTRY) == HIGH) flagEntry = 0;

  // Exit
  if (digitalRead(IR_EXIT) == LOW && flagExit == 0) {
    flagExit = 1;
    if (availableSlots < totalSlots) {
      gateStatus = "Open";
      barrierServo.write(0);
      gateOpenTime = millis();
      gateMoving = true;
      availableSlots++;
      updateLCD();
    }
  }

  if (digitalRead(IR_EXIT) == HIGH) flagExit = 0;

  // Close gate
  if (gateMoving && millis() - gateOpenTime >= gateOpenDuration) {
    barrierServo.write(90);
    gateStatus = "Closed";
    gateMoving = false;
    updateLCD();
  }

  // ThingSpeak
  uploadToThingSpeak();
}
