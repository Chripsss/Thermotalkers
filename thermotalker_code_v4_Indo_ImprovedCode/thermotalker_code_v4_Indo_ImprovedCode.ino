#include "Talkie.h"
#include "Vocab_ID_Large.h"
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

const uint8_t sp2_TEMPERATURE[] PROGMEM = {0x00,0x48,0x6E,0x3E,0x3C,0xDD,0x1C,0x15,0x35,0x29,0xAF,0x6C,0xB4,0xD3,0xF4,0xAC,0x75,0xAA,0x30,0x72,0xD3,0x8B,0xD6,0xA9,0x42,0xDB,0x49,0x0B,0xC9,0xD2,0x92,0x54,0x29,0x95,0x84,0x2E,0x3A,0xB5,0x25,0x14,0xD2,0xD4,0xEA,0xCD,0x16,0x52,0xC0,0x08,0xF7,0xEB,0x5A,0x46,0x1C,0xDA,0xCD,0x24,0x12,0x17,0x31,0x56,0x29,0x2F,0x91,0xDB,0xA4,0x14,0x3C,0xAC,0xD5,0x4A,0x93,0x63,0xB5,0x8C,0x14,0x25,0x4D,0x8E,0x2D,0x3C,0xCD,0xD4,0x34,0x39,0x66,0x8F,0x6C,0x74,0x22,0xE4,0x90,0x25,0xD2,0x20,0x35,0x52,0x42,0x96,0x48,0xA3,0x2C,0x00,0x85,0xEB,0x4A,0xD5,0x24,0xDA,0x04,0x4E,0xA5,0x14,0x3D,0x57,0xEC,0x68,0x53,0x95,0xEB,0x54,0x69,0xA0,0x43,0x11,0xCE,0x17,0xB9,0x85,0x4E,0x41,0xDC,0xDB,0xA3,0x3A,0x2A,0x19,0x8E,0xA8,0xAE,0x6C,0xA8,0x64,0xD9,0xAB,0xAA,0x2A,0x13,0x5A,0xEB,0x24,0xB1,0x24,0x44,0xAA,0x2D,0x84,0xC3,0xA2,0x20,0xA5,0xE6,0x64,0x49,0x8B,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x80,0x2E,0xC3};




void (*FuncReset)() = 0;

void sayNumber(long n) {
  IntegerPart = n;
  if (n < 0) {
    voice.say(sp2_MINUS_ID);
    sayNumber(-n);
  } else if (n == 0) {
    voice.say(sp2_NOL);
  } else {
    if (n >= 1000) {
      int thousands = n / 1000;
      sayNumber(thousands);
      voice.say(sp2_RIBU);
      n %= 1000;
     
    }
    if (n >= 100) {
      int hundreds = n / 100;
      sayNumber(hundreds);
      voice.say(sp2_RATUS);
      n %= 100;

    }
    if (n > 19) {
      int tens = n / 10;
      switch (tens) {
        case 2: voice.say(sp2_DUA); voice.say(sp2_PULUH); break;
        case 3: voice.say(sp2_TIGA); voice.say(sp2_PULUH); break;
        case 4: voice.say(sp2_EMPATPULUH); break;
        case 5: voice.say(sp2_LIMA_ID); voice.say(sp2_PULUH); break;
        case 6: voice.say(sp2_ENAMPULUH); break;
        case 7: voice.say(sp2_TUJUHPULUH);break;
        case 8: voice.say(sp2_DELAPANPULUH); break;
        case 9: voice.say(sp2_SEMBILAN); voice.say(sp2_PULUH); break;
      }
      n %= 10;
    }
    switch (n) {
      case 1: voice.say(sp2_SATU); break;
      case 2: voice.say(sp2_DUA); break;
      case 3: voice.say(sp2_TIGA); break;
      case 4: voice.say(sp2_EMPAT); break;
      case 5: voice.say(sp2_LIMA_ID); break;
      case 6: voice.say(sp2_ENAM); break;
      case 7: voice.say(sp2_TUJUH); break;
      case 8: voice.say(sp2_DELAPAN); break;
      case 9: voice.say(sp2_SEMBILAN); break;
      case 10: voice.say(sp2_SEPULUH); break;
      case 11: voice.say(sp2_SEBELAS); break;
      case 12: voice.say(sp2_DUABELAS); break;
      case 13: voice.say(sp2_TIGABELAS); break;
      case 14: voice.say(sp2_EMPATBELAS); break;
      case 15: voice.say(sp2_LIMABELAS); break;
      case 16: voice.say(sp2_ENAMBELAS); break;
      case 17: voice.say(sp2_TUJUHBELAS); break;
      case 18: voice.say(sp2_DELAPANBELAS); break;
      case 19: voice.say(sp2_SEMBILANBELAS); break;
    }
  }
}

void SayTemperature() {
  voice.say(sp2_TEMPERATURE);
  sayNumber((long)Temperature);

  if (Temperature > IntegerPart) {
    DecimalPart = (Temperature - IntegerPart) * 100;
    voice.say(sp2_KOMA);
    sayNumber(DecimalPart);
  }

  voice.say(sp2_CELSIUS);
  voice.stop();                   // if your Talkie has stop()
  delay(5); 
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
