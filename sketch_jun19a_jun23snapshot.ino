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
GFXcanvas1 canvas(display.height(), display.width());


bool textAlignment = true; //true for left, false for right
u_int16_t x = 0;
int16_t y = 2; //found that this offset, paired with a rowHeight of 16 aligns with the 8x8 partialWindow grid of display
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

chatHistory messageEntry;

std::vector<chatHistory> messageHistory;
int startIndex = 0;
int cursorIndex = 0;

void setup() {
  Serial.begin(115200);

  while(!Serial); // Wait for serial monitor to open

  display.init(115200,true,50,false);
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  display.setTextWrap(true);

  canvas.setFont(&FreeMonoBold9pt7b);
  canvas.fillScreen(GxEPD_WHITE);

  delay(1000);
  //Serial.print("\npage height: "); Serial.println(display.pageHeight());
  //Serial.print("pages available: "); Serial.println(display.pages());
}

void loop() {
  String userInput = collectUserInput("type something to put onto display: ");
  Serial.println(userInput);

  if (userInput != "^^") {

    messageEntry = prepareRow(userInput, textAlignment); //prepare the struct with all txtbox information
    messageHistory.push_back(messageEntry); //append that struct to messageHistory

    //Serial.print("textAlignment: "); Serial.println(messageEntry.textAlignment);
    //Serial.print("messageHistorySize: " ); Serial.println((int)messageHistory.size());

    if ((int)messageHistory.size() > 7) {
      //refresh canvas for every autoscroll down

      startIndex = (int)messageHistory.size() - 7;

      bulkRenderRow(messageHistory, startIndex, false);

      display.setPartialWindow(0, 0, display.width(), display.height());
      display.firstPage();
      do {
        //.drawBitmap() height and width arguments are inverted because display.setRotation = 1
        display.drawBitmap(0, 0, canvas.getBuffer(), display.width(), display.height(), GxEPD_WHITE, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(50);
    }
    else {
      partialRenderRow(messageEntry, y);
    }

    y += (rowHeight * rowMultiple);
    textAlignment = !textAlignment; //toggle between left and right
    cursorIndex = (int)messageHistory.size() - 1;
    //Serial.print("cursorIndex: "); Serial.println(cursorIndex);

  }
  else if (cursorIndex > 6 && userInput == "^^") { //cursorIndex conditional will prevent errors where index = -1
    //one really odd nuance is the decrement of cursorIndex-- has to take effect before printing
    cursorIndex--; //having this line at the end will cause you to not print out the right output at least once
    startIndex = cursorIndex - 6;
    //Serial.print("cursorIndex: "); Serial.println(cursorIndex);

    //Serial.println(startIndex);
    bulkRenderRow(messageHistory, startIndex, true);

    display.setPartialWindow(0, 0, display.width(), display.height());
    display.firstPage();
    do {
      //.drawBitmap() height and width arguments are inverted because display.setRotation = 1
      display.drawBitmap(0, 0, canvas.getBuffer(), display.width(), display.height(), GxEPD_WHITE, GxEPD_BLACK);
    }
    while (display.nextPage());
    delay(50);
  }
  else {
    Serial.println("no output no output no output no output no output no output");
  }
}

String collectUserInput(String serialPrompt) {
  Serial.print(serialPrompt);
  while (!Serial.available());
  String userInput = Serial.readStringUntil('\n');
  userInput.trim();
  return userInput;
}

chatHistory prepareRow(String userInput, bool textAlignment) {
  chatHistory userEntry;
  userEntry.userInput = userInput;
  display.getTextBounds(userInput, 0, 0, &userEntry.fontOffsetX, &userEntry.fontOffsetY, &userEntry.txtboxWidth, &userEntry.txtboxHeight);
  if (userEntry.txtboxWidth >= 239) { //bug somewhere in the library? haven't figure out why not working
    userEntry.txtboxWidth = 250;
  }
  userEntry.textAlignment = textAlignment;
  userEntry.x = userEntry.textAlignment? 0: (display.width() - userEntry.txtboxWidth); //ternary operator, true = leftAlign, false = rightAlign
  //Serial.println(userEntry.userInput);
  return userEntry;
}

//this function has high likelyhood of being absorbed or replaced into bulkRenderRow()
void partialRenderRow(chatHistory userEntry, u_int16_t y) {
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

void bulkRenderRow(std::vector<chatHistory> myHistory, int startIndex, bool biasUP) {

  canvas.fillScreen(GxEPD_WHITE);
  canvas.setTextColor(GxEPD_WHITE);

  int cursor = 0;

  //the purpose for biasUP and biasDOWN is to format overflow entries (where large text inputs have to wrap down to a new line)
  //this gives logic behind aligning overflow entries and new entries

  if (biasUP) { //biasUP generates from top to bottom, the next entries are aligned to top
    Serial.println("Bias UP");
    y = 2; //bias up, start from top of EPD screen

    while (y < 114) {
      chatHistory renderEntry = myHistory.at(startIndex + cursor);
      rowMultiple = ceil((float)renderEntry.txtboxHeight / rowHeight);

      canvas.fillRect(renderEntry.x, y, renderEntry.txtboxWidth, rowHeight * rowMultiple, GxEPD_BLACK);
      canvas.setCursor(renderEntry.x - renderEntry.fontOffsetX, y - renderEntry.fontOffsetY + 1); //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
      canvas.print(renderEntry.userInput);

      y += (rowHeight * rowMultiple); //adjust the y cursor according to text height
      cursor++;
    }
  }
  else { //biasDOWN generates from bottom to top, the next entries are aligned to bottom
    Serial.println("Bias DOWN");
    y = 114; //bias down, start from the bottom of EPD screen

    startIndex = (int)myHistory.size() - 1; //might be redundant

    while (y > 2) {
      chatHistory renderEntry = myHistory.at(startIndex - cursor);
      rowMultiple = ceil((float)renderEntry.txtboxHeight / rowHeight);

      y -= (rowHeight * rowMultiple); //adjust the y cursor according to text height

      canvas.fillRect(renderEntry.x, y, renderEntry.txtboxWidth, rowHeight * rowMultiple, GxEPD_BLACK);
      canvas.setCursor(renderEntry.x - renderEntry.fontOffsetX, y - renderEntry.fontOffsetY + 1); //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
      canvas.print(renderEntry.userInput);

      cursor++;
    }
  }

}
