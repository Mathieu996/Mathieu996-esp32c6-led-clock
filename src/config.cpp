#include "config.h"
#include <Preferences.h>

static const char *NS = "clockcfg";

AppConfig gConfig;

void configLoad(AppConfig &cfg) {
  Preferences prefs;
  prefs.begin(NS, true); // lecture seule

  String s;
  s = prefs.getString("ssid", "");         s.toCharArray(cfg.wifiSsid, sizeof(cfg.wifiSsid));
  s = prefs.getString("pass", "");         s.toCharArray(cfg.wifiPass, sizeof(cfg.wifiPass));
  s = prefs.getString("host", DEFAULT_HOSTNAME); s.toCharArray(cfg.hostname, sizeof(cfg.hostname));
  s = prefs.getString("appass", DEFAULT_AP_PASSWORD); s.toCharArray(cfg.apPassword, sizeof(cfg.apPassword));
  s = prefs.getString("ntp", DEFAULT_NTP_SERVER); s.toCharArray(cfg.ntpServer, sizeof(cfg.ntpServer));
  s = prefs.getString("tz", DEFAULT_TZ);   s.toCharArray(cfg.tzString, sizeof(cfg.tzString));

  cfg.brightness      = prefs.getUChar("bright", DEFAULT_BRIGHTNESS);
  cfg.format24h        = prefs.getBool("f24h", true);
  cfg.showSeconds       = prefs.getBool("secs", false);
  cfg.showDateScroll    = prefs.getBool("datesc", true);
  cfg.dateIntervalSec   = prefs.getUShort("dateiv", 30);
  cfg.flipDisplay       = prefs.getBool("flip", false);
  cfg.hwType            = prefs.getUChar("hwtype", DEFAULT_HW_TYPE_INDEX) & 7;
  cfg.showIpAtBoot      = prefs.getBool("bootip", true);

  cfg.matrixDinPin      = prefs.getUChar("dinpin", DEFAULT_MATRIX_DIN_PIN);
  cfg.matrixClkPin      = prefs.getUChar("clkpin", DEFAULT_MATRIX_CLK_PIN);
  cfg.matrixCsPin       = prefs.getUChar("cspin", DEFAULT_MATRIX_CS_PIN);
  cfg.sensorSdaPin      = prefs.getUChar("sdapin", DEFAULT_SENSOR_SDA_PIN);
  cfg.sensorSclPin      = prefs.getUChar("sclpin", DEFAULT_SENSOR_SCL_PIN);

  cfg.showTemp         = prefs.getBool("showtemp", true);
  cfg.tempOffset        = prefs.getFloat("tempoff", 0.0f);
  cfg.nightEnabled      = prefs.getBool("nighton", true);
  cfg.nightStartMin     = prefs.getUShort("nightst", 22 * 60);
  cfg.nightEndMin       = prefs.getUShort("nightend", 7 * 60);
  cfg.nightBrightness   = prefs.getUChar("nightbr", 0);
  cfg.nightOff          = prefs.getBool("nightoff", false);

  prefs.end();
}

void configSave(const AppConfig &cfg) {
  Preferences prefs;
  prefs.begin(NS, false); // lecture/ecriture

  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("pass", cfg.wifiPass);
  prefs.putString("host", cfg.hostname);
  prefs.putString("appass", cfg.apPassword);
  prefs.putString("ntp", cfg.ntpServer);
  prefs.putString("tz", cfg.tzString);

  prefs.putUChar("bright", cfg.brightness);
  prefs.putBool("f24h", cfg.format24h);
  prefs.putBool("secs", cfg.showSeconds);
  prefs.putBool("datesc", cfg.showDateScroll);
  prefs.putUShort("dateiv", cfg.dateIntervalSec);
  prefs.putBool("flip", cfg.flipDisplay);
  prefs.putUChar("hwtype", cfg.hwType);
  prefs.putBool("bootip", cfg.showIpAtBoot);

  prefs.putUChar("dinpin", cfg.matrixDinPin);
  prefs.putUChar("clkpin", cfg.matrixClkPin);
  prefs.putUChar("cspin", cfg.matrixCsPin);
  prefs.putUChar("sdapin", cfg.sensorSdaPin);
  prefs.putUChar("sclpin", cfg.sensorSclPin);

  prefs.putBool("showtemp", cfg.showTemp);
  prefs.putFloat("tempoff", cfg.tempOffset);
  prefs.putBool("nighton", cfg.nightEnabled);
  prefs.putUShort("nightst", cfg.nightStartMin);
  prefs.putUShort("nightend", cfg.nightEndMin);
  prefs.putUChar("nightbr", cfg.nightBrightness);
  prefs.putBool("nightoff", cfg.nightOff);

  prefs.end();
}

void configFactoryReset() {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.clear();
  prefs.end();
}
