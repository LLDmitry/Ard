//Room data modified (get T_out and p_v, send CO2)
//Аппаратный SPI  http://www.poprobot.ru/home/Arduino-TFT-SPI  http://robotchip.ru/podklyuchenie-tft-displeya-1-8-k-arduino/
//Как известно, Arduino имеет встроенный аппаратный SPI. На Arduino Nano для этого используются выводы с 10 по 13.
//TFT: https://www.arduino.cc/en/Guide/TFT
// TFT 160x128
// GND   GND
// VCC   +5В
// RESET   8
// A0/RS  9
// SDA   11
// SCK   13
// CS  10

//NRF24L01
//MOSI 11 (51 для Arduino Mega)
//SCK 13 (52 для Arduino Mega)
//MISO 12 (50 для Arduino Mega)
//CE и CSN подключаются к любому  цифровому пину Ардуино.
//Питание – на 3,3 В

#include <NrfCommands.h>
#include <TFT.h>
#include "SoftwareSerial.h"
#include "DHT.h"
#include <elapsedMillis.h>
#include <SPI.h>                 // Подключаем библиотеку SPI
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RF24.h>
#include <RF24_config.h>
#include <stdio.h> // for function sprintf

//#include <avr/sleep.h>
//#include <avr/wdt.h>
//#include <util/delay.h>

#include <avr/sleep.h>     //AVR MCU power management
#include <avr/power.h>     //disable/anable AVR MCU peripheries (Analog Comparator, ADC, USI, Timers/Counters)
#include <avr/wdt.h>       //AVR MCU watchdog timer
#include <avr/io.h>        //includes the apropriate AVR MCU IO definitions
#include <avr/interrupt.h> //manipulation of the interrupt flags
#include <util/delay.h>


//RNF  SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
#define RNF_CE_PIN    10  //6
#define RNF_CSN_PIN   9   //7
#define RNF_MOSI      11  //SDA
#define RNF_MISO      12
#define RNF_SCK       13

#define ONE_WIRE_PIN 5       // DS18b20

const byte ROOM_NUMBER = ROOM_SENSOR;
const uint32_t REFRESH_SENSOR_INTERVAL_S = 30;

const byte calcNumberSleeps8s = round(REFRESH_SENSOR_INTERVAL_S / 8);

NRFResponse nrfResponse;
NRFRequest nrfRequest;

volatile byte watchdogCounter;
volatile byte old_ADCSRA;

float tOut = 0.0f;
bool doubleFaileSend = false;

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress tempDeviceAddress;


void setup()
{
  Serial.begin(9600);

  // RF24
  radio.begin();                          // Включение модуля;
  _delay_ms(2);
  radio.enableAckPayload();                     // если раскоментарить, будет отправка на все модули
  radio.setPayloadSize(32); //18
  radio.setChannel(ArRoomsChannelsNRF[ROOM_NUMBER]);
  radio.setRetries(10, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  radio.openWritingPipe(CentralReadingPipe);     // Активация данных для отправки
  radio.openReadingPipe(1, RoomReadingPipe);   // Активация данных для чтения
  radio.stopListening();
  radio.printDetails();

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, 12);

  Serial.print("rOOM_NUMBER=");
  Serial.println(ROOM_NUMBER);
  _delay_ms(10);

  doubleFaileSend = false;
  wdt_enable(WDTO_8S);
}

void(* resetFunc) (void) = 0; // объявляем функцию reset с адресом 0

void RefreshSensorData()
{
  Serial.println("RefreshSensorData");
  sensors.requestTemperatures();
  //tOut = sensors.getTempCByIndex(0); // 
 //tOut = 11.01 + random(1, 10);
  Serial.print("tOut=");
  Serial.println(tOut);
}

//send room data
void PrepareCommandNRF()
{
  //  nrfResponse.Command = command;
  nrfResponse.roomNumber = ROOM_NUMBER;
  nrfResponse.tOut = tOut;
}

void SendCommandNRF()
{
  radio.stopListening();
  PrepareCommandNRF();

  Serial.print("SendNRF ");
  if (radio.write(&nrfResponse, sizeof(nrfResponse)))
  {
    Serial.println("Success Send");
    doubleFaileSend = false;
    _delay_ms(10);
  }
  else
  {
    Serial.println("Failed Send");
    if (doubleFaileSend)
    {
      Serial.println("RESET");
      _delay_ms(50);
      resetFunc(); //вызов reset
    }
    else
      doubleFaileSend = true;
  }
}

