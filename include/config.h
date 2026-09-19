#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Broches (a adapter si besoin selon votre cablage / silkscreen de la carte)
// ---------------------------------------------------------------------------
// Cablage MAX7219 (chaine de 4 modules 1288BB, DOUT du module 1 -> DIN du
// module 2, etc.) en "software SPI" (n'importe quelles broches libres) :
//   VCC  -> 5V (les 4 modules chaines consomment jusqu'a ~1A a pleine luminosite)
//   GND  -> GND (commun avec l'ESP32-C6)
//   DIN  -> voir MATRIX_DIN_PIN  (entree donnees du 1er module)
//   CS   -> voir MATRIX_CS_PIN   (LOAD/CS, relie a tous les modules)
//   CLK  -> voir MATRIX_CLK_PIN  (horloge, reliee a tous les modules)
//
// Broches choisies ci-dessous car libres sur le header de l'ESP32-C6-DEV-KIT-N8
// et hors broches de strapping/boot (GPIO4, 5, 8, 9, 15) et hors USB-JTAG
// (GPIO12/13). Verifiez le silkscreen de votre carte et ajustez si necessaire.
#define MATRIX_DIN_PIN   18
#define MATRIX_CLK_PIN   19
#define MATRIX_CS_PIN    20

// Nombre de modules 8x8 chaines (4 modules -> affichage 32x8)
#define MATRIX_COUNT     4

// Orientation des modules 8x8 (type de cablage MD_MAX72XX). Les 8 types de la
// bibliotheque couvrent les 8 orientations possibles d'un module ; on les
// designe par un indice 0-7 = (DR << 2) | (CR << 1) | RR, reglable ensuite
// depuis l'interface web (voir README). Ci-dessous : l'indice par defaut.
//   2 = DR0CR1RR0 = GENERIC_HW   (modules 1288BB "nus")
//   4 = DR1CR0RR0 = FC16_HW      (blocs 4-en-1 FC-16 courants)
//   6 = DR1CR1RR0 = PAROLA_HW
//   7 = DR1CR1RR1 = ICSTATION_HW
#define DEFAULT_HW_TYPE_INDEX  4

// Sonde de temperature I2C BME280 / BMP280 (optionnelle : sans sonde, la
// temperature n'est simplement pas affichee).
//   VCC -> 3V3 (PAS le 5V), GND -> GND, SDA/SCL -> broches ci-dessous.
// Ce sont les broches I2C par defaut de la carte (GPIO22/23).
#define SENSOR_SDA_PIN   23
#define SENSOR_SCL_PIN   22

// Bouton BOOT de la carte (GPIO9 sur la plupart des cartes ESP32-C6) :
// un appui long au demarrage force le mode point d'acces de configuration.
#define BOOT_BUTTON_PIN   9

// ---------------------------------------------------------------------------
// Reglages par defaut (modifiables ensuite depuis l'interface web)
// ---------------------------------------------------------------------------
#define DEFAULT_AP_SSID_PREFIX  "HorlogeLED-"
#define DEFAULT_AP_PASSWORD     "12345678"   // 8 caracteres min, "" = ouvert
#define DEFAULT_HOSTNAME        "horloge"
#define DEFAULT_NTP_SERVER      "pool.ntp.org"
#define DEFAULT_TZ              "CET-1CEST,M3.5.0,M10.5.0/3" // Europe/Paris
#define DEFAULT_BRIGHTNESS      4            // 0-15
#define WIFI_CONNECT_TIMEOUT_MS 15000

struct AppConfig {
  char wifiSsid[33]     = "";
  char wifiPass[65]     = "";
  char hostname[33]     = DEFAULT_HOSTNAME;
  char apPassword[65]   = DEFAULT_AP_PASSWORD;
  char ntpServer[65]    = DEFAULT_NTP_SERVER;
  char tzString[65]     = DEFAULT_TZ;
  uint8_t brightness    = DEFAULT_BRIGHTNESS; // 0-15
  bool format24h        = true;
  bool showSeconds       = false; // fait defiler HH:MM:SS au lieu de l'affichage statique HH:MM
  bool showDateScroll    = true;  // fait defiler la date periodiquement
  uint16_t dateIntervalSec = 30;  // toutes les X secondes
  bool flipDisplay       = false; // rotation 180 degres
  uint8_t hwType         = DEFAULT_HW_TYPE_INDEX; // orientation des modules, 0-7 (redemarrage requis)

  // Temperature (sonde I2C) : ajoutee au defilement periodique de la date
  bool showTemp          = true;
  float tempOffset       = 0.0f;  // correction en degres C (echauffement du module, etc.)

  // Mode nuit : entre nightStartMin et nightEndMin (minutes depuis minuit,
  // peut passer minuit), la luminosite passe a nightBrightness, ou l'affichage
  // s'eteint completement si nightOff est vrai.
  bool nightEnabled        = true;
  uint16_t nightStartMin   = 22 * 60;
  uint16_t nightEndMin     = 7 * 60;
  uint8_t nightBrightness  = 0;     // 0-15
  bool nightOff            = false;
};

// Charge la config depuis la NVS (Preferences). Remplit les valeurs par
// defaut si rien n'est encore enregistre.
void configLoad(AppConfig &cfg);

// Sauvegarde la config en NVS.
void configSave(const AppConfig &cfg);

// Remet la config a zero (efface le namespace NVS utilise).
void configFactoryReset();

extern AppConfig gConfig;
