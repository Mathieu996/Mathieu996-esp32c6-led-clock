#include "touch_button.h"
#include "config.h"
#include "display.h"

// TTP223 reglages d'usine : sortie "directe" (non verrouillee), active a
// l'etat haut pendant le contact, basse au repos. Le module a sa propre
// sortie push-pull : pas de pull-up/down cote ESP32 (INPUT simple).
static const unsigned long DEBOUNCE_MS   = 30;
static const unsigned long LONG_PRESS_MS = 800;

static bool rawPrev = false;
static unsigned long rawChangedAt = 0;
static bool stableState = false;   // etat debounce actuel (true = appuye)
static unsigned long pressStartAt = 0;
static bool longFired = false;     // appui long deja declenche pour cet appui

void touchButtonBegin() {
  touchButtonApplySettings();
}

void touchButtonApplySettings() {
  if (gConfig.touchEnabled) pinMode(gConfig.touchPin, INPUT);
  // Repart d'un etat neutre : evite qu'un changement de broche en cours
  // d'appui ne declenche une action parasite.
  rawPrev = false;
  stableState = false;
  longFired = false;
}

void touchButtonLoop() {
  if (!gConfig.touchEnabled) return;

  bool raw = digitalRead(gConfig.touchPin) == HIGH;
  unsigned long now = millis();

  if (raw != rawPrev) {
    rawPrev = raw;
    rawChangedAt = now;
  }

  if (raw != stableState && (now - rawChangedAt) >= DEBOUNCE_MS) {
    stableState = raw;
    if (stableState) {
      // Debut d'un appui
      pressStartAt = now;
      longFired = false;
    } else if (!longFired) {
      // Relachement d'un appui court (l'appui long, lui, a deja agi avant le relachement)
      displayCycleView();
    }
  }

  if (stableState && !longFired && (now - pressStartAt) >= LONG_PRESS_MS) {
    longFired = true;
    displayToggleManualOff();
  }
}
