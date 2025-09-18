#include "Talkie.h"
#include "Vocab_ID_Large.h"
Talkie voice;

#include <math.h>

// -----------------------------
// Optional: amplifier mute control (set to -1 if unused)
#define AUDIO_MUTE_PIN 6
// -----------------------------

// --- Vocab compatibility shims (avoid name collisions) ---
#ifndef sp2_MINUS_ID
  #ifdef sp2_MINUS
    #define sp2_MINUS_ID sp2_MINUS
  #endif
#endif
#ifndef sp2_LIMA_ID
  #ifdef sp2_LIMA
    #define sp2_LIMA_ID sp2_LIMA
  #endif
#endif
#ifndef sp2_KOMA
  #ifdef sp2_KOMA_ID
    #define sp2_KOMA sp2_KOMA_ID
  #endif
#endif

// If your vocab already includes this word, delete this array.
const uint8_t sp2_TEMPERATURE[] PROGMEM = {
  0x00,0x48,0x6E,0x3E,0x3C,0xDD,0x1C,0x15,0x35,0x29,0xAF,0x6C,0xB4,0xD3,0xF4,0xAC,0x75,0xAA,0x30,0x72,0xD3,0x8B,0xD6,0xA9,0x42,0xDB,0x49,0x0B,0xC9,0xD2,0x92,0x54,0x29,0x95,0x84,0x2E,0x3A,0xB5,0x25,0x14,0xD2,0xD4,0xEA,0xCD,0x16,0x52,0xC0,0x08,0xF7,0xEB,0x5A,0x46,0x1C,0xDA,0xCD,0x24,0x12,0x17,0x31,0x56,0x29,0x2F,0x91,0xDB,0xA4,0x14,0x3C,0xAC,0xD5,0x4A,0x93,0x63,0xB5,0x8C,0x14,0x25,0x4D,0x8E,0x2D,0x3C,0xCD,0xD4,0x34,0x39,0x66,0x8F,0x6C,0x74,0x22,0xE4,0x90,0x25,0xD2,0x20,0x35,0x52,0x42,0x96,0x48,0xA3,0x2C,0x00,0x85,0xEB,0x4A,0xD5,0x24,0xDA,0x04,0x4E,0xA5,0x14,0x3D,0x57,0xEC,0x68,0x53,0x95,0xEB,0x54,0x69,0xA0,0x43,0x11,0xCE,0x17,0xB9,0x85,0x4E,0x41,0xDC,0xDB,0xA3,0x3A,0x2A,0x19,0x8E,0xA8,0xAE,0x6C,0xA8,0x64,0xD9,0xAB,0xAA,0x2A,0x13,0x5A,0xEB,0x24,0xB1,0x24,0x44,0xAA,0x2D,0x84,0xC3,0xA2,0x20,0xA5,0xE6,0x64,0x49,0x8B,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x80,0x2E,0xC3
};

// ---------------- Hardware / NTC model ----------------
#define OutputOpampPin A0
#define R_FIXED 10000.0
#define VCC 5.0
#define BETA 3950.0
#define T0   298.15
#define R0   10000.0

// ---------------- Fast sampler & filter ----------------
const float EMA_ALPHA = 0.20f;
float emaAdc = -1.0f;

// ---------- Prediction & preemption (speech latency control) ----------
const float SLOPE_FAST_THRESH     = 0.40f;    // degC/s -> considered "moving"
const float LEAD_TIME_S           = 0.60f;    // speak this far ahead
const float PREEMPT_DELTA_C       = 0.30f;    // abort mid-phrase if drift > this
const unsigned long CHECK_PERIOD_MS = 120;    // detection cadence
const unsigned long MIN_SPEAK_INTERVAL_MS = 900; // lower to allow fixed cadence

