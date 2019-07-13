
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
bool role = 0;

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
  //RF24
  //стоило переключить CS с 9 ножки в + и все бодренько заработало

  radio.begin();                          // Включение модуля;
  _delay_ms(2);

  radio.enableAckPayload();
  radio.setPayloadSize(32);
  radio.setChannel(0x60);             // Установка канала вещания;
  //radio.setRetries(10, 10);               // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);            // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);            // Установка максимальной мощности;
  //radio.setPALevel(RF24_PA_LOW);

  //  if (radioNumber) {
  //    radio.openReadingPipe(1, CentralReadingPipe);
  //    radio.openWritingPipe(ArRoomsReadingPipes[ROOM_SENSOR]);
  //  }
  //  else
  //  {
  //    radio.openWritingPipe(CentralReadingPipe);
  //    radio.openReadingPipe(1, ArRoomsReadingPipes[ROOM_SENSOR]);
  //  }

  //  if (radioNumber) {
  //    radio.openWritingPipe(addresses[1]);
  //    radio.openReadingPipe(1, addresses[0]);
  //  } else {
  //    radio.openWritingPipe(addresses[0]);
  //    radio.openReadingPipe(1, addresses[1]);
  //  }
  //
  //
  if (radioNumber) {
    radio.openReadingPipe(1, CentralReadingPipe);
    radio.openWritingPipe(ArRoomsReadingPipes[ROOM_SENSOR]);
  }
  else
  {
    radio.openWritingPipe(CentralReadingPipe);
    radio.openReadingPipe(1, ArRoomsReadingPipes[ROOM_SENSOR]);
  }

  radio.printDetails();
}

void loop() {


  /****************** Ping Out Role ***************************/
  if (role == 1)  {

    radio.stopListening();                                    // First, stop listening so we can talk.


    Serial.println(F("Now sending"));

    unsigned long start_time = micros();                             // Take the time, and send it.  This will block until complete
    if (!radio.write( &start_time, sizeof(unsigned long) )) {
      Serial.println(F("failed"));
    }

    radio.startListening();                                    // Now, continue listening

    unsigned long started_waiting_at = micros();               // Set up a timeout period, get the current microseconds
    boolean timeout = false;                                   // Set up a variable to indicate if a response was received or not

    while ( ! radio.available() ) {                            // While nothing is received
      if (micros() - started_waiting_at > 200000 ) {           // If waited longer than 200ms, indicate timeout and exit while loop
        timeout = true;
        break;
      }
    }

    if ( timeout ) {                                            // Describe the results
      Serial.println(F("Failed, response timed out."));
    } else {
      unsigned long got_time;                                 // Grab the response, compare, and send to debugging spew
      radio.read( &got_time, sizeof(unsigned long) );
      unsigned long end_time = micros();

      // Spew it
      Serial.print(F("Sent "));
      Serial.print(start_time);
      Serial.print(F(", Got response "));
      Serial.print(got_time);
      Serial.print(F(", Round-trip delay "));
      Serial.print(end_time - start_time);
      Serial.println(F(" microseconds"));
    }

    // Try again 1s later
    delay(1000);
  }



  /****************** Pong Back Role ***************************/

  if ( role == 0 )
  {
    unsigned long got_time;

    if ( radio.available()) {
      // Variable for the received timestamp
      while (radio.available()) {                                   // While there is data ready
        radio.read( &got_time, sizeof(unsigned long) );             // Get the payload
      }

      radio.stopListening();                                        // First, stop listening so we can talk
      radio.write( &got_time, sizeof(unsigned long) );              // Send the final one back.
      radio.startListening();                                       // Now, resume listening so we catch the next packets.
      Serial.print(F("Sent response "));
      Serial.println(got_time);
    }
  }



  //
  ///****************** Change Roles via Serial Commands ***************************/
  //
  //  if ( Serial.available() )
  //  {
  //    char c = toupper(Serial.read());
  //    if ( c == 'T' && role == 0 ){
  //      Serial.println(F("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK"));
  //      role = 1;                  // Become the primary transmitter (ping out)
  //
  //   }else
  //    if ( c == 'R' && role == 1 ){
  //      Serial.println(F("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK"));
  //       role = 0;                // Become the primary receiver (pong back)
  //       radio.startListening();
  //
  //    }
  //  }


} // Loop
