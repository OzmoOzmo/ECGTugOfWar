/*
  Tug32 - ESP32 Tug of war using nerosky ECG headsets
  (c) A. Clarke Aug 2025
  License: Creative Commons 4.0 Attribution - Share Alike

  Uses 2 mindflex devices 
  this version has both headsets wired direct to the ESP32 
  I have another version were headsets are wireless - fitted with 2 HC-05 BLE modules - but this is less reliable.

  Some of the light particle effects and sounds are taken from Creative Commons code here-> https://github.com/Critters/TWANG

  Some unique features of this code are the estimation of the attention level from the headset when the signal quality is poor.
  This allows the game to continue even when the headset connectio is not perfect - which is vital in a busy show it works just as well as it is able.

  when one headset is worn - the strip shows a bar graph of the attention level - green leds here means perfect signal - yellow is good enough
  when both headsets are worn - the game starts after a 3 second countdown - and continues until one side makes the puck reach the end of the strip by thought alone.

  Usage Notes:
    - Changes to LED strip and other hardware are in config.h
    - Change the strip type to what you are using and compile/load firmware
    - Use Serial port or Wifi to set your strip length.
*/

#include <FastLED.h>
#include "Arduino.h"
#include <RunningMedian.h>
#include "screensavers.h"

#include "config.h"
#include "particle.h"
#include "sound.h"
#include "sfx.h"

//#include "bluetooth_ap.h"
#include "serial_ap.h"

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 301000)
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif


//procedure declarations
void getInput();

//#define VERSION_2 true  //uncomment for a more epic battle

int playerA = 0; // Stores the player A power value (0-PLAYERMAX)
int playerB = 0; // Stores the player A power value (0-PLAYERMAX)

int playerA_Cal = 0; //Average of player A power values before game starts
int playerB_Cal = 0; //Average of player A power values before game starts


CRGB leds[NUM_LEDS+5];
extern int bDebug;

#define PARTICLE_COUNT 100
Particle particlePool[PARTICLE_COUNT] = {
    Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle()};

enum stages
{
  STARTUP,    //startup sequence
  CALIBRATE, //calibration sequence
  PLAY,   //playing the game 
  DEAD, //dead sequence
  SCREENSAVER  // screensaver
} stage;

// Timings
unsigned long previousMillis = 0; // Time of the last redraw
unsigned long lastInputTime = 0;  //last time there was input
unsigned long timeOfStageStart;    // Stores the time the current Game Started


int puckPosition;         // Stores the puck position (0..NUM_LEDS-1)
int puckPositionHighRes;  // Stores the puck position at a higher resolution


//Multithreadded stuff
// -- Task handles for use in the notifications
#define FASTLED_SHOW_CORE 0 // -- The core to run FastLED.show()
static TaskHandle_t FastLEDshowTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;

//brain parsers
#include "Brain.h"
Brain brainA("A");
Brain brainB("B");

/** FastLEDshowESP32()
 *  Call this function instead of FastLED.show(). It signals core 0 to issue a show,
 *  then waits for a notification that it is done.
 */
void FastLEDshowESP32()
{
  if (userTaskHandle == 0)
  {
    // -- Store the handle of the current task, so that the show task can
    //    notify it when it's done
    userTaskHandle = xTaskGetCurrentTaskHandle();

    // -- Trigger the show task
    xTaskNotifyGive(FastLEDshowTaskHandle);

    // -- Wait to be notified that it's done
    const TickType_t xMaxBlockTime = pdMS_TO_TICKS(200);
    ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    userTaskHandle = 0;
  }
}

/** show Task
 *  This function runs on core 0 and just waits for requests to call FastLED.show()
 */
void FastLEDshowTask(void *pvParameters)
{
  // -- Run forever...
  for (;;)
  {
    // -- Wait for the trigger
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // -- Do the show (synchronously)
    FastLED.show();

    // -- Notify the calling task
    xTaskNotifyGive(userTaskHandle);
  }
}

