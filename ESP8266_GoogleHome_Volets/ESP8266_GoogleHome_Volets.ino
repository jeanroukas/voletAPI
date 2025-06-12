/*
 * Contrôle de Volets Roulants avec ESP8266 et Google Home
 *
 * Contrôle 6 volets roulants via Google Home et des modules I2C GPIO
 * Chaque volet a un contrôle montée/descente via relais
 * Fonctionnalités:
 * - Contrôle individuel de 6 volets
 * - Suivi de position
 * - Contrôle de groupe
 * - Intégration Google Home
 * - Timeout de sécurité
 * - Mémoire de position et calibration
 *
 * Matériel:
 * - ESP8266 NodeMCU v3
 * - Modules I2C GPIO HW-171 (PCF8574)
 * - 12 relais pour contrôler les volets
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <EEPROM.h>
#include <PCF8574.h>

// Configuration WiFi
const char *ssid = "VOTRE_SSID";
const char *password = "VOTRE_MOT_DE_PASSE";

// Configuration MQTT pour Google Home
const char *mqtt_server = "YOUR_MQTT_BROKER";
const int mqtt_port = 1883;
const char *mqtt_user = "YOUR_MQTT_USER";
const char *mqtt_password = "YOUR_MQTT_PASSWORD";
const char *mqtt_client_id = "ESP8266_Volets";
const char *mqtt_topic_sub = "home/volets/cmd/#";
const char *mqtt_topic_pub = "home/volets/state";

// Configuration des modules PCF8574
PCF8574 pcf8574_1(0x20); // Adresse I2C du premier module
PCF8574 pcf8574_2(0x21); // Adresse I2C du deuxième module

// Constantes
const int NUM_SHUTTERS = 6;  // Nombre de volets
const int NUM_CONTROLS = 12; // Nombre total de contrôles (montée/descente pour chaque volet)
const int DIR_UP = 0;        // Offset pour la direction montée
const int DIR_DOWN = 6;      // Offset pour la direction descente

// Constantes de temps
const unsigned long SAFETY_TIMEOUT = 22000;  // Timeout de sécurité
const unsigned long RELAY_SWITCH_DELAY = 50; // Délai entre commutations de relais

// Variables d'état du système
bool relayState[NUM_CONTROLS] = {false}; // État actuel des relais

// Variables de temps
unsigned long lastUpdateTime = 0; // Dernier moment de mise à jour des positions
unsigned long lastActionTime = 0; // Moment de la dernière action
bool safetyTimeoutActive = false; // État du timeout de sécurité

// Suivi de position
float shutterPosition[NUM_SHUTTERS] = {1, 1, 1, 1, 1, 1};  // 0=fermé, 1=ouvert
float shutterDirection[NUM_SHUTTERS] = {0, 0, 0, 0, 0, 0}; // Direction du mouvement

// Valeurs de calibration (ajustables par volet)
float travelTimeSeconds[NUM_SHUTTERS] = {20, 20, 20, 20, 20, 20};       // Temps pour parcourir la distance complète
float upDownRatio[NUM_SHUTTERS] = {0.96, 0.96, 0.96, 0.96, 0.96, 0.96}; // Ratio entre vitesse montée/descente

// Adresses EEPROM
const int EEPROM_CALIBRATION_START = 0; // Adresse de début pour les données de calibration
const int EEPROM_POSITION_START = 24;   // Adresse de début pour les données de position

// Objets pour WiFi et MQTT
WiFiClient espClient;
PubSubClient client(espClient);

void setup()
{
    // Initialisation de la communication série
    Serial.begin(115200);
    Serial.println("\nDémarrage du système de contrôle de volets...");

    // Initialisation de l'EEPROM
    EEPROM.begin(512);

    // Initialisation I2C
    Wire.begin(D2, D1); // SDA, SCL

    // Initialisation des modules PCF8574
    if (pcf8574_1.begin())
    {
        Serial.println("Module PCF8574 #1 initialisé");
    }
    else
    {
        Serial.println("Erreur d'initialisation du module PCF8574 #1");
    }

    if (pcf8574_2.begin())
    {
        Serial.println("Module PCF8574 #2 initialisé");
    }
    else
    {
        Serial.println("Erreur d'initialisation du module PCF8574 #2");
    }

    // Configuration de tous les pins des modules PCF8574 comme sorties
    for (int i = 0; i < 8; i++)
    {
        pcf8574_1.pinMode(i, OUTPUT);
        pcf8574_2.pinMode(i, OUTPUT);

        // Initialisation des relais (HIGH = inactif)
        pcf8574_1.digitalWrite(i, HIGH);
        pcf8574_2.digitalWrite(i, HIGH);
    }

    // Initialisation de la LED intégrée
    pinMode(LED_BUILTIN, OUTPUT);

    // Connexion WiFi
    setupWifi();

    // Configuration MQTT
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    // Chargement des données de calibration et de position depuis l'EEPROM
    loadCalibrationData();
    loadPositionData();

    // Indication de statut initial
    blinkLED(3); // 3 clignotements indiquent une initialisation réussie
}

void loop()
{
    // Vérification de la connexion MQTT
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

    // Vérification du timeout de sécurité
    checkSafetyTimeout();

    // Mise à jour des positions des volets
    updateShutterPositions();

    // Toutes les 30 secondes, sauvegarde des positions dans l'EEPROM
    static unsigned long lastSaveTime = 0;
    if (millis() - lastSaveTime > 30000)
    {
        savePositionData();
        lastSaveTime = millis();
    }
}

// Configuration WiFi
void setupWifi()
{
    delay(10);
    Serial.println();
    Serial.print("Connexion à ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connecté");
    Serial.print("Adresse IP: ");
    Serial.println(WiFi.localIP());
}

// Reconnexion MQTT
void reconnect()
{
    while (!client.connected())
    {
        Serial.print("Tentative de connexion MQTT...");
        if (client.connect(mqtt_client_id, mqtt_user, mqtt_password))
        {
            Serial.println("connecté");
            client.subscribe(mqtt_topic_sub);

            // Publier l'état initial
            publishState();
        }
        else
        {
            Serial.print("échec, rc=");
            Serial.print(client.state());
            Serial.println(" nouvelle tentative dans 5 secondes");
            delay(5000);
        }
    }
}

// Callback pour les messages MQTT reçus
void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message reçu [");
    Serial.print(topic);
    Serial.print("] ");

    // Convertir payload en string
    String message;
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    Serial.println(message);

    // Analyser le topic pour déterminer l'action
    String topicStr = String(topic);

    if (topicStr.endsWith("/all"))
    {
        // Commande pour tous les volets
        if (message == "OPEN")
        {
            moveAllShutters(true);
        }
        else if (message == "CLOSE")
        {
            moveAllShutters(false);
        }
        else if (message == "STOP")
        {
            stopAllShutters();
        }
    }
    else
    {
        // Commande pour un volet spécifique
        // Format: home/volets/cmd/N où N est l'index du volet (0-5)
        int lastSlash = topicStr.lastIndexOf('/');
        if (lastSlash > 0)
        {
            String shutterIndexStr = topicStr.substring(lastSlash + 1);
            int shutterIndex = shutterIndexStr.toInt();

            if (shutterIndex >= 0 && shutterIndex < NUM_SHUTTERS)
            {
                if (message == "OPEN")
                {
                    setRelay(shutterIndex + DIR_DOWN, true);
                }
                else if (message == "CLOSE")
                {
                    setRelay(shutterIndex, true);
                }
                else if (message == "STOP")
                {
                    setRelay(shutterIndex, false);
                    setRelay(shutterIndex + DIR_DOWN, false);
                }
                else if (message.startsWith("POSITION:"))
                {
                    // Format: POSITION:X.X où X.X est une valeur entre 0 et 1
                    float targetPosition = message.substring(9).toFloat();
                    moveToPosition(shutterIndex, targetPosition);
                }
            }
        }
    }

    // Publier le nouvel état
    publishState();
}

// Publier l'état actuel via MQTT
void publishState()
{
    DynamicJsonDocument doc(1024);
    JsonArray shutters = doc.createNestedArray("shutters");

    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        JsonObject shutter = shutters.createNestedObject();
        shutter["id"] = i;
        shutter["position"] = shutterPosition[i];
        shutter["moving"] = (shutterDirection[i] != 0);
        shutter["direction"] = shutterDirection[i];
    }

    String output;
    serializeJson(doc, output);

    client.publish(mqtt_topic_pub, output.c_str(), true);
}

// Déplacer un volet vers une position spécifique
void moveToPosition(int shutterIndex, float targetPosition)
{
    // Contraindre la position cible entre 0 et 1
    targetPosition = constrain(targetPosition, 0, 1);

    // Déterminer la direction
    if (abs(targetPosition - shutterPosition[shutterIndex]) < 0.01)
    {
        // Déjà à la position cible
        setRelay(shutterIndex, false);
        setRelay(shutterIndex + DIR_DOWN, false);
        return;
    }

    if (targetPosition < shutterPosition[shutterIndex])
    {
        // Fermer (monter)
        setRelay(shutterIndex + DIR_DOWN, false);
        setRelay(shutterIndex, true);
    }
    else
    {
        // Ouvrir (descendre)
        setRelay(shutterIndex, false);
        setRelay(shutterIndex + DIR_DOWN, true);
    }

    // Calculer le temps nécessaire pour atteindre la position
    float distance = abs(targetPosition - shutterPosition[shutterIndex]);
    float timeNeeded = distance * travelTimeSeconds[shutterIndex] * 1000;

    // Programmer l'arrêt après le temps calculé
    unsigned long stopTime = millis() + (unsigned long)timeNeeded;

    // Cette partie est simplifiée - dans une implémentation réelle,
    // vous utiliseriez un timer ou une tâche asynchrone
    // Pour l'instant, nous nous appuyons sur le suivi de position dans updateShutterPositions()
}

// Déplacer tous les volets dans une direction
void moveAllShutters(bool openDirection)
{
    // D'abord arrêter tout mouvement
    for (int i = 0; i < NUM_CONTROLS; i++)
    {
        setRelay(i, false);
        delay(30); // Petit délai entre les opérations de relais
    }

    delay(100); // Pause avant de changer de direction

    // Puis démarrer le mouvement dans la direction souhaitée
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        if (openDirection)
        {
            setRelay(i + DIR_DOWN, true);
        }
        else
        {
            setRelay(i, true);
        }
        delay(30); // Petit délai entre les opérations de relais
    }

    // Mettre à jour le timeout de sécurité
    lastActionTime = millis();
    safetyTimeoutActive = true;
}

// Arrêter tous les volets
void stopAllShutters()
{
    for (int i = 0; i < NUM_CONTROLS; i++)
    {
        setRelay(i, false);
        delay(30); // Petit délai entre les opérations de relais
    }

    // Désactiver le timeout de sécurité
    safetyTimeoutActive = false;
}

// Définir l'état du relais avec vérifications de sécurité
void setRelay(int relayIndex, bool active)
{
    // Mettre à jour le timeout de sécurité
    lastActionTime = millis();
    safetyTimeoutActive = true;

    if (active)
    {
        // Éteindre d'abord le relais opposé
        setRelayPin(getOppositeButton(relayIndex), HIGH); // HIGH = inactif
        relayState[getOppositeButton(relayIndex)] = false;
        delay(RELAY_SWITCH_DELAY);
    }

    // Définir l'état du relais demandé
    setRelayPin(relayIndex, active ? LOW : HIGH); // LOW = actif
    relayState[relayIndex] = active;
}

// Définir l'état physique d'une broche de relais
void setRelayPin(int relayIndex, int state)
{
    // Déterminer quel module PCF8574 et quelle broche utiliser
    if (relayIndex < 8)
    {
        pcf8574_1.digitalWrite(relayIndex, state);
    }
    else if (relayIndex < 16)
    {
        pcf8574_2.digitalWrite(relayIndex - 8, state);
    }
}

// Mettre à jour les positions des volets en fonction du mouvement
void updateShutterPositions()
{
    unsigned long currentTime = millis();
    unsigned long deltaTime = currentTime - lastUpdateTime;
    lastUpdateTime = currentTime;

    // Calculer la direction pour chaque volet
    calculateShutterDirections();

    // Mettre à jour les positions en fonction de la direction et du temps
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        if (shutterDirection[i] != 0)
        {
            // Calculer le changement de position
            float change = (deltaTime / (1000.0 * travelTimeSeconds[i])) * shutterDirection[i];

            // Mettre à jour la position
            shutterPosition[i] += change;

            // Contraindre la position entre 0 et 1
            shutterPosition[i] = constrain(shutterPosition[i], 0, 1);

            // Si la position atteint une limite, arrêter le mouvement
            if ((shutterPosition[i] <= 0 && shutterDirection[i] < 0) ||
                (shutterPosition[i] >= 1 && shutterDirection[i] > 0))
            {
                setRelay(i, false);
                setRelay(i + DIR_DOWN, false);
            }
        }
    }
}

// Calculer la direction de mouvement pour chaque volet
void calculateShutterDirections()
{
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        shutterDirection[i] = 0;

        // Vérifier la direction montée (mouvement négatif = fermeture)
        if (relayState[i])
        {
            shutterDirection[i] -= 1.0;
        }

        // Vérifier la direction descente (mouvement positif = ouverture)
        if (relayState[i + DIR_DOWN])
        {
            shutterDirection[i] += upDownRatio[i];
        }
    }
}

// Vérifier le timeout de sécurité et arrêter tous les volets si nécessaire
void checkSafetyTimeout()
{
    if (safetyTimeoutActive && (millis() - lastActionTime > SAFETY_TIMEOUT))
    {
        stopAllShutters();
        safetyTimeoutActive = false;
    }
}

// Charger les données de calibration depuis l'EEPROM
void loadCalibrationData()
{
    // Vérifier si l'EEPROM a été initialisée
    if (EEPROM.read(0) == 0xFF)
    {
        // EEPROM non initialisée, utiliser les valeurs par défaut
        return;
    }

    int address = EEPROM_CALIBRATION_START;

    // Charger les temps de parcours
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        EEPROM.get(address, travelTimeSeconds[i]);
        address += sizeof(float);
    }

    // Charger les ratios montée/descente
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        EEPROM.get(address, upDownRatio[i]);
        address += sizeof(float);
    }
}

// Sauvegarder les données de calibration dans l'EEPROM
void saveCalibrationData()
{
    int address = EEPROM_CALIBRATION_START;

    // Sauvegarder les temps de parcours
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        EEPROM.put(address, travelTimeSeconds[i]);
        address += sizeof(float);
    }

    // Sauvegarder les ratios montée/descente
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        EEPROM.put(address, upDownRatio[i]);
        address += sizeof(float);
    }

    EEPROM.commit();
}

// Charger les données de position depuis l'EEPROM
void loadPositionData()
{
    // Vérifier si l'EEPROM a été initialisée
    if (EEPROM.read(0) == 0xFF)
    {
        // EEPROM non initialisée, utiliser les valeurs par défaut
        return;
    }

    int address = EEPROM_POSITION_START;

    // Charger les positions
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        EEPROM.get(address, shutterPosition[i]);
        address += sizeof(float);
    }
}

// Sauvegarder les données de position dans l'EEPROM
void savePositionData()
{
    int address = EEPROM_POSITION_START;

    // Sauvegarder les positions
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        EEPROM.put(address, shutterPosition[i]);
        address += sizeof(float);
    }

    EEPROM.commit();
}

// Faire clignoter la LED intégrée
void blinkLED(int times)
{
    for (int i = 0; i < times; i++)
    {
        digitalWrite(LED_BUILTIN, LOW); // LOW = allumé pour ESP8266
        delay(200);
        digitalWrite(LED_BUILTIN, HIGH); // HIGH = éteint pour ESP8266
        delay(200);
    }
}

// Fonctions utilitaires

// Obtenir le bouton de direction opposée
int getOppositeButton(int buttonIndex)
{
    return buttonIndex < DIR_DOWN ? (buttonIndex + DIR_DOWN) : (buttonIndex - DIR_DOWN);
}