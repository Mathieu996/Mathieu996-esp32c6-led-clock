#pragma once
#include <Arduino.h>

// Configure le fuseau horaire (chaine POSIX TZ) et demarre la synchronisation
// NTP en arriere-plan (SNTP integre a l'ESP32). A rappeler si le serveur NTP
// ou le fuseau horaire changent.
void timeSyncStart();

// true des qu'une premiere synchronisation NTP a reussi.
bool timeIsSynced();
