
#include <Wire.h> 
#include <Adafruit_GFX.h>
#include <limits.h>
#include <EEPROM.h>

#include "Adafruit_LEDBackpack.h"
#include "RTClib.h"

Adafruit_7segment matrix = Adafruit_7segment();

const int I2C_7SEGMENT = 0x70;
const int BUTTON_UP_PIN = 8;
const int BUTTON_DOWN_PIN = 7;
const int BUZZER_PIN = 9;
const int ORIENTATION_PIN = 12;

RTC_DS1307 rtc = RTC_DS1307();

int hours[2] = {0};
int minutes[2] = {0};

void setup() {
  matrix.begin(0x70);
  matrix.setBrightness(5);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(ORIENTATION_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  // Serial.begin(115200);
  rtc.begin();
  EEPROM.get(0, hours[1]);
  EEPROM.get(sizeof(int), minutes[1]);
  // TODO: use a function (map, constrain?) to clip these values
  if (hours[1] < 0 || hours[1] > 23) hours[1] = 0; 
  if (minutes[1] < 0 || minutes[1] > 59) minutes[1] = 0;
}


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

inline int positive_modulo(int i, int n) {
    return (i % n + n) % n;
}

// state to generate buzzer
bool playing = false;
unsigned long nextChange = 0L;
const int note = 523;
const int noteDuration = 1000 / 4;

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
      activeButton = orientation == LOW ? 1 : -1;
      button = LOW;
    } else if (buttonDown == LOW) {
      activeButton = orientation == LOW ? -1 : 1;
      button = LOW;
    }

    // alarm?
    const int d = positive_modulo((now.hour() * 60 + now.minute() - (hours[1] * 60 + minutes[1])), 1440); // "now" - "alarm"
    const bool shouldBuz = orientation == 1 && d >=0 && d < 10;
    
    // while 'shoudBuz', flip between note and silence
    if (!playing && shouldBuz) {
      if (currentMillis > nextChange) {
        playing = true;
        tone(BUZZER_PIN, note);
        nextChange = currentMillis + noteDuration;
      }
    } else {
      if (currentMillis > nextChange) {
        playing = false;
        noTone(BUZZER_PIN);
        nextChange = currentMillis + noteDuration * 1.3;
      }
    }

    // are we displaying actual clock time or buffer?
    boolean showBufferTime = (currentMillis - lastTriggerDisplayAlarm) < 5000;

    // the most complicated part: the key processing state machine
    if (buttonsState.activeState == HIGH) {
      // current state: INACTIVE (clock is running)
      
      if (button == LOW) {
        // going to ACTIVE (setting time of alarm, not yet 'holding')

        // init clock-setbuffer from RTC
        hours[0] = now.hour();
        minutes[0] = now.minute();

        // first key is to display the alarm- or clock-setbuffer, subsequent presses increment or decrement the buffer
        if (showBufferTime) {
          // if already showing the buffer, increment of decrement it
          minutes[orientation] = minutes[orientation] + activeButton;
        } else {
          //... otherwise, 'eat' this press to display the buffer
          showBufferTime = true;
        }
        // shift state
        buttonsState.activeState = LOW;
        buttonsState.holdState = HIGH;
        buttonsState.activeEntered = currentMillis;
        lastTriggerDisplayAlarm = currentMillis;
      }
    } else if (buttonsState.activeState == LOW) {
      // ACTIVE or ACTIVE HOLD
      if (button == HIGH) {
        // going to INACTIVE
        buttonsState.activeState = HIGH;
        if (orientation == 0 ) { 
          // remember to set the RTC from buffer (but buffer needs to be corrected first)
          setRTC = true;
        } else {
          // save alarm time in EEPROM
          EEPROM.put(0, hours[1]);
          EEPROM.put(sizeof(int), minutes[1]);
        }

      } else {
        if (buttonsState.holdState == HIGH) {
          // ACTIVE, but not (yet) holding
          if ((currentMillis - buttonsState.activeEntered) > 2000) {
            // but now HOLDING, as 2000 millisecs elapsed since first press
            buttonsState.holdState = LOW;
          }
        } else {
          // ACTIVE HOLDING, increment or decrement with 30 minutes every 500 millieseconds
          if ((currentMillis - buttonsState.lastIncrementedDuringHold) > 500) {
            minutes[orientation] += 30 * activeButton; 
            buttonsState.lastIncrementedDuringHold = currentMillis;
            lastTriggerDisplayAlarm = currentMillis;
          }
        }
      }
    } 


    // correct buffer from over-/underflow
    for (int i=0; i<2; ++i) {

      // TODO: use positive_modulo
      if (minutes[i] < 0) {
        hours[i]--;
        minutes[i] += 60;
      }
      
      hours[i] += (minutes[i] / 60);
      minutes[i] = minutes[i] % 60;
  
      // TODO: use positive_modulo
      hours[i] = hours[i] % 24;
      if (hours[i] < 0) {
        hours[i] += 24; 
      }
    }

    // set RTC from buffer
    if (setRTC) {
      // we actually don't care about the date
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