// ---------- Onset & stabilization ----------
const float ONSET_SLOPE_THRESH    = 0.20f;    // degC/s to declare onset
const float ONSET_DELTA_THRESH    = 0.20f;    // absolute jump to declare onset
const float STABLE_SLOPE_THRESH   = 0.04f;    // degC/s is "flat"
const unsigned long STABLE_WINDOW_MS = 1500;  // must remain flat this long

// ---------- Fixed-cadence "during" narration ----------
const unsigned long FIXED_CALL_INTERVAL_MS = 5000; // speak every N ms during change
// Optional: also force an extra mid-call if a big jump occurs between beats
const float EXTRA_JUMP_DELTA = 0.60f;               // degC triggers extra call

// ---------------- Curve narration: state machine ----------------
enum Phase { IDLE, CHANGING, STABILIZING };
Phase phase = IDLE;

// Temperature state
float lastT = NAN, prevT = NAN, prevPrevT = NAN;
unsigned long lastCheck = 0, lastSpoken = 0;

// Curve narration memory
float startTemp = NAN;                 // for reference/metrics
float lastAnnounced = NAN;             // last narrated temp
unsigned long onsetMs = 0;             // when CHANGING started
unsigned long nextSpeakMs = 0;         // next fixed-cadence speak time
unsigned long flatSince = 0;

// Small ring buffer for slope estimation
struct TSample { float T; unsigned long ms; };
TSample tbuf[8];
uint8_t tbi = 0;

// ---------------- Utilities ----------------
static inline float clampF(float x, float lo, float hi){ return x<lo?lo:(x>hi?hi:x); }
static inline void audioMute(bool on) { if (AUDIO_MUTE_PIN >= 0) digitalWrite(AUDIO_MUTE_PIN, on ? LOW : HIGH); }

// ---------------- Indonesian numbers ----------------
void sayUnder20(int n) {
  switch (n) {
    case 0: voice.say(sp2_NOL); break;    case 1: voice.say(sp2_SATU); break;
    case 2: voice.say(sp2_DUA); break;    case 3: voice.say(sp2_TIGA); break;
    case 4: voice.say(sp2_EMPAT); break;  case 5: voice.say(sp2_LIMA_ID); break;
    case 6: voice.say(sp2_ENAM); break;   case 7: voice.say(sp2_TUJUH); break;
    case 8: voice.say(sp2_DELAPAN); break;case 9: voice.say(sp2_SEMBILAN); break;
    case 10: voice.say(sp2_SEPULUH); break; case 11: voice.say(sp2_SEBELAS); break;
    case 12: voice.say(sp2_DUABELAS); break; case 13: voice.say(sp2_TIGABELAS); break;
    case 14: voice.say(sp2_EMPATBELAS); break; case 15: voice.say(sp2_LIMABELAS); break;
    case 16: voice.say(sp2_ENAMBELAS); break; case 17: voice.say(sp2_TUJUHBELAS); break;
    case 18: voice.say(sp2_DELAPANBELAS); break; case 19: voice.say(sp2_SEMBILANBELAS); break;
  }
}
void sayNumber(long n) {
  if (n < 0) { voice.say(sp2_MINUS_ID); sayNumber(-n); return; }
  if (n == 0) { voice.say(sp2_NOL); return; }
  if (n >= 100) {
    long ratus = n / 100;
    if (ratus == 1) voice.say(sp2_SERATUS);
    else { sayNumber(ratus); voice.say(sp2_RATUS); }
    n %= 100;
  }
  if (n >= 20) {
    int puluh = n / 10;
    switch (puluh) {
      case 2: voice.say(sp2_DUA); break;   case 3: voice.say(sp2_TIGA); break;
      case 4: voice.say(sp2_EMPAT); break; case 5: voice.say(sp2_LIMA_ID); break;
      case 6: voice.say(sp2_ENAM); break;  case 7: voice.say(sp2_TUJUH); break;
      case 8: voice.say(sp2_DELAPAN); break; case 9: voice.say(sp2_SEMBILAN); break;
    }
    voice.say(sp2_PULUH);
    n %= 10;
  }
  if (n > 0 && n < 20) sayUnder20((int)n);
}

