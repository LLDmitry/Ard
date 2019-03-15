//WatchDogTimer to reset if stuck
#include <Arduino.h>
#include <avr/wdt.h>
/*
watchdog timer example code.

flashes LED three times quickly on boot up. Then goes thru a loop delaying
an additional 250ms on each iteration. The LED is on during each delay.
Once the delay is long enough, the WDT will reboot the MCU.
*/

const int                                onboardLED = 13;

void setup() {

  int                                  k;

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

  for (k = 1; k <= 3; k = k + 1) {
    digitalWrite(onboardLED, HIGH);
    delay(250L);
    digitalWrite(onboardLED, LOW);
    delay(250L);
  }
  // delay a bit more so it is clear we are done with setup
  delay(750L);
}

void loop() {

  int                                    k;

  // this loop simply turns the LED on and then waits k*250ms. As k increases, the amount of time
  // increases. Until finally the watch dog timer doesn't get reset quickly enough.
  for (k = 1; k <= 10000; k = k + 1) {
    // at the top of this infinite loop, reset the watchdog timer
    wdt_reset();
    digitalWrite(onboardLED, HIGH);
    delay(k * 250L);
    digitalWrite(onboardLED, LOW);
    delay(250L);
  }
}
