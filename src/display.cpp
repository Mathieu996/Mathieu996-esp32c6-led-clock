#include "display.h"
#include "config.h"
#include "sensor.h"
#include "time_sync.h"
#include <sys/time.h>
#include <time.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>

// Cree dans displayInit() : l'orientation des modules (gConfig.hwType) est un
// reglage enregistre, pris en compte au demarrage.
static MD_Parola *P = nullptr;

// Les 8 types de cablage, indices par (DR << 2) | (CR << 1) | RR.
static const MD_MAX72XX::moduleType_t HW_TYPES[8] = {
  MD_MAX72XX::DR0CR0RR0_HW, MD_MAX72XX::DR0CR0RR1_HW,
  MD_MAX72XX::DR0CR1RR0_HW, MD_MAX72XX::DR0CR1RR1_HW,
  MD_MAX72XX::DR1CR0RR0_HW, MD_MAX72XX::DR1CR0RR1_HW,
  MD_MAX72XX::DR1CR1RR0_HW, MD_MAX72XX::DR1CR1RR1_HW,
};

// Deux-points de HH:MM : la police standard les place une ligne trop bas par
// rapport aux chiffres (lignes 0-6, centre = ligne 3). Points 2x2 sur les
// lignes 1-2 et 4-5. Format d'un caractere de police : largeur, puis colonnes.
static const uint8_t COLON_GLYPH[] = { 2, 0x36, 0x36 };

enum DisplayMode : uint8_t { DM_STATIC_CLOCK, DM_SCROLLING, DM_BOOT };
static DisplayMode dispMode = DM_STATIC_CLOCK;
static bool bootWaitSync = false;
static unsigned long bootStartedAt = 0;
static const unsigned long BOOT_IP_MAX_MS = 90000; // sans synchro NTP, on affiche l'heure ("--:--") apres ce delai
static unsigned long lastDateShown = 0;
static unsigned long lastStaticRefresh = 0;
static unsigned long lastNightCheck = 0;
static long lastDrawnKey = -1000; // etat deja affiche (heure + deux-points), -1000 = a redessiner
static bool nightActive = false;

// ---------------------------------------------------------------------------
// Heure fixe avec secondes : HH:MM en chiffres 4x7 sur les 3 premiers
// modules, secondes en chiffres 3x5 en bas a droite du dernier module.
// Dessine directement dans la matrice (les polices de MD_Parola sont trop larges).
// ---------------------------------------------------------------------------
static const uint8_t DISPLAY_COLS = MATRIX_COUNT * 8;
static_assert(MATRIX_COUNT == 4, "la disposition heure + secondes suppose 4 modules 8x8");

static const char *const DIGITS_4X7[10][7] = {
  {".##.", "#..#", "#..#", "#..#", "#..#", "#..#", ".##."}, // 0
  {"..#.", ".##.", "..#.", "..#.", "..#.", "..#.", ".###"}, // 1
  {".##.", "#..#", "...#", "..#.", ".#..", "#...", "####"}, // 2
  {".##.", "#..#", "...#", "..#.", "...#", "#..#", ".##."}, // 3
  {"...#", "..##", ".#.#", "#..#", "####", "...#", "...#"}, // 4
  {"####", "#...", "###.", "...#", "...#", "#..#", ".##."}, // 5
  {".##.", "#...", "#...", "###.", "#..#", "#..#", ".##."}, // 6
  {"####", "...#", "..#.", "..#.", ".#..", ".#..", ".#.."}, // 7
  {".##.", "#..#", "#..#", ".##.", "#..#", "#..#", ".##."}, // 8
  {".##.", "#..#", "#..#", ".###", "...#", "...#", ".##."}, // 9
};

