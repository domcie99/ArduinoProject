#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>
#include <Servo.h>
#include <ThingSpeak.h>
#include "config.h"

// Ustawienia sieci WiFi
const char *ssid = SSID;
const char *password = PASSWORD;

// Ustawienia thingspeak
const unsigned long channelNumber = CHANNEL_ID;
const char *api = THINGSPEAK_API;
String apiKey = api;


int targetTemperature = 25;  // Temperatura docelowa
int targetHumidity = 50;    // Wilgotność docelowa
bool coolingMode = true;    // Tryb chłodzenia (true) lub grzania (false)
bool automationEnabled = true;  // Automatyzacja włączona (true) lub wyłączona (false)



// Ustawienia czujników DHT
#define DHT_PIN_1 D1
#define DHT_PIN_2 D2
#define DHT_PIN_3 D3

#define DHT_TYPE DHT11
DHT dht1(DHT_PIN_1, DHT_TYPE);
DHT dht2(DHT_PIN_2, DHT_TYPE);
DHT dht3(DHT_PIN_3, DHT_TYPE);

// Ustawienia przekaźników
#define RELAY_PIN_1 D6
#define RELAY_PIN_2 D7
bool relay1State = HIGH;
bool relay2State = HIGH;

// Ustawienia serwa
#define SERVO_PIN D8
Servo servo;
int servoAngle = 0;

ESP8266WebServer server(80);

// Zmienne Czujników
float humidityDolot, temperatureDolot;
float humidityWylot, temperatureWylot;
float humidityPokoj, temperaturePokoj;

// Odczyt Czujników
void getSensorsData(){
  humidityDolot = dht1.readHumidity();
  temperatureDolot = dht1.readTemperature();
  delay(100);
  humidityWylot = dht2.readHumidity();
  temperatureWylot = dht2.readTemperature();
  delay(100);
  humidityPokoj = dht3.readHumidity();
  temperaturePokoj = dht3.readTemperature();
}

//Funkcja do aktualizacji ustawień temperatury i wilgotności:
void updateSettings(int newTargetTemperature, int newTargetHumidity, bool newCoolingMode, bool newAutomationEnabled) {
  targetTemperature = newTargetTemperature;
  targetHumidity = newTargetHumidity;
  coolingMode = newCoolingMode;
  automationEnabled = newAutomationEnabled;
}

// Interval do odczytu czujników
unsigned long previousMillis = 0;
const long interval = 10000;
const long intervalThingSpeak = 5000;

// Obsługa AJAX
void handleAjaxRequest() {
  String response = "";

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    getSensorsData();
  }

  response += "{\"humidityDolot\":\"" + String(humidityDolot) + "\",";
  response += "\"temperatureDolot\":\"" + String(temperatureDolot) + "\",";
  response += "\"humidityWylot\":\"" + String(humidityWylot) + "\",";
  response += "\"temperatureWylot\":\"" + String(temperatureWylot) + "\",";
  response += "\"humidityPokoj\":\"" + String(humidityPokoj) + "\",";
  response += "\"temperaturePokoj\":\"" + String(temperaturePokoj) + "\",";
  response += "\"relay1\":\"" + String(relay1State) + "\",";
  response += "\"relay2\":\"" + String(relay2State) + "\",";
  response += "\"servo\":\"" + String(servoAngle) + "\",";
  response += "\"targetTemperature\":\"" + String(targetTemperature) + "\",";
  response += "\"targetHumidity\":\"" + String(targetHumidity) + "\",";
  response += "\"coolingMode\":\"" + String(coolingMode) + "\",";
  response += "\"automationEnabled\":\"" + String(automationEnabled) + "\"}";

  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", response);

  if (currentMillis - previousMillis >= intervalThingSpeak) {
    sendToThingSpeak(temperatureDolot, humidityDolot, 
                    temperatureWylot, humidityWylot, 
                    temperaturePokoj, humidityPokoj, 
                    relay1State, relay2State);
  }
}

