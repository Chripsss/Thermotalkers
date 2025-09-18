#include "Talkie.h"
#include "Vocab_US_Large.h"
#include "Vocab_US_Acorn.h"
#include "Vocab_Special.h"
Talkie voice;

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
