# Horloge LED ESP32-C6 / ESP32-C3 (4x matrice 8x8, MAX7219 / 1288BB)

Firmware PlatformIO/Arduino pour une horloge murale :
- ESP32-C6-DEV-KIT-N8 (Waveshare) ou ESP32-C3 SuperMini
- 4 modules LED 8x8 a MAX7219 (ref. 1288BB) chaines en serie -> affichage 32x8
- Heure recuperee par NTP (fuseau horaire POSIX, gestion auto heure ete/hiver)
- Portail Wi-Fi + interface web embarquee pour toute la configuration

## 1. Materiel et cablage

Chainez les 4 modules : `DOUT` du module 1 -> `DIN` du module 2, etc. Seul le
premier module recoit `DIN`/`CLK`/`CS` de l'ESP32 ; les 4 modules partagent
`VCC` et `GND`.

| Module (1er de la chaine) | ESP32-C6-DEV-KIT-N8 | ESP32-C3 SuperMini |
|---|---|---|
| VCC | 5V (voir remarque alimentation) | 5V (voir remarque alimentation) |
| GND | GND | GND |
| DIN | GPIO18 | GPIO6 |
| CS / LOAD | GPIO20 | GPIO10 |
| CLK | GPIO19 | GPIO4 |

Ces broches sont les valeurs par defaut, definies en haut de
[`include/config.h`](include/config.h) (`DEFAULT_MATRIX_DIN_PIN`,
`DEFAULT_MATRIX_CLK_PIN`, `DEFAULT_MATRIX_CS_PIN`, un jeu par carte) —
elles ont ete choisies car libres sur le header et en dehors des broches de
strapping/boot (C6 : GPIO4, 5, 8, 9, 15 ; C3 : GPIO2, 8, 9) et de l'USB
(C6 : GPIO12/13 ; C3 : GPIO18/19). **Verifiez tout de meme le pinout imprime
sur votre carte** avant de cabler, les silkscreens peuvent varier legerement
d'une revision a l'autre.

**Modifiables sans recompiler**, depuis la carte **Broches (GPIO)** de
l'interface web (voir section 4) : pratique si votre cablage differe ou si
vous changez de carte. La carte redemarre automatiquement au changement.

### Sonde de temperature (optionnelle)

Un module **BME280** ou **BMP280** en I2C (adresse 0x76 ou 0x77) permet
d'afficher la temperature. Sans sonde, le firmware fonctionne normalement et
la temperature n'est simplement pas affichee.

| Module | ESP32-C6-DEV-KIT-N8 | ESP32-C3 SuperMini |
|---|---|---|
| VCC | **3V3** (pas le 5V) | **3V3** (pas le 5V) |
| GND | GND | GND |
| SDA | GPIO23 | GPIO0 |
| SCL | GPIO22 | GPIO1 |

Broches par defaut dans [`include/config.h`](include/config.h)
(`DEFAULT_SENSOR_SDA_PIN`, `DEFAULT_SENSOR_SCL_PIN`), modifiables comme
celles de la matrice depuis la carte **Broches (GPIO)** de l'interface web.
La sonde est detectee au demarrage : redemarrez la carte apres l'avoir
branchee (ou apres avoir change ses broches). Eloignez-la de l'ESP32 et des
matrices LED, qui chauffent ; le reglage **Correction temp.** de l'interface
web permet d'ajuster la valeur affichee.

### Bouton tactile (optionnel)

Un module **TTP223** permet de changer manuellement ce qui s'affiche, sans
passer par l'interface web. Desactive par defaut (carte **Bouton tactile**
de l'interface web, voir section 4).

| Module | ESP32-C6-DEV-KIT-N8 | ESP32-C3 SuperMini |
|---|---|---|
| VCC | 3V3 | 3V3 |
| GND | GND | GND |
| I/O | GPIO21 (defaut) | GPIO3 (defaut) |

Broche par defaut dans [`include/config.h`](include/config.h)
(`DEFAULT_TOUCH_PIN`), modifiable comme les autres depuis l'interface web
(ce reglage s'applique immediatement, sans redemarrage).

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

Deux environnements sont definis dans `platformio.ini` :
`esp32-c3-supermini` (environnement par defaut, `default_envs`) et
`esp32-c6-devkitc-1`.

```bash
pio run -t upload                      # carte par defaut (C3 SuperMini)
pio run -e esp32-c6-devkitc-1 -t upload  # ESP32-C6
pio device monitor
```

Au premier lancement, PlatformIO telecharge automatiquement le toolchain
et les librairies listees dans `platformio.ini`
(MD_Parola, MD_MAX72XX, ArduinoJson).

Sur le **C3 SuperMini**, le port serie passe par l'USB natif (active par
`ARDUINO_USB_CDC_ON_BOOT`). Si le port COM n'apparait pas au flash, maintenez
le bouton BOOT (GPIO9) en branchant la carte pour passer en mode bootloader.

### Firmware pret a flasher (sans toolchain local)

