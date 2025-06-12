/*
 * Test de communication I2C pour modules HW-171 (PCF8574)
 *
 * Ce programme permet de tester la communication avec les modules I2C
 * et de vérifier le fonctionnement des relais.
 *
 * Instructions:
 * - Connectez les modules HW-171 comme indiqué dans le README
 * - Téléversez ce programme sur l'ESP8266
 * - Ouvrez le moniteur série (115200 bauds)
 * - Suivez les instructions à l'écran
 */

#include <Wire.h>
#include <ESP8266WiFi.h>

// Adresses I2C des modules PCF8574
const uint8_t PCF8574_ADDRESSES[] = {0x20, 0x21};
const int NUM_MODULES = 2;

// Fonction pour scanner les périphériques I2C
void scanI2CDevices()
{
    byte error, address;
    int deviceCount = 0;

    Serial.println("Scan des périphériques I2C...");

    for (address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0)
        {
            Serial.print("Périphérique I2C trouvé à l'adresse 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.print(address, HEX);
            Serial.print(" (");

            // Identifier les modules PCF8574 connus
            bool isKnown = false;
            for (int i = 0; i < NUM_MODULES; i++)
            {
                if (address == PCF8574_ADDRESSES[i])
                {
                    Serial.print("Module PCF8574 #");
                    Serial.print(i + 1);
                    isKnown = true;
                    break;
                }
            }

            if (!isKnown)
            {
                Serial.print("Inconnu");
            }

            Serial.println(")");
            deviceCount++;
        }
        else if (error == 4)
        {
            Serial.print("Erreur à l'adresse 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.println(address, HEX);
        }
    }

    if (deviceCount == 0)
    {
        Serial.println("Aucun périphérique I2C trouvé!");
    }
    else
    {
        Serial.print(deviceCount);
        Serial.println(" périphérique(s) trouvé(s).");
    }
}

// Fonction pour écrire une valeur sur un module PCF8574
bool writeToPCF8574(uint8_t address, uint8_t value)
{
    Wire.beginTransmission(address);
    Wire.write(value);
    byte error = Wire.endTransmission();

    if (error == 0)
    {
        return true;
    }
    else
    {
        Serial.print("Erreur d'écriture sur le module à l'adresse 0x");
        Serial.print(address, HEX);
        Serial.print(": ");
        Serial.println(error);
        return false;
    }
}

// Fonction pour tester tous les relais
void testAllRelays()
{
    Serial.println("Test de tous les relais...");

    // Désactiver tous les relais d'abord (HIGH = inactif)
    for (int module = 0; module < NUM_MODULES; module++)
    {
        writeToPCF8574(PCF8574_ADDRESSES[module], 0xFF);
        delay(100);
    }

    // Tester chaque relais individuellement
    for (int module = 0; module < NUM_MODULES; module++)
    {
        Serial.print("Test du module #");
        Serial.print(module + 1);
        Serial.print(" (0x");
        Serial.print(PCF8574_ADDRESSES[module], HEX);
        Serial.println(")");

        for (int pin = 0; pin < 8; pin++)
        {
            Serial.print("  Activation du relais ");
            Serial.print(module * 8 + pin);
            Serial.println("...");

            // Activer le relais (LOW = actif)
            uint8_t value = 0xFF & ~(1 << pin);
            writeToPCF8574(PCF8574_ADDRESSES[module], value);

            delay(1000);

            // Désactiver le relais
            writeToPCF8574(PCF8574_ADDRESSES[module], 0xFF);

            delay(500);
        }
    }

    Serial.println("Test des relais terminé.");
}

// Fonction pour tester un relais spécifique
void testSpecificRelay(int moduleIndex, int pinIndex)
{
    if (moduleIndex < 0 || moduleIndex >= NUM_MODULES || pinIndex < 0 || pinIndex >= 8)
    {
        Serial.println("Module ou pin invalide!");
        return;
    }

    uint8_t address = PCF8574_ADDRESSES[moduleIndex];

    Serial.print("Test du relais sur le module #");
    Serial.print(moduleIndex + 1);
    Serial.print(" (0x");
    Serial.print(address, HEX);
    Serial.print("), pin ");
    Serial.println(pinIndex);

    // Désactiver tous les relais d'abord
    writeToPCF8574(address, 0xFF);
    delay(100);

    // Activer le relais spécifié
    uint8_t value = 0xFF & ~(1 << pinIndex);
    writeToPCF8574(address, value);

    Serial.println("Relais activé pendant 3 secondes...");
    delay(3000);

    // Désactiver le relais
    writeToPCF8574(address, 0xFF);
    Serial.println("Relais désactivé.");
}

void setup()
{
    // Initialisation de la communication série
    Serial.begin(115200);
    Serial.println("\n\nTest des modules I2C GPIO HW-171 (PCF8574)");

    // Afficher les informations sur l'ESP8266
    Serial.print("ESP8266 Chip ID: ");
    Serial.println(ESP.getChipId(), HEX);
    Serial.print("ESP8266 Flash Chip ID: ");
    Serial.println(ESP.getFlashChipId(), HEX);

    // Initialisation I2C
    Wire.begin(D2, D1); // SDA, SCL
    Serial.println("I2C initialisé sur les pins D2 (SDA) et D1 (SCL)");

    // Scanner les périphériques I2C
    scanI2CDevices();

    // Menu d'instructions
    Serial.println("\nCommandes disponibles:");
    Serial.println("s - Scanner les périphériques I2C");
    Serial.println("t - Tester tous les relais");
    Serial.println("r - Tester un relais spécifique");
    Serial.println("h - Afficher ce menu d'aide");
}

void loop()
{
    if (Serial.available() > 0)
    {
        char cmd = Serial.read();

        switch (cmd)
        {
        case 's':
            scanI2CDevices();
            break;

        case 't':
            testAllRelays();
            break;

        case 'r':
            Serial.println("Entrez le numéro du module (0 ou 1):");
            while (!Serial.available())
                delay(10);
            int moduleIndex = Serial.parseInt();
            Serial.println(moduleIndex);

            Serial.println("Entrez le numéro du pin (0-7):");
            while (!Serial.available())
                delay(10);
            int pinIndex = Serial.parseInt();
            Serial.println(pinIndex);

            testSpecificRelay(moduleIndex, pinIndex);
            break;

        case 'h':
            Serial.println("\nCommandes disponibles:");
            Serial.println("s - Scanner les périphériques I2C");
            Serial.println("t - Tester tous les relais");
            Serial.println("r - Tester un relais spécifique");
            Serial.println("h - Afficher ce menu d'aide");
            break;

        case '\n':
        case '\r':
            // Ignorer les caractères de nouvelle ligne
            break;

        default:
            Serial.println("Commande inconnue. Tapez 'h' pour l'aide.");
            break;
        }

        // Vider le buffer série
        while (Serial.available())
            Serial.read();
    }
}