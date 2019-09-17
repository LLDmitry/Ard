#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include <elapsedMillis.h>

#include <avr/sleep.h>     //AVR MCU power management
#include <avr/power.h>     //disable/anable AVR MCU peripheries (Analog Comparator, ADC, USI, Timers/Counters)
#include <avr/wdt.h>       //AVR MCU watchdog timer
#include <avr/io.h>        //includes the apropriate AVR MCU IO definitions
#include <avr/interrupt.h> //manipulation of the interrupt flags
#include <util/delay.h>

#define RNF_CE_PIN    10
#define RNF_CSN_PIN   9
#define RNF_MOSI      11
#define RNF_MISO      12
#define RNF_SCK       13

#define BTN_PIN       2
#define SENSOR_PIN    3
#define LED_PIN       5

RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);

const byte SENSOR_NUMBER = 1;
const byte CH_NRF = 15;
const uint64_t central_read_pipe = 0xE8E8F0F0E1LL;
const uint64_t central_write_pipe = 0xFFFFFFFF00ULL;
const unsigned long REFRESH_SENSOR_INTERVAL_S = 5;

int inCommand = 0;
int outCommand = 0;

elapsedMillis lastRefreshSensor_ms = REFRESH_SENSOR_INTERVAL_S * 1000;

// watchdog interrupt
ISR(WDT_vect)
{
  wdt_disable();  // disable watchdog
}  // end of WDT_vect

void setup(void)
{
  wdt_disable();
  // RF24
  radio.begin();                          // Включение модуля;
  delay(2);
  radio.setPayloadSize(8);
  radio.setChannel(CH_NRF);            // Установка канала вещания;
  radio.setRetries(10, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(central_read_pipe);     // Активация данных для отправки
  radio.openReadingPipe(1, central_write_pipe);   // Активация данных для чтения
  radio.startListening();

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  wdt_enable(WDTO_2S);
}

void ReadCommandNRF()
{
  if (radio.available())
  {
    Serial.println("radio.available!!");
    radio.read(&inCommand, sizeof(inCommand));
    delay(20);
    Serial.println("inCommand=" + inCommand);
    HandleCommand();
  }
}

void HandleCommand()
{
  if (highByte(inCommand) == SENSOR_NUMBER)

    switch lowByte(inCommand)
    {
      case 0:
        digitalWrite(LED_PIN, LOW);
        break;
      case 1:
        digitalWrite(LED_PIN, HIGH);
        break;
    }
}

void RefreshSensorData()
{
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    Serial.println("RefreshSensorData");

    byte sensor = digitalRead(SENSOR_PIN);
    outCommand = (SENSOR_NUMBER << 8) | sensor;
    WriteCommandNRF();
    lastRefreshSensor_ms = 0;
  }
}

void WriteCommandNRF()
{
  radio.stopListening();
  if (radio.write(&outCommand, sizeof(outCommand)))
  {
    Serial.println("Success Send");
  }
  radio.startListening();
}

void Wake()
{
  // cancel sleep as a precaution
  sleep_disable();
  // precautionary while we do other stuff
  detachInterrupt(0);
  detachInterrupt(1);
  // enable ADC
  //ADCSRA = old_ADCSRA;
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
  //old_ADCSRA = ADCSRA;
  ADCSRA = 0;

  // turn off various modules
  byte old_PRR = PRR;
  PRR = 0xFF;

  // timed sequence coming up
  noInterrupts();

  // will be called when pin D2 goes low
  attachInterrupt(0, Wake, FALLING ); //FALLING RISING
  EIFR = bit(INTF0);  // clear flag for interrupt 0

  // will be called when pin D3 goes low
  attachInterrupt(1, Wake, FALLING);
  EIFR = bit(INTF1);  // clear flag for interrupt 1

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
  //ADCSRA = old_ADCSRA;
}

void loop(void)
{
  radio.powerDown();
  GoSleep(); //1 sec sleep
  sleep_disable();
  radio.powerUp();
  power_all_enable();         //enable all peripheries (ADC, Timer0, Timer1, Universal Serial Interface)
  RefreshSensorData();
  ReadCommandNRF();
  wdt_reset();
}



