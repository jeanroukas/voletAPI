# Guide de dépannage - Contrôle de volets avec ESP8266 et Google Home

Ce guide vous aidera à résoudre les problèmes courants que vous pourriez rencontrer lors de la mise en place de votre système de contrôle de volets.

## Problèmes de compilation

### Erreurs "cannot open source file"

**Problème**: Lors de la compilation, vous obtenez des erreurs comme:
```
cannot open source file "ESP8266WiFi.h"
cannot open source file "PubSubClient.h"
cannot open source file "ArduinoJson.h"
cannot open source file "PCF8574.h"
```

**Solution**:
1. Assurez-vous d'avoir installé le support ESP8266 dans Arduino IDE:
   - Ouvrez Arduino IDE
   - Allez dans Fichier > Préférences
   - Dans "URL de gestionnaire de cartes supplémentaires", ajoutez:
     `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Allez dans Outils > Type de carte > Gestionnaire de cartes
   - Recherchez "esp8266" et installez "ESP8266 by ESP8266 Community"

2. Installez les bibliothèques requises:
   - Allez dans Croquis > Inclure une bibliothèque > Gérer les bibliothèques
   - Recherchez et installez:
     - PubSubClient
     - ArduinoJson
     - PCF8574 (par Renzo Mischianti)

3. Sélectionnez la bonne carte:
   - Allez dans Outils > Type de carte > ESP8266 Boards
   - Sélectionnez "NodeMCU 1.0 (ESP-12E Module)"

### Erreurs de mémoire

**Problème**: Erreur "Not enough memory" lors de la compilation.

**Solution**:
1. Réduisez la taille des buffers JSON ou MQTT
2. Optimisez votre code pour utiliser moins de variables globales
3. Dans Outils > Flash Size, sélectionnez "4MB (FS:1MB OTA:~1019KB)"

## Problèmes matériels

### Les modules I2C ne sont pas détectés

**Problème**: Le programme ne détecte pas les modules I2C HW-171.

**Solutions**:
1. **Vérifiez les connexions**:
   - Assurez-vous que SDA est connecté à D2 (GPIO 4)
   - Assurez-vous que SCL est connecté à D1 (GPIO 5)
   - Vérifiez que VCC et GND sont correctement connectés

2. **Vérifiez l'alimentation**:
   - Mesurez la tension aux bornes VCC et GND des modules (doit être ~3.3V)
   - Essayez d'alimenter les modules avec une source externe 3.3V

3. **Vérifiez les adresses I2C**:
   - Utilisez le programme `ESP8266_I2C_Test.ino` pour scanner les adresses I2C
   - Vérifiez que les cavaliers A0, A1, A2 sont correctement configurés:
     - Module 1: A0=0, A1=0, A2=0 (adresse 0x20)
     - Module 2: A0=1, A1=0, A2=0 (adresse 0x21)

4. **Testez avec un seul module**:
   - Déconnectez tous les modules sauf un
   - Testez avec le programme de test
   - Ajoutez les modules un par un

5. **Vérifiez les résistances de pull-up**:
   - Les lignes I2C nécessitent des résistances de pull-up
   - L'ESP8266 a des résistances internes, mais elles peuvent être insuffisantes
   - Essayez d'ajouter des résistances de pull-up externes (4.7kΩ) sur SDA et SCL

### Les relais ne s'activent pas

**Problème**: Les relais ne s'activent pas lorsque commandés.

**Solutions**:
1. **Vérifiez les connexions**:
   - Assurez-vous que les sorties des modules HW-171 sont correctement connectées aux relais
   - Vérifiez que les relais ont une alimentation suffisante

2. **Vérifiez la logique d'activation**:
   - La plupart des relais s'activent à l'état bas (LOW)
   - Vérifiez si vos relais s'activent à l'état haut (HIGH) ou bas (LOW)
   - Modifiez le code en conséquence

3. **Testez les relais individuellement**:
   - Utilisez le programme `ESP8266_I2C_Test.ino` pour tester chaque relais
   - Vérifiez que vous entendez le "clic" du relais qui s'active

4. **Vérifiez l'alimentation des relais**:
   - Les relais peuvent nécessiter plus de courant que ce que les modules I2C peuvent fournir
   - Utilisez une alimentation externe pour les relais
   - Assurez-vous que les masses (GND) sont communes

## Problèmes de connexion WiFi

**Problème**: L'ESP8266 ne se connecte pas au WiFi.

**Solutions**:
1. **Vérifiez les identifiants WiFi**:
   - Assurez-vous que le SSID et le mot de passe sont corrects
   - Attention aux majuscules/minuscules

2. **Vérifiez la portée du signal**:
   - Assurez-vous que l'ESP8266 est à portée de votre routeur WiFi
   - Essayez de rapprocher l'ESP8266 du routeur pour les tests

3. **Vérifiez la bande WiFi**:
   - L'ESP8266 ne fonctionne qu'en 2.4 GHz, pas en 5 GHz
   - Assurez-vous que votre routeur diffuse en 2.4 GHz

4. **Ajoutez des informations de débogage**:
   - Modifiez le code pour afficher plus d'informations sur la connexion WiFi
   - Utilisez le moniteur série pour voir les messages d'erreur

## Problèmes de connexion MQTT

**Problème**: L'ESP8266 ne se connecte pas au broker MQTT.

**Solutions**:
1. **Vérifiez les paramètres du broker**:
   - Assurez-vous que l'adresse IP/hostname du broker est correcte
   - Vérifiez que le port est correct (généralement 1883)
   - Vérifiez les identifiants si l'authentification est activée

2. **Vérifiez que le broker est accessible**:
   - Essayez de vous connecter au broker depuis un autre appareil
   - Vérifiez les pare-feu qui pourraient bloquer la connexion

3. **Vérifiez l'ID client MQTT**:
   - Assurez-vous que l'ID client est unique
   - Évitez les caractères spéciaux dans l'ID client

4. **Testez avec un broker public**:
   - Essayez de vous connecter à un broker MQTT public pour tester
   - Par exemple: `test.mosquitto.org` sur le port 1883

## Problèmes d'intégration avec Google Home

**Problème**: Les volets n'apparaissent pas dans Google Home.

**Solutions**:
1. **Vérifiez la configuration de Node-RED**:
   - Assurez-vous que le plugin `node-red-contrib-google-smarthome` est correctement configuré
   - Vérifiez que les appareils sont correctement définis

2. **Vérifiez la configuration de Google Actions Console**:
   - Assurez-vous que le projet est correctement configuré
   - Vérifiez que l'URL de fulfillment pointe vers votre serveur Node-RED

3. **Vérifiez la communication MQTT**:
   - Assurez-vous que Node-RED reçoit les messages d'état des volets
   - Vérifiez que les commandes sont correctement envoyées à l'ESP8266

4. **Redémarrez le processus de découverte**:
   - Dans l'application Google Home, essayez de redécouvrir les appareils
   - Dites "Ok Google, synchronise mes appareils"

## Problèmes de suivi de position

**Problème**: Les positions des volets ne sont pas correctement suivies.

**Solutions**:
1. **Calibrez les temps de parcours**:
   - Mesurez précisément le temps nécessaire pour ouvrir/fermer complètement chaque volet
   - Mettez à jour les valeurs `travelTimeSeconds` dans le code

2. **Ajustez les ratios montée/descente**:
   - Si les volets montent plus vite qu'ils ne descendent (ou vice versa)
   - Ajustez les valeurs `upDownRatio` dans le code

3. **Ajoutez des capteurs de fin de course**:
   - Pour une position plus précise, envisagez d'ajouter des capteurs de fin de course
   - Modifiez le code pour prendre en compte ces capteurs

## Problèmes de sauvegarde EEPROM

**Problème**: Les positions ne sont pas sauvegardées ou restaurées correctement.

**Solutions**:
1. **Vérifiez les appels à EEPROM.commit()**:
   - Sur ESP8266, vous devez appeler `EEPROM.commit()` après `EEPROM.put()`
   - Vérifiez que cette ligne est présente dans les fonctions de sauvegarde

2. **Initialisez l'EEPROM avec la bonne taille**:
   - Assurez-vous que `EEPROM.begin(512)` est appelé avec une taille suffisante

3. **Vérifiez les adresses EEPROM**:
   - Assurez-vous qu'il n'y a pas de chevauchement entre les différentes données
   - Vérifiez que les adresses ne dépassent pas la taille allouée

## Problèmes de performance

**Problème**: Le système est lent ou ne répond pas.

**Solutions**:
1. **Optimisez les délais**:
   - Réduisez ou éliminez les appels à `delay()` dans le code
   - Utilisez des techniques non bloquantes pour les temporisations

2. **Réduisez la fréquence des publications MQTT**:
   - Ne publiez l'état que lorsqu'il change
   - Limitez la fréquence des publications (pas plus d'une fois par seconde)

3. **Optimisez la taille des messages JSON**:
   - Réduisez la taille des messages JSON en n'incluant que les données nécessaires
   - Utilisez des noms de champs courts

4. **Vérifiez la stabilité du WiFi**:
   - Un signal WiFi faible peut causer des problèmes de performance
   - Essayez d'améliorer la position de l'ESP8266 ou utilisez une antenne externe

## Ressources supplémentaires

- [Documentation ESP8266](https://arduino-esp8266.readthedocs.io/)
- [Documentation PCF8574](https://github.com/xreef/PCF8574_library)
- [Documentation PubSubClient](https://pubsubclient.knolleary.net/)
- [Forum ESP8266](https://www.esp8266.com/viewforum.php?f=32)