// ----------- Thermistor conversion -----------
float adcToCelsius(float adcVal /*0..1023*/) {
  float vAdc = (adcVal * VCC) / 1023.0f;
  vAdc = clampF(vAdc, 0.001f, VCC - 0.001f);
  float R_therm = R_FIXED * vAdc / (VCC - vAdc);
  if (R_therm < 1.0f) R_therm = 1.0f;
  float invT = (1.0f / T0) + (1.0f / BETA) * logf(R_therm / R0);
  return (1.0f / invT) - 273.15f;
}

// -------- Prediction & slope helpers --------
float calcSlopeDegPerSec() {
  int i2 = (int)tbi - 1; if (i2 < 0) i2 += 8;
  int i1 = i2 - 3;       if (i1 < 0) i1 += 8;
  unsigned long dt_ms = (tbuf[i2].ms - tbuf[i1].ms);
  if (dt_ms < 150) return 0.0f;
  float dT = tbuf[i2].T - tbuf[i1].T;
  return dT * 1000.0f / (float)dt_ms;
}
float predictTempAhead(float Tnow, float slope_degps, float lead_s) {
  float Tp = Tnow + slope_degps * lead_s;
  if (Tp < -40) Tp = -40; if (Tp > 150) Tp = 150;
  return Tp;
}

// -------- Preemptable, adaptive speech --------
void sayTemperatureAdaptive(float T_target, bool sayLabel, bool oneDecimal) {
  float tRounded = oneDecimal ? roundf(T_target * 10.0f) / 10.0f
                              : roundf(T_target * 100.0f) / 100.0f;
  long  intPart  = (long) tRounded;
  int   decPart  = (int) roundf(fabsf(tRounded - (float)intPart) * (oneDecimal ? 10.0f : 100.0f));

  audioMute(false);

  if (sayLabel) {
    voice.say(sp2_TEMPERATURE);
    voice.stop(); delay(2);
    if (fabsf(tbuf[(tbi+7)&7].T - T_target) > PREEMPT_DELTA_C) { audioMute(true); return; }
  }

  sayNumber(intPart);
  voice.stop(); delay(2);
  if (fabsf(tbuf[(tbi+7)&7].T - T_target) > PREEMPT_DELTA_C) { audioMute(true); return; }

  if (decPart > 0) {
    voice.say(sp2_KOMA);
    if (decPart < 10) { voice.say(sp2_NOL); sayNumber(decPart); }
    else              { sayNumber(decPart); }
    voice.stop(); delay(2);
    if (fabsf(tbuf[(tbi+7)&7].T - T_target) > PREEMPT_DELTA_C) { audioMute(true); return; }
  }

  voice.say(sp2_CELSIUS);
  voice.stop();
  delay(6);
  audioMute(true);
}

