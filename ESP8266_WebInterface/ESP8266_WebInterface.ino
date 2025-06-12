/*
 * Interface Web pour le contrôle de volets avec ESP8266
 *
 * Ce programme ajoute une interface web au système de contrôle de volets,
 * permettant de contrôler les volets via un navigateur web en plus de Google Home.
 *
 * Fonctionnalités:
 * - Interface web responsive
 * - Contrôle individuel des volets
 * - Contrôle de groupe
 * - Affichage des positions en temps réel
 * - Configuration WiFi et MQTT
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <EEPROM.h>
#include <PCF8574.h>
#include <DNSServer.h>
#include <WiFiManager.h>

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
const int EEPROM_CONFIG_START = 100;    // Adresse de début pour la configuration

// Configuration MQTT
char mqtt_server[40] = "192.168.1.100";
int mqtt_port = 1883;
char mqtt_user[20] = "";
char mqtt_password[20] = "";
char mqtt_client_id[20] = "ESP8266_Volets";
char mqtt_topic_sub[30] = "home/volets/cmd/#";
char mqtt_topic_pub[30] = "home/volets/state";

// Noms des volets
char shutterNames[NUM_SHUTTERS][20] = {
    "Salon", "Cuisine", "Chambre 1", "Chambre 2", "Bureau", "Salle de bain"};

// Objets pour WiFi, MQTT et serveur web
WiFiClient espClient;
PubSubClient mqttClient(espClient);
ESP8266WebServer server(80);
DNSServer dnsServer;
WiFiManager wifiManager;

// Intervalle de publication MQTT
unsigned long lastMqttPublish = 0;
const unsigned long MQTT_PUBLISH_INTERVAL = 2000; // 2 secondes

void setup()
{
    // Initialisation de la communication série
    Serial.begin(115200);
    Serial.println("\nDémarrage du système de contrôle de volets avec interface web...");

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

    // Chargement de la configuration depuis l'EEPROM
    loadConfig();

    // Configuration WiFi avec WiFiManager
    // WiFiManager gère automatiquement la connexion WiFi
    // Si la connexion échoue, il crée un point d'accès "Volets-Setup"
    wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    wifiManager.autoConnect("Volets-Setup");

    Serial.println("WiFi connecté");
    Serial.print("Adresse IP: ");
    Serial.println(WiFi.localIP());

    // Configuration MQTT
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);

    // Configuration du serveur web
    setupWebServer();

    // Chargement des données de calibration et de position depuis l'EEPROM
    loadCalibrationData();
    loadPositionData();

    // Indication de statut initial
    blinkLED(3); // 3 clignotements indiquent une initialisation réussie
}

void loop()
{
    // Gestion du serveur DNS (pour le portail captif)
    dnsServer.processNextRequest();

    // Gestion du serveur web
    server.handleClient();

    // Vérification de la connexion MQTT
    if (!mqttClient.connected())
    {
        reconnectMqtt();
    }
    mqttClient.loop();

    // Vérification du timeout de sécurité
    checkSafetyTimeout();

    // Mise à jour des positions des volets
    updateShutterPositions();

    // Publication périodique de l'état via MQTT
    if (millis() - lastMqttPublish > MQTT_PUBLISH_INTERVAL)
    {
        publishState();
        lastMqttPublish = millis();
    }

    // Toutes les 30 secondes, sauvegarde des positions dans l'EEPROM
    static unsigned long lastSaveTime = 0;
    if (millis() - lastSaveTime > 30000)
    {
        savePositionData();
        lastSaveTime = millis();
    }
}

// Configuration du serveur web
void setupWebServer()
{
    // Page d'accueil
    server.on("/", handleRoot);

    // API pour contrôler les volets
    server.on("/api/shutter", HTTP_GET, handleShutterControl);

    // API pour obtenir l'état des volets
    server.on("/api/state", HTTP_GET, handleGetState);

    // Page de configuration
    server.on("/config", handleConfig);

    // API pour sauvegarder la configuration
    server.on("/api/saveconfig", HTTP_POST, handleSaveConfig);

    // API pour la calibration
    server.on("/api/calibration", HTTP_POST, handleCalibration);

    // Gestionnaire de fichiers non trouvés
    server.onNotFound(handleNotFound);

    // Démarrage du serveur
    server.begin();
    Serial.println("Serveur HTTP démarré");
}

// Gestionnaire pour la page d'accueil
void handleRoot()
{
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Contrôle des Volets</title>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += "h1{color:#333;}";
    html += ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:5px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
    html += ".shutter{margin-bottom:20px;padding:15px;border:1px solid #ddd;border-radius:5px;}";
    html += ".shutter h2{margin-top:0;}";
    html += ".controls{display:flex;gap:10px;margin-bottom:10px;}";
    html += "button{padding:8px 15px;border:none;border-radius:4px;cursor:pointer;background:#4CAF50;color:white;}";
    html += "button:hover{background:#45a049;}";
    html += ".stop{background:#f44336;}";
    html += ".stop:hover{background:#d32f2f;}";
    html += ".position{margin-top:10px;}";
    html += ".slider{width:100%;margin-top:10px;}";
    html += ".group-controls{margin-top:30px;padding:15px;border:1px solid #ddd;border-radius:5px;background:#f9f9f9;}";
    html += ".config-link{display:block;margin-top:20px;text-align:center;}";
    html += "</style>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<h1>Contrôle des Volets</h1>";

    // Contrôles individuels pour chaque volet
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        html += "<div class='shutter'>";
        html += "<h2>" + String(shutterNames[i]) + "</h2>";
        html += "<div class='controls'>";
        html += "<button onclick='controlShutter(" + String(i) + ", \"OPEN\")'>Ouvrir</button>";
        html += "<button onclick='controlShutter(" + String(i) + ", \"STOP\")' class='stop'>Stop</button>";
        html += "<button onclick='controlShutter(" + String(i) + ", \"CLOSE\")'>Fermer</button>";
        html += "</div>";
        html += "<div class='position'>Position: <span id='position-" + String(i) + "'>" + String(int(shutterPosition[i] * 100)) + "%</span></div>";
        html += "<input type='range' min='0' max='100' value='" + String(int(shutterPosition[i] * 100)) + "' class='slider' id='slider-" + String(i) + "' onchange='setPosition(" + String(i) + ", this.value)'>";
        html += "</div>";
    }

    // Contrôles de groupe
    html += "<div class='group-controls'>";
    html += "<h2>Contrôle de groupe</h2>";
    html += "<div class='controls'>";
    html += "<button onclick='controlAll(\"OPEN\")'>Tout ouvrir</button>";
    html += "<button onclick='controlAll(\"STOP\")' class='stop'>Tout arrêter</button>";
    html += "<button onclick='controlAll(\"CLOSE\")'>Tout fermer</button>";
    html += "</div>";
    html += "</div>";

    // Lien vers la page de configuration
    html += "<a href='/config' class='config-link'>Configuration</a>";

    // JavaScript pour les contrôles
    html += "<script>";
    html += "function controlShutter(id, cmd) {";
    html += "  fetch('/api/shutter?id=' + id + '&cmd=' + cmd)";
    html += "    .then(response => response.json())";
    html += "    .then(data => updatePositions(data));";
    html += "}";
    html += "function setPosition(id, position) {";
    html += "  fetch('/api/shutter?id=' + id + '&position=' + position)";
    html += "    .then(response => response.json())";
    html += "    .then(data => updatePositions(data));";
    html += "}";
    html += "function controlAll(cmd) {";
    html += "  fetch('/api/shutter?all=1&cmd=' + cmd)";
    html += "    .then(response => response.json())";
    html += "    .then(data => updatePositions(data));";
    html += "}";
    html += "function updatePositions(data) {";
    html += "  for (let i = 0; i < data.shutters.length; i++) {";
    html += "    document.getElementById('position-' + i).textContent = Math.round(data.shutters[i].position * 100) + '%';";
    html += "    document.getElementById('slider-' + i).value = Math.round(data.shutters[i].position * 100);";
    html += "  }";
    html += "}";
    html += "// Mise à jour périodique des positions";
    html += "setInterval(() => {";
    html += "  fetch('/api/state')";
    html += "    .then(response => response.json())";
    html += "    .then(data => updatePositions(data));";
    html += "}, 2000);";
    html += "</script>";

    html += "</div></body></html>";

    server.send(200, "text/html", html);
}

// Gestionnaire pour le contrôle des volets
void handleShutterControl()
{
    bool allShutters = server.hasArg("all");
    String cmd = server.arg("cmd");

    if (allShutters)
    {
        // Contrôle de tous les volets
        if (cmd == "OPEN")
        {
            moveAllShutters(true);
        }
        else if (cmd == "CLOSE")
        {
            moveAllShutters(false);
        }
        else if (cmd == "STOP")
        {
            stopAllShutters();
        }
    }
    else if (server.hasArg("id"))
    {
        // Contrôle d'un volet spécifique
        int id = server.arg("id").toInt();

        if (id >= 0 && id < NUM_SHUTTERS)
        {
            if (cmd == "OPEN")
            {
                setRelay(id + DIR_DOWN, true);
            }
            else if (cmd == "CLOSE")
            {
                setRelay(id, true);
            }
            else if (cmd == "STOP")
            {
                setRelay(id, false);
                setRelay(id + DIR_DOWN, false);
            }
            else if (server.hasArg("position"))
            {
                // Déplacer à une position spécifique
                float targetPosition = server.arg("position").toFloat() / 100.0;
                moveToPosition(id, targetPosition);
            }
        }
    }

    // Renvoyer l'état actuel
    handleGetState();
}

// Gestionnaire pour obtenir l'état des volets
void handleGetState()
{
    DynamicJsonDocument doc(1024);
    JsonArray shutters = doc.createNestedArray("shutters");

    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        JsonObject shutter = shutters.createNestedObject();
        shutter["id"] = i;
        shutter["name"] = shutterNames[i];
        shutter["position"] = shutterPosition[i];
        shutter["moving"] = (shutterDirection[i] != 0);
        shutter["direction"] = shutterDirection[i];
    }

    String output;
    serializeJson(doc, output);

    server.send(200, "application/json", output);
}

// Gestionnaire pour la page de configuration
void handleConfig()
{
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Configuration</title>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}";
    html += "h1,h2{color:#333;}";
    html += ".container{max-width:800px;margin:0 auto;background:white;padding:20px;border-radius:5px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
    html += ".form-group{margin-bottom:15px;}";
    html += "label{display:block;margin-bottom:5px;font-weight:bold;}";
    html += "input[type=text],input[type=number]{width:100%;padding:8px;box-sizing:border-box;border:1px solid #ddd;border-radius:4px;}";
    html += "button{padding:10px 15px;border:none;border-radius:4px;cursor:pointer;background:#4CAF50;color:white;margin-top:10px;}";
    html += "button:hover{background:#45a049;}";
    html += ".tab{overflow:hidden;border:1px solid #ccc;background-color:#f1f1f1;}";
    html += ".tab button{background-color:inherit;float:left;border:none;outline:none;cursor:pointer;padding:10px 16px;transition:0.3s;}";
    html += ".tab button:hover{background-color:#ddd;}";
    html += ".tab button.active{background-color:#ccc;}";
    html += ".tabcontent{display:none;padding:20px;border:1px solid #ccc;border-top:none;}";
    html += ".home-link{display:block;margin-top:20px;text-align:center;}";
    html += "</style>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<h1>Configuration</h1>";

    // Onglets
    html += "<div class='tab'>";
    html += "<button class='tablinks' onclick='openTab(event, \"general\")' id='defaultOpen'>Général</button>";
    html += "<button class='tablinks' onclick='openTab(event, \"mqtt\")'>MQTT</button>";
    html += "<button class='tablinks' onclick='openTab(event, \"calibration\")'>Calibration</button>";
    html += "</div>";

    // Onglet Général
    html += "<div id='general' class='tabcontent'>";
    html += "<h2>Configuration générale</h2>";
    html += "<form id='generalForm'>";

    // Noms des volets
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        html += "<div class='form-group'>";
        html += "<label for='shutterName" + String(i) + "'>Nom du volet " + String(i + 1) + ":</label>";
        html += "<input type='text' id='shutterName" + String(i) + "' name='shutterName" + String(i) + "' value='" + String(shutterNames[i]) + "'>";
        html += "</div>";
    }

    html += "<button type='button' onclick='saveGeneral()'>Enregistrer</button>";
    html += "</form>";
    html += "</div>";

    // Onglet MQTT
    html += "<div id='mqtt' class='tabcontent'>";
    html += "<h2>Configuration MQTT</h2>";
    html += "<form id='mqttForm'>";
    html += "<div class='form-group'>";
    html += "<label for='mqttServer'>Serveur MQTT:</label>";
    html += "<input type='text' id='mqttServer' name='mqttServer' value='" + String(mqtt_server) + "'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='mqttPort'>Port MQTT:</label>";
    html += "<input type='number' id='mqttPort' name='mqttPort' value='" + String(mqtt_port) + "'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='mqttUser'>Utilisateur MQTT:</label>";
    html += "<input type='text' id='mqttUser' name='mqttUser' value='" + String(mqtt_user) + "'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='mqttPassword'>Mot de passe MQTT:</label>";
    html += "<input type='text' id='mqttPassword' name='mqttPassword' value='" + String(mqtt_password) + "'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='mqttClientId'>ID Client MQTT:</label>";
    html += "<input type='text' id='mqttClientId' name='mqttClientId' value='" + String(mqtt_client_id) + "'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='mqttTopicSub'>Topic d'abonnement:</label>";
    html += "<input type='text' id='mqttTopicSub' name='mqttTopicSub' value='" + String(mqtt_topic_sub) + "'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label for='mqttTopicPub'>Topic de publication:</label>";
    html += "<input type='text' id='mqttTopicPub' name='mqttTopicPub' value='" + String(mqtt_topic_pub) + "'>";
    html += "</div>";
    html += "<button type='button' onclick='saveMqtt()'>Enregistrer</button>";
    html += "</form>";
    html += "</div>";

    // Onglet Calibration
    html += "<div id='calibration' class='tabcontent'>";
    html += "<h2>Calibration des volets</h2>";
    html += "<form id='calibrationForm'>";

    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        html += "<h3>" + String(shutterNames[i]) + "</h3>";
        html += "<div class='form-group'>";
        html += "<label for='travelTime" + String(i) + "'>Temps de parcours (secondes):</label>";
        html += "<input type='number' id='travelTime" + String(i) + "' name='travelTime" + String(i) + "' value='" + String(travelTimeSeconds[i]) + "' step='0.1'>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label for='upDownRatio" + String(i) + "'>Ratio montée/descente:</label>";
        html += "<input type='number' id='upDownRatio" + String(i) + "' name='upDownRatio" + String(i) + "' value='" + String(upDownRatio[i]) + "' step='0.01'>";
        html += "</div>";
    }

    html += "<button type='button' onclick='saveCalibration()'>Enregistrer</button>";
    html += "</form>";
    html += "</div>";

    // Lien vers la page d'accueil
    html += "<a href='/' class='home-link'>Retour à l'accueil</a>";

    // JavaScript pour les onglets et les formulaires
    html += "<script>";
    html += "function openTab(evt, tabName) {";
    html += "  var i, tabcontent, tablinks;";
    html += "  tabcontent = document.getElementsByClassName('tabcontent');";
    html += "  for (i = 0; i < tabcontent.length; i++) {";
    html += "    tabcontent[i].style.display = 'none';";
    html += "  }";
    html += "  tablinks = document.getElementsByClassName('tablinks');";
    html += "  for (i = 0; i < tablinks.length; i++) {";
    html += "    tablinks[i].className = tablinks[i].className.replace(' active', '');";
    html += "  }";
    html += "  document.getElementById(tabName).style.display = 'block';";
    html += "  evt.currentTarget.className += ' active';";
    html += "}";

    html += "function saveGeneral() {";
    html += "  const formData = new FormData(document.getElementById('generalForm'));";
    html += "  const data = {};";
    html += "  for (const [key, value] of formData.entries()) {";
    html += "    data[key] = value;";
    html += "  }";
    html += "  fetch('/api/saveconfig', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/json'},";
    html += "    body: JSON.stringify({type: 'general', data})";
    html += "  }).then(response => response.json())";
    html += "    .then(data => alert(data.message));";
    html += "}";

    html += "function saveMqtt() {";
    html += "  const formData = new FormData(document.getElementById('mqttForm'));";
    html += "  const data = {};";
    html += "  for (const [key, value] of formData.entries()) {";
    html += "    data[key] = value;";
    html += "  }";
    html += "  fetch('/api/saveconfig', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/json'},";
    html += "    body: JSON.stringify({type: 'mqtt', data})";
    html += "  }).then(response => response.json())";
    html += "    .then(data => alert(data.message));";
    html += "}";

    html += "function saveCalibration() {";
    html += "  const formData = new FormData(document.getElementById('calibrationForm'));";
    html += "  const data = {};";
    html += "  for (const [key, value] of formData.entries()) {";
    html += "    data[key] = value;";
    html += "  }";
    html += "  fetch('/api/calibration', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/json'},";
    html += "    body: JSON.stringify(data)";
    html += "  }).then(response => response.json())";
    html += "    .then(data => alert(data.message));";
    html += "}";

    html += "document.getElementById('defaultOpen').click();";
    html += "</script>";

    html += "</div></body></html>";

    server.send(200, "text/html", html);
}

// Gestionnaire pour sauvegarder la configuration
void handleSaveConfig()
{
    if (server.hasArg("plain"))
    {
        String body = server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (error)
        {
            server.send(400, "application/json", "{\"message\":\"Erreur de parsing JSON\"}");
            return;
        }

        String type = doc["type"];
        JsonObject data = doc["data"];

        if (type == "general")
        {
            // Sauvegarder les noms des volets
            for (int i = 0; i < NUM_SHUTTERS; i++)
            {
                String key = "shutterName" + String(i);
                if (data.containsKey(key))
                {
                    strlcpy(shutterNames[i], data[key], sizeof(shutterNames[i]));
                }
            }
        }
        else if (type == "mqtt")
        {
            // Sauvegarder la configuration MQTT
            if (data.containsKey("mqttServer"))
                strlcpy(mqtt_server, data["mqttServer"], sizeof(mqtt_server));
            if (data.containsKey("mqttPort"))
                mqtt_port = data["mqttPort"];
            if (data.containsKey("mqttUser"))
                strlcpy(mqtt_user, data["mqttUser"], sizeof(mqtt_user));
            if (data.containsKey("mqttPassword"))
                strlcpy(mqtt_password, data["mqttPassword"], sizeof(mqtt_password));
            if (data.containsKey("mqttClientId"))
                strlcpy(mqtt_client_id, data["mqttClientId"], sizeof(mqtt_client_id));
            if (data.containsKey("mqttTopicSub"))
                strlcpy(mqtt_topic_sub, data["mqttTopicSub"], sizeof(mqtt_topic_sub));
            if (data.containsKey("mqttTopicPub"))
                strlcpy(mqtt_topic_pub, data["mqttTopicPub"], sizeof(mqtt_topic_pub));

            // Reconfigurer MQTT
            mqttClient.disconnect();
            mqttClient.setServer(mqtt_server, mqtt_port);
        }

        // Sauvegarder la configuration dans l'EEPROM
        saveConfig();

        server.send(200, "application/json", "{\"message\":\"Configuration sauvegardée\"}");
    }
    else
    {
        server.send(400, "application/json", "{\"message\":\"Données manquantes\"}");
    }
}

// Gestionnaire pour la calibration
void handleCalibration()
{
    if (server.hasArg("plain"))
        String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, body);

    if (error)
    {
        server.send(400, "application/json", "{\"message\":\"Erreur de parsing JSON\"}");
        return;
    }

    // Mettre à jour les valeurs de calibration
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        String travelTimeKey = "travelTime" + String(i);
        String upDownRatioKey = "upDownRatio" + String(i);

        if (doc.containsKey(travelTimeKey))
        {
            travelTimeSeconds[i] = doc[travelTimeKey];
        }

        if (doc.containsKey(upDownRatioKey))
        {
            upDownRatio[i] = doc[upDownRatioKey];
        }
    }

    // Sauvegarder les données de calibration
    saveCalibrationData();

    server.send(200, "application/json", "{\"message\":\"Calibration sauvegardée\"}");
}
else
{
    server.send(400, "application/json", "{\"message\":\"Données manquantes\"}");
}
}

// Gestionnaire pour les pages non trouvées
void handleNotFound()
{
    server.send(404, "text/plain", "Page non trouvée");
}

// Reconnexion MQTT
void reconnectMqtt()
{
    // Essayer de se connecter au broker MQTT
    if (strlen(mqtt_server) == 0)
    {
        return; // Pas de serveur MQTT configuré
    }

    if (!mqttClient.connected())
    {
        Serial.print("Tentative de connexion MQTT...");

        bool connected = false;
        if (strlen(mqtt_user) > 0)
        {
            connected = mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password);
        }
        else
        {
            connected = mqttClient.connect(mqtt_client_id);
        }

        if (connected)
        {
            Serial.println("connecté");
            mqttClient.subscribe(mqtt_topic_sub);
        }
        else
        {
            Serial.print("échec, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" nouvelle tentative dans 5 secondes");
            delay(5000);
        }
    }
}

// Callback pour les messages MQTT reçus
void mqttCallback(char *topic, byte *payload, unsigned int length)
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
}

// Publier l'état actuel via MQTT
void publishState()
{
    if (!mqttClient.connected())
    {
        return;
    }

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

    mqttClient.publish(mqtt_topic_pub, output.c_str(), true);
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

// Charger la configuration depuis l'EEPROM
void loadConfig()
{
    // Vérifier si l'EEPROM a été initialisée
    if (EEPROM.read(0) == 0xFF)
    {
        // EEPROM non initialisée, utiliser les valeurs par défaut
        return;
    }

    int address = EEPROM_CONFIG_START;

    // Charger les noms des volets
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        for (int j = 0; j < 20; j++)
        {
            shutterNames[i][j] = EEPROM.read(address++);
        }
    }

    // Charger la configuration MQTT
    for (int i = 0; i < 40; i++)
    {
        mqtt_server[i] = EEPROM.read(address++);
    }

    EEPROM.get(address, mqtt_port);
    address += sizeof(mqtt_port);

    for (int i = 0; i < 20; i++)
    {
        mqtt_user[i] = EEPROM.read(address++);
    }

    for (int i = 0; i < 20; i++)
    {
        mqtt_password[i] = EEPROM.read(address++);
    }

    for (int i = 0; i < 20; i++)
    {
        mqtt_client_id[i] = EEPROM.read(address++);
    }

    for (int i = 0; i < 30; i++)
    {
        mqtt_topic_sub[i] = EEPROM.read(address++);
    }

    for (int i = 0; i < 30; i++)
    {
        mqtt_topic_pub[i] = EEPROM.read(address++);
    }
}

// Sauvegarder la configuration dans l'EEPROM
void saveConfig()
{
    int address = EEPROM_CONFIG_START;

    // Sauvegarder les noms des volets
    for (int i = 0; i < NUM_SHUTTERS; i++)
    {
        for (int j = 0; j < 20; j++)
        {
            EEPROM.write(address++, shutterNames[i][j]);
        }
    }

    // Sauvegarder la configuration MQTT
    for (int i = 0; i < 40; i++)
    {
        EEPROM.write(address++, mqtt_server[i]);
    }

    EEPROM.put(address, mqtt_port);
    address += sizeof(mqtt_port);

    for (int i = 0; i < 20; i++)
    {
        EEPROM.write(address++, mqtt_user[i]);
    }

    for (int i = 0; i < 20; i++)
    {
        EEPROM.write(address++, mqtt_password[i]);
    }

    for (int i = 0; i < 20; i++)
    {
        EEPROM.write(address++, mqtt_client_id[i]);
    }

    for (int i = 0; i < 30; i++)
    {
        EEPROM.write(address++, mqtt_topic_sub[i]);
    }

    for (int i = 0; i < 30; i++)
    {
        EEPROM.write(address++, mqtt_topic_pub[i]);
    }

    EEPROM.commit();
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

// Obtenir le bouton de direction opposée
int getOppositeButton(int buttonIndex)
{
    return buttonIndex < DIR_DOWN ? (buttonIndex + DIR_DOWN) : (buttonIndex - DIR_DOWN);
}
{