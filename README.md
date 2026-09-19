# Horloge LED ESP32-C6 (4x matrice 8x8, MAX7219 / 1288BB)

Firmware PlatformIO/Arduino pour une horloge murale :
- ESP32-C6-DEV-KIT-N8 (Waveshare)
- 4 modules LED 8x8 a MAX7219 (ref. 1288BB) chaines en serie -> affichage 32x8
- Heure recuperee par NTP (fuseau horaire POSIX, gestion auto heure ete/hiver)
- Portail Wi-Fi + interface web embarquee pour toute la configuration

## 1. Materiel et cablage

Chainez les 4 modules : `DOUT` du module 1 -> `DIN` du module 2, etc. Seul le
premier module recoit `DIN`/`CLK`/`CS` de l'ESP32 ; les 4 modules partagent
`VCC` et `GND`.

| Module (1er de la chaine) | ESP32-C6-DEV-KIT-N8 |
|---|---|
| VCC | 5V (voir remarque alimentation) |
| GND | GND |
| DIN | GPIO18 |
| CS / LOAD | GPIO20 |
| CLK | GPIO19 |

Ces broches sont definies en haut de [`include/config.h`](include/config.h)
(`MATRIX_DIN_PIN`, `MATRIX_CLK_PIN`, `MATRIX_CS_PIN`) — modifiez-les si votre
cablage differe. Elles ont ete choisies car libres sur le header et en dehors
des broches de strapping/boot (GPIO4, 5, 8, 9, 15) et de l'USB-JTAG
(GPIO12/13). **Verifiez tout de meme le pinout imprime sur votre carte /
le schema Waveshare** avant de cabler, les silkscreens peuvent varier
legerement d'une revision a l'autre.

### Sonde de temperature (optionnelle)

Un module **BME280** ou **BMP280** en I2C (adresse 0x76 ou 0x77) permet
d'afficher la temperature. Sans sonde, le firmware fonctionne normalement et
la temperature n'est simplement pas affichee.

| Module | ESP32-C6-DEV-KIT-N8 |
|---|---|
| VCC | **3V3** (pas le 5V) |
| GND | GND |
| SDA | GPIO23 |
| SCL | GPIO22 |

Broches modifiables dans [`include/config.h`](include/config.h)
(`SENSOR_SDA_PIN`, `SENSOR_SCL_PIN`). La sonde est detectee au demarrage :
redemarrez la carte apres l'avoir branchee. Eloignez-la de l'ESP32 et des
matrices LED, qui chauffent ; le reglage **Correction temp.** de l'interface
web permet d'ajuster la valeur affichee.

**Alimentation** : 4 matrices MAX7219 a pleine luminosite peuvent consommer
jusqu'a ~800 mA-1 A sous 5V. Ne les alimentez pas depuis la broche 5V/3V3 de
l'ESP32 si vous montez la luminosite haute : utilisez une alimentation 5V
externe (chargeur USB par ex.) avec le **GND commun** entre l'alimentation,
les matrices et l'ESP32. Le firmware demarre a une luminosite moderee
(4/15) par defaut, reglable ensuite dans l'interface web.

### Type de cablage MD_MAX72XX

Selon leur fabrication, les modules 8x8 n'ont pas tous la meme orientation
interne. Les 8 types de MD_MAX72XX couvrent les 8 orientations possibles
(rotations et miroirs). Ce reglage s'appelle **Orientation des modules** dans
la carte *Affichage* de l'interface web (la carte redemarre a chaque
changement) ; sa valeur par defaut est `DEFAULT_HW_TYPE_INDEX` dans
`include/config.h`.

Si les chiffres sont couches, a l'envers, en miroir ou decoupes (des formes
qui ne ressemblent a aucune police), essayez les orientations une par une :
`4` (FC-16, blocs 4-en-1 courants), `2` (generique 1288BB), `6` (Parola),
`7` (ICStation), puis les autres. Le reglage "Retourner l'affichage" gere lui
la rotation 180 degres de tout l'ecran, ordre des modules compris.

