#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "Adafruit_seesaw.h"
#include "esp_sleep.h"

#define uS_TO_S_FACTOR 1000000ULL
#define PERIOD_SEC 60

const char* ssid     = ""; // Wi-Fi network name
const char* password = ""; // Wi-Fi password
const char* serverUrl = ""; // Formatted like this: http://0.0.0.0:8000/update

Adafruit_seesaw ss;

const int PWM_CHANNEL = 0;
const int PWM_FREQ = 50;
const int PWM_RESOLUTION = 16;

// Comment out the pin not currently in use
const int SERVO_PIN = 25;
//const int ledPin = 25;

float tempC = 0.0;
uint16_t moisture = 0;

bool connectWiFiWithTimeout(uint32_t timeoutMs = 8000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool postReading(uint16_t moisture, float tempC) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.setTimeout(3000); // ms
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  String body = "{";
  body += "\"moisture\":" + String(moisture) + ",";
  body += "\"tempC\":" + String(tempC, 2);
  body += "}";

  int code = http.POST(body);
  http.end();

  return (code > 0 && code < 400);
}

void wifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void setServoPulse(uint16_t pulseWidthUs) {
  
  float duty = (float)pulseWidthUs / 20000.0;

  uint32_t maxDuty = (1 << PWM_RESOLUTION) - 1;
  uint32_t dutyValue = duty * maxDuty;

  ledcWrite(SERVO_PIN, dutyValue);
}

void setup() {
  uint32_t cycleStartMs = millis();

  Serial.begin(115200);
  delay(200);

  ledcAttach(SERVO_PIN, PWM_FREQ, PWM_RESOLUTION); // Remove for LED setup
  // pinMode(ledPin, OUTPUT); // For LED setup

  delay(2000);
  setServoPulse(1500); // Remove for LED setup
  delay(2000);

  Wire.begin(21, 22);

  if (!ss.begin(0x36)) {
    Serial.println("ERROR! seesaw not found");
    // Sleep a bit and try again later
    esp_sleep_enable_timer_wakeup(10ULL * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }

  moisture = ss.touchRead(0);
  tempC = ss.getTemp();

  // Dummy control output
  // digitalWrite(ledPin, (moisture < 700) ? HIGH : LOW); // For LED setup
  if (moisture < 700) {
    setServoPulse(900);
  } else {
    setServoPulse(2100);
  }
  Serial.printf("moisture=%u tempC=%.2f\n", moisture, tempC);

  bool wifiOk = connectWiFiWithTimeout(8000);
  Serial.println(wifiOk ? "WiFi OK" : "WiFi TIMEOUT");

  bool posted = false;
  if (wifiOk) {
    posted = postReading(moisture, tempC);
    Serial.println(posted ? "POST ok" : "POST failed");
    delay(200);
  }

  wifiOff();

  // Sleep for remainder of the 60-second period
  uint32_t elapsedMs = millis() - cycleStartMs;
  uint32_t sleepUs = (uint64_t)PERIOD_SEC * uS_TO_S_FACTOR;

  if (elapsedMs < PERIOD_SEC * 1000UL) {
    sleepUs = (uint64_t)(PERIOD_SEC * 1000UL - elapsedMs) * 1000ULL;
  } else {
    sleepUs = 1ULL * uS_TO_S_FACTOR;
  }

  Serial.printf("Elapsed=%lu ms, sleeping %llu us\n",
                (unsigned long)elapsedMs, (unsigned long long)sleepUs);

  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_start();
}

void loop() {}