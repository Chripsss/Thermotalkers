#include "Talkie.h"
#include "Vocab_ID_Large.h"
Talkie voice;

#include <math.h>

// -----------------------------
// Optional: audio mute control
// Wire your amplifier/booster EN (enable) pin here if available.
// Set to -1 if you don’t have a mute/enable line.
#define AUDIO_MUTE_PIN 6   // <- change to your amp's EN pin; set -1 if none
// -----------------------------

// --- Vocab compatibility shims (avoid name collisions) ---
// #ifndef sp2_MINUS_ID
//   #ifdef sp2_MINUS
//     #define sp2_MINUS_ID sp2_MINUS
//   #endif
// #endif

// #ifndef sp2_LIMA_ID
//   #ifdef sp2_LIMA
//     #define sp2_LIMA_ID sp2_LIMA
//   #endif
// #endif

// #ifndef sp2_KOMA
//   #ifdef sp2_KOMA_ID
//     #define sp2_KOMA sp2_KOMA_ID
//   #endif
// #endif

// If your vocab already includes this word, delete this array.
const uint8_t sp2_TEMPERATURE[] PROGMEM = {
  0x00,0x48,0x6E,0x3E,0x3C,0xDD,0x1C,0x15,0x35,0x29,0xAF,0x6C,0xB4,0xD3,0xF4,0xAC,0x75,0xAA,0x30,0x72,0xD3,0x8B,0xD6,0xA9,0x42,0xDB,0x49,0x0B,0xC9,0xD2,0x92,0x54,0x29,0x95,0x84,0x2E,0x3A,0xB5,0x25,0x14,0xD2,0xD4,0xEA,0xCD,0x16,0x52,0xC0,0x08,0xF7,0xEB,0x5A,0x46,0x1C,0xDA,0xCD,0x24,0x12,0x17,0x31,0x56,0x29,0x2F,0x91,0xDB,0xA4,0x14,0x3C,0xAC,0xD5,0x4A,0x93,0x63,0xB5,0x8C,0x14,0x25,0x4D,0x8E,0x2D,0x3C,0xCD,0xD4,0x34,0x39,0x66,0x8F,0x6C,0x74,0x22,0xE4,0x90,0x25,0xD2,0x20,0x35,0x52,0x42,0x96,0x48,0xA3,0x2C,0x00,0x85,0xEB,0x4A,0xD5,0x24,0xDA,0x04,0x4E,0xA5,0x14,0x3D,0x57,0xEC,0x68,0x53,0x95,0xEB,0x54,0x69,0xA0,0x43,0x11,0xCE,0x17,0xB9,0x85,0x4E,0x41,0xDC,0xDB,0xA3,0x3A,0x2A,0x19,0x8E,0xA8,0xAE,0x6C,0xA8,0x64,0xD9,0xAB,0xAA,0x2A,0x13,0x5A,0xEB,0x24,0xB1,0x24,0x44,0xAA,0x2D,0x84,0xC3,0xA2,0x20,0xA5,0xE6,0x64,0x49,0x8B,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x80,0x2E,0xC3
};

// ---------------- Hardware / NTC model ----------------
#define OutputOpampPin A0
#define R_FIXED 10000.0      // 10k series resistor
#define VCC 5.0              // 5V boards; set to 3.3 if using 3V3

// MF52AT-like NTC
#define BETA 3950.0
#define T0   298.15          // 25°C in K
#define R0   10000.0

// ---------------- Fast sampler & filter ----------------
// We’ll read as fast as analogRead allows (~100 µs/sample on AVR),
// then apply an EMA to stabilize but keep responsiveness.
const float EMA_ALPHA = 0.20f;           // 0..1; higher = faster
float emaAdc = -1.0f;                    // initialized on first sample

// For edge detection and speech pacing
const float ReferencePrecisionFall = 0.07f;  // announce on sufficient drop
const float ReferencePrecisionRise = 0.01f;  // announce when rising run detected
const unsigned long CHECK_PERIOD_MS = 200;   // detection check cadence
const unsigned long MIN_SPEAK_INTERVAL_MS = 1800; // avoid chattiness

// Temperature state
float lastT = NAN;               // last temp (°C)
float prevT = NAN;               // previous temp (°C)
float prevPrevT = NAN;           // 2-back temp (°C)
unsigned long lastCheck = 0;
unsigned long lastSpoken = 0;

// ---------------- Utilities ----------------
static inline float clampF(float x, float lo, float hi){ return x<lo?lo:(x>hi?hi:x); }