void display_setup(){
  //setup the display pins
  pinMode(led1Pin, OUTPUT);
  pinMode(led2Pin, OUTPUT);
  analogWrite(led1Pin, 0); // off
  analogWrite(led2Pin, 0); // off
}

void setup()
{
  Serial.begin(115200);
  logln("\r\nTUG32 VERSION: ");
  logln(VERSION);

  //important- make sure no old fastled in arduino library - needs latest for rgbw
  FastLED.addLeds<WS2812, FASTLED_DATA_PIN, GRB>(leds, NUM_LEDS).setRgbw(RgbwDefault());

  FastLED.setBrightness(led_brightness);
  //FastLED.setDither(1);

  // -- Create the ESP32 FastLED show task
  xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", 2048, NULL, 2, &FastLEDshowTaskHandle, FASTLED_SHOW_CORE);
  
  sound_init(DAC_AUDIO_PIN);

  bt_setup();

  display_setup();

  stage = STARTUP;
  timeOfStageStart = millis();
}

void loop()
{
  unsigned long millisNow = millis();
  int brightness = 0;

  //sound
  if(stage == PLAY){
    //SFXAttention(brainA.getAverage(), brainB.getAverage());
    //SFXAttention(brainA.attention, brainB.attention);
    SFXPuckPosition(puckPosition);
  }
  else if(stage == DEAD){
      SFXdead();
  }

  if (millisNow - previousMillis >= MIN_REDRAW_INTERVAL)
  {//here 60 times per second
    getInput();

    long frameTimer = millisNow;
    previousMillis = millisNow;

    if (brainA.signalQuality >= 100 && brainB.signalQuality >= 100)
    {//no signal from either brain controller
      if (stage != SCREENSAVER && lastInputTime + SCREENSAVER_STARTS_TIMEOUT < millisNow)
      {
        logln("No signal from either brain, going to screensaver");
        stage = SCREENSAVER;
      }
    }
    else{
      lastInputTime = millisNow; //someone is playing
      if (stage == SCREENSAVER)
      {//we were in screensaver, so exit
        logln("Signal received, exiting screensaver");
        stage = CALIBRATE; //exit screensaver
      }
    }

    if (stage == SCREENSAVER)
    {//Screensaver 
      //Enters screensave when no user detected for 5 seconds
      //Exits Screensaver above when we detect user connected
      screenSaverTick();
      //SFXRaceStart(0);
    }
    else if (stage == STARTUP)
    {//this is the startup sequence when powered on
      //Enters STARTUP only at power on
      if (tickStartup(millisNow))
      {//Sequence completed - Exit STARTUP
        SFXcomplete();
        startAGame();
      }
    }
    else if (stage == CALIBRATE)
    {// CALIBRATE STAGE...  3.2.1 GO!
      tickCalibrate(millisNow);
    }
    else if (stage == PLAY)
    {
      // Ticks and draw calls
      FastLED.clear();
      drawPlayers();
      drawExit();
    }
    else if (stage == DEAD)
    {// DEAD
      FastLED.clear();
      tickDie(millisNow);
      if (!tickParticles())
      {
        startAGame();
      }
    }
    
    if (bDebug >= 0)
        leds[bDebug]= CRGB(255, 255, 0); //debugging led

    FastLEDshowESP32(); // FastLED.show() but on core 0
    displayTick();
  }
}

// ------------ LEVELS -------------
void startAGame()
{
  // -- Reset the game state to game On!
  logln("Reset Game Board");
  FastLED.setBrightness(led_brightness);

  puckPosition = NUM_LEDS/2; // start in the middle of the strip
  puckPositionHighRes = puckPosition * 1000; // start in the middle of the strip
  
  timeOfStageStart = millis();
  stage = CALIBRATE;
}

