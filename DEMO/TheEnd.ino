////////////////////////////////////////
// Author: Antonio Hernandez Olivares
// Date: 11/27/25
// Rev: 1.3
// File: TheEnd.ino
// Description: Three-channel audio effects processor with rotary encoder controls
//              for bass boost, reverb, and delay effects using Teensy 4.1 Audio Library
// Project: Teensy Audio Effects Processor
//
// Hardware: Teensy 4.1 with Teensy Audio Shield
//           3x KY-040 Rotary Encoders with push buttons
//           3x LEDs for effect status indication
//
// Features: - Bass Boost (0.85-1.0, adjustable)
//           - Stereo Reverb (roomsize 0.8-1.0)
//           - Multi-tap Delay (300-800ms base, 3 taps)
//           - Always-on surround sound processing
//
// Dependencies: Teensy Audio Library, KY040 encoder library
////////////////////////////////////////

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <KY040.h>

// GUItool: begin automatically generated code
AudioInputI2S            i2s1;
AudioMixer4              mixer_pre;      // Combines L/R for effects
AudioMixer4              mixer_dry;      // Dry signal path
AudioEffectDelay         delay1;         // Delay effect
AudioEffectFreeverbStereo freeverbs1;    // Reverb effect
AudioMixer4              mixer_delay;    // Delay taps mixer
AudioMixer4              mixer_final;    // Final output mixer
AudioOutputI2S           i2s2;
AudioConnection          patchCord1(i2s1, 0, mixer_pre, 0);
AudioConnection          patchCord2(i2s1, 1, mixer_pre, 1);
AudioConnection          patchCord3(i2s1, 0, mixer_dry, 0);
AudioConnection          patchCord4(i2s1, 1, mixer_dry, 1);
AudioConnection          patchCord5(mixer_pre, delay1);
AudioConnection          patchCord6(mixer_pre, freeverbs1);
AudioConnection          patchCord7(delay1, 0, mixer_delay, 0);
AudioConnection          patchCord8(delay1, 1, mixer_delay, 1);
AudioConnection          patchCord9(delay1, 2, mixer_delay, 2);
AudioConnection          patchCord10(mixer_dry, 0, mixer_final, 0);
AudioConnection          patchCord11(mixer_delay, 0, mixer_final, 1);
AudioConnection          patchCord12(freeverbs1, 0, mixer_final, 2);
AudioConnection          patchCord13(freeverbs1, 1, mixer_final, 3);
AudioConnection          patchCord14(mixer_final, 0, i2s2, 0);
AudioConnection          patchCord15(mixer_final, 0, i2s2, 1);
AudioControlSGTL5000     sgtl5000_1;
// GUItool: end automatically generated code

const int myInput = AUDIO_INPUT_LINEIN;

//
// ─────────────────────────────────────────
//   LED PINS
// ─────────────────────────────────────────
//
#define LED1_PIN 35  // Bass Boost LED
#define LED2_PIN 34  // Reverb LED
#define LED3_PIN 33  // Delay LED

bool bassBoostOn = false;
bool reverbOn = false;
bool delayOn = false;

//
// ─────────────────────────────────────────
//   ENCODER 1 - BASS BOOST (pins 4,5,6)
// ─────────────────────────────────────────
//
#define ENC1_CLK 4
#define ENC1_DT  5
#define ENC1_SW  6

KY040 enc1(ENC1_CLK, ENC1_DT);
volatile int enc1_value = 15;  // Start at 15 (= 1.0 bass, range 0-15 for 0.85-1.0)

void ISR_Enc1() {
  switch (enc1.getRotation()) {
    case KY040::CLOCKWISE:        
      if (enc1_value < 15) enc1_value++; 
      break;
    case KY040::COUNTERCLOCKWISE: 
      if (enc1_value > 0) enc1_value--; 
      break;
  }
}

//
// ─────────────────────────────────────────
//   ENCODER 2 - REVERB (pins 9,10,11)
// ─────────────────────────────────────────
//
#define ENC2_CLK 9
#define ENC2_DT  10
#define ENC2_SW  11

KY040 enc2(ENC2_CLK, ENC2_DT);
volatile int enc2_value = 0;  // Start at 0 (= 0.8 roomsize, range 0-20 for 0.8-1.0)