Les binaires compiles sont disponibles dans la page
[**Releases**](https://github.com/Mathieu996/Mathieu996-esp32c6-led-clock/releases)
du depot : telechargez `horloge-led-esp32-c3-supermini.bin` ou
`horloge-led-esp32-c6.bin` (1er flash uniquement — pour une mise a jour
ulterieure, voir la section [OTA](#5-mise-a-jour-du-firmware-ota)),
puis ecrivez-le a l'adresse `0x0` :

```bash
python -m esptool --chip esp32c3 write-flash 0x0 horloge-led-esp32-c3-supermini.bin
```

Le workflow [`.github/workflows/build.yml`](.github/workflows/build.yml)
compile les deux cartes a chaque push (artefacts dans l'onglet *Actions*) et
publie une release quand on pousse un tag `vX.Y.Z` :

```bash
git tag v1.2.0 && git push origin v1.2.0
```

Pensez a aligner `FIRMWARE_VERSION` dans `src/web_portal.h` avec le tag.

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

### Adresse IP au demarrage

Par defaut, l'adresse IP de la carte defile en clair au demarrage
(`IP 192.168.1.20`), en boucle, jusqu'a la synchronisation de l'heure (apres
un passage complet au minimum), puis l'heure s'affiche. En mode point d'acces,
`AP 192.168.4.1` defile jusqu'a la configuration. Ce comportement se desactive
avec **Afficher l'adresse IP au demarrage** dans la carte *Affichage*.

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
  (HH:MM fixe sur les 3 premiers modules, secondes en petits chiffres en bas
  a droite du dernier ; en 12h, sans AM/PM dans ce mode), defilement periodique de la
  date suivie de la temperature (si sonde), correction de temperature,
  rotation 180 degres, affichage de l'adresse IP au demarrage.
- **Mode nuit** : plage horaire (peut passer minuit, ex. 22:00 -> 07:00)
  pendant laquelle la luminosite passe a un niveau de nuit (0-15), ou
  l'affichage s'eteint completement. Actif par defaut de 22:00 a 07:00, au
  niveau 0. Sans heure NTP, l'affichage reste en luminosite normale.
- **Broches (GPIO)** : DIN/CLK/CS de la matrice et SDA/SCL de la sonde. Les 5
  doivent etre differentes ; toute modification redemarre la carte pour
  reinitialiser la matrice et le bus I2C avec les nouvelles broches.
- **Bouton tactile (TTP223)** : activation et broche (voir section 1).
  Reglage applique immediatement, sans redemarrage.
- **Systeme** : redemarrage, reinitialisation usine, statut (IP, heure,
  etat NTP, uptime, version).
- **Mise a jour du firmware** : televersement OTA d'un fichier `.bin`
  (voir section suivante).

Une fois le bouton tactile active : un **appui court** fait passer au mode
d'affichage fixe suivant, sans defilement — **Heure** (par defaut) ->
**Date** (`JJ/MM`) -> **Temperature** (si une sonde est detectee, sinon ce
mode est saute) -> retour a l'heure. Un **appui long** (~1 seconde) eteint
l'affichage ; un nouvel appui long le rallume (independamment du mode nuit).
Ce mode d'affichage et l'extinction manuelle ne sont pas sauvegardes : la
carte redemarre toujours sur l'heure, affichage allume.

## 5. Mise a jour du firmware (OTA)

Une fois la carte connectee a votre reseau (mode STA), les mises a jour
suivantes ne necessitent plus de cable USB : la carte accepte un nouveau
firmware via l'onglet **Mise a jour du firmware** de l'interface web.

1. Telechargez le fichier `horloge-led-<carte>-ota.bin` correspondant a
   votre carte depuis la page
   [**Releases**](https://github.com/Mathieu996/Mathieu996-esp32c6-led-clock/releases)
   du depot — **pas** le `horloge-led-<carte>.bin` complet qui sert au flash
   USB initial (il contient aussi le bootloader et la table de partitions,
   que l'OTA ne doit pas ecraser). Si vous compilez vous-meme, c'est
   `.pio/build/<carte>/firmware.bin`.
2. Dans l'onglet **Mise a jour du firmware**, choisissez ce fichier puis
   cliquez **Televerser et installer**.
3. Ne fermez pas la page pendant le transfert (jusqu'a une minute selon le
   Wi-Fi) ; la carte redemarre automatiquement une fois l'installation
   terminee.

Techniquement : le firmware ecrit le nouveau binaire dans la partition
applicative OTA inactive (`Update.h`, partitions `app0`/`app1` du schema
`default` — deja en place, aucune modification de `platformio.ini`
necessaire) puis bascule dessus au redemarrage. En cas d'echec en cours de
transfert (fichier invalide, coupure Wi-Fi), la carte redemarre sur le
firmware actuel sans dommage.

**Limite** : chaque partition OTA fait 1,25 Mo ; le firmware actuel en
occupe deja ~90 %. Si une future evolution approche la limite, il faudra
changer de schema de partitions (`board_build.partitions` dans
`platformio.ini`, ex. `min_spiffs.csv` sur 4 Mo ou passer par un module plus
grand).

## 6. Structure du projet

```
platformio.ini        Configuration PlatformIO (cartes C3/C6, librairies)
.github/workflows/     Compilation automatique (GitHub Actions)
include/config.h       Broches, reglages par defaut, structure AppConfig
src/
  config.cpp            Sauvegarde/chargement des reglages (NVS/Preferences)
  time_sync.{h,cpp}      Configuration NTP + fuseau horaire (SNTP integre ESP32)
  display.{h,cpp}        Pilotage des 4 matrices via MD_Parola/MD_MAX72XX, mode nuit
  sensor.{h,cpp}          Sonde de temperature I2C BME280/BMP280
  touch_button.{h,cpp}   Bouton tactile TTP223 (appui court/long)
  web_ui.h                Page HTML/CSS/JS de configuration (embarquee en flash)
  web_portal.{h,cpp}     Portail Wi-Fi (AP + portail captif) + serveur web/API
  main.cpp                 setup()/loop()
```

## 7. Pistes d'evolution (non incluses)

- Variation automatique de luminosite selon un capteur LDR.
- Affichage de l'humidite/pression (le BME280 les mesure) ou de la meteo
  via une API tierce.
- Authentification sur l'interface web si elle doit rester accessible en
  dehors d'un reseau domestique de confiance.
