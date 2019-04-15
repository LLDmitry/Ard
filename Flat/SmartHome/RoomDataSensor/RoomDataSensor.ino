//https://github.com/enjoyneering/Arduino_Deep_Sleep
//Room data sensor (send T)


//NRF24L01
//MOSI 11 (51 для Arduino Mega)
//SCK 13 (52 для Arduino Mega)
//MISO 12 (50 для Arduino Mega)
//CE и CSN подключаются к любому  цифровому пину Ардуино.
//Питание – на 3,3 В

#include <NrfCommands.h>
#include "SoftwareSerial.h"
#include "DHT.h"
#include <elapsedMillis.h>
#include <SPI.h>                 // Подключаем библиотеку SPI
#include <Wire.h>
#include <EEPROM.h>
#include <RF24.h>
#include <RF24_config.h>
#include <stdio.h> // for function sprintf
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/sleep.h>     //AVR MCU power management
#include <avr/power.h>     //disable/anable AVR MCU peripheries (Analog Comparator, ADC, USI, Timers/Counters)
#include <avr/wdt.h>       //AVR MCU watchdog timer
#include <avr/io.h>        //includes the apropriate AVR MCU IO definitions
#include <avr/interrupt.h> //manipulation of the interrupt flags
#include <util/delay.h>

#define DHTTYPE DHT22



//RNF  SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
#define RNF_CE_PIN    6
#define RNF_CSN_PIN   7
#define RNF_MOSI      11  //SDA
#define RNF_MISO      12
#define RNF_SCK       13



//#define DHT_PIN               5
#define ONE_WIRE_PIN 11       // DS18b20


//DHT dht(DHT_PIN, DHTTYPE);

const byte ROOM_NUMBER = 3; //1,2,3,4; 0 -main control (if exists)
const uint32_t REFRESH_SENSOR_INTERVAL_S = 120;  //2 мин


elapsedMillis refreshSensors_ms = REFRESH_SENSOR_INTERVAL_S * 1000 + 1;


float t = 0.0f;

NRFResponse nrfResponse;
NRFRequest nrfRequest;
volatile byte watchdogCounter;
volatile bool isActiveWork = true;


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
  delay(2);
  radio.enableAckPayload();                     // Allow optional ack payloads
  //radio.enableDynamicPayloads();                // Ack payloads are dynamic payloads

  radio.setPayloadSize(32); //18
  radio.setChannel(ChannelNRF);            // Установка канала вещания;
  radio.setRetries(0, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(CentralReadingPipe);     // Активация данных для отправки
  radio.openReadingPipe(1, ArRoomsReadingPipes[ROOM_NUMBER]);   // Активация данных для чтения
  radio.startListening();

  radio.printDetails();

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, 12);

  setup_watchdog(WDTO_8S); //approximately 8 sec. of sleep
}

void RefreshSensorData()
{
  if (refreshSensors_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    Serial.println("RefreshSensorData");

    sensors.requestTemperatures();
    t = sensors.getTempCByIndex(0);

    PrepareCommandNRF();

    refreshSensors_ms = 0;
  }
}

//Get Command
bool ReadCommandNRF()
{
  if (radio.available())
  {
    Serial.println("radio.available!!");
    //radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
    while (radio.available()) // While there is data ready
    {
      radio.read(&nrfRequest, sizeof(nrfRequest)); // по адресу переменной nrfRequest функция записывает принятые данные
      delay(20);
      Serial.println("radio.available: ");
      Serial.println(nrfRequest.tOut);
    }
    radio.startListening();   // Now, resume listening so we catch the next packets.
    nrfResponse.Command == RSP_NO;
    nrfResponse.tOut = 99.9;
    return true;
  }
}

//send room data
void PrepareCommandNRF()
{
  Serial.println("PrepareCommandNRF1");
  nrfResponse.Command = RSP_INFO;

  nrfResponse.roomNumber = ROOM_NUMBER;

  nrfResponse.tOut = t;

  radio.flush_tx();
  radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
}


void arduino_sleep()
{
  cli();                               //disable interrupts for time critical operations below

  power_all_disable();                 //disable all peripheries (ADC, Timer0, Timer1, Universal Serial Interface)
  /*              
  power_adc_disable();                 //disable ADC
  power_timer0_disable();              //disable Timer0
  power_timer1_disable();              //disable Timer2
  power_usi_disable();                 //disable the Universal Serial Interface module
  */
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //set sleep type

  #if defined(BODS) && defined(BODSE)  //if MCU has bulit-in BOD it will be disabled, ATmega328P, ATtiny85, AVR_ATtiny45, ATtiny25  
  sleep_bod_disable();                 //disable Brown Out Detector (BOD) before going to sleep, saves more power
  #endif

  sei();                               //re-enable interrupts

  sleep_mode();                        /*
                                         system stops & sleeps here, it automatically sets Sleep Enable (SE) bit, 
                                         so sleep is possible, goes to sleep, wakes-up from sleep after interrupt,
                                         if interrupt is enabled or WDT enabled & timed out, than clears the SE bit.
                                       */

  /*** NOTE: sketch will continue from this point after sleep ***/
}

void setup_watchdog(byte sleep_time)
{
  cli();                           //disable interrupts for time critical operations below

  wdt_enable(sleep_time);          //set WDCE, WDE change prescaler bits
  
  MCUSR &= ~_BV(WDRF);             //must be cleared first, to clear WDE

  #if defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny87__) || defined(__AVR_ATtiny167__)
  WDTCR  |= _BV(WDCE) & ~_BV(WDE); //set WDCE first, clear WDE second, changes have to be done within 4-cycles
  WDTCR  |= _BV(WDIE);             //set WDIE to Watchdog Interrupt
  #else
  WDTCSR |= _BV(WDCE) & ~_BV(WDE); //set WDCE first, clear WDE second, changes have to be done within 4-cycles
  WDTCSR |= _BV(WDIE);             //set WDIE to Watchdog Interrupt
  #endif

  sei();                           //re-enable interrupts
}

/**************************************************************************/
/*
    ISR(WDT_vect)
    Watchdog Interrupt Service Routine, executed when watchdog is timed out
    NOTE:
    - if WDT ISR is not defined, MCU will reset after WDT
*/
/**************************************************************************/
ISR(WDT_vect)
{
  watchdogCounter++;
}


void loop()
{
   
  if (isActiveWork)
  {
    RefreshSensorData();
    isActiveWork = !ReadCommandNRF(); //each loop try get command from central control and auto-send nrfResponse
  }
  else
  {
   while (watchdogCounter < 4) //wait for watchdog counter reached the limit, WDTO_8S * 4 = 32sec.
    {
      //all_pins_output();
      arduino_sleep();
    }

    //wdt_disable();            //disable & stop wdt timer
  watchdogCounter = 0;        //reset counter

  power_all_enable();         //enable all peripheries (ADC, Timer0, Timer1, Universal Serial Interface)
  /*
  power_adc_enable();         //enable ADC
  power_timer0_enable();      //enable Timer0
  power_timer1_enable();      //enable Timer1
  power_usi_enable();         //enable the Universal Serial Interface module
  */
  delay(5);                   //to settle down ADC & peripheries

  isActiveWork = true;

  //wdt_enable(WDTO_8S);      //enable wdt timer
  }
}


