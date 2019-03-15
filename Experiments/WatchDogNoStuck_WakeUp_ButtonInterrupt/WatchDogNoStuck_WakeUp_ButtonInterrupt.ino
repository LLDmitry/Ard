// WatchDogNoStuckWakeUp
#include <Arduino.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
/*
  watchdog timer example code.

  flashes LED three times quickly on boot up. Then goes thru a loop delaying
  an additional 250ms on each iteration. The LED is on during each delay.
  Once the delay is long enough, the WDT will reboot the MCU.
*/
#define BTTN_PIN1 2 //чтобы работало прерывание 0

const int                                onboardLED = 13;

volatile byte old_ADCSRA;

// watchdog interrupt
ISR(WDT_vect)
{
  //wdt_disable();  // disable watchdog
}  // end of WDT_vect

void setup() {

  pinMode(BTTN_PIN1, INPUT_PULLUP);

  Serial.begin(9600);
  Serial.println("Setup..");

  // immediately disable watchdog timer so set will not get interrupted

  wdt_disable();

  // I often do serial i/o at startup to allow the user to make config changes of
  // various constants. This is often using fgets which will wait for user input.
  // any such 'slow' activity needs to be completed before enabling the watchdog timer.

  // the following forces a pause before enabling WDT. This gives the IDE a chance to
  // call the bootloader in case something dumb happens during development and the WDT
  // resets the MCU too quickly. Once the code is solid, remove this.

  delay(2L * 1000L);

  // enable the watchdog timer. There are a finite number of timeouts allowed (see wdt.h).
  // Notes I have seen say it is unwise to go below 250ms as you may get the WDT stuck in a
  // loop rebooting.
  // The timeouts I'm most likely to use are:
  // WDTO_1S
  // WDTO_2S
  // WDTO_4S
  // WDTO_8S

  wdt_enable(WDTO_4S);

  // initialize the digital pin as an output.
  // Pin 13 has an LED connected on most Arduino boards:

  pinMode(onboardLED, OUTPUT);

  // at bootup, flash LED 3 times quick so I know the reboot has occurred.

  for (int k = 1; k <= 3; k = k + 1) {
    digitalWrite(onboardLED, HIGH);
    delay(250L);
    digitalWrite(onboardLED, LOW);
    delay(250L);
  }
  // delay a bit more so it is clear we are done with setup
  delay(750L);
}

void Wake()
{
  // cancel sleep as a precaution
  sleep_disable();
  // precautionary while we do other stuff
  detachInterrupt(0);
  // enable ADC
  ADCSRA = old_ADCSRA;
}

void GoSleep()
{
  // clear various "reset" flags
  MCUSR = 0;
  // allow changes, disable reset
  WDTCSR = bit(WDCE) | bit(WDE);
  // set interrupt mode and an interval
  //WDTCSR = bit(WDIE) | bit(WDP2) | bit(WDP1);    // set WDIE, and 1 second delay
  WDTCSR = bit(WDIE) | bit(WDP3) | bit(WDP0);    // set WDIE, and 8 seconds delay
  wdt_reset();  // pat the dog

  // disable ADC
  old_ADCSRA = ADCSRA;
  ADCSRA = 0;

  // turn off various modules
  byte old_PRR = PRR;
  PRR = 0xFF;

  // timed sequence coming up
  noInterrupts();

  // will be called when pin D2 goes low
  attachInterrupt(0, Wake, FALLING);
  EIFR = bit(INTF0);  // clear flag for interrupt 0

  // ready to sleep
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // turn off brown-out enable in software
  MCUCR = bit(BODS) | bit(BODSE);
  MCUCR = bit(BODS);
  interrupts(); // guarantees next instruction executed
  sleep_cpu();

  // cancel sleep as a precaution
  sleep_disable();
  PRR = old_PRR;
  ADCSRA = old_ADCSRA;
}

void loop()
{
  // this loop simply turns the LED on and then waits k*250ms. As k increases, the amount of time
  // increases. Until finally the watch dog timer doesn't get reset quickly enough.
  for (int k = 1; k <= 10000; k = k + 1)
  {
    Serial.println(k);
    // at the top of this infinite loop, reset the watchdog timer
    wdt_reset();
    wdt_enable(WDTO_4S);
    digitalWrite(onboardLED, HIGH);
    delay(k * 1000L);

    

    GoSleep(); //1 sec sleep
    // cancel sleep as a precaution
    sleep_disable();

    digitalWrite(onboardLED, LOW);
    delay(250L);
  }
}
