#include <WiFi.h>
#include <algorithm> // Required for std::find
#include <string> // Required for stoi()
#include <array>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "tinyexpr.h"

//This section should likely be in the main directory.
const char* ssid = "enter ssid here";
const char* password = "enter password here";
const char* hostname = "Your name for ESP32";
String openNetworks[50]; //I have this array capped at 50, but realistically no one would probably encounter 50 open networks in one scan

const char* geminiAPIKey = "enter API key here";
const int geminiMaxTokens = 1000;
const int geminiThinkingBudget = 100;
const float geminiTemperature = 0.2;
String geminiModelSelection = "gemini-flash-latest"; //this alias will hopefully prevent api service disruption as older models become depricated

DynamicJsonDocument chatHistory(8192);
JsonArray contentsArray = chatHistory.createNestedArray("contents");

enum appState {
  appCalculator,
  appGemini,
  appMenu
};

appState currentAppState = appMenu; //set initial state to menu by default

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    Serial.println("Waiting for the serial port connection via USB");
    delay(1000);  // wait for serial port to connect. Needed for native USB port only
  }
  
  initWifi();

  runAppMenu();

  Serial.println("\n========================================================");
  Serial.println("Enter 1 or 2 to choose your model:");
  Serial.println("[1] Latest Gemini Flash");
  Serial.println("[2] Latest Gemini Flash Lite");
  Serial.println("[3] Gemini Flash 2.5");
  Serial.println("========================================================");

  while (true) {//main block to get user input on desired Gemini model
    if (Serial.available() > 0) {
      char choice = Serial.read();
      
      if (choice == '1') {
        geminiModelSelection = "gemini-flash-latest";
        Serial.println("\n-> Selected: Latest Gemini Flash");
        break; // Exit the while loop
      } 
      else if (choice == '2') {
        geminiModelSelection = "gemini-flash-lite-latest";
        Serial.println("\n-> Selected: Latest Gemini Flash Lite");
        break; // Exit the while loop
      }
      else if (choice == '3') {
        geminiModelSelection = "gemini-2.5-flash";
        Serial.println("\n-> Selected: Latest Gemini Flash Lite");
        break; // Exit the while loop
      }
    }
    delay(10); // Tiny delay to keep the watchdog timer happy
  }

  while (Serial.available() > 0) {
    Serial.read();
  } // Clear out any residual newline (\n) or carriage return (\r) characters

  //configuration profile, would like to add tuning functionality
  chatHistory["generationConfig"]["maxOutputTokens"] = geminiMaxTokens;
  chatHistory["generationConfig"]["temperature"] = geminiTemperature;
  chatHistory["generationConfig"]["thinkingConfig"]["thinkingBudget"] = geminiThinkingBudget;
  chatHistory["generationConfig"]["thinkingConfig"]["includeThoughts"] = 0;
}

void loop() {
  String userQuery = "";
  while (Serial.available() > 0) { // Clear out any residual newline (\n) or carriage return (\r) characters
  Serial.read();
  }
  switch (currentAppState) {
    case appMenu:
      runAppMenu();
      break;
    case appGemini:
      runGeminiApp();
      break;
    case appCalculator:
      runCalculatorApp();
      break;
  }
}

String collectUserInput(String serialPrompt) {
  Serial.print(serialPrompt);
  while (!Serial.available());
  String userInput = Serial.readStringUntil('\n');
  userInput.trim();
  return userInput;
}

void displayWifiList() {
  Serial.println("\nscan start");
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
      Serial.println("no networks found");
  } 
  else {
    int itemCount = 0;
    Serial.print(n); Serial.println(" networks found:");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");

      //this next block is for keeping track of any open networks we encounter during the for{} loop.
      if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN && itemCount < 50) {
        openNetworks[itemCount] = WiFi.SSID(i); // Append the new string
        itemCount++;                     // Move the end marker forward
      }
      else if (itemCount > 50) {
        Serial.println("Error: openNetworks[] container is full!");
      }
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)? "*":""); //this line will print an "*" for open networks on the list
      delay(10);
    }
  }
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname);

  displayWifiList();
  String ssid = collectUserInput("\nPlease enter in a SSID to connect to: ");
  Serial.println(ssid);
  if (std::find(openNetworks, openNetworks + 50, ssid) == openNetworks + 50) {
    String password = collectUserInput("Please enter in the WIFI password. ");
    Serial.println(password);
    WiFi.begin(ssid.c_str(), password.c_str());
  }
  else {
    WiFi.begin(ssid.c_str());
  }
  Serial.print("Connecting to " + ssid + " ...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  
  Serial.print("\n\nESP32 IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP32 HostName: ");
  Serial.println(WiFi.getHostname());
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());

  WiFi.scanDelete();
}

