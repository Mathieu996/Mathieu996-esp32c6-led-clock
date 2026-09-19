#include "sensor.h"
#include "config.h"
#include <Wire.h>

// Registres communs BMP280 / BME280 (seule la temperature nous interesse).
static const uint8_t REG_ID        = 0xD0;
static const uint8_t REG_CALIB_T   = 0x88; // dig_T1..dig_T3 (6 octets)
static const uint8_t REG_CTRL_MEAS = 0xF4;
static const uint8_t REG_TEMP_DATA = 0xFA; // 3 octets

// ctrl_meas : temperature x1, pression desactivee, mode "forced" (une mesure
// puis retour en veille : pas d'auto-echauffement notable).
static const uint8_t CTRL_MEAS_FORCED_T1 = 0x21;

static const unsigned long READ_INTERVAL_MS = 10000;
static const unsigned long CONVERSION_MS   = 15; // ~8 ms max a x1

static uint8_t  i2cAddr = 0;
static bool     present = false;
static bool     valid = false;
static float    tempC = 0;

static uint16_t digT1;
static int16_t  digT2, digT3;

static bool     measuring = false;
static unsigned long nextStartAt = 0;
static unsigned long readyAt = 0;

static bool readRegs(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(i2cAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(i2cAddr, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

// Compensation entiere de la datasheet Bosch : centiemes de degre C.
static int32_t compensateTemp(int32_t adc) {
  int32_t v1 = (((adc >> 3) - ((int32_t)digT1 << 1)) * (int32_t)digT2) >> 11;
  int32_t d  = (adc >> 4) - (int32_t)digT1;
  int32_t v2 = (((d * d) >> 12) * (int32_t)digT3) >> 14;
  return (((v1 + v2) * 5) + 128) >> 8;
}

static bool probe(uint8_t addr) {
  i2cAddr = addr;
  uint8_t id;
  if (!readRegs(REG_ID, &id, 1)) return false;
  // 0x56/0x57/0x58 = BMP280, 0x60 = BME280
  if (id != 0x56 && id != 0x57 && id != 0x58 && id != 0x60) return false;
  uint8_t c[6];
  if (!readRegs(REG_CALIB_T, c, 6)) return false;
  digT1 = (uint16_t)(c[0] | (c[1] << 8));
  digT2 = (int16_t)(c[2] | (c[3] << 8));
  digT3 = (int16_t)(c[4] | (c[5] << 8));
  Serial.printf("[Sonde] %s detecte a l'adresse 0x%02X\n", id == 0x60 ? "BME280" : "BMP280", addr);
  return true;
}

void sensorBegin() {
  Wire.begin(SENSOR_SDA_PIN, SENSOR_SCL_PIN);
  present = probe(0x76) || probe(0x77);
  if (!present) Serial.println("[Sonde] Aucune sonde BME280/BMP280 detectee (temperature desactivee).");
}

void sensorLoop() {
  if (!present) return;
  unsigned long now = millis();

  if (!measuring) {
    if ((long)(now - nextStartAt) < 0) return;
    nextStartAt = now + READ_INTERVAL_MS;
    Wire.beginTransmission(i2cAddr);
    Wire.write(REG_CTRL_MEAS);
    Wire.write(CTRL_MEAS_FORCED_T1);
    if (Wire.endTransmission() == 0) {
      measuring = true;
      readyAt = now + CONVERSION_MS;
    } else {
      valid = false; // sonde debranchee : on cesse d'afficher une valeur perimee
    }
    return;
  }

  if ((long)(now - readyAt) < 0) return;
  measuring = false;
  uint8_t d[3];
  if (!readRegs(REG_TEMP_DATA, d, 3)) { valid = false; return; }
  int32_t adc = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
  if (adc == 0x80000) { valid = false; return; } // mesure non effectuee
  tempC = compensateTemp(adc) / 100.0f;
  valid = true;
}

bool sensorAvailable() {
  return valid;
}

float sensorTemperature() {
  return tempC + gConfig.tempOffset;
}
