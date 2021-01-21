#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"


RF24 radio(9,10);
const uint64_t pipe = 0xE8E8F0F0E1LL;


void setup(void)
{
  Serial.begin(9600);
  printf_begin();
  radio.begin();
  //radio.setChannel(0x40);
  //radio.setPALevel(RF24_PA_MAX);
  //radio.setDataRate(RF24_250KBPS);
  radio.setDataRate(RF24_2MBPS);
 
  radio.enableAckPayload();
  radio.openReadingPipe(1,pipe);
  radio.startListening();
  radio.setAutoAck(1);
 
  radio.printDetails();

  printf("\n\r");
}


void loop(void)
{
  uint32_t message = 111;
  radio.writeAckPayload( 1, &message, sizeof(message) );
  if ( radio.available() ) {
      int dataIn;
      bool done = false;
      while (!done) {
          done = radio.read( &dataIn, sizeof(dataIn));
          Serial.println(dataIn);
      }
  }
}
int serial_putc( char c, FILE * ) {
  Serial.write( c );
  return c;
}

void printf_begin(void) {
  fdevopen( &serial_putc, 0 );
}













///*
//   See documentation at https://nRF24.github.io/RF24
//   See License information at root directory of this library
//   Author: Brendan Doherty (2bndy5)
//*/
//
///**
//   A simple example of sending data from 1 nRF24L01 transceiver to another.
//
//   This example was written to be used on 2 devices acting as "nodes".
//   Use the Serial Monitor to change each node's behavior.
//*/
////#include "printf.h"
////#include "RF24.h"
//
////Для управления (с дисплеем и кнопками) температурой в каждой комнате и связи с CentralControl.
////Также передает сигнал тревоги в CentralControl. Для некоторых комнат включает сигнал тревоги
//
//#include <EEPROM.h>
//#include <NrfCommandsESP32.h> // C:\Program Files (x86)\Arduino\libraries\NrfCommandsESP32
//#include "sav_button.h" // Библиотека работы с кнопками
//#include <elapsedMillis.h>
//#include <Arduino.h>
//#include <avr/wdt.h>
//#include <RF24.h>
//#include <nRF24L01.h>
////#include <RF24_config.h>
//#include <OneWire.h>
//#include <DallasTemperature.h>
//
//#include <Wire.h>
//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>
//#include <SPI.h>
//
////RNF
//#define RNF_CE_PIN    6
//#define RNF_CSN_PIN   7
//#define RNF_MOSI      11
//#define RNF_MISO      12
//#define RNF_SCK       13
//
//const byte ROOM_NUMBER = ROOM_BED;
//
//NRFResponse nrfResponse;
//NRFRequest nrfRequest;
//
//float payload = 0.0;
//
//// instantiate an object for the nRF24L01 transceiver
//RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);
//
//void setup() {
//
//  Serial.begin(9600);
//
//
//  // initialize the transceiver on the SPI bus
//  //  if (!radio.begin()) { rf24_esp
//  //    Serial.println(F("radio hardware is not responding!!"));
//  //    while (1) {} // hold in infinite loop
//  //  }
//
//  radio.begin();
//  Serial.println("radio.begin");
//  delay(20);
//
//  // RF24
//  delay(2);
//  //radio.enableAckPayload();                     // Allow optional ack payloads
//  //radio.enableDynamicPayloads();                // Ack payloads are dynamic payloads
//  radio.setPayloadSize(32); //18
//  //radio.setPayloadSize(sizeof(payload)); // float datatype occupies 4 bytes
//  radio.setChannel(ArRoomsChannelsNRF[ROOM_NUMBER]);
//  radio.setRetries(0, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
//  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
//  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
//  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
//  radio.openWritingPipe(CentralReadingPipe);     // Активация данных для отправки
//  radio.openReadingPipe(1, RoomReadingPipe);   // Активация данных для чтения
//  radio.startListening();
//  //radio.printDetails();
//
//  // For debugging info
//  // printf_begin();             // needed only once for printing details
//  // radio.printDetails();       // (smaller) function that prints raw register values
//  // radio.printPrettyDetails(); // (larger) function that prints human readable data
//
//  Serial.println("setup done");
//} // setup
//
//
//void PrepareCommandNRF()
//{
//  Serial.println("PrepareCommandNRF 1");
//  delay(20);
//  nrfResponse.Command = RSP_INFO;
//  nrfResponse.roomNumber = ROOM_NUMBER;
//  nrfResponse.alarmType = ALR_NO;
//  nrfResponse.tInn = 11;
//  nrfResponse.tOut = 22;
//  nrfResponse.tOutDec = 88;
//
//  uint8_t f = radio.flush_tx();
//  radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
//  Serial.println("PrepareCommandNRF 2");
//  delay(20);
//}
//
////Get Command
//void ReadCommandNRF()
//{
//  //radio.startListening();
//  Serial.println("ReadCommandNRF 1");
//  delay(20);
//  bool done = false;
//
//  uint32_t message = 111;  //Вот какой потенциальной длины сообщение - uint32_t!
//  //туда можно затолкать значение температуры от датчика или еще что-то полезное.
//  radio.writeAckPayload( 1, &message, sizeof(message) ); // Грузим сообщение для автоотправки;
//
//  if (radio.available())
//  {
//    Serial.println("radio.available!!");
//    radio.read(&nrfRequest, sizeof(nrfRequest));
//    //radio.stopListening();
//    delay(20);
//    _delay_ms(20);
//    Serial.println("radio.read: ");
//    Serial.println(nrfRequest.roomNumber);
//    //    Serial.println("sizeof1: ");
//    //    Serial.println(sizeof(nrfRequest));
//    //    Serial.println("sizeof2: ");
//    //    Serial.println(sizeof(nrfRequest.p_v));
//    //    Serial.println("sizeof3: ");
//    //    Serial.println(sizeof(nrfRequest.Command));
//    //    Serial.println("sizeof4: ");
//    //    Serial.println(sizeof(nrfRequest.p_v));
//    //    Serial.println("sizeof5: ");
//    //    Serial.println(sizeof(nrfRequest.tOut));
//    Serial.println(nrfRequest.p_v);
//    Serial.println(nrfRequest.Command);
//    Serial.println(nrfRequest.minutes);
//    Serial.println(nrfRequest.tOut);
//    Serial.println(nrfRequest.tOutDec);
//    Serial.println(nrfRequest.tInnSet);
//    _delay_ms(20);
//
//    nrfResponse.Command == RSP_NO;
//    nrfResponse.tInn = 7;
//  }
//  Serial.println("ReadCommandNRF 2");
//  delay(20);
//}
//
//void loop()
//{
//  delay(1000);
//  Serial.println("loop");
//  ReadCommandNRF();
//  //PrepareCommandNRF();
//} // loop
