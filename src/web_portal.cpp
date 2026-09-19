#include "web_portal.h"
#include "web_ui.h"
#include "config.h"
#include "display.h"
#include "sensor.h"
#include "time_sync.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <time.h>

static WebServer server(80);
static DNSServer dnsServer;
static bool apMode = false;
static bool restartPending = false;
static unsigned long restartAtMs = 0;

static const char *CAPTIVE_PROBE_PATHS[] = {
  "/generate_204", "/gen_204", "/ncsi.txt", "/hotspot-detect.html",
  "/library/test/success.html", "/connecttest.txt", "/redirect", "/canonical.html"
};

static void scheduleRestart(unsigned long delayMs) {
  restartPending = true;
  restartAtMs = millis() + delayMs;
}

static String apSsidName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(DEFAULT_AP_SSID_PREFIX) + suffix;
}

static void startAPMode() {
  apMode = true;
  WiFi.mode(WIFI_AP_STA); // AP_STA permet aussi de scanner les reseaux depuis le portail
  String ssid = apSsidName();
  const char *pass = (strlen(gConfig.apPassword) >= 8) ? gConfig.apPassword : nullptr;
  WiFi.softAP(ssid.c_str(), pass);
  delay(200);
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(53, "*", apIP);
  Serial.printf("[WiFi] Mode point d'acces : %s (mdp: %s) -> %s\n",
                ssid.c_str(), pass ? pass : "(ouvert)", apIP.toString().c_str());
  displayShowStatus("AP");
}

static bool connectSTA() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(gConfig.hostname);
  WiFi.begin(gConfig.wifiSsid, gConfig.wifiPass);
  Serial.printf("[WiFi] Connexion a \"%s\"...\n", gConfig.wifiSsid);
  displayShowStatus("...");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    Serial.printf("[WiFi] Connecte, IP = %s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin(gConfig.hostname)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[mDNS] http://%s.local/\n", gConfig.hostname);
    }
    return true;
  }
  Serial.println("[WiFi] Echec de connexion.");
  return false;
}

