#pragma once
#include <Arduino.h>

// Bouton tactile TTP223 (optionnel, voir gConfig.touchEnabled/touchPin) :
// appui court -> mode d'affichage suivant (display.h, displayCycleView()) ;
// appui long (~1 s) -> eteint/rallume l'affichage (displayToggleManualOff()).
// Inactif si gConfig.touchEnabled est faux.
void touchButtonBegin();

// A relire apres un changement de reglage (activation ou broche) : pas de
// redemarrage necessaire pour ce peripherique.
void touchButtonApplySettings();

// A appeler en boucle. Non bloquant (debounce par horodatage, sans delay()).
void touchButtonLoop();
