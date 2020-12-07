// NOM DU FICHIER : main.cpp
// AUTEUR : Raphaël  Desjardins
// DATE DE CREATION : 2020/11/05
// OBJET : Programme du ESP32 pour la gestion de l'aquarium.

#include <Arduino.h>
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_SPIDevice.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiManager wm;
AsyncWebServer server(80);

//SSID et mot de passe du réseau ESP32
const char *ssid = "ESP32";
const char *password = "Patate123";

//Constantes des variables enregistrés dans le SPIFF.
const char *PARAM_NOM = "inputNom";     
const char *PARAM_TEMPERATURE = "inputTemperature";
const char *PARAM_FREQUENCEPOMPE = "inputFrequencePompe";
const char *PARAM_TEMPSPOMPE = "inputTempsPompe";
bool pompe = false;

String jsonresponse;
StaticJsonDocument<200> doc;

const int oneWireBus = 4;   //La pin GPIO de connexion.
const int LED = 2;          //La PIN GPIO de la LED.
const int pinPompe = 26;    //La PIN GPIO de la pompe.
bool verifIntervalle;       //Variable qui permet de vérifier lorsque la température atteint le maximum et le minimum de l'intervalle.

float timerFrequence = 0;   //Timer pour la fréquence de la pompe.
float timerTemps = 0;       //Timer pour le temps d'action de la pompe.

// Initialisation d'une instance onewire
OneWire oneWire(oneWireBus);
// Passer la référence au sensor Dallas Temperature
DallasTemperature sensors(&oneWire);

//Page web HTML.
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
    
<head>
    <title>Serveur ESP32</title>
    <meta name="viewport" content="width=device-width, initial-scale=1"charset="UTF-8" />
    <script>
      function submitMessage() {
        setTimeout(function(){ document.location.reload(false); }, 500);   
      }
    </script>
</head>

<body>

    <div>
        <h1>Configuration de l'ESP32</h1>
    </div>

    <div style="margin:auto;display:flex;justify-content: space-between;flex-flow: row wrap;">
        <form action="/get" target="hidden-form" style="border: 3px groove;padding: 0 20px 20px 20px;width: 350px;margin-bottom:20px;">
            <h2>Nom de l'aquarium</h2>
            <br>
            Nom : <input type="text" value="%inputNom%" name="inputNom">
            <br><br><br>
            <input type="submit" value="Appliquer" onclick="submitMessage()">
        </form>

        <form action="/get" target="hidden-form" style="border: 3px groove;padding: 0 20px 20px 20px;width: 350px;margin-bottom:20px;">
            <h2>Température cible</h2>
            <br>
            Température (actuelle : %inputTemperature% °C): 
              <select name="inputTemperature">
                <option value="20">20°C</option>
                <option value="21">21°C</option>
                <option value="22">22°C</option>
                <option value="23">23°C</option>
                <option value="24">24°C</option>
                <option value="25">25°C</option>
                <option value="26">26°C</option>
                <option value="27">27°C</option>
                <option value="28">28°C</option>
                <option value="29">29°C</option>
                <option value="30">30°C</option>
              </select>
            <br><br><br>
            <input type="submit" value="Appliquer" onclick="submitMessage()">
        </form>

        <form action="/get" target="hidden-form" style="border: 3px groove;padding: 0 20px 20px 20px;width: 350px;margin-bottom:20px;">
            <h2>Fréquence de la pompe</h2>
            <br>
            Fréquence d'activation de la pompe (en minutes) : <br><input type="text" value="%inputFrequencePompe%" name="inputFrequencePompe">
            <br><br>
            <input type="submit" value="Appliquer" onclick="submitMessage()">
        </form>

        <form action="/get" target="hidden-form" style="border: 3px groove;padding: 0 20px 20px 20px;width: 350px;margin-bottom:20px;">
            <h2>Temps d'action de la pompe</h2>
            <br>
            Temps que la pompe reste activée (en minutes) : <input type="text" value="%inputTempsPompe%" name="inputTempsPompe">
            <br><br><br>
            <input type="submit" value="Appliquer" onclick="submitMessage()">
        </form>

    </div>
    <iframe style="display:none" name="hidden-form"></iframe>
</body>

</html>)rawliteral";

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

//Fonction qui sert à lire la valeur enregistré dans le fichier texte.
String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory())
  {
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while (file.available())
  {
    fileContent += String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

//Fonction qui sert à enregistrer une valeur dans un fichier texte.
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- write failed");
  }
}

//Fonction qui sert à remplacer les valeurs par défaut par les valeurs enregistrés.
String processor(const String &var)
{
  //Serial.println(var);
  if (var == "inputNom")
  {
    return readFile(SPIFFS, "/inputNom.txt");
  }
  else if (var == "inputTemperature")
  {
    return readFile(SPIFFS, "/inputTemperature.txt");
  }
  else if (var == "inputFrequencePompe")
  {
    return readFile(SPIFFS, "/inputFrequencePompe.txt");
  }
  else if (var == "inputTempsPompe")
  {
    return readFile(SPIFFS, "/inputTempsPompe.txt");
  }
  return String();
}