static const char *const DIGITS_3X5[10][5] = {
  {"###", "#.#", "#.#", "#.#", "###"}, // 0
  {".#.", "##.", ".#.", ".#.", "###"}, // 1
  {"###", "..#", "###", "#..", "###"}, // 2
  {"###", "..#", "###", "..#", "###"}, // 3
  {"#.#", "#.#", "###", "..#", "..#"}, // 4
  {"###", "#..", "###", "..#", "###"}, // 5
  {"###", "#..", "###", "#.#", "###"}, // 6
  {"###", "..#", "..#", "..#", "..#"}, // 7
  {"###", "#.#", "###", "#.#", "###"}, // 8
  {"###", "#.#", "###", "..#", "###"}, // 9
};

// Image en cours de composition : une colonne par octet, de gauche a droite
// (a l'ecran), le bit r = ligne r (0 = en haut).
static uint8_t frame[DISPLAY_COLS];

// Dessine un glyphe ('#' = allume) dont le coin haut-gauche est en (x, y).
static void drawGlyph(const char *const *rows, uint8_t w, uint8_t h, uint8_t x, uint8_t y) {
  for (uint8_t r = 0; r < h; r++)
    for (uint8_t c = 0; c < w; c++)
      if (rows[r][c] == '#') frame[x + c] |= 1 << (y + r);
}