// ---------------------------------------------------------------------------
// Handlers HTTP
// ---------------------------------------------------------------------------
static void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
  JsonDocument doc;
  doc["mode"] = apMode ? "AP" : "STA";
  doc["ip"] = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["ssid"] = apMode ? apSsidName() : String(gConfig.wifiSsid);
  doc["ntpSynced"] = timeIsSynced();
  doc["version"] = FIRMWARE_VERSION;

  unsigned long s = millis() / 1000;
  char up[24];
  snprintf(up, sizeof(up), "%lu:%02lu:%02lu", s / 3600, (s % 3600) / 60, s % 60);
  doc["uptime"] = up;

  struct tm ti;
  char timeBuf[16] = "--:--:--";
  if (getLocalTime(&ti, 5)) strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &ti);
  doc["time"] = timeBuf;

  if (sensorAvailable()) { // absent du JSON tant qu'aucune sonde ne repond
    char tempBuf[12];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", sensorTemperature());
    doc["temp"] = tempBuf;
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleGetConfig() {
  JsonDocument doc;
  doc["ssid"] = gConfig.wifiSsid;
  doc["ntp"] = gConfig.ntpServer;
  doc["tz"] = gConfig.tzString;
  doc["f24h"] = gConfig.format24h;
  doc["bright"] = gConfig.brightness;
  doc["secs"] = gConfig.showSeconds;
  doc["datesc"] = gConfig.showDateScroll;
  doc["dateiv"] = gConfig.dateIntervalSec;
  doc["flip"] = gConfig.flipDisplay;
  doc["hwType"] = gConfig.hwType;
  doc["showTemp"] = gConfig.showTemp;
  doc["tempOff"] = gConfig.tempOffset;
  doc["nightOn"] = gConfig.nightEnabled;
  doc["nightStart"] = gConfig.nightStartMin;
  doc["nightEnd"] = gConfig.nightEndMin;
  doc["nightBr"] = gConfig.nightBrightness;
  doc["nightOff"] = gConfig.nightOff;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handlePostConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"body manquant\"}");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"json invalide\"}");
    return;
  }

  bool wifiChanged = false;
  bool timeChanged = false;
  bool hwChanged = false;

  if (!doc["ssid"].isNull()) {
    const char *v = doc["ssid"] | "";
    if (strcmp(v, gConfig.wifiSsid) != 0) wifiChanged = true;
    strlcpy(gConfig.wifiSsid, v, sizeof(gConfig.wifiSsid));
  }
  if (!doc["pass"].isNull()) {
    const char *v = doc["pass"] | "";
    if (strlen(v) > 0) { // champ vide = mot de passe inchange
      strlcpy(gConfig.wifiPass, v, sizeof(gConfig.wifiPass));
      wifiChanged = true;
    }
  }
  if (!doc["ntp"].isNull()) {
    const char *v = doc["ntp"] | DEFAULT_NTP_SERVER;
    if (strcmp(v, gConfig.ntpServer) != 0) timeChanged = true;
    strlcpy(gConfig.ntpServer, v, sizeof(gConfig.ntpServer));
  }
  if (!doc["tz"].isNull()) {
    const char *v = doc["tz"] | DEFAULT_TZ;
    if (strcmp(v, gConfig.tzString) != 0) timeChanged = true;
    strlcpy(gConfig.tzString, v, sizeof(gConfig.tzString));
  }
  if (!doc["f24h"].isNull()) gConfig.format24h = doc["f24h"];
  if (!doc["bright"].isNull()) gConfig.brightness = constrain((int)doc["bright"], 0, 15);
  if (!doc["secs"].isNull()) gConfig.showSeconds = doc["secs"];
  if (!doc["datesc"].isNull()) gConfig.showDateScroll = doc["datesc"];
  if (!doc["dateiv"].isNull()) gConfig.dateIntervalSec = constrain((int)doc["dateiv"], 5, 3600);
  if (!doc["flip"].isNull()) gConfig.flipDisplay = doc["flip"];
  if (!doc["hwType"].isNull()) {
    uint8_t v = constrain((int)doc["hwType"], 0, 7);
    if (v != gConfig.hwType) hwChanged = true;
    gConfig.hwType = v;
  }
  if (!doc["showTemp"].isNull()) gConfig.showTemp = doc["showTemp"];
  if (!doc["tempOff"].isNull()) gConfig.tempOffset = constrain(doc["tempOff"].as<float>(), -10.0f, 10.0f);
  if (!doc["nightOn"].isNull()) gConfig.nightEnabled = doc["nightOn"];
  if (!doc["nightStart"].isNull()) gConfig.nightStartMin = constrain((int)doc["nightStart"], 0, 1439);
  if (!doc["nightEnd"].isNull()) gConfig.nightEndMin = constrain((int)doc["nightEnd"], 0, 1439);
  if (!doc["nightBr"].isNull()) gConfig.nightBrightness = constrain((int)doc["nightBr"], 0, 15);
  if (!doc["nightOff"].isNull()) gConfig.nightOff = doc["nightOff"];

  configSave(gConfig);
  displayApplySettings();
  if (timeChanged) timeSyncStart();

  server.send(200, "application/json", "{\"ok\":true}");

  if (wifiChanged || hwChanged) scheduleRestart(1500); // laisse le temps a la reponse HTTP de partir
}

static void handleWifiScan() {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleReboot() {
  server.send(200, "application/json", "{\"ok\":true}");
  scheduleRestart(800);
}

static void handleFactoryReset() {
  server.send(200, "application/json", "{\"ok\":true}");
  configFactoryReset();
  scheduleRestart(800);
}

static void handleNotFound() {
  if (apMode) {
    for (auto path : CAPTIVE_PROBE_PATHS) {
      if (server.uri() == path) {
        server.sendHeader("Location", "http://192.168.4.1/", true);
        server.send(302, "text/plain", "");
        return;
      }
    }
    // Toute autre URL inconnue en mode AP : redirige aussi vers le portail
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "Not found");
}

// ---------------------------------------------------------------------------
void webPortalBegin() {
  bool connected = false;
  if (strlen(gConfig.wifiSsid) > 0) {
    connected = connectSTA();
  }
  if (!connected) {
    startAPMode();
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/wifiscan", HTTP_GET, handleWifiScan);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/factoryreset", HTTP_POST, handleFactoryReset);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Serveur web demarre sur le port 80");
}

void webPortalLoop() {
  if (apMode) dnsServer.processNextRequest();
  server.handleClient();

  if (restartPending && millis() >= restartAtMs) {
    Serial.println("[Systeme] Redemarrage...");
    delay(50);
    ESP.restart();
  }
}

bool webPortalIsAPMode() {
  return apMode;
}
