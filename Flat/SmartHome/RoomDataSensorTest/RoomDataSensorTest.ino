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
#include <EEPROM.h>
#include <RF24.h>
#include <RF24_config.h>
#include <stdio.h> // for function sprintf
#include <IRremote.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
//
//#define CO2_TX        A0
//#define CO2_RX        A1


//RNF  SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
#define RNF_CE_PIN    6
#define RNF_CSN_PIN   7
#define RNF_MOSI      11  //SDA
#define RNF_MISO      12
#define RNF_SCK       13
//
//#define TFT_CS        10                  // Указываем пины cs
//#define TFT_DC        9                   // Указываем пины dc (A0)
//#define TFT_RST       8                   // Указываем пины reset

const byte ROOM_NUMBER = 6;//ROOM_BED;

const uint32_t REFRESH_SENSOR_INTERVAL_S = 60;  //1 мин

elapsedMillis refreshSensors_ms = REFRESH_SENSOR_INTERVAL_S * 1000 + 1;

NRFResponse nrfResponse;
NRFRequest nrfRequest;

//SoftwareSerial mySerial(CO2_TX, CO2_RX); // TX, RX сенсора

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);

void setup()
{
  Serial.begin(9600);

  //  mySerial.begin(9600);

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

  Serial.print("ROOM_NUMBER=");
  Serial.println(ROOM_NUMBER);

  wdt_enable(WDTO_8S);
}

void RefreshSensorData()
{
  byte cmd[9] = { 0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79 };
  //char response[9];
  unsigned char response[9];

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  if (refreshSensors_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    Serial.println("RefreshSensorData");
    PrepareCommandNRF(RSP_INFO, 100, -100, 99, 99);
    refreshSensors_ms = 0;
  }
}


//Get T out, Pressure and Command
void ReadCommandNRF()
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
    nrfResponse.ventSpeed = 0;
  }
}

void HandleInputNrfCommand()
{
  nrfRequest.Command = RQ_NO;
}

//send room data and set Vent On/Off
void PrepareCommandNRF(byte command, byte ventSpeed, float t_set, byte scenarioVent, byte scenarioNagrev)
{
  Serial.println("PrepareCommandNRF1");
  if (nrfResponse.Command == RSP_NO || command == RSP_COMMAND) //не ставить RSP_INFO пока не ушло RSP_COMMAND
  {
    nrfResponse.Command = command;
  }
  nrfResponse.roomNumber = ROOM_NUMBER;
  nrfResponse.tOut = 11.22;

  radio.flush_tx();
  radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
}

void loop()
{
  ReadCommandNRF(); //each loop try read t_out and other info from central control
  HandleInputNrfCommand();
  RefreshSensorData();
  wdt_reset();
}
