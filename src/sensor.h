#pragma once
#include <Arduino.h>

// Sonde de temperature I2C BME280 / BMP280 (optionnelle, adresse 0x76 ou 0x77).
// Detectee une seule fois au demarrage ; sans sonde, tout reste inactif.
void sensorBegin();

// A appeler en boucle : lance une mesure toutes les 10 s et la lit un peu
// plus tard, sans jamais bloquer.
void sensorLoop();

// Vrai si une mesure valide est disponible.
bool sensorAvailable();

// Derniere temperature en degres C, avec la correction gConfig.tempOffset.
float sensorTemperature();