static uint8_t reverseBits(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

// Envoie l'image aux modules, avec la rotation 180 deg si "Retourner l'affichage".
// La colonne 0 de MD_MAX72XX est a droite de l'ecran.
static void pushFrame() {
  MD_MAX72XX *mx = P->getGraphicObject();
  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (uint8_t x = 0; x < DISPLAY_COLS; x++) {
    if (gConfig.flipDisplay) mx->setColumn(x, reverseBits(frame[x]));
    else mx->setColumn(DISPLAY_COLS - 1 - x, frame[x]);
  }
  mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

static void drawClockWithSeconds(const struct tm &ti, bool colon) {
  memset(frame, 0, sizeof(frame));
  int hour = ti.tm_hour;
  if (!gConfig.format24h) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }
  const uint8_t x = 2; // HH:MM occupe les colonnes 2 a 22, secondes 25 a 31
  if (gConfig.format24h || hour >= 10) drawGlyph(DIGITS_4X7[hour / 10], 4, 7, x, 0);
  drawGlyph(DIGITS_4X7[hour % 10], 4, 7, x + 5, 0);
  if (colon) frame[x + 10] = (1 << 2) | (1 << 4);
  drawGlyph(DIGITS_4X7[ti.tm_min / 10], 4, 7, x + 12, 0);
  drawGlyph(DIGITS_4X7[ti.tm_min % 10], 4, 7, x + 17, 0);
  // Secondes : meme ligne de base que l'heure (ligne 6)
  drawGlyph(DIGITS_3X5[ti.tm_sec / 10], 3, 5, 25, 2);
  drawGlyph(DIGITS_3X5[ti.tm_sec % 10], 3, 5, 29, 2);
  pushFrame();
}

// ---------------------------------------------------------------------------
// Textes
// ---------------------------------------------------------------------------
// HH:MM (ou H:MM AM/PM en 12h) ; deux-points eteints = espace.
static void buildTimeStr(char *out, size_t n, const struct tm *ti, bool colon) {
  if (!ti) {
    snprintf(out, n, "--:--");
  } else if (gConfig.format24h) {
    strftime(out, n, colon ? "%H:%M" : "%H %M", ti);
  } else {
    char tmp[16];
    strftime(tmp, sizeof(tmp), colon ? "%I:%M %p" : "%I %M %p", ti);
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
  // MD_Parola ne copie pas le texte : elle garde un pointeur et le relit a
  // chaque image du defilement. Il faut donc un tampon qui reste valide (un
  // tableau local a l'appelant serait deja ecrase -> caracteres aleatoires).
  static char scrollBuf[48];
  strlcpy(scrollBuf, text, sizeof(scrollBuf));
  P->displayClear();
  P->displayText(scrollBuf, PA_CENTER, 60, 300, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  dispMode = DM_SCROLLING;
}

static void applyBrightness();

// Revient a l'heure fixe, redessinee immediatement.
static void resumeClock() {
  dispMode = DM_STATIC_CLOCK;
  lastDrawnKey = -1000;
  lastStaticRefresh = 0;
  applyBrightness(); // fin eventuelle de la sequence de demarrage : le mode nuit reprend
}

// Redessine l'heure fixe si elle a change (minute, ou seconde en mode secondes)
// ou si les deux-points doivent changer d'etat. Les deux-points clignotent en
// phase avec les secondes : allumes pendant la premiere moitie de chaque seconde.
static void updateStaticClock() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  bool colon = tv.tv_usec < 500000;

  struct tm ti;
  bool haveTime = getLocalTime(&ti, 5);
  long key;
  if (!haveTime) {
    key = colon;
  } else {
    long t = ti.tm_hour * 60L + ti.tm_min;
    if (gConfig.showSeconds) t = t * 60 + ti.tm_sec;
    key = 2 * t + colon + 10;
  }
  if (key == lastDrawnKey) return;
  lastDrawnKey = key;

  if (haveTime && gConfig.showSeconds) {
    drawClockWithSeconds(ti, colon);
  } else {
    char buf[16];
    buildTimeStr(buf, sizeof(buf), haveTime ? &ti : nullptr, colon);
    P->setTextAlignment(PA_CENTER);
    P->print(buf);
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
  // Pendant la sequence de demarrage, l'adresse IP reste lisible meme la nuit.
  bool night = nightActive && dispMode != DM_BOOT;
  bool off = night && gConfig.nightOff;
  P->displayShutdown(off);
  if (!off) {
    uint8_t level = night ? gConfig.nightBrightness : gConfig.brightness;
    P->setIntensity(constrain(level, 0, 15));
  }
}

// ---------------------------------------------------------------------------
void displayInit() {
  P = new MD_Parola(HW_TYPES[gConfig.hwType & 7], MATRIX_DIN_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, MATRIX_COUNT);
  P->begin();
  P->addChar(':', COLON_GLYPH);
  displayApplySettings();
  P->displayClear();
}

void displayApplySettings() {
  nightActive = isNightNow();
  P->setZoneEffect(0, gConfig.flipDisplay, PA_FLIP_UD);
  P->setZoneEffect(0, gConfig.flipDisplay, PA_FLIP_LR);

  // Reinitialise proprement l'etat d'affichage apres un changement de reglage
  P->displayClear();
  lastDateShown = millis();
  resumeClock(); // applique aussi la luminosite
}

void displayShowStatus(const char *msg) {
  P->displayClear();
  P->setTextAlignment(PA_CENTER);
  P->print(msg);
  lastDrawnKey = -1000; // l'heure sera redessinee au prochain passage
}

void displayStartBootSequence(const char *text, bool untilTimeSynced) {
  bootWaitSync = untilTimeSynced;
  bootStartedAt = millis();
  startScroll(text);
  dispMode = DM_BOOT; // startScroll() a mis DM_SCROLLING
  applyBrightness();
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

  if (dispMode == DM_BOOT) {
    if (P->displayAnimate()) { // un passage complet de l'adresse est termine
      bool done = bootWaitSync && (timeIsSynced() || now - bootStartedAt >= BOOT_IP_MAX_MS);
      if (done) {
        lastDateShown = now;
        resumeClock();
      } else {
        P->displayReset(); // meme texte, on recommence
      }
    }
    return;
  }

  if (dispMode == DM_SCROLLING) {
    if (P->displayAnimate()) { // fin du defilement de la date
      lastDateShown = now;
      resumeClock();
    }
    return;
  }

  // DM_STATIC_CLOCK : heure fixe, avec defilement periodique de la date
  if (gConfig.showDateScroll &&
      (now - lastDateShown >= (unsigned long)gConfig.dateIntervalSec * 1000)) {
    char buf[32];
    buildDateStr(buf, sizeof(buf));
    startScroll(buf);
    return;
  }

  if (now - lastStaticRefresh >= 50) {
    lastStaticRefresh = now;
    updateStaticClock();
  }
}
