#include <Arduino.h>
#include <FastLED.h>
#include <Encoder.h>
#include <OneButton.h>
#include <DigitalPin.h>

#define NUM_LEDS     255

#define PIN_LEDS       5
#define PIN_CLOCK      2
#define PIN_DATA       3
#define PIN_SW         4

#define MIN_TIME     512
#define MAX_TIME  512000

#define STEPS         32
#define FADE_BY       32

#define HUE         0x80
#define SAT         0xFF
#define VAL_HI      0x80
#define VAL_LO      0x40

#define CW           (1)
#define CCW         (-1)

#define toggle(pin)     digitalWrite(pin, !digitalRead(pin))

CRGB leds[NUM_LEDS];
Encoder pot(PIN_DATA, PIN_CLOCK);
DigitalPin led(LED_BUILTIN);
OneButton sw(PIN_SW);

CHSV color = CHSV(HUE, SAT, VAL_HI);
int32_t sleepTime = 400;

enum {
  READ_HUE = 0,
  READ_TIME
} reading;

enum {
  DIR_CW = -1,
  DIR_CCW = 1
} direction;

void toggleReading() {
  reading = reading == READ_HUE ? READ_TIME : READ_HUE;
}

enum Mode {
  MODE_SINGLE = 0,
  MODE_CW,
  MODE_CCW,
  MODE_RAINBOW_CW,
  MODE_RAINBOW_CCW,
  MODE_LAST
} mode;

void enterMode() {
  CHSV c(0, SAT, VAL_LO);
  switch (mode) {
    case MODE_RAINBOW_CW:
    case MODE_RAINBOW_CCW:
      reading = READ_TIME;
      for (uint8_t *i = &c.hue; *i < NUM_LEDS; (*i)++) {
        leds[*i] = c;
      }
      break;

    default:
      reading = READ_HUE;
      break;
  }
}

void nextMode() {
  mode = (Mode) (mode + 1);
  if (mode == MODE_LAST) {
    mode = MODE_CW;
  }
  enterMode();
}

void updateSingle() {
  CRGB rgb = color;
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = rgb;
  }
}

void updateWave(int8_t delta) {
  static int8_t rem;
  rem += delta;
  switch (rem) {
    case STEPS:
      rem = 0;
      break;

    case -1:
      rem = STEPS - 1;
      break;
  }

  fadeToBlackBy(leds, NUM_LEDS, FADE_BY);
  CRGB rgb = color;
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i % STEPS == rem) {
      leds[i] = rgb;
    }
  }
}

void updateRainbowCW() {
  CRGB saved = leds[NUM_LEDS - 1];
  for (int i = NUM_LEDS - 1; i > 0; i--) {
    leds[i] = leds[i - 1];
  }
  leds[0] = saved;
}

void updateRainbowCCW() {
  CRGB saved = leds[0];
  for (int i = 0; i < NUM_LEDS - 1; i++) {
    leds[i] = leds[i + 1];
  }
  leds[NUM_LEDS - 1] = saved;
}

void updateLeds() {
  switch (mode) {
    case MODE_SINGLE:
      updateSingle();
      break;

    case MODE_CW:
      updateWave(CW);
      break;

    case MODE_CCW:
      updateWave(CCW);
      break;

    case MODE_RAINBOW_CW:
      updateRainbowCW();
      break;

    case MODE_RAINBOW_CCW:
      updateRainbowCCW();
      break;
  }
}

void updateEncoder(int8_t change) {
  switch (reading) {
    case READ_HUE:
      color.hue += change;
      break;

    case READ_TIME:
      int32_t delta = -change;
      if (sleepTime > 100000) {
        sleepTime += delta * 10000;
      } else if (sleepTime > 10000) {
        sleepTime += delta * 1000;
      } else if (sleepTime > 1000) {
        sleepTime += delta * 100;
      } else if (sleepTime > 100) {
        sleepTime += delta * 10;
      } else {
        sleepTime += delta;
      }
      sleepTime = constrain(sleepTime, MIN_TIME, MAX_TIME);
      break;
  }
}

void readEncoder() {
  static int32_t previous;
  int32_t value = pot.read();
  if (value != previous) {
    auto change = (int8_t) (value - previous);
    updateEncoder(change);
    previous = value;
  }
}

void showLeds(uint64_t now) {
  static uint64_t last;
  auto elapsed = (int32_t) (now - last);
  if (elapsed > sleepTime) {
    last = now;

    updateLeds();
    FastLED.show();
    toggle(LED_BUILTIN);
  }
}

void loop() {
  const uint64_t now = micros();
  sw.tick();
  readEncoder();
  showLeds(now);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  CFastLED::addLeds<NEOPIXEL, PIN_LEDS>(leds, NUM_LEDS);
  sw.attachDoubleClick(toggleReading);
  sw.attachClick(nextMode);
  enterMode();
}
