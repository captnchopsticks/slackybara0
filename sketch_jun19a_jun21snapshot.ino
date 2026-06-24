// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <algorithm> //Required for std::max() and std::find()
#include <map> // Required for std::map<> data structure
#include <cmath> //Required for ceiling function for rowMultiple var
#include <vector>

#define CS_PIN (D10)
#define BUSY_PIN (D3)
#define RES_PIN (D4)
#define DC_PIN (D5)
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=5*/ CS_PIN, /*DC=*/ DC_PIN, /*RES=*/ RES_PIN, /*BUSY=*/ BUSY_PIN)); // DEPG0213BN 122x250, SSD1680
GFXcanvas1 canvas(display.width(), display.height());


bool textAlignment = true; //true for left, false for right
u_int16_t x = 0;
u_int16_t y = 2; //found that this offset, paired with a rowHeight of 16 aligns with the 8x8 partialWindow grid of display
u_int16_t rowHeight = 16; //16 pixels height
u_int16_t rowMultiple;

//for capturing history
struct chatHistory {
  String userInput;
  bool textAlignment;
  u_int16_t x;
  int16_t fontOffsetX; //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
  int16_t fontOffsetY; //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
  u_int16_t txtboxWidth;
  u_int16_t txtboxHeight;
};

chatHistory entry1;

std::vector<chatHistory> messageHistory;

void setup()
{
  Serial.begin(115200);

  while(!Serial); // Wait for serial monitor to open

  display.init(115200,true,50,false);
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  display.setTextWrap(true);

  canvas.fillScreen(GxEPD_WHITE);
}

void loop() {
  String userInput = collectUserInput("type something to put onto display: ");

  if (y > 100) {
    display.setPartialWindow(0, 0, display.width(), display.height());
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
    }
    while (display.nextPage());
    delay(50);
    //reset historyCount
    //historyCount = 0;
    y = 2;
  }

  if (userInput == "REPLAY") {
    display.setPartialWindow(0, 0, display.width(), display.height());
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
    }
    while (display.nextPage());
    delay(50);
    y = 2;

    for (int i = 0; i < 7; ++i) {
      renderRow(messageHistory.at(i), y);
      y += (rowHeight * rowMultiple);
    }
  }
  else {
    entry1 = prepareRow(userInput); //prepare the struct with all txtbox information
    entry1.textAlignment = textAlignment;
    messageHistory.push_back(entry1); //append that struct to messageHistory
    renderRow(entry1, y);

    textAlignment = !textAlignment; //toggle between left and right
    y += (rowHeight * rowMultiple);
  }
}

String collectUserInput(String serialPrompt) {
  Serial.print(serialPrompt);
  while (!Serial.available());
  String userInput = Serial.readStringUntil('\n');
  userInput.trim();
  return userInput;
}

chatHistory prepareRow(String userInput) {
  chatHistory userEntry;
  userEntry.userInput = userInput;
  display.getTextBounds(userInput, 0, 0, &userEntry.fontOffsetX, &userEntry.fontOffsetY, &userEntry.txtboxWidth, &userEntry.txtboxHeight);
  Serial.println(userEntry.userInput);
  return userEntry;
}

//pass the string input, y, alignment, and func will handle printing.
void renderRow(chatHistory userEntry, u_int16_t y) {
  if (userEntry.txtboxWidth >= 239) { //bug somewhere in the library? haven't figure out why not working
    userEntry.txtboxWidth = 250;
  }

  userEntry.x = userEntry.textAlignment? 0: (display.width() - userEntry.txtboxWidth); //ternary operator, true = leftAlign, false = rightAlign
  rowMultiple = ceil((float)userEntry.txtboxHeight / rowHeight);
  //Serial.println(rowMultiple);

  display.setPartialWindow(userEntry.x, y, userEntry.txtboxWidth, rowMultiple * rowHeight); //I want height to be in multiples of rowHeight
  //display.setTextColor(GxEPD_BLACK);
  display.firstPage();
  do {
    display.fillRect(userEntry.x, y, userEntry.txtboxWidth, rowMultiple * rowHeight, GxEPD_BLACK);
    display.setCursor(userEntry.x - userEntry.fontOffsetX, y - userEntry.fontOffsetY + 1); //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
    display.print(userEntry.userInput);
  }
  while (display.nextPage());
  delay(50);
}
