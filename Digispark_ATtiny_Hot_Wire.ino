/*
 * Hot Wire game using an ATtiny85 as its core
 * 
 * Author: NeoBurn (github.com/TheNeoBurn)
 * 
 * The game:
 *  - Start the game by starting up the ATtiny85 or reset it
 *  - Move the loop along the wire without touching the wire
 *  - When the loop touches the wire, a life is lost (warn sound)
 *  - When the wire is touched the third time, the game is lost
 *  - When the goal is touched, the game is won (plays melody)
 *
 * 
 * My wiring using a Digispark Rev3:
 *       ┌─────────────┐        ┌──[51kΩ]───────────○ Goal
 *       │        PB5 ○│─────┬──┴───────────────────○ Reset
 * ┌─────┘        PB4 ○│─────╪──[Poti]──[Piezo]──┐
 * │              PB3 ○│──┬──╪───────────────────╪──○ Wire
 * │       + G V  PB2 ○│──╪──╪──[100Ω]────Red►|──┤
 * │       5 N I  PB1 ○│──╪──╪──[100Ω]─Yellow►|──┤
 * └─────┐ V D N  PB0 ○│──╪──╪──[100Ω]──Green►|──┤
 *       │ ○ ○ ○       │  │  │                   │
 *       └─────────────┘  │  │  ╪ := jumping     │
 *         └─╪─╪─┬─[10kΩ]─┘  │        over       │
 * Bat+ ○────╪─┘ └────[10kΩ]─┘                   │
 * Bat- ○────┴───────────────────────────────────┴──○ Loop
 *      
 * 
 * My wiring using a bare ATtiny85 chip:
 *                 ┌───[10kΩ]────┐   ╪ := jumping
 *                 │ ┌─[10kΩ]────┤         over
 *  Goal ○──[51kΩ]─┤ │ ┌──────┐  │
 * Reset ○─────────┴─╪─┤1 AT 8├──┴──────────────────○ Bat+
 *  Wire ○───────────┴─┤2 ti 7├──[100Ω]────Red►|─┐
 *        ┌──[Poti]────┤3 ny 6├──[100Ω]─Yellow►|─┤
 *        └─[Piezo]─┬──┤4 85 5├──[100Ω]──Green►|─┤
 *                  │  └──────┘                  │
 *  Loop ○──────────┴────────────────────────────┴──○ Bat-
 * 
 * 
 * A advise to use an ArduinoISP programmer with as well a bare ATtiny85 chip as a
 * Digispark. The latter will lose its USB funcionality (at least as long as the 
 * bootloader is flashed again) but like this the 5 second pause will be removed.
 * 
 *    ArduinoISP     Digispark    ATtiny85 Pins
 *        5V   -------  5V   --------  8
 *        GND  -------  GND  --------  4
 *        10   -------  PB5  --------  1
 *        11   -------  PB0  --------  5
 *        12   -------  PB1  --------  6
 *        13   -------  PB2  --------  7
 */


// Load NOTE_* definitions for melodies
#include "Notes.h"
// When not using 16MHz clock, you can tune the sound speed here:
// {BaseLength} * TONE_MULTI / TONE_DIV       (default * 7 / 3)
#define TONE_MULTI 7
#define TONE_DIV 3
// When not using 16MHz clock, you can tune the slide sound step length here (default: 10)
#define SLIDE_LENGTH 10
// The threshold of the analog read value when the reset pin is read as goal when below
#define GOAL_THRESHOLD 900
// The goal melody to use (a melody is a int[] of note, length pairs, see examples)
#define MELODY Beethoven
// Set your pins here (G := green, Y := yellow, R := red, W := wire, A := speaker)
#define PIN_G 0
#define PIN_Y 1
#define PIN_R 2
#define PIN_W 3
#define PIN_A 4


const int Entertainer[] = {
    NOTE_D5, 53, NOTE_DS5, 53, NOTE_E5, 53, NOTE_C6, 107, NOTE_E5, 53, NOTE_C6, 107, NOTE_E5, 53, NOTE_C6, 321, 
    NOTE_C6, 53, NOTE_D6, 53, NOTE_DS6, 53, NOTE_E6, 53, NOTE_C6, 53, NOTE_D6, 53, NOTE_E6, 107, NOTE_B5, 53, 
    NOTE_D6, 107, NOTE_C6, 214, NOTE_P, 107, NOTE_D5, 53, NOTE_DS5, 53, NOTE_E5, 53, NOTE_C6, 107, NOTE_E5, 53, 
    NOTE_C6, 107, NOTE_E5, 53, NOTE_C6, 321, NOTE_P, 53, NOTE_A5, 53, NOTE_G5, 53, NOTE_FS5, 53, NOTE_A5, 53, 
    NOTE_C6, 53, NOTE_E6, 107, NOTE_D6, 53, NOTE_C6, 53, NOTE_A5, 53, NOTE_D6, 214
};

