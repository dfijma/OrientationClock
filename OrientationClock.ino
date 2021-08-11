
#include <Wire.h> 
#include <Adafruit_GFX.h>
#include <limits.h>

#include "Adafruit_LEDBackpack.h"

#include "RTClib.h"

Adafruit_7segment matrix = Adafruit_7segment();

const int I2C_7SEGMENT = 0x70;
const int BUTTON_UP_PIN = 8;
const int BUTTON_DOWN_PIN = 7;
const int BUZZER_PIN = 9;
const int ORIENTATION_PIN = 12;

// RTC_Millis rtc;
RTC_DS1307 rtc = RTC_DS1307();

void setup() {
  matrix.begin(0x70);
  matrix.setBrightness(5);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(ORIENTATION_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(115200);
  // rtc.begin(DateTime(F(__DATE__), F(__TIME__)));
  rtc.begin();
}

int hours[2] = {0};
int minutes[2] = {0};

struct BUTTON_STATE {
  int activeState; // active or inactive
  int holdState; // if active, normal or hold
  unsigned long activeEntered;
  unsigned long lastIncrementedDuringHold;  
};

BUTTON_STATE buttonsState = { HIGH, HIGH, 0L, 0L };
unsigned long lastTriggerDisplayAlarm = LONG_MIN;

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

void drawHoursMinutes(int orientation, int hours, int minutes) {
  drawDigit(orientation ? 3 : 0, hours / 10, false, orientation);
  drawDigit(orientation ? 2 : 1, hours % 10, false, orientation);
  drawDigit(orientation ? 1 : 2, minutes / 10, false, orientation);
  drawDigit(orientation ? 0 : 3, minutes % 10, false, orientation);
}

void loop() {

    // get input and current time
    int orientation = digitalRead(ORIENTATION_PIN); // "Normal" orientation == LOW (tilt sensor closed, active LOW)
    int buttonDown = digitalRead(BUTTON_DOWN_PIN);
    int buttonUp = digitalRead(BUTTON_UP_PIN);
    unsigned long currentMillis = millis();
    DateTime now = rtc.now();


    bool setRTC = false;

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

    // are we displaying actual clock time or buffer?
    boolean showBufferTime = (currentMillis - lastTriggerDisplayAlarm) < 5000;

    // the most complicated part: the key processing state machine
    if (buttonsState.activeState == HIGH) {
      // current state: inactive
      if (button == LOW) {
          if (orientation == 0) {
            hours[0] = now.hour();
            minutes[0] = now.minute();
          }
        if (!showBufferTime) {
          showBufferTime = true;
        } else {

          minutes[orientation] = minutes[orientation] + activeButton;
        }
        // going to 'active'
        buttonsState.activeState = LOW;
        buttonsState.holdState = HIGH;
        buttonsState.activeEntered = currentMillis;
        lastTriggerDisplayAlarm = currentMillis;
      }
    } else if (buttonsState.activeState == LOW) {
      // active or active hold
      if (button == HIGH) {
        // going to inactive
        buttonsState.activeState = HIGH;
        if (orientation == 0 ) { 
          setRTC = true;
        }

      } else {
        if (buttonsState.holdState == HIGH) {
          // active, but not (yet) holding
          if ((currentMillis - buttonsState.activeEntered) > 2000) {
            buttonsState.holdState = LOW;
          }
        } else {
          // active hold
          if ((currentMillis - buttonsState.lastIncrementedDuringHold) > 500) {
            minutes[orientation] += 30 * activeButton; 
            buttonsState.lastIncrementedDuringHold = currentMillis;
            lastTriggerDisplayAlarm = currentMillis;
          }
        }
      }
    } 


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

    if (setRTC) {
      rtc.adjust(DateTime(2020,1,1, hours[0], minutes[0], 0));
    }
    
    // update display   
    if (showBufferTime) {
      drawHoursMinutes(orientation, hours[orientation], minutes[orientation]);   
      matrix.drawColon(true);   
    } else {
      drawHoursMinutes(orientation, now.hour(), now.minute());
      matrix.drawColon(now.second() % 2 == 0);   
    }
    matrix.writeDisplay();

    // small delay for button debounce
    delay(10);
        
}
