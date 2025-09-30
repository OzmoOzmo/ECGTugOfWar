
#pragma once

#include "Arduino.h"
#include "config.h"
#include "sound.h"


extern unsigned long timeOfStageStart;    // Stores the time the stage changed for stages that are time based
extern int puckPosition;         // Stores the puck position (0..NUM_LEDS-1)

// ---------------------------------
// -------------- SFX --------------
// ---------------------------------

void SFXcomplete()
{
  soundOff();
}

/*
  This works just like the map function except x is constrained to the range of in_min and in_max
*/
long map_constrain(long x, long in_min, long in_max, long out_min, long out_max)
{
  // constain the x value to be between in_min and in_max
  if (in_max > in_min)
  { // map allows min to be larger than max, but constrain does not
    x = constrain(x, in_min, in_max);
  }
  else
  {
    x = constrain(x, in_max, in_min);
  }
  return map(x, in_min, in_max, out_min, out_max);
}


/*
   This is used sweep across (up or down) a frequency range for a specified duration.
   A sin based warble is added to the frequency. This function is meant to be called
   on each frame to adjust the frequency in sync with an animation

   duration 	= over what time period is this mapped
   elapsedTime 	= how far into the duration are we in
   freqStart 	= the beginning frequency
   freqEnd 		= the ending frequency
   warble 		= the amount of warble added (0 disables)


*/
void SFXFreqSweepWarble(int duration, int elapsedTime, int freqStart, int freqEnd, int warble)
{
  int freq = map_constrain(elapsedTime, 0, duration, freqStart, freqEnd);
  if (warble)
    warble = map(sin(millis() / 20.0) * 1000.0, -1000, 1000, 0, warble);

  sound(freq + warble, audio_volume);
}

/*

   This is used sweep across (up or down) a frequency range for a specified duration.
   Random noise is optionally added to the frequency. This function is meant to be called
   on each frame to adjust the frequency in sync with an animation

   duration 	= over what time period is this mapped
   elapsedTime 	= how far into the duration are we in
   freqStart 	= the beginning frequency
   freqEnd 		= the ending frequency
   noiseFactor 	= the amount of noise to added/subtracted (0 disables)


*/
void SFXFreqSweepNoise(int duration, int elapsedTime, int freqStart, int freqEnd, uint8_t noiseFactor)
{
  int freq;

  if (elapsedTime > duration)
    freq = freqEnd;
  else
    freq = map(elapsedTime, 0, duration, freqStart, freqEnd);

  if (noiseFactor)
    noiseFactor = noiseFactor - random8(noiseFactor / 2);

  sound(freq + noiseFactor, audio_volume);
}

// convert MIDI note to frequency (Hz)
static inline float midiToFreq(int midiNote) {
  return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
}

void SFXPuckPosition(int amount)
{//values are between 0 and Num_Leds-1
  #define MAX_AMOUNT NUM_LEDS
  // normalize to 0..100 (guard against larger ranges)
  int val = constrain(abs(amount), 0, MAX_AMOUNT);

  // Pentatonic scale intervals (nice consonant notes)
  const int scale[] = {0, 2, 4, 7, 9}; // semitone offsets from root
  const int SCALE_SIZE = sizeof(scale) / sizeof(scale[0]);
  const int OCTAVES = 3; // how many octaves to span
  const int TOTAL_STEPS = OCTAVES * SCALE_SIZE - 1;

  // map val (0..100) to a step in the scale across octaves
  int step = map(val, 0, MAX_AMOUNT, 0, TOTAL_STEPS);
  int octave = step / SCALE_SIZE;
  int degree = step % SCALE_SIZE;

  // root = C4 (MIDI 60) - adjust if you want a different key
  int midi = 60 + octave * 12 + scale[degree];
  int freq = (int)round(midiToFreq(midi));

  // small tasteful variation (a little detune, not large random noise)
  freq += random(-6, 7); // +/- ~6 Hz variation

  // volume mapping: gentle scale from quieter to configured volume
  int vol = map(val, 0, MAX_AMOUNT, audio_volume / 3, audio_volume);
  vol = constrain(vol, 0, 255);

  sound(freq, vol);
}


void SFXRaceStart(int stage)
{
  //stage is 0..8 (9 steps)
  int vol = constrain(audio_volume, 0, 255);
  int vol_short = max(8, vol * 3 / 4);
  int vol_final = vol;

  // melodic steps (MIDI numbers) — final beep is higher and longer
  const int midi1 = 64; // ~329 Hz
  const int midi2 = 66; // ~370 Hz
  const int midi3 = 68+1+1; // ~415 Hz
  const int midiFinal = 72; // ~523 Hz (higher, longer)

  int f1 = (int)round(midiToFreq(midi1));
  int f2 = (int)round(midiToFreq(midi2));
  int f3 = (int)round(midiToFreq(midi3));
  int fFinal = (int)round(midiToFreq(midiFinal));

  const int shortDur = 90*3;   // ms for each short beep
  const int gap = 90*3;        // ms gap between beeps
  const int finalDur = 380*2;  // ms for final longer beep

  if (stage == 0||stage == 2||stage == 4||stage == 6||stage == 7)
    sound(f3, vol_short);
  if (stage == 1||stage == 3||stage == 5||stage == 8)
    soundOff();
}

void SFXAttention(int amountA, int amountB)
{//values are between -100 and +100
  int amount = amountA - amountB; // A is left, B is right

  if (abs(amount) < 5)
  {//not enough attention - dont reward with sound!
    SFXcomplete();
    return;
  }

  int f = map(abs(amount), -100, +100, 80, 900) + random8(100);
  int vol = map(abs(amount), -100, +100, audio_volume / 2, audio_volume * 3 / 4);
  sound(f, vol);
}




void SFXattacking()
{
  int freq = map(sin(millis() / 2.0) * 1000.0, -1000, 1000, 500, 600);
  if (random8(5) == 0)
  {
    freq *= 3;
  }
  sound(freq, audio_volume);
}

void SFXdead()
{//plays next part of sound - since we are dead(that time is held in timeOfStageStart)
  SFXFreqSweepNoise(1000, millis() - timeOfStageStart, 1000, 10, 200);
}


void SFXkill()
{
  sound(2000, audio_volume);
}

/*void SFXwin()
{
  SFXFreqSweepWarble(WIN_OFF_DURATION, millis() - timeOfGameStart, 40, 400, 20);
}

void SFXbosskilled()
{
  SFXFreqSweepWarble(7000, millis() - timeOfGameStart, 75, 1100, 60);
}*/

/*void SFXgameover()
{
  SFXFreqSweepWarble(GAMEOVER_SPREAD_DURATION, millis() - timeOfStageStart, 440, 20, 60);
}*/