// Fonction qui permet d'aller chercher une température avec le senseur.
float obtenirTemperatureActuelle()
{
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

//Fonction qui permet d'aller chercher la température cible.
float obtenirTemperatureCible()
{
  return readFile(SPIFFS, "/inputTemperature.txt").toFloat();
}

//Fonction qui permet d'aller chercher le nom de l'aquarium.
String obtenirNom()
{
  return readFile(SPIFFS, "/inputNom.txt");
}

//Fonction qui permet d'aller chercher la fréquence de la pompe.
float obtenirFrequencePompe()
{
  float frequence = readFile(SPIFFS, "/inputFrequencePompe.txt").toFloat();
  return (frequence * 60/1.5);
}

//Fonction qui permet d'aller chercher le temps d'action de la pompe.
float obtenirTempsPompe()
{
  float temps = readFile(SPIFFS, "/inputTempsPompe.txt").toFloat();
  return (temps * 60/1.5);
}

//Fonction qui sert à afficher les informations de connexion pour débuter aevc l'ESP32.
void afficherConfigWifi()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  display.println("Configuration du Wifi ");
  display.setCursor(0, 20);
  display.println("Nom : ESP32");
  display.setCursor(0, 35);
  display.println("Mot de passe :");
  display.setCursor(0, 50);
  display.println("Patate123");
  display.display();
}

//Fonciton qui sert à afficher les informations pour le contrôle de l'aquarium.
void afficherOLED()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  //Affichage du nom.
  display.setCursor(0, 5);
  display.println(obtenirNom());

  //Affichage de la temperature.
  display.setCursor(0, 20);
  display.print("Temp cible: ");
  display.println(obtenirTemperatureCible());
  display.setCursor(0, 32);
  display.print("Temp actuelle: ");
  display.println(obtenirTemperatureActuelle());

  //Pompe ON/OFF
  display.setCursor(0, 44);
  if (pompe)
    display.println("Pompe : ON");
  else
    display.println("Pompe : OFF");

  //Affichage de l'ip.
  display.setCursor(0, 57);
  display.println("IP : " + WiFi.localIP().toString());

  display.display();
}

// Vérifie si la température est dans l'intervalle permise.
void verifierTemperature()
{
  if (obtenirTemperatureActuelle() <= obtenirTemperatureCible() - 1)
  {
    verifIntervalle = true;
  }

  if (obtenirTemperatureActuelle() <= obtenirTemperatureCible() && verifIntervalle)
  {
    digitalWrite(LED, HIGH);
  }
  else
  {
    digitalWrite(LED, LOW);
    verifIntervalle = false;
  }
}

//Fonction qui sert à activer et désactiver la pompe.
void verifierPompe()
{
  //Quand il est fermé, on descend le timer fréquence.
  if (digitalRead(pinPompe) == HIGH)
  {
    timerFrequence--;
    if (timerFrequence <= 0 && timerTemps != 0)
    {
      digitalWrite(pinPompe, LOW);
      pompe = true;
      timerFrequence = obtenirFrequencePompe();
      timerTemps = obtenirTempsPompe(); 
    }
  }
  //Quand il est ouvert, on descend le timer temps.
  else if (digitalRead(pinPompe) == LOW)
  {
    timerTemps--;
    if (timerTemps <= 0)
    {
      digitalWrite(pinPompe, HIGH);
      pompe = false;
      timerTemps = obtenirTempsPompe();  
      timerFrequence = obtenirFrequencePompe();
    }
  }
}

