#include <Arduino.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ---------- WiFi ----------
#define WIFI_SSID      "POCO M6 5G"
#define WIFI_PASSWORD  "1234567890"

// ---------- Firebase ----------
#define API_KEY        "AIzaSyCxNbpe2iuvtz1pQUwLeBf-DIn71zhMu84"
#define DATABASE_URL   "https://virawear-aee99-default-rtdb.firebaseio.com"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ---------- Telegram ----------
String BOT_TOKEN = "8056436628:AAFJXICQ5NPrEnMjyy8z2x2wivozN5Zrpgo";
String CHAT_ID   = "7283664397";

// ---------- Location / flags ----------
float latitude  = 17.0893347;
float longitude = 82.0623243;
int   accuracy  = 50;
int   emergency = 0;

// timing
unsigned long sendDataPrevMillis = 0;
const unsigned long sendInterval = 5000;   // 5 seconds
bool signupOK = false;

// ---------- WiFi connect ----------
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
}

// ---------- Telegram send ----------
void sendTelegramLocation(float lat, float lng) {
  WiFiClientSecure client;
  client.setInsecure();    // for testing, no cert check

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("Telegram: connection failed!");
    return;
  }

  String path = "/bot" + BOT_TOKEN +
                "/sendLocation?chat_id=" + CHAT_ID +
                "&latitude=" + String(lat, 6) +
                "&longitude=" + String(lng, 6);

  Serial.println("Telegram request: " + path);

  client.println("GET " + path + " HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("User-Agent: ESP8266");
  client.println("Connection: close");
  client.println();

  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }

  Serial.println("Telegram: location sent.\n");
}

// ---------- Arduino setup ----------
void setup() {
  Serial.begin(115200);
  delay(1000);

  connectWiFi();

  // Firebase config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase signUp ok");
    signupOK = true;
  } else {
    Serial.printf("signUp error: %s\n",
                  config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// ---------- Arduino loop ----------
void loop() {
  if (!Firebase.ready() || !signupOK) return;

  if (millis() - sendDataPrevMillis > sendInterval ||
      sendDataPrevMillis == 0) {

    sendDataPrevMillis = millis();

    // 1) Write to Firebase
    bool ok = true;
    ok &= Firebase.RTDB.setFloat(&fbdo, "device/location/lat", latitude);
    ok &= Firebase.RTDB.setFloat(&fbdo, "device/location/lng", longitude);
    ok &= Firebase.RTDB.setInt(&fbdo,   "device/location/accuracy", accuracy);
    ok &= Firebase.RTDB.setInt(&fbdo,   "device/emergency", emergency);

    if (ok) {
      Serial.println("Firebase: location/emergency updated");
    } else {
      Serial.println("Firebase write failed");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    // 2) Send Telegram location
    sendTelegramLocation(latitude, longitude);
  }
}
