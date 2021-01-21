#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"


RF24 radio(9, 10);
const uint64_t pipe = 0xE8E8F0F0E1LL;
uint32_t message;
int counter = 0;

void setup(void)
{
  Serial.begin(9600);
  printf_begin();
  radio.begin();
  //radio.setChannel(0x40);
  //radio.setPALevel(RF24_PA_MAX);
  //radio.setDataRate(RF24_250KBPS);
  //radio.setRetries(15, 15);
  radio.setDataRate(RF24_2MBPS);

  radio.enableAckPayload();
  radio.openWritingPipe(pipe);
}


void loop(void)
{
  int command = random(1, 4);
  Serial.print("Send: ");
  Serial.println(command);

  radio.flush_tx();
  radio.startListening();
  radio.stopListening();
  radio.write( &command, sizeof(command) );

  if ( radio.isAckPayloadAvailable() ) {
    radio.read(&message, sizeof(message));
    Serial.print("Receive: ");
    Serial.println(message);
  }

  delay(1000);
}
int serial_putc( char c, FILE * ) {
  Serial.write( c );
  return c;
}

void printf_begin(void) {
  fdevopen( &serial_putc, 0 );
}







////#include <printf.h>
//#include <SPI.h>
//#include "time.h"
//#include <NrfCommandsESP32.h> // C:\Program Files (x86)\Arduino\libraries\NrfCommandsESP32
//#include <elapsedMillis.h>
//#include <OneWire.h>
//#include <DallasTemperature.h>
//#include "sav_button.h" // Библиотека работы с кнопками
//#include <RF24.h>
//#include <nRF24L01.h>
//#include <Adafruit_BMP085.h> //давление Vcc – +5в; //esp32 SDA(SDI) – 21;SCL(SCK) - 22 //mega SDA – A4;SCL - A5
//
////RNF SPI bus plus pins  9,10 для Уно или 9, 53 для Меги
////NRF24L01 для ESP32 - ce-17, cs-5, sck-18, miso-19, mosi-23)
//#define RNF_CE_PIN    6   //17
//#define RNF_CSN_PIN   7   //5
//#define RNF_MOSI      11  //23
//#define RNF_MISO      12  //19
//#define RNF_SCK       13  //18
//
////#define RNF_CE_PIN    17
////#define RNF_CSN_PIN   5
////#define RNF_MOSI      23
////#define RNF_MISO      19
////#define RNF_SCK       18
//
//
//// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
//RF24 radio(RNF_CE_PIN, RNF_CSN_PIN); //esp32 NRF24L01 - ce-17, cs-5, sck-18, miso-19, mosi-23)
//
//float payload = 0.0;
//
//NRFRequest nrfRequest;
//NRFResponse nrfResponse;
//
//#define LED 2
//void setup()
//{
//  Serial.begin(9600);
//  pinMode(LED, OUTPUT);
//  RadioSetup();
//}
//
//void RadioSetup()
//{
//  Serial.println("RadioSetup 1");
//  delay(10);
//  //RF24
//  radio.begin();                          // Включение модуля;
//  delay(20);
//  //radio.enableAckPayload();       //+
//  radio.setPayloadSize(32);
//  //radio.setPayloadSize(sizeof(payload)); // float datatype occupies 4 bytes
//  radio.setChannel(ArRoomsChannelsNRF[ROOM_BED]);             // Установка канала вещания;
//  radio.setRetries(10, 10);               // Установка интервала и количества попыток "дозвона" до приемника;
//  radio.setDataRate(RF24_1MBPS);            // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
//  radio.setPALevel(RF24_PA_MAX);            // Установка максимальной мощности;
//  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
//  radio.openWritingPipe(RoomReadingPipe);   // Активация данных для отправки
//  radio.openReadingPipe(1, CentralReadingPipe);    // Активация данных для чтения
//  radio.stopListening();
//
//  //radio.printDetails();
//  Serial.println("RadioSetup 2");
//  delay(10);
//}
//
//void ReadCommandNRF() //from reponse
//{
//  Serial.println("                                     ReadCommandNRF()");
//  if ( radio.available() )
//  {
//    bool done = false;
//    Serial.println("radio.available!!");
//    //while (!done)
//    //{
//    radio.read(&nrfResponse, sizeof(nrfResponse));
//    delay(20);
//    //radio.stopListening();
//    Serial.print("received data from room: ");
//    Serial.println(nrfResponse.roomNumber);
//    //}
//    // ParseAndHandleInputNrfCommand();
//  }
//}
//
//void NrfCommunication()
//{
//  //    radio.setChannel(ArRoomsChannelsNRF[ROOM_SENSOR]);
//  //    radio.openWritingPipe(ArRoomsReadingPipes[ROOM_SENSOR]); //for confirm writes
//  //ReadCommandNRF(); // from ROOM_SENSOR                                         пока отключил!!!
//  Serial.println("NrfCommunication 1");
//  delay(10);
//  radio.stopListening();
//  Serial.println("stopListening");
//  delay(10);
//  //  for (byte iRoom = 0; iRoom < ROOM_SENSOR; iRoom++)
//  //  {
//  //    SendCommandNRF(iRoom);
//  //  }
//  SendCommandNRF(ROOM_BED);
//  //  radio.setChannel(ArRoomsChannelsNRF[ROOM_SENSOR]);
//  //  radio.startListening();
//  Serial.println("NrfCommunication 2");
//  delay(10);
//}
//
//void PrepareRequestCommand(byte roomNumber)
//{
//  Serial.println("PrepareRequestCommand 1");
//  delay(10);
//  nrfRequest.Command = RQ_T_INFO;
//  nrfRequest.roomNumber = roomNumber;
//  //nrfRequest.tOut = t_out;
//  nrfRequest.tOut = 33;
//  nrfRequest.tOutDec = random(100);
//  Serial.println("tOutDec");
//  Serial.println(nrfRequest.tOutDec);
//  nrfRequest.p_v = 123;
//  nrfRequest.tInnSet = 27;
//  nrfRequest.hours = 22;
//  nrfRequest.minutes = 44;
//  //  Serial.println("sizeof1: ");
//  //  Serial.println(sizeof(nrfRequest));
//  //  Serial.println("sizeof2: ");
//  //  Serial.println(sizeof(nrfRequest.p_v));
//  //  Serial.println("sizeof3: ");
//  //  Serial.println(sizeof(nrfRequest.Command));
//  //  Serial.println("sizeof4: ");
//  //  Serial.println(sizeof(nrfRequest.p_v));
//  //  Serial.println("sizeof5: ");
//  //  Serial.println(sizeof(nrfRequest.tOut));
//  //  Serial.println("PrepareRequestCommand 2");
//  delay(10);
//}
//
//void SendCommandNRF(byte roomNumber)
//{
//  Serial.print("roomNumber: ");
//  Serial.println(roomNumber);
//  delay(10);
//
//  PrepareRequestCommand(roomNumber);
//
//  radio.setChannel(ArRoomsChannelsNRF[roomNumber]);
//  radio.flush_tx();
//  if (radio.write(&nrfRequest, sizeof(nrfRequest)))
//  {
//    Serial.println("Success Send");
//    delay(10);
//    if (!radio.isAckPayloadAvailable() )   // Ждем получения..
//      Serial.println(F("Empty response."));
//    else
//    {
//      //radio.read(&nrfResponse, sizeof(nrfResponse));
//      uint32_t message;
//      radio.read(&message, sizeof(message)); //... и имеем переменную message с числом 111 от приемника
//      delay(20);
//      Serial.print(F("RESPONSE! from "));
//      Serial.println(message);
//      //      Serial.println(nrfResponse.roomNumber);
//      //      Serial.println(nrfResponse.tOut);
//      //      Serial.println(nrfResponse.tOutDec);
//      //ParseAndHandleInputNrfCommand();
//    }
//  }
//  else
//    Serial.println("Failed Send");
//  delay(10);
//}
//
////void SendCommand()
////{
////  unsigned long start_timer = micros();                    // start the timer
////  bool report = radio.write(&payload, sizeof(float));      // transmit & save the report
////  unsigned long end_timer = micros();                      // end the timer
////
////  if (report) {
////    Serial.print(F("Transmission successful! "));          // payload was delivered
////    Serial.print(F("Time to transmit = "));
////    Serial.print(end_timer - start_timer);                 // print the timer result
////    Serial.print(F(" us. Sent: "));
////    Serial.println(payload);                               // print payload sent
////    payload += 0.01;                                       // increment float payload
////  } else {
////    Serial.println(F("Transmission failed or timed out")); // payload was not delivered
////  }
////
////  // to make this example readable in the serial monitor
////  delay(1000);  // slow transmissions down by 1 second
////}
//
//void loop()
//{
//  Serial.println("loop");
//  delay(10);
//  delay(5000);
//  //  digitalWrite(LED, HIGH);
//  //  delay(1000);
//  //  digitalWrite(LED, LOW);
//
//  NrfCommunication(); //read / send / read response
//  //SendCommand();
//}
