#pragma once
#include <Arduino.h>

#define FIRMWARE_VERSION "1.1.0"

// Se connecte au Wi-Fi enregistre, ou bascule en point d'acces de
// configuration ("HorlogeLED-XXXX") si aucun reseau n'est configure ou si
// la connexion echoue. Demarre ensuite le serveur web (port 80) + mDNS.
void webPortalBegin();

// A appeler en boucle : DNS captif (mode AP), requetes HTTP, redemarrage
// differe apres un changement de reglage Wi-Fi / une demande de reboot.
void webPortalLoop();

// true si actuellement en mode point d'acces de configuration.
bool webPortalIsAPMode();