void die()
{
  // -- Puck explodes signaling one side won 
  for (int p = 0; p < PARTICLE_COUNT; p++)
  {
    particlePool[p].Spawn(puckPosition);
  }
  timeOfStageStart = millis();
  stage = DEAD;
}

// -------- TICKS & RENDERS ---------
bool tickStartup(unsigned long millisNow)
{//called repeatily during startup sequence
  FastLED.clear();

  int timePassed = millisNow - timeOfStageStart;
  SFXFreqSweepWarble(STARTUP_FADE_DUR, timePassed, 40, 400, 20);
  if ( timePassed < STARTUP_WIPEUP_DUR ) // fill to the top with green
  {
    //logln("Startup Stage1");
    int n = map(((millisNow - timeOfStageStart)), 0, STARTUP_WIPEUP_DUR, 0, NUM_LEDS); // fill from top to bottom
    for (int i = 0; i <= n; i++)
    {
      leds[i] = CRGB(0, 255, 0);
    }
  }
  else if ( timePassed < STARTUP_SPARKLE_DUR ) // sparkle the full green bar
  {
    //logln("Startup Stage2");
    for (int i = 0; i < NUM_LEDS; i++)
    {
      if (random8(30) < 28)
        leds[i] = CRGB(0, 255, 0); // most are green
      else
      {
        int flicker = random8(250);
        leds[i] = CRGB(flicker, 150, flicker); // some flicker brighter
      }
    }
  }
  else if ( timePassed < STARTUP_FADE_DUR ) // fade it out to bottom
  {
    //logln("Startup Stage3: ");
    int n = map((millisNow - timeOfStageStart), STARTUP_SPARKLE_DUR, STARTUP_FADE_DUR, 0, NUM_LEDS); // fill from top to bottom
    logln(n);
    int brightness = _max(map((millisNow - timeOfStageStart), STARTUP_SPARKLE_DUR, STARTUP_FADE_DUR, 255, 0), 0);
    for(int i = 0; i<= n; i++)
    {
      leds[i] = CRGB(0, brightness, 0);
    }
  }
  else{
    logln("start sequence complete");
    return true; //start sequence complete
  }
  return false;
}


