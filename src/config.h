#ifndef SRC_CONFIG_H
#define SRC_CONFIG_H


#define averagingLength 5 // how many samples to average for the player power - keep low (5 or under)
#define led_brightness 150 //80
#define audio_volume 20	// 0 to 255


#define SCREENSAVER_STARTS_TIMEOUT 5000 // time until screen saver in milliseconds
#define CALIBRATE_TIMEOUT 2000 //3000 //calibrate for 3 or 5 seconds - set to 1000 for Quick calibration


#define VERSION "2025-08-17"

#define FASTLED_DATA_PIN        19			//for fastled library
#define DAC_AUDIO_PIN 		25     // on ESP - should be 25 or 26 only
#define led1Pin 13            // GPIO12 -PWM to Display
#define led2Pin 12            // GPIO13 - PWM to Display


#define NUM_LEDS        		144
#define PLAYERMAX 100 //0..100 from the nerosky


#define PLAYER_COLOUR_A CRGB(255/4, 165/4, 0) // ORANGE
#define PLAYER_COLOUR_B CRGB(0, 255/4, 0)   //GREEN

//CRGB(0, 35, 00); // faint green toward Player A
//CRGB(0, 0, 35); // faint blue toward Player B

#define STARTUP_WIPEUP_DUR 200
#define STARTUP_SPARKLE_DUR 1300
#define STARTUP_FADE_DUR 1500 //1500 Boot sequence time in milliseconds

#define GAMEOVER_SPREAD_DURATION 1000

//NEOPIXEL details
#define MIN_REDRAW_INTERVAL 	1000.0 / 60.0    // divide by frames per second..if you tweak adjust player speed
#define led_count NUM_LEDS


#include <arduino.h>
void logln(char* s);
void logln(const char* s);
void logln(int s);
void logln();
void log(char* s);
void log(const char* s);
void log(unsigned char b, int base);

/*
void logln(int s){}
void logln(char* s){}
void logln(const char* s){}
void logln(){}
void log(char* s){}
void log(const char* s){}
*/

#endif