const int Beethoven[] = {
    NOTE_E6, 60, NOTE_DS6, 60, NOTE_E6, 60, NOTE_DS6, 60, NOTE_E6, 60, NOTE_B5, 60, NOTE_D6, 60, NOTE_C6, 60, 
    NOTE_A5, 120, NOTE_P, 60, NOTE_C5, 60, NOTE_E5, 60, NOTE_A5, 60, NOTE_B5, 120, NOTE_P, 60, NOTE_E5, 60, 
    NOTE_GS5, 60, NOTE_B5, 60, NOTE_C6, 120, NOTE_P, 60, NOTE_E5, 60, NOTE_E6, 60, NOTE_DS6, 60, NOTE_E6, 60, 
    NOTE_DS6, 60, NOTE_E6, 60, NOTE_B5, 60, NOTE_D6, 60, NOTE_C6, 60, NOTE_A5, 120, NOTE_P, 60, NOTE_C5, 60, 
    NOTE_E5, 60, NOTE_A5, 60, NOTE_B5, 120, NOTE_P, 60, NOTE_E5, 60, NOTE_C6, 60, NOTE_B5, 60, NOTE_A5, 120
};


// This is used to maintain the current state between loops
int currState = 0;


void setup() {
  // Here we set the LED pins to output
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_Y, OUTPUT);
  pinMode(PIN_R, OUTPUT);
  // The wire pin to input
  pinMode(PIN_W, INPUT_PULLUP);
  // And the audio pin to output
  pinMode(PIN_A, OUTPUT);
  // The reset pin 5 (analog 0) is used as analog input (possible if the value doesn't drop too low)
  pinMode(5, INPUT_PULLUP);

  // Here we set out start state 0
  currState = 0;
  // Switch the LEDs to green
  setState(0);
  // ANd play the start sound
  playStart();
}

// Sets the LEDs according to the state
void setState(int state) {
  switch (state) {
    case 0:
      // Start: Green on, everything else off
      digitalWrite(PIN_G, HIGH);
      digitalWrite(PIN_Y, LOW);
      digitalWrite(PIN_R, LOW);
      break;
    case 1:
      // 1. Warn: Green and yellow on, red off
      digitalWrite(PIN_G, HIGH);
      digitalWrite(PIN_Y, HIGH);
      digitalWrite(PIN_R, LOW);
      break;
    case 2:
      // 2. Warn: Yellow on, everything else off
      digitalWrite(PIN_G, LOW);
      digitalWrite(PIN_Y, HIGH);
      digitalWrite(PIN_R, LOW);
      break;
    default:
      // Lost: Red on, everything else off
      digitalWrite(PIN_G, LOW);
      digitalWrite(PIN_Y, LOW);
      digitalWrite(PIN_R, HIGH);
      break;
  }
}

// This function is used to play a sliding note from one frequency to another
void playSlide(int freq1, int freq2, int step) {
  if (PIN_A < 0) return;
  if (freq1 < freq2) {
    for (int i = freq1; i < freq2; i += step) {
      tone(PIN_A, i, SLIDE_LENGTH);
      delay(SLIDE_LENGTH);
    }    
  } else {
    for (int i = freq1; i > freq2; i -= step) {
      tone(PIN_A, i, SLIDE_LENGTH);
      delay(SLIDE_LENGTH);
    }
  }
}

// Play a start sound, 2 deep, 1 high note
void playStart() {
  tone(PIN_A, 220, 53 * TONE_MULTI / TONE_DIV);
  delay(53 * TONE_MULTI / TONE_DIV + 20);
  tone(PIN_A, 220, 53 * TONE_MULTI / TONE_DIV);
  delay(53 * TONE_MULTI / TONE_DIV + 20);
  tone(PIN_A, 440, 53 * TONE_MULTI / TONE_DIV);
  delay(53 * TONE_MULTI / TONE_DIV + 20);
}

// Play warn sound, sliding tone up long, down, up
void playWarn() {
  playSlide(220, 1600, 26 * TONE_MULTI / TONE_DIV);
  playSlide(1600, 880, 26 * TONE_MULTI / TONE_DIV);
  playSlide(880, 1600, 26 * TONE_MULTI / TONE_DIV);
  delay(26 * TONE_MULTI / TONE_DIV);
}

// Play loosing sound, falling tone
void playLoose() {
  playSlide(540, 780, 12);
  delay(100);
  playSlide(540, 700, 8);
  delay(100);
  playSlide(540, 653, 6);
  for (int n = 0; n < 4; n++) {
    playSlide(653, 600, 6);
    playSlide(600, 653, 6);
  } 
}

// Play a winning sound, a slide up
void playWin() {
  playSlide(600, 1200, 30);
  delay(100);
}

// Play the defined melody
void playMelody() {
  int len = sizeof(MELODY) / sizeof(int) - 1;
  int note, duration;
  for (int i = 0; i < len; i += 2) {
    note = MELODY[i];
    duration = MELODY[i | 1] * TONE_MULTI / TONE_DIV;
    tone(PIN_A, note, duration);
    delay(duration + 20);
  }
}

// Check again and again
void loop() {
  if (currState < 3) { // If we haven't lost or won yet
    if (digitalRead(PIN_W) == LOW) { // If the wire is touched
      currState++; // adjust current state by 1 up
      setState(currState); // set LEDs
      if (currState == 3) { // If new state is lost
        playLoose(); // play  loose sound
      } else {
        playWarn(); // else play warn sound
      }
    } else if (analogRead(0) < GOAL_THRESHOLD) { // win switch is touched
      /* The reset pin is analog 0, the value must be higher than about 600 to avoid a
       * reset and can be archived by a voltage divider circuit like this:
       * PIN 5 O┬───[10k]────────────O VCC
       *        └───[51k]──[Switch]──O GND                                    */ 
      playWin();
      currState = 100;
      delay(500);
    }
  } else if (currState == 100) {
    playMelody();
  }
}