void ISR_Enc2() {
  switch (enc2.getRotation()) {
    case KY040::CLOCKWISE:        
      if (enc2_value < 20) enc2_value++; 
      break;
    case KY040::COUNTERCLOCKWISE: 
      if (enc2_value > 0) enc2_value--; 
      break;
  }
}

//
// ─────────────────────────────────────────
//   ENCODER 3 - DELAY (pins 12,24,25)
// ─────────────────────────────────────────
//
#define ENC3_CLK 12
#define ENC3_DT  24
#define ENC3_SW  25

KY040 enc3(ENC3_CLK, ENC3_DT);
volatile int enc3_value = 8;  // Start at 8 (= 700ms, range 0-10 for 300-800ms in 50ms steps)

void ISR_Enc3() {
  switch (enc3.getRotation()) {
    case KY040::CLOCKWISE:        
      if (enc3_value < 10) enc3_value++; 
      break;
    case KY040::COUNTERCLOCKWISE: 
      if (enc3_value > 0) enc3_value--; 
      break;
  }
}

//
// ─────────────────────────────────────────
//   SETUP
// ─────────────────────────────────────────
//
void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("3-Effect Audio Processor Starting...");

  // Audio setup
  AudioMemory(1500);
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(myInput);
  sgtl5000_1.volume(0.5);
  
  // Surround sound - always on
  sgtl5000_1.audioPreProcessorEnable();
  sgtl5000_1.audioPostProcessorEnable();
  sgtl5000_1.autoVolumeDisable();
  sgtl5000_1.surroundSoundEnable();
  sgtl5000_1.surroundSound(5, 3);

  // Initialize effects to defaults (but off)
  sgtl5000_1.enhanceBassDisable();
  
  freeverbs1.roomsize(0.8);
  freeverbs1.damping(0.5);
  
  delay1.delay(0, 700);
  delay1.delay(1, 1400);
  delay1.delay(2, 2100);
  
  mixer_delay.gain(0, 1.0);
  mixer_delay.gain(1, 0.7);
  mixer_delay.gain(2, 0.4);
  mixer_delay.gain(3, 0.0);

  // Mixer setup
  mixer_pre.gain(0, 1.0);
  mixer_pre.gain(1, 1.0);
  mixer_pre.gain(2, 0.0);
  mixer_pre.gain(3, 0.0);

  mixer_dry.gain(0, 1.0);
  mixer_dry.gain(1, 1.0);
  mixer_dry.gain(2, 0.0);
  mixer_dry.gain(3, 0.0);

  // Start with all effects OFF
  updateEffectMixing();

  // LEDs
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);

  // Encoder 1 - Bass Boost
  attachInterrupt(digitalPinToInterrupt(ENC1_CLK), ISR_Enc1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC1_DT),  ISR_Enc1, CHANGE);
  pinMode(ENC1_SW, INPUT_PULLUP);

  // Encoder 2 - Reverb
  attachInterrupt(digitalPinToInterrupt(ENC2_CLK), ISR_Enc2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC2_DT),  ISR_Enc2, CHANGE);
  pinMode(ENC2_SW, INPUT_PULLUP);

  // Encoder 3 - Delay
  attachInterrupt(digitalPinToInterrupt(ENC3_CLK), ISR_Enc3, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC3_DT),  ISR_Enc3, CHANGE);
  pinMode(ENC3_SW, INPUT_PULLUP);

  Serial.println("Ready! Turn encoders to adjust effects, click to enable/disable.");
}

//
// ─────────────────────────────────────────
//   UPDATE EFFECT MIXING
// ─────────────────────────────────────────
//
void updateEffectMixing() {
  // Calculate wet/dry mix based on which effects are on
  float dryGain = 0.6;
  float delayGain = delayOn ? 0.4 : 0.0;
  float reverbGain = reverbOn ? 0.2 : 0.0;
  
  mixer_final.gain(0, dryGain);
  mixer_final.gain(1, delayGain);
  mixer_final.gain(2, reverbGain);
  mixer_final.gain(3, reverbGain);
}

