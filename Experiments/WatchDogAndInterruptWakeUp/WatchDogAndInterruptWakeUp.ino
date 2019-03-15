//WatchDogTimer+Interrupt, wakeUp
//https://www.gammon.com.au/power
#include <avr/sleep.h>
#include <avr/wdt.h>

const byte LED = 13;
const byte BUTTON_PIN = 2;
#define BAT_PIN A0

volatile byte old_ADCSRA;

// watchdog interrupt
ISR(WDT_vect)
{
  wdt_disable();  // disable watchdog
}  // end of WDT_vect

void setup()
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(9600);
  Serial.println("start");
}

void wake()
{
  // cancel sleep as a precaution
  sleep_disable();
  // precautionary while we do other stuff
  detachInterrupt(0);
  // enable ADC
  ADCSRA = old_ADCSRA;
}  // end of wake

void sleep()
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
  attachInterrupt(0, wake, FALLING);
  EIFR = bit(INTF0);  // clear flag for interrupt 0

  // ready to sleep
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // turn off brown-out enable in software
  MCUCR = bit(BODS) | bit(BODSE);
  MCUCR = bit(BODS);
  interrupts();
  sleep_cpu();

  // cancel sleep as a precaution
  sleep_disable();
  PRR = old_PRR;
  ADCSRA = old_ADCSRA;
}

void loop()
{
  int i = analogRead(BAT_PIN);
  Serial.println(i);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  delay(100);
  digitalWrite(LED, LOW);
  pinMode(LED, INPUT);

  sleep();

} // end of loop