## 2. Compilation et flash (PlatformIO)

Ce projet est pret a l'emploi avec [PlatformIO](https://platformio.org/)
(extension VS Code, ou CLI `pip install platformio`).

```bash
pio run -t upload
pio device monitor
```

Au premier lancement, PlatformIO telecharge automatiquement le toolchain
ESP32-C6 et les librairies listees dans `platformio.ini`
(MD_Parola, MD_MAX72XX, ArduinoJson).

## 3. Premiere configuration (portail Wi-Fi)

Au premier demarrage (ou si la connexion Wi-Fi enregistree echoue), la
carte cree son propre point d'acces :

- SSID : `HorlogeLED-XXXXXX` (suffixe = adresse MAC)
- Mot de passe : `12345678`

Connectez-vous a ce reseau avec un telephone/PC, une page de connexion
devrait s'ouvrir automatiquement (portail captif). Sinon, ouvrez
`http://192.168.4.1/` manuellement. Depuis cette page :

1. Cliquez **Scanner les reseaux**, choisissez votre Wi-Fi, entrez le mot
   de passe.
2. Verifiez/ajustez le serveur NTP et le fuseau horaire (Europe/Paris est
   preselectionne).
3. Cliquez **Enregistrer** : la carte redemarre et se connecte a votre
   reseau domestique.

Une fois connectee, l'interface reste accessible en permanence depuis votre
reseau, a l'adresse affichee sur le port serie, ou via
`http://horloge.local/` (mDNS ; nom modifiable dans `config.h`,
`DEFAULT_HOSTNAME`).

### Reinitialisation

Pour tout effacer (Wi-Fi inclus) et revenir au mode point d'acces :
- soit via le bouton **Reinitialiser** de l'interface web,
- soit en maintenant le bouton **BOOT** de la carte enfonce 5 secondes au
  demarrage.

## 4. Interface web — reglages disponibles

- **Wi-Fi** : SSID/mot de passe, scan des reseaux environnants.
- **Heure & NTP** : serveur NTP, fuseau horaire (presets ou chaine POSIX TZ
  personnalisee), format 12h/24h.
- **Affichage** : luminosite (0-15), affichage avec ou sans secondes
  (defilement continu si secondes activees), defilement periodique de la
  date suivie de la temperature (si sonde), correction de temperature,
  rotation 180 degres.
- **Mode nuit** : plage horaire (peut passer minuit, ex. 22:00 -> 07:00)
  pendant laquelle la luminosite passe a un niveau de nuit (0-15), ou
  l'affichage s'eteint completement. Actif par defaut de 22:00 a 07:00, au
  niveau 0. Sans heure NTP, l'affichage reste en luminosite normale.
- **Systeme** : redemarrage, reinitialisation usine, statut (IP, heure,
  etat NTP, uptime, version).

## 5. Structure du projet

```
platformio.ini        Configuration PlatformIO (carte, librairies)
include/config.h       Broches, reglages par defaut, structure AppConfig
src/
  config.cpp            Sauvegarde/chargement des reglages (NVS/Preferences)
  time_sync.{h,cpp}      Configuration NTP + fuseau horaire (SNTP integre ESP32)
  display.{h,cpp}        Pilotage des 4 matrices via MD_Parola/MD_MAX72XX, mode nuit
  sensor.{h,cpp}          Sonde de temperature I2C BME280/BMP280
  web_ui.h                Page HTML/CSS/JS de configuration (embarquee en flash)
  web_portal.{h,cpp}     Portail Wi-Fi (AP + portail captif) + serveur web/API
  main.cpp                 setup()/loop()
```

## 6. Pistes d'evolution (non incluses)

- Variation automatique de luminosite selon un capteur LDR.
- Affichage de l'humidite/pression (le BME280 les mesure) ou de la meteo
  via une API tierce.
- Authentification sur l'interface web si elle doit rester accessible en
  dehors d'un reseau domestique de confiance.