void sendToThingSpeak(float tempDolot, float humDolot, float tempWylot, float humWylot, float tempPokoj, float humPokoj, int relay1, int relay2) {
  WiFiClient client;
  HTTPClient http;

  String url = "http://api.thingspeak.com/update?api_key=" + apiKey;
  url += "&field1=" + String(tempDolot);
  url += "&field2=" + String(humDolot);
  url += "&field3=" + String(tempWylot);
  url += "&field4=" + String(humWylot);
  url += "&field5=" + String(tempPokoj);
  url += "&field6=" + String(humPokoj);
  url += "&field7=" + String(relay1);
  url += "&field8=" + String(relay2);

  http.begin(client, url);

  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.println("[ThingSpeak] Data sent successfully");
  } else {
    Serial.println("[ThingSpeak] Error sending data");
  }

  http.end();
}

void toggleRelay(int relay) {
  if (relay == 1) {
    relay1State = !relay1State;
    digitalWrite(RELAY_PIN_1, relay1State);
  } else if (relay == 2) {
    relay2State = !relay2State;
    digitalWrite(RELAY_PIN_2, relay2State);
  }
}

void setServoAngle(int angle) {
  servoAngle = angle;
  servo.write(angle);
}

void handleToggleRequest() {
  String relayParam = server.arg("relay");
  String automationParam = server.arg("automation");

  if (relayParam.length() > 0) {
    int relay = relayParam.toInt();
    toggleRelay(relay);
  }

  if (automationParam.length() > 0) {
    automationEnabled = !automationEnabled;
  }

  server.send(200, "text/plain", "OK");
}


void handleServoRequest() {
  String angleParam = server.arg("angle");

  if (angleParam.length() > 0) {
    int angle = angleParam.toInt();
    setServoAngle(angle);
  }

  server.send(200, "text/plain", "OK");
}

void handleSaveSettingsRequest() {
  String targetTemperatureParam = server.arg("targetTemperature");
  String targetHumidityParam = server.arg("targetHumidity");
  String coolingModeParam = server.arg("coolingMode");
  String automationEnabledParam = server.arg("automationEnabled");

  if (targetTemperatureParam.length() > 0 && targetHumidityParam.length() > 0 &&
      coolingModeParam.length() > 0 && automationEnabledParam.length() > 0) {

    int newTargetTemperature = targetTemperatureParam.toInt();
    int newTargetHumidity = targetHumidityParam.toInt();
    bool newCoolingMode = (coolingModeParam.toInt() == 1);
    bool newAutomationEnabled = (automationEnabledParam.toInt() == 1);

    updateSettings(newTargetTemperature, newTargetHumidity, newCoolingMode, newAutomationEnabled);

    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  dht1.begin();
  dht2.begin();
  dht3.begin();

  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  digitalWrite(RELAY_PIN_1, relay1State);
  digitalWrite(RELAY_PIN_2, relay2State);

  servo.attach(SERVO_PIN);
  servo.write(servoAngle);

  server.on("/ajax", HTTP_GET, handleAjaxRequest);
  server.on("/toggle", HTTP_GET, handleToggleRequest);
  server.on("/servo", HTTP_GET, handleServoRequest);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTML());
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not Found");
  });

  server.on("/save-settings", HTTP_GET, handleSaveSettingsRequest);

  server.begin();
}

void loop() {
  server.handleClient();
}

