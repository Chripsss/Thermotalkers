#include "Arduino.h"
#include "Talkie.h"
#include "Vocab_US_Large.h"
#include "Vocab_US_Acorn.h"
#include "Vocab_Special.h"

Talkie voice;

extern const uint8_t sp2_DERAJATCELCIUS[] PROGMEM = {0x00,0x00,0x00,0xE0,0x20,0x8D,0xD5,0x11,0x8C,0xB6,0x41,0x11,0xA3,0xAA,0x04,0x79,0x83,0x22,0xD5,0x58,0x32,0xB2,0x06,0x45,0xEA,0x11,0x33,0xE6,0x05,0x8A,0x38,0xC3,0xE6,0xCC,0x0A,0x14,0x71,0x4A,0xC3,0xD0,0x15,0x28,0xE2,0x94,0x86,0xA3,0x2B,0x50,0x86,0x2E,0x6B,0xE2,0x5A,0x8C,0x48,0x55,0x51,0xA8,0x25,0x41,0x91,0x23,0x27,0x48,0x6D,0x83,0xB2,0x44,0x4D,0x30,0xB3,0x01,0x55,0xCA,0xAE,0x56,0xD6,0x03,0x1A,0x9F,0x1D,0xCB,0x6A,0x1A,0x54,0xA9,0xA6,0x8A,0x99,0x18,0x28,0x42,0x64,0x2A,0x2B,0x35,0x50,0xF2,0xA9,0x0A,0x66,0x52,0xA0,0xF4,0x59,0x13,0x90,0x76,0x40,0x59,0xB3,0x27,0x99,0xEE,0x80,0xAA,0x66,0x77,0x09,0x93,0x06,0x65,0xC9,0xDA,0x44,0xB9,0x01,0xCA,0x9C,0x39,0x09,0xCE,0x10,0x14,0x35,0xA3,0x1C,0x9D,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xBA,0x89,0xB8,0xBB,0xB9,0x62,0x04,0x8A,0xF4,0x40,0xA0,0xE9,0x70,0x03,0x59,0xEE,0xCE,0x04,0xCE,0x05,0x4A,0x9F,0xDD,0x8D,0x5E,0x1A,0x54,0xB1,0xB2,0x82,0xD4,0x36,0xA8,0x6A,0x65,0x27,0x32,0x2B,0x50,0xF6,0xF6,0x2C,0x54,0x13,0xA0,0x48,0xA3,0xB1,0xE0,0xC6,0x40,0x3A,0x47,0x7C,0xD0,0x89,0x80,0x5A,0x74,0x87,0x99,0x1C,0x05,0xAA,0x75,0x27,0xA0,0x1A,0x0B,0x05,0x25,0xB3,0xEE,0x31,0xC8,0x0E,0x0A,0x1E,0x5F,0x0A,0xCA,0x1D,0x94,0xCC,0x7E,0x0D,0xD4,0x05,0x24,0x85,0x7E,0x07,0x22,0x75,0x84,0xC5,0x3C,0x0F,0x42,0x6C,0x28,0x5D,0xD5,0x9E,0x82,0x44,0x30,0x52,0xB3,0x2D,0x09,0xAD,0xE2,0x46,0xA9,0x65,0x05,0x65,0x06,0xB2,0x74,0x27,0x30,0x2B,0x0F,0xB7,0x90,0x4D,0x40,0x56,0x91,0x08,0x64,0x11,0x81,0x40,0x12,0x11,0x08,0x44,0x19,0x81,0x44,0xE0,0x1E,0xEE,0x26,0x05,0x09,0x2F,0x23,0x22,0x5D,0x0E,0x3C,0x10,0x75}


#include <math.h>

#define OutputOpampPin A0
#define R_FIXED 10000.0    // 10k resistor in series with thermistor
#define VCC 5.0            // Supply voltage

// Beta equation parameters for MF52AT thermistor
#define BETA 3950.0
#define T0 298.15          // 25°C in Kelvin
#define R0 10000.0         // Resistance at 25°C in ohms

const float ReferencePrecision1 = 0.07;
const float ReferencePrecision2 = 0.01;

int Buf[10];
int Temporary;
int Count1;
int Count2;
int Count3;
int IntegerPart;
int DecimalPart;
float Temperature;
float Temperature_1;
float Temperature_2;
float MilivoltValue[100];
float TemperatureValue[100];

void (*FuncReset)() = 0;

