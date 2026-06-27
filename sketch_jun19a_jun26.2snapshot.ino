// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <algorithm> //Required for std::max() and std::find()
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
  u_int16_t rowMultiple;
};

chatHistory messageEntry;
chatHistory cursorSelection;

std::vector<chatHistory> messageHistory;
bool initFlag = false;
int nextBottomIndex = 1;
int nextTopIndex = 0;

int cursorY;
int cursorIndex = 0;
int prevSelectY;
int prevSelectIndex = 1;

bool colorInvert = true; //true for black highlight, false for no highlight

void setup() {
  Serial.begin(115200);

  while(!Serial); // Wait for serial monitor to open

  display.init(0,true,50,false);
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  display.setTextWrap(true);

  canvas.setFont(&FreeMonoBold9pt7b);
  canvas.fillScreen(GxEPD_WHITE);

  delay(1000);
}

void loop() {
  String userInput = collectUserInput("type something to put onto display: ");
  Serial.println(userInput);

  if (userInput != "^^" && userInput != "VV") {
    messageEntry = prepareRow(userInput, textAlignment); //prepare the struct with all txtbox information
    messageHistory.push_back(messageEntry); //append that struct to messageHistory

    int checkY = y + (rowHeight * messageEntry.rowMultiple);
    if (checkY > 114) {
      initFlag = true; //flag needs to just turn true once
      nextBottomIndex = (int)messageHistory.size() - 1;
      Serial.println("initFlag = true; no longer doing partialRenderRow");
    }

    if (initFlag) { //initFlag dynamically handles overflow rows as opposed to a fixed checker of (int)messageHistory.size() > 7
      //refresh canvas for every autoscroll down
      cursorIndex = (int)messageHistory.size() - 1;
      cursorSelection = messageHistory.at(cursorIndex);
      cursorY = 114 - cursorSelection.rowMultiple * rowHeight;
      
      bulkRenderRow(messageHistory, nextBottomIndex, false, false);
      partialCanvasRender(cursorSelection, cursorY, true); //highlight the cursorSelection from the bottom
      displayCanvasBuffer();

      nextBottomIndex++;
    }
    else {
      partialCanvasRender(messageEntry, y, false);
      displayCanvasBuffer();
      y += (rowHeight * messageEntry.rowMultiple);
    }

    textAlignment = !textAlignment; //toggle between left and right
  }
  else if (cursorIndex > 0 && userInput == "^^") {
    if (cursorIndex == (nextTopIndex + 1)) { //if cursor is at the top of screen, shift window up
      bulkRenderRow(messageHistory, nextTopIndex, true, false);
      partialCanvasRender(messageHistory.at(cursorIndex - 1), 2, true);
      displayCanvasBuffer();

      nextTopIndex--;
      cursorIndex--;
    }
    else {
      moveCursor(true);
    }
  }
  else if (cursorIndex < ((int)messageHistory.size() - 1) && userInput == "VV") {
    if (cursorIndex == nextBottomIndex - 1) {
      cursorSelection = messageHistory.at(cursorIndex + 1);
      bulkRenderRow(messageHistory, nextBottomIndex, false, false);
      partialCanvasRender(cursorSelection, (114 - cursorSelection.rowMultiple * rowHeight), true);
      displayCanvasBuffer();

      nextBottomIndex++;
      cursorIndex++;
    }
    else {
      moveCursor(false);
    }
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
  userEntry.rowMultiple = ceil((float)userEntry.txtboxHeight / rowHeight);
  //Serial.println(userEntry.userInput);
  return userEntry;
}

void partialCanvasRender(chatHistory &userEntry, u_int16_t y, bool colorInvert) {
  u_int16_t textColor = colorInvert ? GxEPD_WHITE : GxEPD_BLACK;
  u_int16_t backgroundColor = colorInvert ? GxEPD_BLACK : GxEPD_WHITE;

  canvas.setTextColor(textColor);
  canvas.fillRect(userEntry.x, y, userEntry.txtboxWidth, rowHeight * userEntry.rowMultiple, backgroundColor);
  canvas.setCursor(userEntry.x - userEntry.fontOffsetX, y - userEntry.fontOffsetY + 1); //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
  canvas.print(userEntry.userInput);
}

void moveCursor(bool cursorDirection) { //true for UP, false for DOWN
  prevSelectIndex = cursorIndex;
  prevSelectY = cursorY;

  if (cursorDirection) {
    cursorIndex--;
    cursorY -= rowHeight * messageHistory.at(cursorIndex).rowMultiple;
  }
  else {
    cursorIndex++;
    cursorY += rowHeight * messageHistory.at(prevSelectIndex).rowMultiple;
  }
  //Serial.print("cursorIndex: "); Serial.println(cursorIndex);
  partialCanvasRender(messageHistory.at(prevSelectIndex), prevSelectY, false);
  partialCanvasRender(messageHistory.at(cursorIndex), cursorY, true);
  displayCanvasBuffer();
}

void bulkRenderRow(std::vector<chatHistory> &myHistory, int startIndex, bool biasTOP, bool colorInvert) {
  int cursor = 0;
  u_int16_t textColor = colorInvert ? GxEPD_WHITE : GxEPD_BLACK;
  u_int16_t backgroundColor = colorInvert ? GxEPD_BLACK : GxEPD_WHITE;

  canvas.fillScreen(GxEPD_WHITE);
  canvas.setTextColor(textColor);
  //canvas.setTextColor(GxEPD_BLACK);

  if (biasTOP) { //biasTOP generates from top to bottom, the next entries are aligned to top
    //Serial.println("Bias UP");
    y = 2; //bias up, start from top of EPD screen
    while (y < 114) {
      chatHistory renderEntry = myHistory.at(startIndex + cursor);

      canvas.fillRect(renderEntry.x, y, renderEntry.txtboxWidth, rowHeight * renderEntry.rowMultiple, backgroundColor);
      canvas.setCursor(renderEntry.x - renderEntry.fontOffsetX, y - renderEntry.fontOffsetY + 1); //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
      canvas.print(renderEntry.userInput);

      y += (rowHeight * renderEntry.rowMultiple); //adjust the y cursor according to text height
      cursor++;
    }

    nextBottomIndex = (y == 114) ? (startIndex + cursor) : (startIndex + (cursor -1));
  }
  else { //biasBOTTOM generates from bottom to top, the next entries are aligned to bottom
    //Serial.println("Bias DOWN");
    y = 114; //bias down, start from the bottom of EPD screen
    while (y > 2) {
      chatHistory renderEntry = myHistory.at(startIndex - cursor);
      y -= (rowHeight * renderEntry.rowMultiple); //adjust the y cursor according to text height

      canvas.fillRect(renderEntry.x, y, renderEntry.txtboxWidth, rowHeight * renderEntry.rowMultiple, backgroundColor);
      canvas.setCursor(renderEntry.x - renderEntry.fontOffsetX, y - renderEntry.fontOffsetY + 1); //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
      canvas.print(renderEntry.userInput);

      cursor++;
    }

    nextTopIndex = (y == 2) ? (startIndex - cursor) : (startIndex - (cursor - 1));
  }
}

//found the firstpage-nextpage approach more cleaner than drawPaged() approach
void displayCanvasBuffer() {
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do {
    //.drawBitmap() height and width arguments are inverted because display.setRotation = 1
    display.drawBitmap(0, 0, canvas.getBuffer(), display.width(), display.height(), GxEPD_WHITE, GxEPD_BLACK);
  }
  while (display.nextPage());
  delay(50);
}