void tickCalibrate(unsigned long millisNow)
{//called here repeatily before game starts
  FastLED.clear();
  //we want to check the headsets are connected and working
  //if both are working - take some samples and average them

  /*
  //A(headset 1) is on the left side of the strip (entry point to strip)
  //if (brainA.signalQuality != 0)
  {
    //int qA = constrain(brainA.signalQuality, 0, 100);
    //loglnf("Brain A signal quality: %d\n", qA);
    //int nQ = map(qA, 0, 100, 0, NUM_LEDS/2); // bar graph from 0 to max half of the strip

    //int nQ = map(brainA.getAverage(), 0, 100, 0, NUM_LEDS/2); // bar graph from 0 to max half of the strip
    int nQ = map(brainA.attention, 0, 100, 0, NUM_LEDS/2); // bar graph from 0 to max half of the strip
    //Serial.print(brainA.getAverage()); Serial.print(" ");
    //Serial.printf("{%d} ",nQ);
    
    for (int i = 0; i <= nQ; i++)
    {
      leds[i] = brainA.signalQuality > 100 ? CRGB(255, 0, 0) : CRGB(0, 255, 255);
      //Serial.print(i);
      //Serial.print(" ");
    }
  }
  
  //B(headset 2) is on the right side of the strip (far from Esp32)
  //if (brainB.signalQuality != 0)
  {
    //int qB = constrain(brainB.signalQuality, 0, 100);
    //loglnf("Brain B signal quality: %d\n", qB);
    //int nQ = map(qB, 0, 100, 0, NUM_LEDS/2); // bar graph from 0 to max half of the strip
    
    int nQ = map(brainB.getAverage(), 0, 100, 0, NUM_LEDS/2); // bar graph from 0 to max half of the strip

    for (int i = NUM_LEDS-1; i >= (NUM_LEDS - nQ); i--)
    {
      leds[i] = brainB.signalQuality > 100 ? CRGB(255, 0, 0) : CRGB(0, 0, 255);
    }
  }*/

  //if both are working - we can calibrate
  static unsigned long timeStartedCalibrated = -1;
  if (brainA.signalQuality == 0 && brainB.signalQuality == 0)
  {//Great - both brains are working - calibrate
    //we have option to calibrate here - take some samples and subtract when in play - but it doesnt play well.
    //So we will keep this as a countdown to game start only.
    if (timeStartedCalibrated == -1)
    {//start calibrate
      timeStartedCalibrated = millisNow; //start the calibration timer
      logln("Both brains are working, starting calibration");
      //playerA_avg.clear(); playerB_avg.clear();
    }
    else{
      //calibrate a bit more
      //do averaging here
      //playerA_avg.add(brainA.attention); playerB_avg.add(brainB.attention);

      int timePassed = millisNow - timeStartedCalibrated;

      //a count down 3..2..1
      int calibrateProgress = map(timePassed, 0, CALIBRATE_TIMEOUT, 6, 0);
      int lpos = calibrateProgress * 6;
      leds[(NUM_LEDS/2) + lpos] = CRGB(255, 255, 255); // show countdown on the strip
      leds[(NUM_LEDS/2) + lpos-1] = CRGB(255, 255, 255); // show countdown on the strip
      leds[(NUM_LEDS/2) - lpos] = CRGB(255, 255, 255); // show countdown on the strip
      leds[(NUM_LEDS/2) - lpos+1] = CRGB(255, 255, 255); // show countdown on the strip

      //_._._.__
      SFXRaceStart(calibrateProgress);

      if (CALIBRATE_TIMEOUT < timePassed)
      {
        playerA_Cal = brainA.getAverage();
        playerB_Cal = brainB.getAverage();
        //Serial.printf("Calibrated OK - A: %d, B: %d\n", playerA_Cal, playerB_Cal);
        logln("Calibrated OK");
        stage = PLAY; //go to play stage
        timeStartedCalibrated = -1; //so next time we start calibrating again
      }
    }
  }
  else
  {
    if (timeStartedCalibrated == -1)
    {// if either headset is not being worn - do demo where show the current power of one headset
      //logln("Waiting for both brains to be connected and working");
      soundOff(); //stop any sounds
     
      //A(headset 1) is on the left side of the strip (entry point to strip)
      int nQA = map(brainA.getAverage(), 0, 100, 0, NUM_LEDS/2); // bar graph from 0 to max half of the strip
      for (int i = 0; i <= nQA; i++)
      {
        if (brainA.signalQualityNotEstimated == 0){
          leds[i] = CRGB(0, 255, 0); //perfect connection
        }
        else if (brainA.signalQualityNotEstimated < 30){
          leds[i] = CRGB(255/4, 165/4, 0);
        }
        else
          leds[i] = CRGB(255,0 , 0);
        //else
        //  leds[i] = brainA.signalQuality > 100 ? CRGB(255, 0, 0) : CRGB(0, 255, 255);
      }
  
      //B(headset 2) is on the right side of the strip (far from Esp32)
      int nQB = map(brainB.getAverage(), 0, 100, 0, NUM_LEDS/2); // bar graph from 0 to max half of the strip
      for (int i = NUM_LEDS-1; i >= (NUM_LEDS - nQB); i--)
      {
        if (brainB.signalQualityNotEstimated == 0){
          leds[i] = CRGB(0, 255, 0); //perfect connection
        }
        else if (brainB.signalQualityNotEstimated < 30){
          leds[i] = CRGB(255/4, 165/4, 0);
        }
        else
          leds[i] = CRGB(255,0 , 0);
        //leds[i] = brainB.signalQuality > 100 ? CRGB(255, 0, 0) : CRGB(0, 0, 255);}
      }
    }
    else{
      logln("Oops - lost a brain - restart calibration");
      timeStartedCalibrated = -1;
    }
  }
}