void setup()
{
  Serial.begin(9600);
  pinMode(LED, OUTPUT);         //Définition de la LED en OUTPUT
  pinMode(pinPompe, OUTPUT);    //Définition de la pompe en OUTPUT
  digitalWrite(pinPompe, HIGH);  

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  writeFile(SPIFFS, "/inputUsername.txt", "admin");
  writeFile(SPIFFS, "/inputPassword.txt", "admin");
  timerFrequence = obtenirFrequencePompe();
  timerTemps= obtenirTempsPompe();

  //Affichage de la config Wifi.
  afficherConfigWifi();

  //WifiManager
  WiFi.mode(WIFI_STA);
  if (!wm.autoConnect(ssid, password))
    Serial.println("Erreur de connexion.");
  else
    Serial.println("Connexion etablie!");
  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  //Envoi de la page web avec les valeurs des champs.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  //Requêtes GET.
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    String inputMessage;
    // GET inputNom
    if (request->hasParam(PARAM_NOM))
    {
      inputMessage = request->getParam(PARAM_NOM)->value();
      writeFile(SPIFFS, "/inputNom.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputTemperature=<inputMessage>
    else if (request->hasParam(PARAM_TEMPERATURE))
    {
      inputMessage = request->getParam(PARAM_TEMPERATURE)->value();
      writeFile(SPIFFS, "/inputTemperature.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputFrequencePompe=<inputMessage>
    else if (request->hasParam(PARAM_FREQUENCEPOMPE))
    {
      inputMessage = request->getParam(PARAM_FREQUENCEPOMPE)->value();
      writeFile(SPIFFS, "/inputFrequencePompe.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputTempsPompe=<inputMessage>
    else if (request->hasParam(PARAM_TEMPSPOMPE))
    {
      inputMessage = request->getParam(PARAM_TEMPSPOMPE)->value();
      writeFile(SPIFFS, "/inputTempsPompe.txt", inputMessage.c_str());
    }
    else
    {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/text", inputMessage);
  });

  //Route de l'API qui sert à obtenir le nom de l'aquarium.
  server.on("/obtenirNomAquarium", HTTP_GET, [](AsyncWebServerRequest *request) {
    jsonresponse = "";
    doc.clear();
    if (request->authenticate(readFile(SPIFFS, "/inputUsername.txt").c_str(), readFile(SPIFFS, "/inputPassword.txt").c_str()))
    {
      doc["nom"] = readFile(SPIFFS, "/inputNom.txt");
      serializeJsonPretty(doc, jsonresponse);
      request->send(200, "application/json", jsonresponse);
    }
    else
    {
      doc["message"] = "Non autorisé";
      serializeJsonPretty(doc, jsonresponse);
      request->send(401, "application/json", jsonresponse);
    }
  });

  //Route de l'API qui sert à obtenir la température cible de l'aquarium.
  server.on("/obtenirTemperatureCible", HTTP_GET, [](AsyncWebServerRequest *request) {
    jsonresponse = "";
    doc.clear();
    if (request->authenticate(readFile(SPIFFS, "/inputUsername.txt").c_str(), readFile(SPIFFS, "/inputPassword.txt").c_str()))
    {
      doc["temperature"] = readFile(SPIFFS, "/inputTemperature.txt");
      serializeJsonPretty(doc, jsonresponse);
      request->send(200, "application/json", jsonresponse);
    }
    else
    {
      doc["message"] = "Non autorisé";
      serializeJsonPretty(doc, jsonresponse);
      request->send(401, "application/json", jsonresponse);
    }
  });

  //Route de l'API qui sert à obtenir la température actuelle de l'aquarium.
  server.on("/obtenirTemperatureActuelle", HTTP_GET, [](AsyncWebServerRequest *request) {
    jsonresponse = "";
    doc.clear();
    if (request->authenticate(readFile(SPIFFS, "/inputUsername.txt").c_str(), readFile(SPIFFS, "/inputPassword.txt").c_str()))
    {
      doc["temperature"] = obtenirTemperatureActuelle();
      serializeJsonPretty(doc, jsonresponse);
      request->send(200, "application/json", jsonresponse);
    }
    else
    {
      doc["message"] = "Non autorisé";
      serializeJsonPretty(doc, jsonresponse);
      request->send(401, "application/json", jsonresponse);
    }
  });

  //Routes API pour les POST.
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    jsonresponse = "";
    doc.clear();
    if (request->authenticate(readFile(SPIFFS, "/inputUsername.txt").c_str(), readFile(SPIFFS, "/inputPassword.txt").c_str()))
    {
      //Route de l'API qui sert à modifier le nom de l'aquarium.
      if (request->url() == "/changerNomAquarium" && request->method() == HTTP_POST)
      {
        deserializeJson(doc, (const char *)data);
        writeFile(SPIFFS, "/inputNom.txt", doc["nomAquarium"]);
        doc["message"] = "Changement de nom reussi";
        serializeJsonPretty(doc, jsonresponse);
        doc.clear();
        request->send(200, "application/json", jsonresponse);
      }
      //Route de l'API qui sert à modifier le mot de passe admin de l'aquarium.
      else if (request->url() == "/modifierMotdepasse" && request->method() == HTTP_POST)
      {
        deserializeJson(doc, (const char *)data);
        writeFile(SPIFFS, "/inputPassword.txt", doc["motdepasse"]);
        doc["message"] = "Changement de mot de passe réussi";
        serializeJsonPretty(doc, jsonresponse);
        doc.clear();
        request->send(200, "application/json", jsonresponse);
      }
      //Route de l'API qui sert à modifier la température cible de l'aquarium.
      else if (request->url() == "/changerTemperatureCible" && request->method() == HTTP_POST)
      {
        deserializeJson(doc, (const char *)data);
        writeFile(SPIFFS, "/inputTemperature.txt", doc["temperature"]);
        doc["message"] = "Changement de la température cible réussi";
        serializeJsonPretty(doc, jsonresponse);
        doc.clear();
        request->send(200, "application/json", jsonresponse);
      }
    }
    else
    {
      doc["message"] = "Non autorisé";
      serializeJsonPretty(doc, jsonresponse);
      request->send(401, "application/json", jsonresponse);
    }
    //request->send(200, "application/json", jsonresponse);
  });

  server.onNotFound(notFound);
  server.begin();
}

void loop()
{
  Serial.println(obtenirTemperatureActuelle());
  afficherOLED();
  verifierTemperature();
  verifierPompe();
  delay(1000);
}