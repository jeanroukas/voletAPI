# Intégration avec Google Home

Ce document explique comment intégrer votre système de contrôle de volets avec Google Home en utilisant Node-RED et le plugin node-red-contrib-google-smarthome.

## Prérequis

1. Un broker MQTT (comme Mosquitto) installé et fonctionnel
2. Node-RED installé
3. Un compte Google et accès à Google Actions Console
4. Un serveur accessible depuis Internet (ou utiliser un service comme ngrok pour exposer temporairement votre serveur local)

## Installation des composants nécessaires

### 1. Installation de Node-RED

Si vous n'avez pas encore installé Node-RED, suivez les instructions sur [le site officiel de Node-RED](https://nodered.org/docs/getting-started/installation).

### 2. Installation des plugins Node-RED nécessaires

Dans Node-RED, installez les plugins suivants via le menu "Manage palette":

- `node-red-contrib-mqtt-broker` (si vous n'avez pas déjà un broker MQTT)
- `node-red-contrib-google-smarthome`
- `node-red-dashboard` (optionnel, pour une interface utilisateur web)

## Configuration du broker MQTT

Si vous utilisez Mosquitto, assurez-vous qu'il est configuré pour accepter les connexions de votre ESP8266 et de Node-RED.

Exemple de configuration Mosquitto (`mosquitto.conf`):

```
listener 1883
allow_anonymous true
```

Pour une configuration plus sécurisée, vous devriez configurer l'authentification.

## Configuration de Node-RED

### 1. Configuration du nœud MQTT

1. Ajoutez un nœud "mqtt in" à votre flow
2. Configurez-le pour se connecter à votre broker MQTT
3. Abonnez-vous au topic `home/volets/state`

### 2. Configuration du nœud Google Smart Home

1. Ajoutez un nœud "google-smarthome-in" à votre flow
2. Configurez-le avec les paramètres suivants:
   - Client ID: un identifiant unique pour votre projet
   - Client Secret: un secret généré pour votre projet
   - Authorization URI: laissez la valeur par défaut
   - Token URI: laissez la valeur par défaut
   - Refresh Token: sera généré lors de l'authentification
   - JWT Key: sera généré lors de l'authentification

### 3. Configuration des appareils

Créez un flow qui définit vos volets comme des appareils Google Home. Voici un exemple de configuration JSON pour vos volets:

```json
[
  {
    "id": "volet_0",
    "type": "action.devices.types.BLINDS",
    "traits": [
      "action.devices.traits.OpenClose"
    ],
    "name": {
      "name": "Volet Salon"
    },
    "willReportState": true,
    "roomHint": "Salon",
    "attributes": {
      "openDirection": ["UP", "DOWN"]
    }
  },
  {
    "id": "volet_1",
    "type": "action.devices.types.BLINDS",
    "traits": [
      "action.devices.traits.OpenClose"
    ],
    "name": {
      "name": "Volet Cuisine"
    },
    "willReportState": true,
    "roomHint": "Cuisine",
    "attributes": {
      "openDirection": ["UP", "DOWN"]
    }
  },
  // Répétez pour les autres volets...
]
```

## Exemple de flow Node-RED

Voici un exemple de flow Node-RED pour gérer vos volets:

```json
[
    {
        "id": "mqtt-in",
        "type": "mqtt in",
        "z": "flow-id",
        "name": "État des volets",
        "topic": "home/volets/state",
        "qos": "2",
        "datatype": "json",
        "broker": "broker-id",
        "x": 150,
        "y": 100,
        "wires": [
            [
                "process-state"
            ]
        ]
    },
    {
        "id": "process-state",
        "type": "function",
        "z": "flow-id",
        "name": "Traiter l'état",
        "func": "// Convertir l'état MQTT en format Google Home\nconst mqttState = msg.payload;\nconst googleState = {};\n\nfor (const shutter of mqttState.shutters) {\n    googleState[`volet_${shutter.id}`] = {\n        online: true,\n        openPercent: Math.round(shutter.position * 100)\n    };\n}\n\nmsg.payload = googleState;\nreturn msg;",
        "outputs": 1,
        "noerr": 0,
        "x": 350,
        "y": 100,
        "wires": [
            [
                "google-state"
            ]
        ]
    },
    {
        "id": "google-state",
        "type": "google-smarthome-state",
        "z": "flow-id",
        "name": "Mettre à jour l'état",
        "client": "google-client-id",
        "x": 550,
        "y": 100,
        "wires": []
    },
    {
        "id": "google-command",
        "type": "google-smarthome-command",
        "z": "flow-id",
        "name": "Commandes Google",
        "client": "google-client-id",
        "x": 150,
        "y": 200,
        "wires": [
            [
                "process-command"
            ]
        ]
    },
    {
        "id": "process-command",
        "type": "function",
        "z": "flow-id",
        "name": "Traiter la commande",
        "func": "// Convertir la commande Google en commande MQTT\nconst cmd = msg.payload;\nconst output = [];\n\nfor (const device of cmd.devices) {\n    const deviceId = device.id;\n    const shutterIndex = deviceId.split('_')[1];\n    \n    for (const execution of device.execution) {\n        if (execution.command === 'action.devices.commands.OpenClose') {\n            const openPercent = execution.params.openPercent;\n            \n            // Créer un message MQTT pour cette commande\n            if (openPercent === 0) {\n                output.push({\n                    topic: `home/volets/cmd/${shutterIndex}`,\n                    payload: 'CLOSE'\n                });\n            } else if (openPercent === 100) {\n                output.push({\n                    topic: `home/volets/cmd/${shutterIndex}`,\n                    payload: 'OPEN'\n                });\n            } else {\n                output.push({\n                    topic: `home/volets/cmd/${shutterIndex}`,\n                    payload: `POSITION:${openPercent/100}`\n                });\n            }\n        }\n    }\n}\n\nreturn output;",
        "outputs": 1,
        "noerr": 0,
        "x": 350,
        "y": 200,
        "wires": [
            [
                "mqtt-out"
            ]
        ]
    },
    {
        "id": "mqtt-out",
        "type": "mqtt out",
        "z": "flow-id",
        "name": "Commandes aux volets",
        "topic": "",
        "qos": "1",
        "retain": "false",
        "broker": "broker-id",
        "x": 550,
        "y": 200,
        "wires": []
    }
]
```

## Configuration de Google Actions Console

1. Allez sur [Google Actions Console](https://console.actions.google.com/)
2. Créez un nouveau projet
3. Sélectionnez "Smart Home" comme type de projet
4. Configurez les informations de base du projet (nom, description, etc.)
5. Dans la section "Actions", configurez l'URL de fulfillment avec l'URL de votre serveur Node-RED suivi de `/api/smarthome`
   - Par exemple: `https://votre-serveur.com/api/smarthome`
6. Dans la section "Account linking", configurez les paramètres d'authentification:
   - Client ID: le même que dans Node-RED
   - Client secret: le même que dans Node-RED
   - Authorization URL: URL de votre serveur Node-RED suivi de `/oauth/auth`
   - Token URL: URL de votre serveur Node-RED suivi de `/oauth/token`
7. Testez votre configuration en utilisant l'application Google Home sur votre smartphone

## Test et dépannage

1. Assurez-vous que votre ESP8266 est connecté au broker MQTT
2. Vérifiez que Node-RED reçoit les messages d'état des volets
3. Testez les commandes en envoyant manuellement des messages MQTT
4. Utilisez l'application Google Home pour tester les commandes vocales

### Commandes vocales typiques

- "Ok Google, ouvre le volet du salon"
- "Ok Google, ferme tous les volets"
- "Ok Google, ouvre le volet de la cuisine à 50%"

## Sécurité

Pour une installation en production, assurez-vous de:

1. Sécuriser votre broker MQTT avec authentification
2. Utiliser HTTPS pour votre serveur Node-RED
3. Configurer correctement les pare-feu pour n'exposer que les ports nécessaires
4. Mettre à jour régulièrement tous les composants logiciels

## Ressources supplémentaires

- [Documentation officielle de node-red-contrib-google-smarthome](https://flows.nodered.org/node/node-red-contrib-google-smarthome)
- [Documentation Google Smart Home](https://developers.google.com/assistant/smarthome/overview)
- [Documentation MQTT](https://mqtt.org/documentation/)