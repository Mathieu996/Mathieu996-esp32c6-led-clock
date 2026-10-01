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

// Sequence de demarrage : fait defiler `text` (ex. "IP 192.168.1.20") en
// boucle. Si untilTimeSynced, s'arrete apres un passage complet des que
// l'heure NTP est synchronisee (ou apres 90 s), puis affiche l'heure ; sinon
// (mode point d'acces, pas d'heure possible) continue jusqu'a un changement
// de reglage.
void displayStartBootSequence(const char *text, bool untilTimeSynced);

// Fait defiler `text` une seule fois, de facon BLOQUANTE (a utiliser
// uniquement dans setup(), avant que la boucle principale ne tourne).
// maxMs borne le blocage par securite.
void displayScrollOnceBlocking(const char *text, unsigned long maxMs = 15000);

// Fait defiler un texte personnalise (interface web), une seule fois (retour
// automatique a l'affichage normal) ou en boucle (jusqu'a l'appel de
// displayStopCustomText()). Non bloquant, utilisable depuis la boucle
// principale.
void displayShowCustomText(const char *text, bool loop);

// Arrete un defilement de texte personnalise en cours (sans effet sinon).
void displayStopCustomText();

// Passe au mode d'affichage fixe suivant (Heure -> Date -> Temperature,
// sans defilement ; Temperature sautee si aucune sonde). Appelee par le
// bouton tactile (appui court).
void displayCycleView();

// Bascule l'affichage force eteint/rallume, independamment du mode nuit.
// Appelee par le bouton tactile (appui long).
void displayToggleManualOff();