// ---------------- Indonesian numbers ----------------
void sayUnder20(int n) {
  switch (n) {
    case 0: voice.say(sp2_NOL); break;
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

void sayNumber(long n) {
  if (n < 0) { voice.say(sp2_MINUS_ID); sayNumber(-n); return; }
  if (n == 0) { voice.say(sp2_NOL); return; }

  // if (n >= 1000000) {
  //   long juta = n / 1000000;
  //   // Optional: if vocab has SEJUTA & JUTA variants
  //   sayNumber(juta);
  //   voice.say(sp2_SEJUTA);          // comment if not available
  //   n %= 1000000;
  // }
  // if (n >= 1000) {
  //   long ribu = n / 1000;
  //   if (ribu == 1) voice.say(sp2_SERIBU);
  //   else           { sayNumber(ribu); voice.say(sp2_RIBU); }
  //   n %= 1000;
  // }
  if (n >= 100) {
    long ratus = n / 100;
    if (ratus == 1) voice.say(sp2_SERATUS);
    else            { sayNumber(ratus); voice.say(sp2_RATUS); }
    n %= 100;
  }
  if (n >= 20) {
    int puluh = n / 10;
    switch (puluh) {
      case 2: voice.say(sp2_DUA); break;
      case 3: voice.say(sp2_TIGA); break;
      case 4: voice.say(sp2_EMPAT); break;
      case 5: voice.say(sp2_LIMA_ID); break;
      case 6: voice.say(sp2_ENAM); break;
      case 7: voice.say(sp2_TUJUH); break;
      case 8: voice.say(sp2_DELAPAN); break;
      case 9: voice.say(sp2_SEMBILAN); break;
    }
    voice.say(sp2_PULUH);
    n %= 10;
  }
  if (n > 0 && n < 20) sayUnder20((int)n);
}

static inline void audioMute(bool on) {
  if (AUDIO_MUTE_PIN >= 0) digitalWrite(AUDIO_MUTE_PIN, on ? LOW : HIGH);
}

void sayTemperatureNow(float tC) {
  // round to 2 decimals; enforce “nol” for 0.x
  float tRounded = roundf(tC * 100.0f) / 100.0f;
  long  intPart  = (long) tRounded;
  int   decPart  = (int) roundf(fabsf(tRounded - (float)intPart) * 100.0f);

  audioMute(false);                // unmute amp
  voice.say(sp2_TEMPERATURE);
  sayNumber(intPart);

  if (decPart > 0) {
    voice.say(sp2_KOMA);
    if (decPart < 10) {            // keep “nol” for .01 .. .09
      voice.say(sp2_NOL);
      sayNumber(decPart);
    } else {
      sayNumber(decPart);
    }
  }

  voice.say(sp2_CELSIUS);
  voice.stop();
  delay(10);                        // small settle
  audioMute(true);                 // mute to kill PWM tail / chirp
}

// ----------- Thermistor conversion -----------
float adcToCelsius(float adcVal /*0..1023*/) {
  // ADC -> Vout
  float vAdc = (adcVal * VCC) / 1023.0f;
  vAdc = clampF(vAdc, 0.001f, VCC - 0.001f);      // stability

  // Divider: top R_FIXED, bottom NTC -> Vout at node
  float R_therm = R_FIXED * vAdc / (VCC - vAdc);
  R_therm = fmaxf(R_therm, 1.0f);

  float invT = (1.0f / T0) + (1.0f / BETA) * logf(R_therm / R0);
  float tK   = 1.0f / invT;
  return tK - 273.15f;
}

// ----------- Detection logic -----------
void tryAnnounce(float curT) {
  unsigned long now = millis();
  if (now - lastCheck < CHECK_PERIOD_MS) return;
  lastCheck = now;

  // shift history
  prevPrevT = prevT;
  prevT = lastT;
  lastT = curT;

  if (isnan(prevT) || isnan(prevPrevT)) return;          // need history
  if (now - lastSpoken < MIN_SPEAK_INTERVAL_MS) return;  // pace announcements

  // Falling edge announce previous peak
  if ((prevT - curT) > ReferencePrecisionFall) {
    sayTemperatureNow(prevT);
    lastSpoken = now;
    return;
  }

  // Rising run: a < b < c and (b - a) > threshold -> announce a
  if ((prevPrevT < prevT) && (prevT < curT) && ((prevT - prevPrevT) > ReferencePrecisionRise)) {
    sayTemperatureNow(prevPrevT);
    lastSpoken = now;
    return;
  }
}

// ---------------- Arduino lifecycle ----------------
void setup() {
  Serial.begin(9600);
  if (AUDIO_MUTE_PIN >= 0) {
    pinMode(AUDIO_MUTE_PIN, OUTPUT);
    audioMute(true);              // start muted
  }
  // NOTE: If using 3.3V board, set VCC above accordingly.
  // analogReference(DEFAULT); // or EXTERNAL if you wire AREF.
  Serial.println("ThermoTalker fast mode ready");
}

void loop() {
  // Fast raw sample
  int raw = analogRead(OutputOpampPin);

  // EMA filter (fast & responsive)
  if (emaAdc < 0) emaAdc = raw;                   // first-run init
  else emaAdc = (EMA_ALPHA * raw) + (1.0f - EMA_ALPHA) * emaAdc;

  // Convert & print occasionally (throttle printing to avoid serial lag)
  static unsigned long lastPrint = 0;
  unsigned long now = millis();
  float tC = adcToCelsius(emaAdc);

  if (now - lastPrint > 150) {                    // ~6–7 Hz console
    float vAdc = (emaAdc * VCC) / 1023.0f;
    Serial.print("ADC(EMA): "); Serial.print(emaAdc, 1);
    Serial.print("  V: "); Serial.print(vAdc, 3);
    Serial.print("  T: "); Serial.print(tC, 2);
    Serial.println(" C");
    lastPrint = now;
  }

  // Check announcement logic regularly
  tryAnnounce(tC);

  // No fixed delays → ultra responsive loop
}
