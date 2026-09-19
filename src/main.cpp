#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "sensor.h"
#include "time_sync.h"
#include "web_portal.h"

// Maintenir le bouton BOOT enfonce 5s au demarrage reinitialise tous les
// reglages (Wi-Fi inclus) et redemarre en mode point d'acces.
static void checkFactoryResetButton() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  if (digitalRead(BOOT_BUTTON_PIN) != LOW) return; // pas appuye au boot

  Serial.println("[Boot] Bouton BOOT maintenu, verification reinitialisation...");
  displayShowStatus("RST?");
  unsigned long start = millis();
  while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    if (millis() - start > 5000) {
      Serial.println("[Boot] Reinitialisation usine demandee.");
      displayShowStatus("RST");
      configFactoryReset();
      delay(500);
      ESP.restart();
    }
    delay(50);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[Boot] Horloge LED ESP32-C6");

  configLoad(gConfig);
  displayInit();
  sensorBegin();

  checkFactoryResetButton();

  webPortalBegin();
  timeSyncStart();
}

void loop() {
  webPortalLoop();
  sensorLoop();
  displayLoop();
}
