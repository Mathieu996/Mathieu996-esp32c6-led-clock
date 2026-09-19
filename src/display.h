#pragma once
#include <Arduino.h>

// Initialise les 4 matrices MAX7219 (MD_Parola / MD_MAX72XX).
void displayInit();

// Applique la luminosite (0-15) et le flip depuis gConfig.
void displayApplySettings();

// A appeler en boucle : gere l'animation en cours (defilement de la date)
// et rafraichit l'heure. Non bloquant.
void displayLoop();

// Affiche un message de statut court et statique (ex: "AP", "...", "ERR").
void displayShowStatus(const char *msg);