void appendChatTurn(JsonArray targetArray, String role, String textContent) {
  JsonObject turnObj = targetArray.createNestedObject();
  turnObj["role"] = role;
  
  JsonArray partsArray = turnObj.createNestedArray("parts");
  JsonObject textObj = partsArray.createNestedObject();
  textObj["text"] = textContent;

  const int maxHistoryItems = 20;
  if (role == "user") {
    while (targetArray.size() > maxHistoryItems) {
      targetArray.remove(0); // Remove oldest user message
      targetArray.remove(0); // Remove oldest model response
      Serial.println("[Memory Guard] Pruned oldest turn inside function.");
    }
  }
}

void runGeminiApp() {
  String userQuery = "";
  HTTPClient https;
  https.setTimeout(20000); //timeout clock, if the microcontroller doesn't recieve a response from API, quit and send error timeout

  while (userQuery != "EXIT") {
    userQuery = collectUserInput("\nAsk a question: ");
    Serial.println(userQuery);
    if (userQuery == "EXIT") {
      currentAppState = appMenu; //esc sequence
      Serial.println("escape seq successful, menu initialized");
      break;
    }
    
    appendChatTurn(contentsArray, "user", userQuery);
    String payload;
    serializeJsonPretty(chatHistory, payload);
    //Serial.println(payload); //for debug purposes

    //main block for Gemini API POST
    if (https.begin("https://generativelanguage.googleapis.com/v1beta/models/" + geminiModelSelection + ":generateContent?key=" + (String)geminiAPIKey)) {
      https.addHeader("Content-Type", "application/json");
      int httpCode = https.POST(payload); // start connection and send HTTP header

      if (httpCode == HTTP_CODE_OK) {
        String answerBody = https.getString();
        //Serial.println(answerBody); //for debug purposes
        DynamicJsonDocument answerDoc(4096);
        deserializeJson(answerDoc, answerBody);
        String extractedAnswer = answerDoc["candidates"][0]["content"]["parts"][0]["text"];
        extractedAnswer.trim();

        Serial.println("\nHere is your Answer: ");
        Serial.println(extractedAnswer);
        appendChatTurn(contentsArray, "model", extractedAnswer);
      } 
      else {
        String answerBody = https.getString();
        Serial.println(answerBody);
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        contentsArray.remove(contentsArray.size() - 1); //remove the failed userQuery
      }
      https.end();
    } 
    else {
        Serial.printf("[HTTPS] Unable to connect\n");
        contentsArray.remove(contentsArray.size() - 1); //remove the failed userQuery
    }
  }
}

void runCalculatorApp() {
  String userQuery = "";
  while (userQuery != "EXIT") {
    userQuery = collectUserInput("\nEnter a mathematical expression to calculate: ");
    Serial.println(userQuery);
    if (userQuery == "EXIT") {
      currentAppState = appMenu; //esc sequence
      Serial.println("escape seq successful, menu initialized");
      break;
    }
    Serial.print("Answer: ");
    Serial.printf("%f", te_interp(userQuery.c_str(), 0)); //note that the .c_str() converts userQuery to const char* for required func. input
    Serial.println();
  }
}

void runAppMenu() {
  Serial.println("\n========================================================");
  Serial.println("Options:\n[1]: Calculator\n[2]: Gemini Chat");
  Serial.println("========================================================");

  while (true) {//main block to get user input on app
    if (Serial.available() > 0) {
      char choice = Serial.read();
      
      if (choice == '1') {
        currentAppState = appCalculator;
        Serial.println("\n-> Selected: Calculator");
        break; // Exit the while loop
      } 
      else if (choice == '2') {
        currentAppState = appGemini;
        Serial.println("\n-> Selected: Gemini Chat");
        break; // Exit the while loop
      }
    }
    delay(10); // Tiny delay to keep the watchdog timer happy
  }
}
