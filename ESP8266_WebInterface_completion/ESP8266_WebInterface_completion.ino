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