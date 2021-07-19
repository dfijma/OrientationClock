
#include <Wire.h> 
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_7segment matrix = Adafruit_7segment();

const int I2C_7SEGMENT = 0x70;
const int BUTTON_UP_PIN = 8;
const int BUTTON_DOWN_PIN = 7;
const int BUZZER_PIN = 9;
const int ORIENTATION_PIN = 10;

void setup() {
  matrix.begin(0x70);
  matrix.setBrightness(5);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(ORIENTATION_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(115200);
}

int hours = 0;
int minutes = 0;
int seconds = 0;
int ms = 0;

struct BUTTON_STATE {
  int activeState; // active or inactive
  int holdState; // if active, normal or hold
  unsigned long activeEntered;
  unsigned long lastIncrementedDuringHold;  
};

BUTTON_STATE buttonsState = { HIGH, HIGH, 0L, 0L };


unsigned long previousMillis = 0;

static const uint8_t numbertable[2][10] = {{
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F, /* 9 */
}, {
    0x3F, /* 0 */
    0x30, /* 1 */
    0x5B, /* 2 */
    0x79, /* 3 */
    0x74, /* 4 */
    0x6D, /* 5 */
    0x6F, /* 6 */
    0x38, /* 7 */
    0x7F, /* 8 */
    0x7D, /* 9 */
}};

void drawDigit(int pos, int x, boolean dot, int orientation) {
  if (pos > 1) pos++;
  matrix.writeDigitRaw(pos, numbertable[orientation][x] | (dot << 7));
}
void drawHoursMinutes(boolean orientation) {
  drawDigit(orientation ? 3 : 0, hours / 10, false, orientation);
  drawDigit(orientation ? 2 : 1, hours % 10, false, orientation);
  drawDigit(orientation ? 1 : 2, minutes / 10, false, orientation);
  drawDigit(orientation ? 0 : 3, minutes % 10, false, orientation);
}

void loop() {

    // orientation switch closed == LOW (wires down) == "NORMAL" // switch open == HIGH, consider "upside down"
    boolean orientation = digitalRead(ORIENTATION_PIN) == HIGH;
    int buttonDown = digitalRead(BUTTON_DOWN_PIN);
    int buttonUp = digitalRead(BUTTON_UP_PIN);

    // just one button can be active (up overrides down)
    int activeButton = 1;
    int button = HIGH; 
    if (buttonUp == LOW) {
      activeButton = 1;
      button = LOW;
    } else if (buttonDown == LOW) {
      activeButton = -1;
      button = LOW;
    }
    

    if (orientation == 0) {
      noTone(BUZZER_PIN);            
    } else {
      tone(BUZZER_PIN, 220);
    }
    
    drawHoursMinutes(orientation);
    matrix.drawColon(seconds % 2 == 0);
    matrix.writeDisplay();

    // update the time (only if no button is active)
    unsigned long currentMillis = millis();
    if (button == HIGH) {
      ms += int (currentMillis - previousMillis);
    }
    previousMillis = currentMillis;

    if (buttonsState.activeState == HIGH) {
      // inactive
      if (button == LOW) {
        // going to 'active'
        buttonsState.activeState = LOW;
        buttonsState.holdState = HIGH;
        buttonsState.activeEntered = currentMillis;
        minutes = minutes + activeButton;
      }
    } else if (buttonsState.activeState == LOW) {
      // active or active hold
      if (button == HIGH) {
        // going to inactive
        buttonsState.activeState = HIGH;
        seconds = 0;
        ms = 0;
      } else {
        if (buttonsState.holdState == HIGH) {
          // active, but not (yet) holding
          if ((currentMillis - buttonsState.activeEntered) > 2000) {
            buttonsState.holdState = LOW;
            minutes += 30 * activeButton;
            buttonsState.lastIncrementedDuringHold = currentMillis;
          }
        } else {
          // active hold
          if ((currentMillis - buttonsState.lastIncrementedDuringHold) > 500) {
            minutes += 30 * activeButton; 
            buttonsState.lastIncrementedDuringHold = currentMillis;
          }
        }
      }
    } 
        
    seconds += (ms / 1000);
    ms = ms % 1000;

    minutes += (seconds / 60);
    seconds = seconds % 60;

    if (minutes < 0) {
      hours--;
      minutes += 60;
    }
    
    hours += (minutes / 60);
    minutes = minutes % 60;

    hours = hours % 24;
    if (hours < 0) {
      hours += 24;
    }

    // small delay for button debounce
    delay(10);
        
}
