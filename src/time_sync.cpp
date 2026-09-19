#include "time_sync.h"
#include "config.h"
#include <time.h>

static bool synced = false;

void timeSyncStart() {
  synced = false;
  // configTzTime regle a la fois le fuseau horaire (POSIX TZ, gere le
  // changement heure ete/hiver automatiquement) et lance le client SNTP
  // en arriere-plan (aucun blocage ici).
  configTzTime(gConfig.tzString, gConfig.ntpServer);
}

bool timeIsSynced() {
  if (synced) return true;
  // getLocalTime() avec un timeout tres court : ne bloque pas la boucle
  // principale, sert juste a detecter la toute premiere synchronisation.
  struct tm ti;
  if (getLocalTime(&ti, 5)) {
    synced = true;
    Serial.println("[NTP] Heure synchronisee");
  }
  return synced;
}
