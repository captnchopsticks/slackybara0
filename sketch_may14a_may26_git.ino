#include <WiFi.h>
#include <algorithm> // Required for std::find
#include <string> // Required for std::stoi()
#include <map> // Required for std::map<> data structure
#include <array>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "tinyexpr.h"

//This section should likely be in the main directory.
const char* ssid = "your ssid here";
const char* password = "your password here";
const char* hostname = "any name for your esp32";
String openNetworks[50]; //I have this array capped at 50, but realistically no one would probably encounter 50 open networks in one scan

const char* geminiAPIKey = "your API key here"; //ideally should be flashed and not able to change
int geminiMaxTokens = 1000;
int geminiThinkingBudget = 100;
float geminiTemperature = 0.2;
String geminiModelSelection = "gemini-flash-latest"; //this alias will hopefully prevent api service disruption as older models become depricated

//I know this is a really clunky use of maps if the key is just going to be 1,2,3,... but I wanted to try it out as a proof of concept for collectUserChoice() function 
std::map<int, String> geminiParameters = {{1, "Gemini Max Tokens"}, {2, "Gemini Thinking Budget"}, {3, "Gemini Model Temperature"}, {4, "Gemini Model Version"}};
std::map<int, String> geminiModelVersions = {{1, "gemini-flash-latest"}, {2, "gemini-flash-lite-latest"}, {3, "gemini-2.5-flash"}};

DynamicJsonDocument chatHistory(8192);
JsonArray contentsArray = chatHistory.createNestedArray("contents");

enum appState {
  appCalculator,
  appGemini,
  appMenu,
  appSettings
};

appState currentAppState = appMenu; //set initial state to menu by default



void setup() {
  Serial.begin(115200);
  while (!Serial) {
    Serial.println("Waiting for the serial port connection via USB");
    delay(1000);  // wait for serial port to connect. Needed for native USB port only
  }
  
  initWifi(15); //connection timeout = 15 seconds

  geminiModelSelection = collectUserChoice("Enter 1, 2, or 3 to choose your model:",geminiModelVersions);
  Serial.println("Selected -> " + geminiModelSelection);

  while (Serial.available() > 0) {
    Serial.read();
  } // Clear out any residual newline (\n) or carriage return (\r) characters

  runAppMenu();

  //configuration profile, able to adjust through the settings app
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
    case appSettings:
      runSettingsApp();
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

template <typename T>
T collectUserChoice(String serialPrompt, std::map<int, T> myMap) {
  bool exitWhileLoop = false;
  T changeVariable;
  Serial.println("\n========================================================");
  Serial.println(serialPrompt);
  for (auto myMap : myMap) {
    Serial.print(myMap.first);
    Serial.print(": ");
    Serial.println(myMap.second);
  }
  Serial.println("========================================================");
  while (exitWhileLoop == false) {
    while (!Serial.available());
    String userChoice = Serial.readStringUntil('\n');
    userChoice.trim();

    //Serial.println(userChoice); for debug purposes
    for (auto myMap : myMap) {
      if (userChoice.toInt() == myMap.first) {
        changeVariable = myMap.second;
        exitWhileLoop = true;
        break;
      }
    }
    delay(10);
  }
  return changeVariable;
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

      //not really thrilled by this approach, but this next block is for keeping track of any open networks we encounter during the for{} loop.
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

void initWifi(int connectTimeout) {
  //this block is for initializing wifi parameters
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname);

  displayWifiList();

  while (WiFi.status() != WL_CONNECTED) {
    int connectTimer = 0; //restart timer at 0
    String ssid = collectUserInput("\nPlease enter in a SSID to connect to: ");
    Serial.println(ssid);

    if (std::find(openNetworks, openNetworks + 50, ssid) == openNetworks + 50) { //check if the user's ssid is closed
      String password = collectUserInput("Please enter in the WIFI password: ");
      Serial.println(password);
      WiFi.begin(ssid.c_str(), password.c_str());
    }
    else {
      WiFi.begin(ssid.c_str());
    }
    Serial.print("Connecting to " + ssid + " ...");
    while (connectTimer < connectTimeout && WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(1000);
      connectTimer++;
    }
    if (connectTimer == connectTimeout) {
      Serial.println(" Connection Timeout: SSID or Password incorrect");
    }
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
    Serial.println(payload); //for debug purposes

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
  Serial.println("Options:\n[1]: Calculator\n[2]: Gemini Chat\n[3]: Settings");
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
      else if (choice == '3') {
        currentAppState = appSettings;
        Serial.println("\n-> Selected: Settings");
        break; // Exit the while loop
      }
    }
    delay(10); // Tiny delay to keep the watchdog timer happy
  }
}

//this code is so clunky, want to trim down with switch case eventually
void runSettingsApp() {
  String userQuery = "";
  while (userQuery != "EXIT") {
    String userChoice = collectUserChoice("Enter 1, 2, 3, to change settings/parameters.", geminiParameters);
    Serial.println("Selected -> " + userChoice);

    if (userChoice == "Gemini Max Tokens") {
      geminiMaxTokens = collectUserInput("Enter in an integer amount: ").toInt();
      chatHistory["generationConfig"]["maxOutputTokens"] = geminiMaxTokens;
      Serial.println(geminiMaxTokens);
    } 
    else if (userChoice == "Gemini Thinking Budget") {
      geminiThinkingBudget = collectUserInput("Enter in an integer amount: ").toInt();
      chatHistory["generationConfig"]["thinkingConfig"]["thinkingBudget"] = geminiThinkingBudget;
      Serial.println(geminiThinkingBudget);
    }
    else if (userChoice == "Gemini Model Temperature") {
      geminiTemperature = collectUserInput("Enter in an decimal amount: ").toFloat();
      chatHistory["generationConfig"]["temperature"] = geminiTemperature;
      Serial.println(geminiTemperature);
    }
    else if (userChoice == "Gemini Model Version") {
      geminiModelSelection = collectUserChoice("Enter 1, 2, or 3 to choose your model:",geminiModelVersions);
      Serial.println("Selected -> " + geminiModelSelection);
    }
    userQuery = collectUserInput("exit? ");
    Serial.println(userQuery);
    
    if (userQuery == "EXIT") {
      currentAppState = appMenu; //esc sequence
      Serial.println("escape seq successful, menu initialized");
      break;
    }
  }
}
