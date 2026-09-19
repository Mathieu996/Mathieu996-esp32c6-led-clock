#include "display.h"
#include "config.h"
#include "sensor.h"
#include <time.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>

static MD_Parola P(MATRIX_HARDWARE_TYPE, MATRIX_DIN_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, MATRIX_COUNT);

enum DisplayMode : uint8_t { DM_STATIC_CLOCK, DM_SCROLLING };
static DisplayMode dispMode = DM_STATIC_CLOCK;
static bool scrollingIsDate = false;
static unsigned long lastDateShown = 0;
static unsigned long lastStaticRefresh = 0;
static unsigned long lastNightCheck = 0;
static bool colonOn = true;
static bool nightActive = false;

static void buildTimeStr(char *out, size_t n, bool withSeconds) {
  struct tm ti;
  if (!getLocalTime(&ti, 5)) {
    snprintf(out, n, "--:--");
    return;
  }
  if (gConfig.format24h) {
    if (withSeconds) strftime(out, n, "%H:%M:%S", &ti);
    else strftime(out, n, colonOn ? "%H:%M" : "%H %M", &ti);
  } else {
    char tmp[16];
    strftime(tmp, sizeof(tmp), withSeconds ? "%I:%M:%S %p" : "%I:%M %p", &ti);
    const char *src = (tmp[0] == '0') ? tmp + 1 : tmp; // enleve le zero initial (ex "01" -> "1")
    strncpy(out, src, n - 1);
    out[n - 1] = 0;
  }
}

// Date, suivie de la temperature si la sonde est presente et activee.
static void buildDateStr(char *out, size_t n) {
  struct tm ti;
  if (getLocalTime(&ti, 5)) {
    static const char *days[] = {"DIM", "LUN", "MAR", "MER", "JEU", "VEN", "SAM"};
    char buf[16];
    strftime(buf, sizeof(buf), "%d/%m/%Y", &ti);
    snprintf(out, n, "%s %s", days[ti.tm_wday], buf);
  } else {
    snprintf(out, n, "--/--/----");
  }
  size_t len = strlen(out);
  if (gConfig.showTemp && sensorAvailable() && len < n) {
    snprintf(out + len, n - len, "   %.1f%cC", sensorTemperature(), 176); // 176 = symbole degre de la police
  }
}

static void startScroll(const char *text) {
  P.displayClear();
  P.displayText(text, PA_CENTER, 60, 300, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  dispMode = DM_SCROLLING;
}

// Revient a l'heure : defilement HH:MM:SS, ou affichage fixe HH:MM.
static void resumeClock() {
  if (gConfig.showSeconds) {
    char buf[16];
    buildTimeStr(buf, sizeof(buf), true);
    startScroll(buf);
  } else {
    dispMode = DM_STATIC_CLOCK;
    lastStaticRefresh = 0; // force un rafraichissement immediat
  }
}

// ---------------------------------------------------------------------------
// Mode nuit
// ---------------------------------------------------------------------------
static bool isNightNow() {
  if (!gConfig.nightEnabled || gConfig.nightStartMin == gConfig.nightEndMin) return false;
  struct tm ti;
  if (!getLocalTime(&ti, 5)) return false; // heure inconnue : affichage normal
  uint16_t nowMin = ti.tm_hour * 60 + ti.tm_min;
  uint16_t s = gConfig.nightStartMin, e = gConfig.nightEndMin;
  return (s < e) ? (nowMin >= s && nowMin < e)   // ex. 01:00 -> 06:00
                 : (nowMin >= s || nowMin < e);  // passe minuit, ex. 22:00 -> 07:00
}

static void applyBrightness() {
  bool off = nightActive && gConfig.nightOff;
  P.displayShutdown(off);
  if (!off) {
    uint8_t level = nightActive ? gConfig.nightBrightness : gConfig.brightness;
    P.setIntensity(constrain(level, 0, 15));
  }
}

// ---------------------------------------------------------------------------
void displayInit() {
  P.begin();
  displayApplySettings();
  P.displayClear();
}

void displayApplySettings() {
  nightActive = isNightNow();
  applyBrightness();
  P.setZoneEffect(0, gConfig.flipDisplay, PA_FLIP_UD);
  P.setZoneEffect(0, gConfig.flipDisplay, PA_FLIP_LR);

  // Reinitialise proprement l'etat d'affichage apres un changement de reglage
  P.displayClear();
  scrollingIsDate = false;
  lastDateShown = millis();
  colonOn = true;
  resumeClock();
}

void displayShowStatus(const char *msg) {
  P.displayClear();
  P.setTextAlignment(PA_CENTER);
  P.print(msg);
}

void displayLoop() {
  unsigned long now = millis();

  if (now - lastNightCheck >= 1000) {
    lastNightCheck = now;
    bool night = isNightNow();
    if (night != nightActive) {
      nightActive = night;
      applyBrightness();
    }
  }

  if (dispMode == DM_SCROLLING) {
    if (P.displayAnimate()) {
      if (scrollingIsDate) {
        scrollingIsDate = false;
        lastDateShown = now;
        resumeClock();
      } else {
        // Fin d'un defilement de l'heure (mode showSeconds)
        bool dateDue = gConfig.showDateScroll &&
                        (now - lastDateShown >= (unsigned long)gConfig.dateIntervalSec * 1000);
        if (dateDue) {
          char buf[32];
          buildDateStr(buf, sizeof(buf));
          scrollingIsDate = true;
          startScroll(buf);
        } else {
          resumeClock();
        }
      }
    }
    return;
  }

  // DM_STATIC_CLOCK : heure fixe, avec defile periodique de la date
  if (gConfig.showDateScroll &&
      (now - lastDateShown >= (unsigned long)gConfig.dateIntervalSec * 1000)) {
    char buf[32];
    buildDateStr(buf, sizeof(buf));
    scrollingIsDate = true;
    startScroll(buf);
    return;
  }

  if (now - lastStaticRefresh >= 500) {
    lastStaticRefresh = now;
    colonOn = !colonOn;
    char buf[16];
    buildTimeStr(buf, sizeof(buf), false);
    P.setTextAlignment(PA_CENTER);
    P.print(buf);
  }
}
