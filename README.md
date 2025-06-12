# Contrôle de Volets Roulants avec ESP8266 et Google Home

Ce projet permet de contrôler jusqu'à 6 volets roulants à l'aide d'un ESP8266 (NodeMCU v3) et de les intégrer avec Google Home. Il utilise des modules d'extension GPIO I2C (HW-171) pour pallier le manque de ports d'entrée/sortie.

## Matériel nécessaire

- 1× ESP8266 NodeMCU v3
- 2× Modules I2C GPIO HW-171 (basés sur PCF8574)
- 12× Relais pour contrôler les volets (6 volets × 2 directions)
- Câbles de connexion
- Alimentation 5V pour l'ESP8266
- Alimentation pour les relais (selon vos besoins)

## Schéma de connexion

### Connexion des modules I2C GPIO HW-171

Les modules HW-171 utilisent le protocole I2C et peuvent être connectés en parallèle (pas en série) sur le même bus I2C:

```
ESP8266 NodeMCU v3  ->  Modules HW-171
D1 (GPIO 5/SCL)     ->  SCL de tous les modules HW-171
D2 (GPIO 4/SDA)     ->  SDA de tous les modules HW-171
3.3V                ->  VCC de tous les modules HW-171
GND                 ->  GND de tous les modules HW-171
```

### Configuration des adresses I2C

Chaque module HW-171 doit avoir une adresse I2C unique. Les adresses sont configurées à l'aide des cavaliers A0, A1 et A2 sur les modules:

- Module 1: Adresse 0x20 (tous les cavaliers à 0)
- Module 2: Adresse 0x21 (A0=1, A1=0, A2=0)

Si vous avez besoin de plus de modules, vous pouvez continuer avec les adresses 0x22, 0x23, etc.

### Connexion des relais

Les relais sont connectés aux sorties des modules HW-171:

- Module 1 (0x20):
  - P0 à P5: Relais pour la montée des volets 0 à 5
  - P6 à P7: Relais pour la descente des volets 0 à 1

- Module 2 (0x21):
  - P0 à P3: Relais pour la descente des volets 2 à 5
  - P4 à P7: Non utilisés (disponibles pour extension)

## Installation des bibliothèques

Pour ce projet, vous aurez besoin d'installer les bibliothèques suivantes via le gestionnaire de bibliothèques d'Arduino IDE:

1. **ESP8266WiFi**: Pour la connexion WiFi (incluse dans le package ESP8266)
2. **PubSubClient**: Pour la communication MQTT
3. **ArduinoJson**: Pour le traitement des données JSON
4. **PCF8574**: Pour contrôler les modules I2C GPIO HW-171

### Étapes d'installation:

1. Ouvrez Arduino IDE
2. Allez dans Outils > Gestionnaire de cartes
3. Recherchez "esp8266" et installez "ESP8266 by ESP8266 Community"
4. Allez dans Croquis > Inclure une bibliothèque > Gérer les bibliothèques
5. Recherchez et installez les bibliothèques suivantes:
   - PubSubClient
   - ArduinoJson
   - PCF8574 (par Renzo Mischianti)

## Configuration du code

Avant de téléverser le code, vous devez configurer les paramètres WiFi et MQTT dans le fichier `ESP8266_GoogleHome_Volets.ino`:

```cpp
// Configuration WiFi
const char* ssid = "VOTRE_SSID";
const char* password = "VOTRE_MOT_DE_PASSE";

// Configuration MQTT pour Google Home
const char* mqtt_server = "YOUR_MQTT_BROKER";
const int mqtt_port = 1883;
const char* mqtt_user = "YOUR_MQTT_USER";
const char* mqtt_password = "YOUR_MQTT_PASSWORD";
```

## Intégration avec Google Home

Pour intégrer avec Google Home, vous avez besoin d'un broker MQTT et d'un service de pont comme Node-RED avec le plugin node-red-contrib-google-smarthome.

### Configuration de base:

1. **Installer un broker MQTT** (comme Mosquitto)
2. **Installer Node-RED** et configurer le plugin Google Smart Home
3. **Créer un projet dans Google Actions Console**
4. **Configurer les appareils dans Node-RED** pour qu'ils apparaissent dans Google Home

Un guide détaillé pour cette configuration est disponible sur: [https://flows.nodered.org/node/node-red-contrib-google-smarthome](https://flows.nodered.org/node/node-red-contrib-google-smarthome)

## Commandes MQTT

Le système répond aux commandes MQTT suivantes:

- `home/volets/cmd/all` avec les valeurs:
  - `OPEN`: Ouvre tous les volets
  - `CLOSE`: Ferme tous les volets
  - `STOP`: Arrête tous les volets

- `home/volets/cmd/N` (où N est l'index du volet de 0 à 5) avec les valeurs:
  - `OPEN`: Ouvre le volet spécifié
  - `CLOSE`: Ferme le volet spécifié
  - `STOP`: Arrête le volet spécifié
  - `POSITION:X.X`: Déplace le volet à la position spécifiée (0.0 = fermé, 1.0 = ouvert)

## État du système

L'état du système est publié sur le topic MQTT `home/volets/state` au format JSON:

```json
{
  "shutters": [
    {
      "id": 0,
      "position": 0.75,
      "moving": true,
      "direction": 1.0
    },
    ...
  ]
}
```

## Calibration

Le système utilise des valeurs de calibration pour chaque volet:

- `travelTimeSeconds`: Temps en secondes pour parcourir la distance complète
- `upDownRatio`: Ratio entre la vitesse de montée et de descente

Ces valeurs peuvent être ajustées dans le code ou via une interface de calibration (à développer).

## Dépannage

### Les modules I2C ne sont pas détectés

1. Vérifiez les connexions I2C (SDA, SCL)
2. Vérifiez l'alimentation des modules
3. Vérifiez les adresses I2C avec un scanner I2C
4. Assurez-vous que les cavaliers d'adresse sont correctement configurés

### Les relais ne s'activent pas

1. Vérifiez les connexions entre les modules HW-171 et les relais
2. Vérifiez l'alimentation des relais
3. Testez les relais individuellement avec un programme simple

### Problèmes de connexion WiFi ou MQTT

1. Vérifiez les paramètres WiFi (SSID, mot de passe)
2. Vérifiez les paramètres MQTT (serveur, port, identifiants)
3. Vérifiez que le broker MQTT est accessible depuis l'ESP8266

## Extensions possibles

- Interface web pour la configuration et le contrôle manuel
- Ajout de capteurs de fin de course pour une position plus précise
- Intégration avec d'autres systèmes domotiques (HomeKit, Alexa)
- Contrôle basé sur des horaires ou des conditions (lever/coucher du soleil)