void sayNumber(long n) {
  IntegerPart = n;
  if (n < 0) {
    voice.say(sp2_MINUS);
    sayNumber(-n);
  } else if (n == 0) {
    voice.say(sp2_ZERO);
  } else {
    if (n >= 1000) {
      int thousands = n / 1000;
      sayNumber(thousands);
      voice.say(sp2_THOUSAND);
      n %= 1000;
      if ((n > 0) && (n < 100)) voice.say(sp2_AND);
    }
    if (n >= 100) {
      int hundreds = n / 100;
      sayNumber(hundreds);
      voice.say(sp2_HUNDRED);
      n %= 100;
      if (n > 0) voice.say(sp2_AND);
    }
    if (n > 19) {
      int tens = n / 10;
      switch (tens) {
        case 2: voice.say(sp2_TWENTY); break;
        case 3: voice.say(sp2_THIR_); voice.say(sp2_T); break;
        case 4: voice.say(sp2_FOUR); voice.say(sp2_T); break;
        case 5: voice.say(sp2_FIF_); voice.say(sp2_T); break;
        case 6: voice.say(sp2_SIX); voice.say(sp2_T); break;
        case 7: voice.say(sp2_SEVEN); voice.say(sp2_T); break;
        case 8: voice.say(sp2_EIGHT); voice.say(sp2_T); break;
        case 9: voice.say(sp2_NINE); voice.say(sp2_T); break;
      }
      n %= 10;
    }
    switch (n) {
      case 1: voice.say(sp2_ONE); break;
      case 2: voice.say(sp2_TWO); break;
      case 3: voice.say(sp2_THREE); break;
      case 4: voice.say(sp2_FOUR); break;
      case 5: voice.say(sp2_FIVE); break;
      case 6: voice.say(sp2_SIX); break;
      case 7: voice.say(sp2_SEVEN); break;
      case 8: voice.say(sp2_EIGHT); break;
      case 9: voice.say(sp2_NINE); break;
      case 10: voice.say(sp2_TEN); break;
      case 11: voice.say(sp2_ELEVEN); break;
      case 12: voice.say(sp2_TWELVE); break;
      case 13: voice.say(sp2_THIR_); voice.say(sp2__TEEN); break;
      case 14: voice.say(sp2_FOUR); voice.say(sp2__TEEN); break;
      case 15: voice.say(sp2_FIF_); voice.say(sp2__TEEN); break;
      case 16: voice.say(sp2_SIX); voice.say(sp2__TEEN); break;
      case 17: voice.say(sp2_SEVEN); voice.say(sp2__TEEN); break;
      case 18: voice.say(sp2_EIGHT); voice.say(sp2__TEEN); break;
      case 19: voice.say(sp2_NINE); voice.say(sp2__TEEN); break;
    }
  }
}

void SayTemperature() {
  voice.say(sp2_TEMPERATURE);
  sayNumber((long)Temperature);

  if (Temperature > IntegerPart) {
    DecimalPart = (Temperature - IntegerPart) * 100;
    voice.say(sp2_POINT);
    sayNumber(DecimalPart);
  }

  voice.say(sp2_DEGREES);
}

void Temperature01() {
  if (TemperatureValue[Count3 - 1] - TemperatureValue[Count3] > ReferencePrecision1) {
    Temperature_1 = TemperatureValue[Count3 - 1];
    Serial.print("Temperature 1 = ");
    Serial.println(Temperature_1);
    Temperature = Temperature_1;
    SayTemperature();
  }
}

void Temperature02() {
  if (TemperatureValue[Count3 - 1] - TemperatureValue[Count3 - 2] > ReferencePrecision2) {
    Temperature_2 = TemperatureValue[Count3 - 2];
    Serial.print("Temperature 2 = ");
    Serial.println(Temperature_2);
    Temperature = Temperature_2;
    SayTemperature();
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Ready");
  Count3 = 1;
  TemperatureValue[0] = 0;
}

void loop() {
  for (Count1 = 0; Count1 < 10; Count1++) {
    Buf[Count1] = analogRead(OutputOpampPin);
    delay(10);
  }

  // Sort and average the middle 6 values
  for (Count1 = 0; Count1 < 9; Count1++) {
    for (Count2 = Count1 + 1; Count2 < 10; Count2++) {
      if (Buf[Count1] > Buf[Count2]) {
        Temporary = Buf[Count1];
        Buf[Count1] = Buf[Count2];
        Buf[Count2] = Temporary;
      }
    }
  }

  unsigned long SumValue = 0;
  for (Count1 = 2; Count1 < 8; Count1++) {
    SumValue += Buf[Count1];
  }

  MilivoltValue[Count3] = (float)SumValue * 5000.0 / 1024.0 / 6.0; // mV

  // Convert to resistance
  float Vout = MilivoltValue[Count3] / 1000.0; // in volts
  float R_thermistor = R_FIXED * Vout / (VCC - Vout);

  // Beta equation
  float Temperature_K = 1.0 / ((1.0 / BETA) * log(R_thermistor / R0) + (1.0 / T0));
  TemperatureValue[Count3] = Temperature_K - 273.15;

  Serial.print(Count3);
  Serial.print("  ");
  Serial.print(MilivoltValue[Count3]);
  Serial.print(" mV  ");
  Serial.print(TemperatureValue[Count3]);
  Serial.println(" °C");

  if (TemperatureValue[Count3] < -25) {
    delay(1000);
    FuncReset();
  }

  if (Count3 > 1 && TemperatureValue[Count3 - 1] > TemperatureValue[Count3]) {
    Temperature01();
  } else if (Count3 > 2 && TemperatureValue[Count3 - 2] < TemperatureValue[Count3 - 1] && TemperatureValue[Count3 - 1] < TemperatureValue[Count3]) {
    Temperature02();
  }

  delay(900);
  Count3++;

  if (Count3 == 100) {
    FuncReset();
  }
}
