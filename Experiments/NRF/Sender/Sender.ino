
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
bool radioNumber = 1;

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 */
RF24 radio(10, 9); //6,7
/**********************************************************/

byte addresses[][6] = {"1Node", "2Node"};

// Used to control whether this node is sending or receiving
bool role = 1;

void setup() {
  Serial.begin(9600);
  Serial.println(F("Sender"));

  RadioSetup();

  // Start the radio listening for data
  // radio.startListening();
}

void RadioSetup()
{
  //RF24
  radio.begin();                          // Включение модуля;
  _delay_ms(2);

//  radio.enableAckPayload();
radio.setAutoAck(true);
  radio.setPayloadSize(32);
  radio.setChannel(0x22);             // Установка канала вещания;
  radio.setRetries(10, 10);               // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);            // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  //radio.setPALevel(RF24_PA_MAX);            // Установка максимальной мощности;
  radio.setPALevel(RF24_PA_LOW);

  radio.openWritingPipe(123);
  radio.openReadingPipe(1, 124);

  radio.stopListening();

  radio.printDetails();
}

void loop() {
  Serial.println(F("Now sending"));
  radio.startListening();
  radio.stopListening();
  unsigned long start_time = micros();                             // Take the time, and send it.  This will block until complete
  if (!radio.write( &start_time, sizeof(unsigned long) ))
    Serial.println("failed");
  else
    Serial.println("SUCCESS");

  Serial.print(F("Sent "));
  Serial.println(start_time);

  delay(2000);
} // Loop