// ----------- Curve-aware narration with FIXED cadence -----------
void narrateCurve(float curT) {
  unsigned long now = millis();
  if (now - lastCheck < CHECK_PERIOD_MS) return;
  lastCheck = now;

  // Update history
  prevPrevT = prevT; prevT = lastT; lastT = curT;

  // Slope & prediction
  float slope = calcSlopeDegPerSec();
  float Tpred = predictTempAhead(curT, slope, LEAD_TIME_S);
  bool movingFast = fabsf(slope) >= SLOPE_FAST_THRESH;

  // ----- IDLE: detect onset (NO announcement) -----
  if (phase == IDLE) {
    bool onset = (fabsf(curT - prevT) >= ONSET_DELTA_THRESH) || (fabsf(slope) >= ONSET_SLOPE_THRESH);
    if (onset) {
      phase = CHANGING;
      onsetMs = now;
      nextSpeakMs = now + FIXED_CALL_INTERVAL_MS; // schedule first fixed-cadence call
      startTemp = curT;
      lastAnnounced = curT;
      flatSince = 0;
    }
    return;
  }

  // ----- CHANGING: speak at fixed cadence (+optional extra on big jumps) -----
  if (phase == CHANGING) {
    // Fixed interval beat
    if ( (long)(now - nextSpeakMs) >= 0 && (now - lastSpoken) >= MIN_SPEAK_INTERVAL_MS ) {
      bool oneDec = true;       // concise, 1 decimal while moving
      bool sayLbl = false;      // skip "temperature" to save time mid-curve
      sayTemperatureAdaptive(Tpred, sayLbl, oneDec);
      lastAnnounced = curT;
      lastSpoken = now;
      nextSpeakMs += FIXED_CALL_INTERVAL_MS;  // schedule next beat
    }

    // Optional: if a very large jump happens between beats, force an extra call
    if (fabsf(curT - lastAnnounced) >= EXTRA_JUMP_DELTA &&
        (now - lastSpoken) >= (FIXED_CALL_INTERVAL_MS / 2)) {
      bool oneDec = true;
      sayTemperatureAdaptive(Tpred, false, oneDec);
      lastAnnounced = curT;
      lastSpoken = now;
      // keep nextSpeakMs as-is (don’t drift the metronome)
    }

    // Stabilization check
    if (fabsf(slope) <= STABLE_SLOPE_THRESH) {
      if (flatSince == 0) flatSince = now;
      if ((now - flatSince) >= STABLE_WINDOW_MS) {
        phase = STABILIZING;
      }
    } else {
      flatSince = 0;
    }
    return;
  }

  // ----- STABILIZING: final callout, then back to IDLE -----
  if (phase == STABILIZING) {
    if ((now - lastSpoken) >= 600) {
      float TfinalPred = predictTempAhead(curT, slope, 0.25f); // small lead is fine at the end
      // Final: use full precision and say label
      sayTemperatureAdaptive(TfinalPred, true, false);
      lastSpoken = now;
      phase = IDLE;
      startTemp = NAN; lastAnnounced = curT; flatSince = 0;
    }
    return;
  }
}

// ---------------- Arduino lifecycle ----------------
void setup() {
  Serial.begin(9600);
  if (AUDIO_MUTE_PIN >= 0) { pinMode(AUDIO_MUTE_PIN, OUTPUT); audioMute(true); }
  Serial.println("ThermoTalker fixed-cadence curve narration ready");
}

void loop() {
  // Fast sample + EMA
  int raw = analogRead(OutputOpampPin);
  if (emaAdc < 0) emaAdc = raw; else emaAdc = EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * emaAdc;

  float tC = adcToCelsius(emaAdc);

  // Store in ring buffer for slope estimation
  tbuf[tbi].T  = tC;
  tbuf[tbi].ms = millis();
  tbi = (tbi + 1) & 7;

  // Serial (for tuning)
  static unsigned long lastPrint = 0;
  unsigned long now = millis();
  if (now - lastPrint > 150) {
    float vAdc = (emaAdc * VCC) / 1023.0f;
    Serial.print("ADC(EMA): "); Serial.print(emaAdc, 1);
    Serial.print("  V: "); Serial.print(vAdc, 3);
    Serial.print("  T: "); Serial.print(tC, 2);
    Serial.print(" C  phase=");
    Serial.print((phase==IDLE)?"IDLE":(phase==CHANGING)?"CHANGING":"STABILIZING");
    Serial.print("  slope=");
    Serial.print(calcSlopeDegPerSec(), 3);
    Serial.print("  t_since_onset=");
    if (onsetMs==0) Serial.print(0); else Serial.print((now - onsetMs)/1000.0f, 2);
    Serial.print("s");
    Serial.println();
    lastPrint = now;
  }

  // Curve narration
  narrateCurve(tC);
}