void drawPlayers()
{
  int lenA = playerA/6; //bar will be max 1/6 of the 100
  int lenB = playerB/6;

  for (int i = puckPosition + 1; i <= (puckPosition + lenA - 1); i++)
  {
    if (i>=0 && i<NUM_LEDS)
      leds[i] = PLAYER_COLOUR_A; // Player A orange drawn toward Player B
  }
  for (int i = puckPosition - 1; i >= (puckPosition - lenB + 1); i--)
  {
    if (i>=0 && i<NUM_LEDS)
      leds[i] = PLAYER_COLOUR_B; // Player B (green) drawn toward Player A
  }

  if (lenA>lenB){
    puckPositionHighRes+=200;
    if (lenA>(lenB+lenB))
      puckPositionHighRes+=100; //A is dominating
  }
  if (lenA<lenB){
    puckPositionHighRes-=200;
    if (lenB>(lenA+lenA))
      puckPositionHighRes-=100; //B is dominating
  }

  puckPositionHighRes = constrain(puckPositionHighRes, 0, (NUM_LEDS*1000)); // don't let puckPosition go above VIRTUAL_LED_COUNT

  puckPosition = puckPositionHighRes/1000;
  puckPosition = constrain(puckPosition, 0, NUM_LEDS-1); // don't let puckPosition go above LED_COUNT

  #ifdef VERBOSE
  printf("Player A: %d, Player B: %d, Player: %d, puckPositionHighRes: %d\n", playerA, playerB, puckPosition, puckPositionHighRes);
  #endif

  leds[puckPosition] = CRGB(155, 0, 0);

  if (puckPosition <5 || puckPosition > (NUM_LEDS - 6))
  {
    die();
  }
}

void drawExit()
{
  leds[0] = CRGB(255, 0, 0);
  leds[NUM_LEDS - 1] = CRGB(255, 0, 0); // exit is red
}

bool tickParticles()
{
  bool stillActive = false;
  uint8_t brightness;
  int stillAlive = 0;
  for (int p = 0; p < PARTICLE_COUNT; p++)
  {
    if (particlePool[p].Alive())
    {
      stillAlive++;
      particlePool[p].Tick();

      if (particlePool[p]._power < 5)
      {
        brightness = (5 - particlePool[p]._power) * 10;
        leds[particlePool[p]._posLed] += CRGB(brightness, brightness / 2, brightness / 2);
      }
      else
        leds[particlePool[p]._posLed] += CRGB(particlePool[p]._power, 0, 0);

      stillActive = true;
    }
  }
  //]printf("Particles still alive: %d\n", stillAlive);
  return stillActive;
}

void tickDie(long millisNow)
{                           // a short bright explosion...particles persist after it.
  #define duration 200      // milliseconds
  const int width = 20;     // half width of the explosion

  int timePassed = millisNow - timeOfStageStart;
  if (timePassed < duration)
  { // Spread red from player position up and down the width

    int brightness = map(timePassed, 0, duration, 255, 150); // this allows a fade from white to red

    // fill up
    int n = constrain(map(timePassed, 0, duration, puckPosition, puckPosition + width), 0, NUM_LEDS - 1);
    for (int i = puckPosition; i <= n; i++)
    {
      leds[i] = CRGB(255, brightness, brightness);
    }

    // fill to down
    n = constrain(map(timePassed, 0, duration, puckPosition, puckPosition - width), 0, NUM_LEDS - 1);
    for (int i = puckPosition; i >= n; i--)
    {
      leds[i] = CRGB(255, brightness, brightness);
    }
  }
}

int getLED(int pos)
{
  return constrain(pos, 0, led_count - 1);
}


