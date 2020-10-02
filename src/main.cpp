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

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WiFiManager wm;

AsyncWebServer server(80);

// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = "ESP32";
const char* password = "Patate123";

const char* PARAM_NOM = "inputNom";
const char* PARAM_TEMPERATURE = "inputTemperature";
const char* PARAM_FREQUENCEPOMPE = "inputFrequencePompe";
const char* PARAM_TEMPSPOMPE = "inputTempsPompe";
bool pompe = false;

// HTML web page to handle 3 input fields (inputString, inputInt, inputFloat)
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
            Fréquence d'activation de la pompe (en minutes) : <br><input type="number" value="%inputFrequencePompe%" name="inputFrequencePompe" min="1" max="120">
            <br><br>
            <input type="submit" value="Appliquer" onclick="submitMessage()">
        </form>

        <form action="/get" target="hidden-form" style="border: 3px groove;padding: 0 20px 20px 20px;width: 350px;margin-bottom:20px;">
            <h2>Temps d'action de la pompe</h2>
            <br>
            Temps que la pompe reste activée (en minutes) : <input type="number" value="%inputTempsPompe%" name="inputTempsPompe" min="1" max="60">
            <br><br><br>
            <input type="submit" value="Appliquer" onclick="submitMessage()">
        </form>

    </div>
    <iframe style="display:none" name="hidden-form"></iframe>
</body>

</html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

// Replaces placeholder with stored values
String processor(const String& var){
  //Serial.println(var);
  if(var == "inputNom"){
    return readFile(SPIFFS, "/inputNom.txt");
  }
  else if(var == "inputTemperature"){
    return readFile(SPIFFS, "/inputTemperature.txt");
  }
  else if(var == "inputFrequencePompe"){
    return readFile(SPIFFS, "/inputFrequencePompe.txt");
  }
  else if(var == "inputTempsPompe"){
    return readFile(SPIFFS, "/inputTempsPompe.txt");
  }
  return String();
}

void setup() {
  Serial.begin(9600);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  //Affichage de la config Wifi.
  display.clearDisplay();
  display.setTextSize(1,1);
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

  //WifiManager
  WiFi.mode(WIFI_STA);
  if(!wm.autoConnect(ssid, password))
		Serial.println("Erreur de connexion.");
	else
		Serial.println("Connexion etablie!");
  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <ESP_IP>/get?inputNom=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    // GET inputNom
    if (request->hasParam(PARAM_NOM)) {
      inputMessage = request->getParam(PARAM_NOM)->value();
      writeFile(SPIFFS, "/inputNom.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputTemperature=<inputMessage>
    else if (request->hasParam(PARAM_TEMPERATURE)) {
      inputMessage = request->getParam(PARAM_TEMPERATURE)->value();
      writeFile(SPIFFS, "/inputTemperature.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputFrequencePompe=<inputMessage>
    else if (request->hasParam(PARAM_FREQUENCEPOMPE)) {
      inputMessage = request->getParam(PARAM_FREQUENCEPOMPE)->value();
      writeFile(SPIFFS, "/inputFrequencePompe.txt", inputMessage.c_str());
    }
    // GET inputInt value on <ESP_IP>/get?inputTempsPompe=<inputMessage>
    else if (request->hasParam(PARAM_TEMPSPOMPE)) {
      inputMessage = request->getParam(PARAM_TEMPSPOMPE)->value();
      writeFile(SPIFFS, "/inputTempsPompe.txt", inputMessage.c_str());
    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/text", inputMessage);
  });
  server.onNotFound(notFound);
  server.begin();
}

void loop() {

  String nom = readFile(SPIFFS, "/inputNom.txt");
  String temperatureCible = readFile(SPIFFS, "/inputTemperature.txt");
  display.clearDisplay();
  display.setTextSize(1,1);
  display.setTextColor(WHITE);

  //Affichage du nom.
  display.setCursor(0, 5);
  display.println(nom);

  //Affichage de la temperature cible.
  display.setCursor(0, 20);
  display.println("Temperature : 25C");

  //Pompe ON OFF
  display.setCursor(0, 35);
  if (pompe)
    display.println("Pompe : ON");
  else
    display.println("Pompe : OFF");

  //Affichage de l'ip.
  display.setCursor(0, 50);
  display.println("IP : " + WiFi.localIP().toString());

  display.display(); 
  
  delay(5000);
}