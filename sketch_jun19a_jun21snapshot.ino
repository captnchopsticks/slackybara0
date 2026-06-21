// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <algorithm> //Required for std::max() and std::find()
#include <map> // Required for std::map<> data structure
#include <cmath> //Required for ceiling function for rowMultiple var

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
struct historyEntry {
  String userInput;
  bool textAlignment;
};
historyEntry chatHistory[15];
int historyCount = 0;

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
    historyCount = 0;
    y = 2;
  }

  chatHistory[historyCount].userInput = userInput;
  chatHistory[historyCount].textAlignment = textAlignment;
  Serial.println(chatHistory[historyCount].userInput);
  Serial.print("alignment: "); Serial.println(chatHistory[historyCount].textAlignment);
  Serial.print("historyCount: "); Serial.println(historyCount);

  renderRow(userInput, y, textAlignment);
  
  textAlignment = !textAlignment; //toggle between left and right
  y += (rowHeight * rowMultiple);
  historyCount++;

}

String collectUserInput(String serialPrompt) {
  Serial.print(serialPrompt);
  while (!Serial.available());
  String userInput = Serial.readStringUntil('\n');
  userInput.trim();
  return userInput;
}

//pass the string input, y, alignment, and func will handle printing.
void renderRow(String userInput, u_int16_t y, bool textAlignment) {
  int16_t fontOffsetX; //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
  int16_t fontOffsetY; //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
  u_int16_t txtboxWidth;
  u_int16_t txtboxHeight;
  u_int16_t x;
  display.getTextBounds(userInput, 0, 0, &fontOffsetX, &fontOffsetY, &txtboxWidth, &txtboxHeight);
  
  Serial.print("fontOffsetX: "); Serial.println(fontOffsetX);
  Serial.print("fontOffsetY: "); Serial.println(fontOffsetY);
  Serial.print("txtboxHeight: "); Serial.println(txtboxHeight);
  Serial.print("txtboxWidth: "); Serial.println(txtboxWidth);
  Serial.print("maxDisplayWidth: "); Serial.println(display.width());
  
  if (txtboxWidth >= 239) { //bug somewhere in the library? haven't figure out why not working
    txtboxWidth = 250;
  }

  x = textAlignment? 0: (display.width() - txtboxWidth); //ternary operator, true = leftAlign, false = rightAlign
  rowMultiple = ceil((float)txtboxHeight / rowHeight);
  //Serial.println(rowMultiple);

  display.setPartialWindow(x, y, txtboxWidth, rowMultiple * rowHeight); //I want height to be in multiples of rowHeight
  //display.setTextColor(GxEPD_BLACK);
  display.firstPage();
  do {
    display.fillRect(x, y, txtboxWidth, rowMultiple * rowHeight, GxEPD_BLACK);
    display.setCursor(x-fontOffsetX,y-fontOffsetY+1); //not all fonts coords have origin (0,0) at topL corner, so need to offset for alignment
    display.print(userInput);
  }
  while (display.nextPage());
  delay(50);
}
