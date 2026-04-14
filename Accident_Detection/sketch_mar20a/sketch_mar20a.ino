#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>   // ✅ ADDED
#include <Wire.h>
#include <MPU9250_asukiaaa.h>
#include <TinyGPSPlus.h>

/* ================= WIFI ================= */
const char* AP_SSID = "ESP32_CAR";
const char* AP_PASS = "12345678";

const char* WIFI_SSID = "iPhone";
const char* WIFI_PASS = "1234567890";

const char* SERVER_URL = "http://172.20.1.20:3000/accident";
const char* PLATE_NUMBER = "RAB123C";

/* ================= IFTTT ================= */
const char* IFTTT_EVENT = "car_accident";   // ✅ ADDED
const char* IFTTT_KEY   = "cmIO5l8nH_xoxCJoGW8Ihg";  // ✅ ADDED

WebServer server(80);

/* ================= MPU ================= */
MPU9250_asukiaaa mpu;

/* ================= GPS ================= */
#define GPS_RX 18
#define GPS_TX 19
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

/* ================= L298N ================= */
#define IN1 25
#define IN2 26
#define IN3 27
#define IN4 14
#define ENA 33
#define ENB 32

/* ================= PWM ================= */
#define PWM_FREQ 5000
#define PWM_RES 8
#define PWM_CH_A 0
#define PWM_CH_B 1

/* ================= THRESHOLDS ================= */
#define ACC_THRESHOLD 5.5
#define TILT_THRESHOLD 100.0

/* ================= VARIABLES ================= */
float ax, ay, az, roll, pitch;
bool accidentDetected = false;
bool dataSent = false;
int speedValue = 150;
String currentAction = "STOP";

/* ================= MOTOR CONTROL ================= */
void stopCar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  ledcWrite(PWM_CH_A, 0);
  ledcWrite(PWM_CH_B, 0);
  currentAction = "STOP";
}

void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  ledcWrite(PWM_CH_A, speedValue);
  ledcWrite(PWM_CH_B, speedValue);
  currentAction = "FORWARD";
}

void backward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  ledcWrite(PWM_CH_A, speedValue);
  ledcWrite(PWM_CH_B, speedValue);
  currentAction = "BACKWARD";
}

void left() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  currentAction = "LEFT";
}

void right() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  currentAction = "RIGHT";
}

/* ================= SEND TO NODE ================= */
void sendToNode(double lat, double lng) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  String json =
    "{"
    "\"plate_number\":\"" + String(PLATE_NUMBER) + "\","
    "\"status\":\"ACCIDENT\","
    "\"latitude\":" + String(lat, 6) + ","
    "\"longitude\":" + String(lng, 6) + ","
    "\"ax\":" + String(ax, 2) + ","
    "\"ay\":" + String(ay, 2) + ","
    "\"az\":" + String(az, 2) + ","
    "\"roll\":" + String(roll, 2) + ","
    "\"pitch\":" + String(pitch, 2) +
    "}";

  Serial.println("\n📡 SENDING TO NODE.JS");
  Serial.println(json);

  http.POST(json);
  http.end();
}

/* ================= IFTTT FUNCTION ================= */
void sendToIFTTT() {   // ✅ ADDED SIMPLE VERSION

  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url = "https://maker.ifttt.com/trigger/" +
               String(IFTTT_EVENT) +
               "/with/key/" +
               String(IFTTT_KEY);

  String message = "🚨 ACCIDENT DETECTED\nCheck dashboard for tracking.";

  String payload = "value1=" + message;

  Serial.println("📤 Sending to IFTTT...");
  Serial.println(url);

  http.begin(client, url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int response = http.POST(payload);

  Serial.print("📡 IFTTT Response: ");
  Serial.println(response);

  http.end();
}

/* ================= WEB PAGE ================= */
void setupWeb() {
  server.on("/", []() {
    server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 CAR</title>
<style>
body {
  font-family: Arial;
  background: #0f172a;
  color: white;
  text-align: center;
}
h2 { margin-top: 20px; }
.container { max-width: 320px; margin: auto; }
.btn {
  width: 100%;
  padding: 16px;
  margin: 8px 0;
  font-size: 18px;
  border-radius: 12px;
  border: none;
  color: white;
}
.forward { background: #22c55e; }
.backward { background: #14b8a6; }
.left { background: #3b82f6; }
.right { background: #6366f1; }
.stop { background: #ef4444; }
</style>
</head>
<body>
<h2>🚗 ESP32 CAR CONTROL</h2>
<div class="container">
<button class="btn left" onclick="fetch('/left')"> ⬆ Forward</button>
<button class="btn forward" onclick="fetch('/forward')"> ⬅ Left</button>
<button class="btn right" onclick="fetch('/right')">➡ Backward</button>
<button class="btn backward" onclick="fetch('/backward')">⬇ Right</button>


<button class="btn stop" onclick="fetch('/stop')">⛔ STOP</button>
</div>
</body>
</html>
)rawliteral");
  });
 
  server.on("/forward", [](){ if(!accidentDetected) forward(); server.send(200,"text/plain","OK"); });
  server.on("/backward", [](){ if(!accidentDetected) backward(); server.send(200,"text/plain","OK"); });
  server.on("/left", [](){ if(!accidentDetected) left(); server.send(200,"text/plain","OK"); });
  server.on("/right", [](){ if(!accidentDetected) right(); server.send(200,"text/plain","OK"); });
  server.on("/stop", [](){ stopCar(); server.send(200,"text/plain","OK"); });

  server.begin();
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  ledcAttach(ENA, PWM_FREQ, PWM_RES);
  ledcAttach(ENB, PWM_FREQ, PWM_RES);

  ledcWrite(ENA, speedValue);
  ledcWrite(ENB, speedValue);

  Wire.begin(21, 22);
  mpu.setWire(&Wire);
  mpu.beginAccel();
  mpu.beginGyro();

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  setupWeb();

  Serial.println("🚀 SYSTEM READY");
}

/* ================= LOOP ================= */
void loop() {
  server.handleClient();

  mpu.accelUpdate();
  ax = mpu.accelX();
  ay = mpu.accelY();
  az = mpu.accelZ();

  roll  = atan2(ay, az) * 180 / PI;
  pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180 / PI;

  if (!accidentDetected &&
     (abs(ax) > ACC_THRESHOLD ||
      abs(ay) > ACC_THRESHOLD ||
      abs(az) > ACC_THRESHOLD ||
      abs(roll) > TILT_THRESHOLD ||
      abs(pitch) > TILT_THRESHOLD)) {

    accidentDetected = true;
    stopCar();

    Serial.println("\n🚨🚨🚨 ACCIDENT DETECTED 🚨🚨🚨");

    sendToIFTTT();   // ✅ ONLY ADDED LINE
  }

  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  if (accidentDetected && gps.location.isValid() && !dataSent) {
    sendToNode(gps.location.lat(), gps.location.lng());
    dataSent = true;
  }

  delay(1000);
}