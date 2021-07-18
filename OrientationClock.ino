
#include <Wire.h> 
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_7segment matrix = Adafruit_7segment();

const int I2C_7SEGMENT = 0x70;
const int BUZZER_PIN = 9;
const int ORIENTATION_PIN = 10;

void setup() {
  matrix.begin(0x70);
  matrix.setBrightness(5);
  pinMode(ORIENTATION_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
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

void drawDigit(int pos, int x, boolean dot, int orientation) {
  if (pos > 1) pos++;
  matrix.writeDigitRaw(pos, numbertable[orientation][x] | (dot << 7));
}

void drawHoursMinutes(int orientation) {
  drawDigit(0, hours / 10, false, orientation);
  drawDigit(1, hours % 10, false, orientation);
  drawDigit(2, minutes / 10, false, orientation);
  drawDigit(3, minutes % 10, false, orientation);
}

void loop() {

    // orientation switch closed == LOW (wires down) == "NORMAL" // switch open == HIGH, consider "upside down"
    int orientation = digitalRead(ORIENTATION_PIN) == HIGH ? 1 : 0;

    if (orientation == 0) {
      drawHoursMinutes(0);
      noTone(BUZZER_PIN);            
    } else {
      tone(BUZZER_PIN, 220);
      drawHoursMinutes(1);
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
