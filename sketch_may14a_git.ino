#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

const char* ssid = "wifiname";
const char* password = "wifipassword";
const char* hostname = "yourdevicename";

const char* geminiAPIKey = "APIkey";
const int geminiMaxTokens = 1000;
const int geminiThinkingBudget = 100;
const float geminiTemperature = 0.2;

DynamicJsonDocument chatHistory(8192);
JsonArray contentsArray = chatHistory.createNestedArray("contents");


void setup() {
  Serial.begin(115200);
  initWifi();
  Serial.println("");
  //Serial.println("Enable history?");
  chatHistory["generationConfig"]["maxOutputTokens"] = geminiMaxTokens;
  chatHistory["generationConfig"]["temperature"] = geminiTemperature;
  chatHistory["generationConfig"]["thinkingConfig"]["thinkingBudget"] = geminiThinkingBudget;
  chatHistory["generationConfig"]["thinkingConfig"]["includeThoughts"] = 0;
}

void loop() {
  Serial.println("\nAsk your Question : ");
  while (!Serial.available()); //pause for serial monitor response
  String userQuery = "";
  while (Serial.available()) {
    char add = Serial.read();
    userQuery = userQuery + add;
    delay(1);
  }
  userQuery.trim();

  Serial.print("\nAsking Your Question : ");
  Serial.println(userQuery);

  appendChatTurn(contentsArray, "user", userQuery);

  String payload;
  serializeJsonPretty(chatHistory, payload);
  Serial.println(payload);
  
  HTTPClient https;
  https.setTimeout(10000);

  if (https.begin("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + (String)geminiAPIKey)) {
    
    https.addHeader("Content-Type", "application/json");
    int httpCode = https.POST(payload); // start connection and send HTTP header

    if (httpCode == HTTP_CODE_OK) {
      String answerBody = https.getString();
      Serial.println(answerBody); //for debug purposes
      DynamicJsonDocument answerDoc(4096);
      deserializeJson(answerDoc, answerBody);
      String extractedAnswer = answerDoc["candidates"][0]["content"]["parts"][0]["text"];
      extractedAnswer.trim();

      Serial.println("\nHere is your Answer: ");
      Serial.println(extractedAnswer);

      appendChatTurn(contentsArray, "model", extractedAnswer);

    } else {
      String answerBody = https.getString();
      Serial.println(answerBody);
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      contentsArray.remove(contentsArray.size() - 1); //remove?
    }
    https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
      contentsArray.remove(contentsArray.size() - 1); //remove?
    }

}

void displayWifiList() {
  Serial.println("scan start");
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
      Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
      delay(10);
    }
  }
  Serial.println("");
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname);

  displayWifiList();
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  
  Serial.print("\nESP32 IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP32 HostName: ");
  Serial.println(WiFi.getHostname());
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());
}

void appendChatTurn(JsonArray targetArray, String role, String textContent) {
  JsonObject turnObj = targetArray.createNestedObject();
  turnObj["role"] = role;
  
  JsonArray partsArray = turnObj.createNestedArray("parts");
  JsonObject textObj = partsArray.createNestedObject();
  textObj["text"] = textContent;
}
