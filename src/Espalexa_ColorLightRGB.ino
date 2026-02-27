#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#define ESPALEXA_ASYNC
#include <Espalexa.h>

// WLAN-Zugangsdaten
const char* ssid = "WLAN-107461";
const char* password = "77412282629652538534";

Espalexa espalexa;

// RGB-Pins definieren (ESP8266)
const int redPin = 12;   // D6
const int greenPin = 13; // D7
const int bluePin = 15;  // D8

// Funktion zur WLAN-Verbindung
bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Verbinde mit WLAN...");

  for (int i = 0; i < 40; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWLAN verbunden!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nVerbindung fehlgeschlagen.");
  return false;
}

void setup() {
  Serial.begin(115200);

  // RGB Pins als Ausgänge setzen
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  // Alles aus beim Start
  analogWrite(redPin, 0);
  analogWrite(greenPin, 0);
  analogWrite(bluePin, 0);

  if (!connectWifi()) {
    while (true) {
      Serial.println("WLAN-Daten prüfen und Neustart durchführen.");
      delay(3000);
    }
  }

  espalexa.addDevice("Color Light", colorLightChanged);
  espalexa.begin();
}

void loop() {
  espalexa.loop();
  delay(1);
}

// Callback wenn Farbe/Helligkeit geändert wird
void colorLightChanged(uint8_t brightness, uint32_t rgb) {
  uint8_t r = ((rgb >> 16) & 0xFF) * brightness / 255;
  uint8_t g = ((rgb >> 8) & 0xFF) * brightness / 255;
  uint8_t b = (rgb & 0xFF) * brightness / 255;

  analogWrite(redPin, r);
  analogWrite(greenPin, g);
  analogWrite(bluePin, b);

  Serial.printf("Helligkeit: %d | RGB: (%d, %d, %d)\n", brightness, r, g, b);
}
