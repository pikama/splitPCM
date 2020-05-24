/*
  This is a modified version of FlagPCM(https://github.com/codemee/FlagPCM). In the 
  FlagPCM version, you need to wait end of playback to do other thing. In this version, 
  you can play PCM data by splitting and do breathing led simultaneously. The original 
  version can be done this action, but you need to split the sound data first and noise 
  problem was found.
*/

/*
  This is a modified version of PCM library (https://github.com/damellis/PCM) to 
  make it compatible with Servo library. Originally,the PCM library uses Timer 1 
  to drive interruppted playback of PCM.But Servo library also uses Timer 1 to send 
  pulse waves to servo.So I change the PCM library to use timer 2 only.But it also 
  changes playback of PCM into non-interrupt driven way.In other words,you have to 
  wait for playback of PCM to do other things.

  meebox@gmail.com 6/29/2017
*/

/*
 * speaker_pcm
 *
 * Plays 8-bit PCM audio on pin 11 using pulse-width modulation (PWM).
 * For Arduino with Atmega168 at 16 MHz.
 *
 * Uses two timers. The first changes the sample value 8000 times a second.
 * The second holds pin 11 high for 0-255 ticks out of a 256-tick cycle,
 * depending on sample value. The second timer repeats 62500 times per second
 * (16000000 / 256), much faster than the playback rate (8000 Hz), so
 * it almost sounds halfway decent, just really quiet on a PC speaker.
 *
 * Takes over Timer 1 (16-bit) for the 8000 Hz timer. This breaks PWM
 * (analogWrite()) for Arduino pins 9 and 10. Takes Timer 2 (8-bit)
 * for the pulse width modulation, breaking PWM for pins 11 & 3.
 *
 * References:
 *     http://www.uchobby.com/index.php/2007/11/11/arduino-sound-part-1/
 *     http://www.atmel.com/dyn/resources/prod_documents/doc2542.pdf
 *     http://www.evilmadscientist.com/article.php/avrdac
 *     http://gonium.net/md/2006/12/27/i-will-think-before-i-code/
 *     http://fly.cc.fer.hr/GDM/articles/sndmus/speaker2.html
 *     http://www.gamedev.net/reference/articles/article442.asp
 *
 * Michael Smith <michael@hurts.ca>
 */

#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "splitPCM.h"

/*
 * The audio data needs to be unsigned, 8-bit, 8000 Hz, and small enough
 * to fit in flash. 10000-13000 samples is about the limit.
 *
 * sounddata.h should look like this:
 *     const int sounddata_length=10000;
 *     const unsigned char sounddata_data[] PROGMEM = { ..... };
 *
 * You can use wav2c from GBA CSS:
 *     http://thieumsweb.free.fr/english/gbacss.html
 * Then add "PROGMEM" in the right place. I hacked it up to dump the samples
 * as unsigned rather than signed, but it shouldn't matter.
 *
 * http://musicthing.blogspot.com/2005/05/tiny-music-makers-pt-4-mac-startup.html
 * mplayer -ao pcm macstartup.mp3
 * sox audiodump.wav -v 1.32 -c 1 -r 8000 -u -1 macstartup-8000.wav
 * sox macstartup-8000.wav macstartup-cut.wav trim 0 10000s
 * wav2c macstartup-cut.wav sounddata.h sounddata
 *
 * (starfox) nb. under sox 12.18 (distributed in CentOS 5), i needed to run
 * the following command to convert my wav file to the appropriate format:
 * sox audiodump.wav -c 1 -r 8000 -u -b macstartup-8000.wav
 */

int speakerPin = 11;
unsigned char const *sounddata_data=0;
int sounddata_length=0;
volatile uint16_t sample;
byte lastSample;

void startPlay(){
	pinMode(speakerPin, OUTPUT);
}

void stopPlay(){
	digitalWrite(speakerPin, LOW);
}

void playback(unsigned char const *data, int stData, int lastData)
{
  sounddata_data = data;
  sounddata_length = lastData - stData + 1;
  //sounddata_length = length;

  
  
  // Set up Timer 2 to do pulse width modulation on the speaker
  // pin.
  
  // Use internal clock (datasheet p.160)
  ASSR &= ~(_BV(EXCLK) | _BV(AS2));
  
  // Set fast PWM mode  (p.157)
  TCCR2A |= _BV(WGM21) | _BV(WGM20);
  TCCR2B &= ~_BV(WGM22);
  
  // Do non-inverting PWM on pin OC2A (p.155)
  // On the Arduino this is pin 11.
  TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0);
  TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0));
  
  // No prescaler (p.158)
  TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);
  
  // Set initial pulse width to the first sample.
  OCR2A = pgm_read_byte(&sounddata_data[stData]);
  
  lastSample = pgm_read_byte(&sounddata_data[lastData]);
  sample = 0;
  sei();

  while(true) {
    if (sample >= sounddata_length) {
        // Disable the PWM timer.
        TCCR2B &= ~_BV(CS10);
        //digitalWrite(speakerPin, LOW);
        break;
    }
    else {
      OCR2A = pgm_read_byte(&sounddata_data[stData+sample]);
    }
    ++sample;
    delayMicroseconds(125);
  }
}
