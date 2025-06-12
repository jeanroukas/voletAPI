# Projet de Contrôle de Volets avec ESP8266 et Google Home

## Vue d'ensemble

Ce projet permet de contrôler jusqu'à 6 volets roulants à l'aide d'un ESP8266 (NodeMCU v3) et de les intégrer avec Google Home. Il utilise des modules d'extension GPIO I2C (HW-171) pour pallier le manque de ports d'entrée/sortie sur l'ESP8266.

## Fichiers du projet

### 1. ESP8266_GoogleHome_Volets.ino
**Description**: Programme principal pour le contrôle des volets avec intégration Google Home.
**Fonctionnalités**:
- Contrôle de 6 volets via des modules I2C GPIO
- Communication MQTT pour l'intégration avec Google Home
- Suivi de position des volets
- Calibration et sauvegarde des paramètres
- Gestion des commandes de groupe

### 2. ESP8266_I2C_Test.ino
**Description**: Programme de test pour vérifier la communication avec les modules I2C et les relais.
**Fonctionnalités**:
- Scan des périphériques I2C
- Test individuel des relais
- Interface série pour le diagnostic

### 3. ESP8266_WebInterface.ino
**Description**: Version du programme principal avec une interface web intégrée.
**Fonctionnalités**:
- Toutes les fonctionnalités du programme principal
- Interface web responsive pour contrôler les volets
- Configuration via interface web (noms des volets, MQTT, calibration)
- Portail captif pour la configuration WiFi

### 4. README_Volets_ESP8266.md
**Description**: Documentation principale du projet.
**Contenu**:
- Description du matériel nécessaire
- Instructions d'installation des bibliothèques
- Guide de configuration
- Dépannage

### 5. schema_cablage.txt
**Description**: Schéma de câblage en ASCII art.
**Contenu**:
- Connexions entre l'ESP8266 et les modules HW-171
- Configuration des adresses I2C
- Connexion des relais

### 6. google_home_integration.md
**Description**: Guide détaillé pour l'intégration avec Google Home.
**Contenu**:
- Configuration de Node-RED
- Configuration de Google Actions Console
- Exemples de flow Node-RED
- Test et dépannage

### 7. guide_depannage.md
**Description**: Guide de dépannage complet.
**Contenu**:
- Problèmes de compilation
- Problèmes matériels
- Problèmes de connexion WiFi et MQTT
- Problèmes d'intégration avec Google Home
- Problèmes de suivi de position

## Architecture du système

```
[Google Home] <---> [MQTT Broker] <---> [ESP8266] <---> [Modules I2C] <---> [Relais] <---> [Volets]
```

1. **Google Home**: Interface utilisateur pour les commandes vocales
2. **MQTT Broker**: Intermédiaire pour la communication entre Google Home et l'ESP8266
3. **ESP8266**: Contrôleur principal qui gère la logique et les communications
4. **Modules I2C**: Extensions GPIO pour augmenter le nombre de ports disponibles
5. **Relais**: Actionneurs qui contrôlent physiquement les volets
6. **Volets**: Les volets roulants à contrôler

## Choix d'implémentation

### 1. Choix du matériel
- **ESP8266 NodeMCU v3**: Choisi pour son WiFi intégré, son faible coût et sa facilité d'utilisation
- **Modules HW-171 (PCF8574)**: Choisis pour leur simplicité d'utilisation via I2C et la possibilité d'avoir jusqu'à 8 modules (64 I/O)

### 2. Protocoles de communication
- **I2C**: Pour communiquer avec les modules d'extension GPIO
- **MQTT**: Pour l'intégration avec Google Home via Node-RED
- **HTTP**: Pour l'interface web de contrôle et configuration

### 3. Stockage des données
- **EEPROM**: Pour sauvegarder les paramètres de calibration, les positions et la configuration

### 4. Interface utilisateur
- **Google Home**: Pour le contrôle vocal
- **Interface Web**: Pour le contrôle manuel et la configuration
- **LED intégrée**: Pour les indications de statut

## Installation et utilisation

### Étape 1: Matériel
1. Assemblez le matériel selon le schéma de câblage
2. Vérifiez les connexions et les adresses I2C

### Étape 2: Logiciel
1. Installez les bibliothèques requises dans Arduino IDE
2. Téléversez le programme de test (ESP8266_I2C_Test.ino)
3. Vérifiez que les modules I2C sont détectés et que les relais fonctionnent
4. Téléversez le programme principal (ESP8266_GoogleHome_Volets.ino ou ESP8266_WebInterface.ino)

### Étape 3: Configuration
1. Connectez-vous au réseau WiFi créé par l'ESP8266 ("Volets-Setup")
2. Configurez les paramètres WiFi
3. Accédez à l'interface web pour configurer les noms des volets et les paramètres MQTT

### Étape 4: Intégration Google Home
1. Configurez Node-RED selon les instructions dans google_home_integration.md
2. Configurez Google Actions Console
3. Testez les commandes vocales

## Extensions possibles

1. **Ajout de capteurs de fin de course**: Pour une position plus précise
2. **Intégration avec d'autres systèmes domotiques**: HomeKit, Alexa, etc.
3. **Contrôle basé sur des horaires**: Ouverture/fermeture automatique à certaines heures
4. **Contrôle basé sur des conditions**: Température, luminosité, etc.
5. **Interface mobile dédiée**: Application mobile pour le contrôle des volets

## Conclusion

Ce projet offre une solution complète pour contrôler des volets roulants avec Google Home, en utilisant un ESP8266 et des modules d'extension GPIO I2C. Il est modulaire, extensible et peut être adapté à différents besoins.