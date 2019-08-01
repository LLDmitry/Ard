
/*
  Getting Started example sketch for nRF24L01+ radios
  This is a very basic example of how to send data from one node to another
  Updated: Dec 2014 by TMRh20
*/

#include <NrfCommands.h> // C:\Program Files (x86)\Arduino\libraries\NrfCommands
#include <SPI.h>
#include "RF24.h"

/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/
bool radioNumber = 0;

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(6, 7); //10,9
/**********************************************************/

byte addresses[][6] = {"1Node", "2Node"};

// Used to control whether this node is sending or receiving

void setup() {
  Serial.begin(9600);
  Serial.println(F("RF24/examples/GettingStarted"));
  Serial.println(F("Receiver"));

  RadioSetup();

  // Start the radio listening for data
  radio.startListening();
}

void RadioSetup()
{
  radio.begin();                          // Включение модуля;
  _delay_ms(2);

  //radio.enableAckPayload();
  radio.setAutoAck(true);
  radio.setPayloadSize(32);
  radio.setChannel(0x22);             // Установка канала вещания;
  radio.setRetries(10, 10);               // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);            // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  //radio.setPALevel(RF24_PA_MAX);            // Установка максимальной мощности;
  radio.setPALevel(RF24_PA_LOW);

//  radio.openReadingPipe(1, CentralReadingPipe);
//  radio.openWritingPipe(ArRoomsReadingPipes[ROOM_SENSOR]);

  radio.openReadingPipe(1, 125);
  radio.openWritingPipe(123);
  

  radio.printDetails();
}

void loop() {
  unsigned long got_time;
  bool done = false;
  if ( radio.available()) {
    // Variable for the received timestamp
    Serial.println("available!!!");
    while (!done) {
      done = radio.read( &got_time, sizeof(unsigned long) );             // Get the payload
      Serial.println("read");
      delay(50);

    }
    // Now, resume listening so we catch the next packets.
    Serial.print(F("got_time = "));
    Serial.println(got_time);
  }
  delay(100);
} // Loop