// --------- SCREENSAVER -----------
void screenSaverTick()
{
  //after 20 seconds of inactivity, the screen saver kicks in - and one of the animations is played
  long millisNow = millis();
  //int mode = 2; //(millisNow / 30000) % 4;
  int mode = (millisNow / 30000) % 4;

  SFXcomplete(); // turn off sound...play testing showed this to be a problem

  if (mode == 0)
    juggle(); //dont like fire... Fire2012();
  else if (mode == 1)
    sinelon();
  else if (mode == 2)
    juggle();
  else
    random_LED_flashes();
}

void displayTick()
{
  if (stage == SCREENSAVER)
  {
    //only do this in screensaver mode
    // Timing constants
    const unsigned long fadeTime1 = 1000;  // 1 second for LED1
    const unsigned long fadeTime2 = 1500;  // 1.5 seconds for LED2

    // Get current time
    unsigned long currentTime = millis();
    
    // Calculate brightness for LED1 (1-second cycle)
    float phase1 = (currentTime % fadeTime1) / (float)fadeTime1;
    int brightness1 = (sin(phase1 * TWO_PI) * 127.5 + 127.5);  // Sine wave from 0 to 255
    
    // Calculate brightness for LED2 (1.5-second cycle, out of phase)
    float phase2 = ((currentTime % fadeTime2) / (float)fadeTime2 + 0.5);  // Offset by 180 degrees
    if (phase2 > 1.0) phase2 -= 1.0;
    int brightness2 = (sin(phase2 * TWO_PI) * 127.5 + 127.5);  // Sine wave from 0 to 255
    
    // Write brightness to LEDs
    analogWrite(led1Pin, brightness1);
    analogWrite(led2Pin, brightness2);
  }
  else{
    //display the scores on the display
    //player A on left, player B on right
    //map 0..PLAYERMAX to 0..255 for PWM
    int dispA = constrain(map(playerA, 0, PLAYERMAX, 0, 255), 0, 255);
    int dispB = constrain(map(playerB, 0, PLAYERMAX, 0, 255), 0, 255);

    analogWrite(led1Pin, dispA); // left side display
    analogWrite(led2Pin, dispB); // right side display
  }
}

void getInput()
{
  /*
  // Read two potentiometers connected to GPIO32 and GPIO33
  int pot1 = analogRead(39); // Potentiometer 1
  int pot2 = analogRead(36); // Potentiometer 2

  //loglnf("Pot1: %d, Pot2: %d\n", pot1, pot2);
 
  //// Map pot1 to 0..255 for player A and pot2 to 0..255 for player B
  int brainA = map(pot1, 0, 4095, 0, PLAYERMAX);
  int brainB = map(pot2, 0, 4095, 0, PLAYERMAX);

  playerA = brainA;
  playerB = brainB;
  */

  /*#ifdef hackhack // - use POT for player A
    brainA.signalQuality = 0;
    brainA.attention = map(analogRead(39), 0, 4095, 0, PLAYERMAX);;
    //playerA_Cal = brainA.attention;
    //playerB_Cal = brainB.attention;
  #endif*/

  //playerA_avg.add(brainA.attention);
  //playerB_avg.add(brainB.attention);

  playerA = brainA.getAverage();// - playerA_Cal;
  playerB = brainB.getAverage();// - playerB_Cal;

  //not sure if using calibrations data will be a better game...
  #ifdef VERSION_2
    playerA -= playerA_Cal;
    playerB -= playerB_Cal;
  #endif

  //loglnf("(A)playerA: %d, playerB: %d\n", playerA, playerB);
}



void logln(char* s){Serial.println(s);}
void logln(const char* s){Serial.println(s);}
void logln(int s){Serial.println(s);}
void logln(){Serial.println();}
void log(char* s){ Serial.print(s); }
void log(const char* s){ Serial.print(s); }
void log(unsigned char b, int base){Serial.print(b, base);}