String getHTML() {
  String html = "<html>";
  html += "<head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta charset='UTF-8'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; text-align: center; background-color: #f3f3f3; }";
  html += "h1 { color: #333; }";
  html += ".container { display: flex; justify-content: space-around; max-width: 800px; margin: 20px auto; }";
  html += ".sensor-data, .relay-control, .servo-control, .settings-form { flex: 1; margin: 10px; padding: 15px; background-color: #f9f9f9; border: 1px solid #ddd; border-radius: 5px; text-align: left; }";
  html += ".device-on, .device-off { padding: 10px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; width: 150px; }";
  html += ".device-on { background-color: #4caf50 !important; color: white; }";
  html += ".device-off { background-color: #f44336 !important; color: white; }";
  html += "button { padding: 10px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; }";
  html += "input[type=range] { width: 80%; }";
  html += ".switch-container { display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px; }";
  html += ".switch { position: relative; display: inline-block; width: 60px; height: 34px; }";
  html += ".switch input { display: none; }";
  html += ".slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }";
  html += ".slider:before { position: absolute; content: ''; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }";
  html += "input:checked + .slider { background-color: #2196F3; }";
  html += "input:checked + .slider:before { transform: translateX(26px); }";
  html += ".mode-label { flex: 1; text-align: center; }";
  html += ".temperature-container, .humidity-container { display: flex; align-items: center; justify-content: center; }";
  html += ".temperature-container button, .humidity-container button { margin: 5px; padding: 8px; background-color: #4caf50; color: white; border: none; border-radius: 5px; cursor: pointer; }";
  html += ".temperature-container input, .humidity-container input { width: 60px; text-align: center; }";
  html += "</style>";
  html += "<script>";
  html += "function updateData() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/ajax', true);";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      var data = JSON.parse(xhr.responseText);";
  html += "      updateSensorData('Dolot Powietrza', 'humidityDolot', 'temperatureDolot', data.humidityDolot, data.temperatureDolot);";
  html += "      updateSensorData('Wylot Powietrza', 'humidityWylot', 'temperatureWylot', data.humidityWylot, data.temperatureWylot);";
  html += "      updateSensorData('Pokój', 'humidityPokoj', 'temperaturePokoj', data.humidityPokoj, data.temperaturePokoj);";
  html += "      updateDeviceStatus('Wentylator', data.relay1);";
  html += "      updateDeviceStatus('Pompa', data.relay2);";
  html += "    }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";
  html += "function updateSensorData(sensorName, humidityId, temperatureId, humidity, temperature) {";
  html += "  document.getElementById(humidityId).innerHTML = sensorName + ' Humidity: ' + humidity + '%';";
  html += "  document.getElementById(temperatureId).innerHTML = sensorName + ' Temperature: ' + temperature + '°C';";
  html += "}";
  html += "function updateDeviceStatus(elementId, status) {";
  html += "  var element = document.getElementById(elementId);";
  html += "  var buttonText = (status == 1 ? 'OFF' : 'ON');";
  html += "  element.innerHTML = elementId + ': ' + buttonText;";
  html += "  element.className = 'device-' + (status == 1 ? 'off' : 'on');";
  html += "}";
  html += "function toggleRelay(relay) {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/toggle?relay=' + relay, true);";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      updateData();";
  html += "    }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";
  html += "function updateServo() {";
  html += "  var slider = document.getElementById('servoSlider');";
  html += "  var value = Math.round((slider.value / 100) * 180);";
  html += "  document.getElementById('servoValue').innerHTML = 'Servo: ' + value + '%';";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/servo?angle=' + value, true);";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      updateData();";
  html += "    }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";
  html += "function updateSettings() {";
  html += "  var targetTemperature = document.getElementById('targetTemperature').value;";
  html += "  var targetHumidity = document.getElementById('targetHumidity').value;";
  html += "  var coolingMode = document.getElementById('coolingMode').checked;";
  html += "  var automationEnabled = document.getElementById('automationEnabled').checked;";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/save-settings?targetTemperature=' + targetTemperature + '&targetHumidity=' + targetHumidity + '&coolingMode=' + coolingMode + '&automationEnabled=' + automationEnabled, true);";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      updateData();";
  html += "    }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";
  html += "document.addEventListener('DOMContentLoaded', function() {";
  html += "  document.getElementById('Wentylator').addEventListener('click', function() {";
  html += "    toggleRelay(1);";
  html += "  });";
  html += "  document.getElementById('Pompa').addEventListener('click', function() {";
  html += "    toggleRelay(2);";
  html += "  });";
  html += "  document.getElementById('servoSlider').addEventListener('input', function() {";
  html += "    updateServo();";
  html += "  });";
  html += "  document.getElementById('coolingModeLabel').addEventListener('click', function() {";
  html += "    document.getElementById('coolingMode').checked = !document.getElementById('coolingMode').checked;";
  html += "    updateSettings();";
  html += "  });";
  html += "  document.getElementById('automationEnabledLabel').addEventListener('click', function() {";
  html += "    document.getElementById('automationEnabled').checked = !document.getElementById('automationEnabled').checked;";
  html += "    updateSettings();";
  html += "  });";
  html += "  updateData();";
  html += "  updateSettings();";
  html += "});";
  html += "setInterval(updateData, 5000);";
  html += "</script>";
  html += "</head>";
  html += "<body>";
  html += "<h1>System Wentylacji</h1>";

  // Zaktualizowane sekcje z nowym układem
  html += "<div class='container'>";

  // Dolot Powietrza
  html += "<div class='sensor-data'>";
  html += "<h2>Dolot Powietrza</h2>";
  html += "<div id='humidityDolot'></div>";
  html += "<div id='temperatureDolot'></div>";
  html += "</div>";

  // Wylot Powietrza
  html += "<div class='sensor-data'>";
  html += "<h2>Wylot Powietrza</h2>";
  html += "<div id='humidityWylot'></div>";
  html += "<div id='temperatureWylot'></div>";
  html += "</div>";

  // Pokój
  html += "<div class='sensor-data'>";
  html += "<h2>Pokój</h2>";
  html += "<div id='humidityPokoj'></div>";
  html += "<div id='temperaturePokoj'></div>";
  html += "</div>";

  html += "</div>"; // Zamknięcie kontenera

  // Przełączniki
  html += "<div class='relay-control'>";
  html += "<h2>Przełączniki</h2>";
  html += "<button id='Wentylator' class='device-off'>Wentylator</button>";
  html += "<button id='Pompa' class='device-off'>Pompa</button>";
  html += "</div>";

  // Serwo Control
  html += "<div class='servo-control'>";
  html += "<h2>Servo Control</h2>";
  html += "<input type='range' id='servoSlider' min='0' max='100' value='0' oninput='updateServo()'>";
  html += "<div id='servoValue'>Servo: 0%</div>";
  html += "</div>";

  // Formularz ustawień
  html += "<div class='settings-form'>";
  html += "<h2>Ustawienia Automatyzacji</h2>";

  // Nastawianie temperatury
  html += "<div class='temperature-container'>";
  html += "<button onclick='changeTemperature(-1)'>-</button>";
  html += "<label for='targetTemperature'>Temperatura Docelowa:</label>";
  html += "<input type='number' id='targetTemperature' value='" + String(targetTemperature) + "' disabled>";
  html += "<button onclick='changeTemperature(1)'>+</button>";
  html += "</div>";

  // Nastawianie wilgotności
  html += "<div class='humidity-container'>";
  html += "<button onclick='changeHumidity(-1)'>-</button>";
  html += "<label for='targetHumidity'>Wilgotność Docelowa:</label>";
  html += "<input type='number' id='targetHumidity' value='" + String(targetHumidity) + "' disabled>";
  html += "<button onclick='changeHumidity(1)'>+</button>";
  html += "</div>";

  // Dodano przełącznik dla trybu grzania/chłodzenia
  html += "<div class='mode-switch'>";
  html += "  <label class='switch'>";
  html += "    <input type='checkbox' id='coolingMode' " + String(coolingMode ? "checked" : "") + ">";
  html += "    <span class='slider'></span>";
  html += "  </label>";
  html += "  <span class='mode-label'>Grzanie</span>";
  html += "  <span class='mode-label'>Chłodzenie</span>";
  html += "</div>";
  html += "<br>";

  // Przycisk do zapisywania ustawień
  html += "<button onclick='saveSettings()'>Zapisz Ustawienia</button>";
  html += "</div>";

  html += "</body>";

  html += "<script>";
  html += "function saveSettings() {";
  html += "  var targetTemperature = document.getElementById('targetTemperature').value;";
  html += "  var targetHumidity = document.getElementById('targetHumidity').value;";
  html += "  var coolingMode = document.getElementById('coolingMode').checked;";
  html += "  var automationEnabled = document.getElementById('automationEnabled').checked;";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/save-settings?targetTemperature=' + targetTemperature + '&targetHumidity=' + targetHumidity + '&coolingMode=' + coolingMode + '&automationEnabled=' + automationEnabled, true);";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      updateData();";
  html += "    }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";
  html += "function changeTemperature(delta) {";
  html += "  var targetTemperature = parseInt(document.getElementById('targetTemperature').value) + delta;";
  html += "  document.getElementById('targetTemperature').value = targetTemperature;";
  html += "  updateSettings();";
  html += "}";
  html += "function changeHumidity(delta) {";
  html += "  var targetHumidity = parseInt(document.getElementById('targetHumidity').value) + delta;";
  html += "  document.getElementById('targetHumidity').value = targetHumidity;";
  html += "  updateSettings();";
  html += "}";
  html += "</script>";

  html += "</html>";
  return html;
}
 
