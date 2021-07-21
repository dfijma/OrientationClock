
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

int hours[2] = {0};
int minutes[2] = {0};
int seconds = 0;
int ms = 0;

struct BUTTON_STATE {
  int activeState; // active or inactive
  int holdState; // if active, normal or hold
  unsigned long activeEntered;
  unsigned long lastIncrementedDuringHold;  
};

BUTTON_STATE buttonsState = { HIGH, HIGH, 0L, 0L };

unsigned long lastTriggerDisplayAlarm = 0;
int lastOrientation = -1; // unknown

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

void drawHoursMinutes(int orientation, int buffer) {
  drawDigit(orientation ? 3 : 0, hours[buffer] / 10, false, orientation);
  drawDigit(orientation ? 2 : 1, hours[buffer] % 10, false, orientation);
  drawDigit(orientation ? 1 : 2, minutes[buffer] / 10, false, orientation);
  drawDigit(orientation ? 0 : 3, minutes[buffer] % 10, false, orientation);
}

void loop() {

    // get input and current time
    int orientation = digitalRead(ORIENTATION_PIN); // "Normal" orientation == LOW (tilt sensor closed, active LOW)
    int buttonDown = digitalRead(BUTTON_DOWN_PIN);
    int buttonUp = digitalRead(BUTTON_UP_PIN);
    unsigned long currentMillis = millis();

    // just one button can be active (UP overrides DOWN)
    int activeButton = 1;
    int button = HIGH; 
    if (buttonUp == LOW) {
      activeButton = 1;
      button = LOW;
    } else if (buttonDown == LOW) {
      activeButton = -1;
      button = LOW;
    }

    // TODO buzz
    if (orientation == 0) {
      noTone(BUZZER_PIN);            
    } else {
      tone(BUZZER_PIN, 220);
    }

    // update time
    ms += int (currentMillis - previousMillis);
    previousMillis = currentMillis;
    
    // are we displaying the alarm time, or the clock time?
    boolean showAlarmTime = orientation && ((currentMillis - lastTriggerDisplayAlarm) < 5000) ? 1 : 0;

    // process orientation switch
    if (lastOrientation == 0 && orientation == 1) {
      lastTriggerDisplayAlarm = currentMillis;
      showAlarmTime = true;
    }
    lastOrientation = orientation;

    // the most complicated part: the key processing state machine
    if (buttonsState.activeState == HIGH) {
      // curren state: inactive
      if (button == LOW) {
        if (orientation == 1 && !showAlarmTime) {
          // if we were not showing alarm time already, "eat" the button press to wake it up
          showAlarmTime = true;
          lastTriggerDisplayAlarm = currentMillis;
          buttonsState.activeState = LOW;
          buttonsState.holdState = HIGH;
          buttonsState.activeEntered = currentMillis;
        } else {
          if (orientation == 1 && showAlarmTime) {
             lastTriggerDisplayAlarm = currentMillis;
          }
          // going to 'active'
          buttonsState.activeState = LOW;
          buttonsState.holdState = HIGH;
          buttonsState.activeEntered = currentMillis;
          minutes[orientation] = minutes[orientation] + activeButton;
        }
      }
    } else if (buttonsState.activeState == LOW) {
      // active or active hold
      if (button == HIGH) {
        // going to inactive
        buttonsState.activeState = HIGH;
        if (orientation == 0) {
          seconds = seconds % 2;
          ms = 0;
        }
      } else {
        if (buttonsState.holdState == HIGH) {
          // active, but not (yet) holding
          if ((currentMillis - buttonsState.activeEntered) > 2000) {
            buttonsState.holdState = LOW;
            minutes[orientation] += 30 * activeButton;
            buttonsState.lastIncrementedDuringHold = currentMillis;
            if (orientation == 1 && showAlarmTime) {
              lastTriggerDisplayAlarm = currentMillis;
            }
          }
        } else {
          // active hold
          if ((currentMillis - buttonsState.lastIncrementedDuringHold) > 500) {
            minutes[orientation] += 30 * activeButton; 
            buttonsState.lastIncrementedDuringHold = currentMillis;
            if (orientation == 1 && showAlarmTime) {
              lastTriggerDisplayAlarm = currentMillis;
            }
          }
        }
      }
    } 

    // update clock and/or alarm time    
    seconds += (ms / 1000);
    ms = ms % 1000;

    minutes[0] += (seconds / 60);
    seconds = seconds % 60;

    for (int i=0; i<2; ++i) {
      if (minutes[i] < 0) {
        hours[i]--;
        minutes[i] += 60;
      }
      
      hours[i] += (minutes[i] / 60);
      minutes[i] = minutes[i] % 60;
  
      hours[i] = hours[i] % 24;
      if (hours[i] < 0) {
        hours[i] += 24;
      }
    }

    // update display   
    drawHoursMinutes(orientation, showAlarmTime);
    if (showAlarmTime) {
      matrix.drawColon(true);
    } else {
      if (buttonsState.activeState == LOW && buttonsState.holdState == LOW) {
        matrix.drawColon(false);
      } else {
        matrix.drawColon(seconds % 2 == 0);
      }
    }
    matrix.writeDisplay();

    // small delay for button debounce
    delay(10);
        
}
