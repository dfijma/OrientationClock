

#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_7segment matrix = Adafruit_7segment();

const int ORIENTATION_PIN = 10;

void setup() {
  matrix.begin(0x70);
  matrix.setBrightness(5);
  pinMode(ORIENTATION_PIN, INPUT_PULLUP);
  pinMode(9, OUTPUT);
}

int hours = 0;
int minutes = 0;
int seconds = 0;


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

static const uint8_t numbertableUpsideDown[] = {
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
};

void drawDigit(int pos, int x, boolean dot, int orientation) {
  if (pos > 1) pos++;
  matrix.writeDigitRaw(pos, numbertable[orientation][x] | (dot << 7));
}

void drawValue(int x, int orientation) {
    if (orientation == 0) {
      drawDigit(0, x / 1000, false, orientation);
      drawDigit(1, (x / 100) % 10, false, orientation);
      drawDigit(2, (x / 10) % 10, false, orientation);
      drawDigit(3, x % 10, false, orientation);
    } else {
      drawDigit(3, x / 1000, false, orientation);
      drawDigit(2, (x / 100) % 10, false, orientation);
      drawDigit(1, (x / 10) % 10, false, orientation);
      drawDigit(0, x % 10, false, orientation);
    }
}

void loop() {

    // orientation switch closed == LOW (wires down) == "NORMAL" // switch open == HIGH, consider "upside down"
    int orientation = digitalRead(ORIENTATION_PIN) == HIGH ? 1 : 0;

    if (orientation == 0) {
      drawValue(hours*100 + minutes, 0);
      noTone(9);            
    } else {
      tone(9, 220);
      drawValue(hours*100 + minutes, 1);
    }
    
    matrix.drawColon(seconds % 2 == 0);

    matrix.writeDisplay();
    delay(1000);  
      // Now increase the seconds by one.
  
    seconds += 1;
    if (seconds > 59) {
      seconds = 0;
      minutes += 1;
      if (minutes > 59) {
        minutes = 0;
        hours += 1;
        if (hours > 23) {
          hours = 0;
        }
      }
    }
}