//
// ─────────────────────────────────────────
//   MAIN LOOP
// ─────────────────────────────────────────
//
void loop() {

  // ===== ENCODER 1: BASS BOOST =====
  static int last1 = 15;
  int v1;
  noInterrupts();
  v1 = enc1_value;
  interrupts();

  if (v1 != last1) {
    // Map 0-15 to 0.85-1.0
    float bassLevel = 0.85 + (v1 * 0.01);
    if (bassBoostOn) {
      sgtl5000_1.enhanceBass(1.0, bassLevel, 1, 0);
      Serial.print("Bass Level: ");
      Serial.println(bassLevel, 2);
    }
    last1 = v1;
  }

  // Encoder 1 button → Bass Boost on/off
  static uint32_t lastPress1 = 0;
  static int lastBtn1 = HIGH;
  int btn1 = digitalRead(ENC1_SW);

  if (btn1 != lastBtn1 && btn1 == LOW && millis() - lastPress1 > 200) {
    bassBoostOn = !bassBoostOn;
    digitalWrite(LED1_PIN, bassBoostOn ? HIGH : LOW);
    
    if (bassBoostOn) {
      enc1_value = 15;  // Reset to default (1.0)
      sgtl5000_1.enhanceBassEnable();
      sgtl5000_1.enhanceBass(1.0, 1.0, 1, 0);
      Serial.println("Bass Boost: ON (1.0)");
    } else {
      sgtl5000_1.enhanceBassDisable();
      Serial.println("Bass Boost: OFF");
    }
    lastPress1 = millis();
  }
  lastBtn1 = btn1;

  // ===== ENCODER 2: REVERB =====
  static int last2 = 0;
  int v2;
  noInterrupts();
  v2 = enc2_value;
  interrupts();

  if (v2 != last2) {
    // Map 0-20 to 0.8-1.0 in 0.01 increments
    float roomsize = 0.8 + (v2 * 0.01);
    if (reverbOn) {
      freeverbs1.roomsize(roomsize);
      Serial.print("Reverb Roomsize: ");
      Serial.println(roomsize, 2);
    }
    last2 = v2;
  }

  // Encoder 2 button → Reverb on/off
  static uint32_t lastPress2 = 0;
  static int lastBtn2 = HIGH;
  int btn2 = digitalRead(ENC2_SW);

  if (btn2 != lastBtn2 && btn2 == LOW && millis() - lastPress2 > 200) {
    reverbOn = !reverbOn;
    digitalWrite(LED2_PIN, reverbOn ? HIGH : LOW);
    
    if (reverbOn) {
      enc2_value = 0;  // Reset to default (0.8)
      freeverbs1.roomsize(0.8);
      freeverbs1.damping(0.5);
      Serial.println("Reverb: ON (0.8 roomsize)");
    } else {
      Serial.println("Reverb: OFF");
    }
    updateEffectMixing();
    lastPress2 = millis();
  }
  lastBtn2 = btn2;

  // ===== ENCODER 3: DELAY =====
  static int last3 = 8;
  int v3;
  noInterrupts();
  v3 = enc3_value;
  interrupts();

  if (v3 != last3) {
    // Map 0-10 to 300-800ms in 50ms steps
    int baseDelay = 300 + (v3 * 50);
    if (delayOn) {
      delay1.delay(0, baseDelay);
      delay1.delay(1, baseDelay * 2);
      delay1.delay(2, baseDelay * 3);
      Serial.print("Delay Times: ");
      Serial.print(baseDelay);
      Serial.print("/");
      Serial.print(baseDelay * 2);
      Serial.print("/");
      Serial.println(baseDelay * 3);
    }
    last3 = v3;
  }

  // Encoder 3 button → Delay on/off
  static uint32_t lastPress3 = 0;
  static int lastBtn3 = HIGH;
  int btn3 = digitalRead(ENC3_SW);

  if (btn3 != lastBtn3 && btn3 == LOW && millis() - lastPress3 > 200) {
    delayOn = !delayOn;
    digitalWrite(LED3_PIN, delayOn ? HIGH : LOW);
    
    if (delayOn) {
      enc3_value = 8;  // Reset to default (700ms)
      delay1.delay(0, 700);
      delay1.delay(1, 1400);
      delay1.delay(2, 2100);
      Serial.println("Delay: ON (700/1400/2100ms)");
    } else {
      Serial.println("Delay: OFF");
    }
    updateEffectMixing();
    lastPress3 = millis();
  }
  lastBtn3 = btn3;
}