//void GoSleep()
//{
//  wdt_reset();
//  cli();                               //disable interrupts for time critical operations below
//
//  power_all_disable();                 //disable all peripheries (ADC, Timer0, Timer1, Universal Serial Interface)
//  /*
//    power_adc_disable();                 //disable ADC
//    power_timer0_disable();              //disable Timer0
//    power_timer1_disable();              //disable Timer2
//    power_usi_disable();                 //disable the Universal Serial Interface module
//  */
//  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //set sleep type
//
//#if defined(BODS) && defined(BODSE)  //if MCU has bulit-in BOD it will be disabled, ATmega328P, ATtiny85, AVR_ATtiny45, ATtiny25
//  sleep_bod_disable();                 //disable Brown Out Detector (BOD) before going to sleep, saves more power
//#endif
//
//  sei();                               //re-enable interrupts
//
//  sleep_mode();                        /*
//                                         system stops & sleeps here, it automatically sets Sleep Enable (SE) bit,
//                                         so sleep is possible, goes to sleep, wakes-up from sleep after interrupt,
//                                         if interrupt is enabled or WDT enabled & timed out, than clears the SE bit.
//*/
//
//  /*** NOTE: sketch will continue from this point after sleep ***/
//}

//void setup_watchdog(byte sleep_time)
//{
//  cli();                           //disable interrupts for time critical operations below
//
//  wdt_enable(sleep_time);          //set WDCE, WDE change prescaler bits
//
//  MCUSR &= ~_BV(WDRF);             //must be cleared first, to clear WDE
//
//#if defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny87__) || defined(__AVR_ATtiny167__)
//  WDTCR  |= _BV(WDCE) & ~_BV(WDE); //set WDCE first, clear WDE second, changes have to be done within 4-cycles
//  WDTCR  |= _BV(WDIE);             //set WDIE to Watchdog Interrupt
//#else
//  WDTCSR |= _BV(WDCE) & ~_BV(WDE); //set WDCE first, clear WDE second, changes have to be done within 4-cycles
//  WDTCSR |= _BV(WDIE);             //set WDIE to Watchdog Interrupt
//#endif
//
//  sei();                           //re-enable interrupts
//}

/**************************************************************************/
/*
    ISR(WDT_vect)
    Watchdog Interrupt Service Routine, executed when watchdog is timed out
    NOTE:
    - if WDT ISR is not defined, MCU will reset after WDT
*/
/**************************************************************************/
// watchdog interrupt
ISR(WDT_vect)
{
  //wdt_disable();  // disable watchdog
  watchdogCounter++;
  //  Serial.println("watchdogCounter++");
  //  _delay_ms(50);
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
  //each wake up get T and send info to central control
  RefreshSensorData();
  SendCommandNRF();
  _delay_ms(50);

  //  Serial.println("GO SLEEP");
  //  _delay_ms(50);
  radio.powerDown();
  while (watchdogCounter < calcNumberSleeps8s) //wait for watchdog counter reached the limit, WDTO_8S * 4 = 32sec.
  {
    //all_pins_output();
    //    Serial.println("GO SLEEP step");
    //    _delay_ms(50);
    GoSleep();  //8 sec sleep
    wdt_reset();
  }
  //  Serial.println("EXIT SLEEP");
  //  _delay_ms(50);

  // wdt_disable();            //disable & stop wdt timer
  watchdogCounter = 0;        //reset counter

  radio.powerUp();

  power_all_enable();         //enable all peripheries (ADC, Timer0, Timer1, Universal Serial Interface)
  _delay_ms(100);
  /*
    power_adc_enable();         //enable ADC
    power_timer0_enable();      //enable Timer0
    power_timer1_enable();      //enable Timer1
    power_usi_enable();         //enable the Universal Serial Interface module
  */
  _delay_ms(5);                   //to settle down ADC & peripheries
  // wdt_enable(WDTO_8S);      //enable wdt timer

  wdt_reset();
}
