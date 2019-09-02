//Room data sensor (send T)


//NRF24L01
//MOSI 11 (51 для Arduino Mega)
//SCK 13 (52 для Arduino Mega)
//MISO 12 (50 для Arduino Mega)
//CE и CSN подключаются к любому  цифровому пину Ардуино.
//Питание – на 3,3 В

#include <NrfCommands.h>
#include "SoftwareSerial.h"
//#include "DHT.h"
#include <elapsedMillis.h>
#include <SPI.h>                 // Подключаем библиотеку SPI
#include <Wire.h>
#include <EEPROM.h>
#include <RF24.h>
#include <RF24_config.h>
#include <stdio.h> // for function sprintf
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <Adafruit_BMP085.h> //давление Vcc – +5в; SDA – (A4);SCL - (A5)


//RNF  SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
#define RNF_CE_PIN    10 //6
#define RNF_CSN_PIN   9 //7
#define RNF_MOSI      11  //SDA
#define RNF_MISO      12
#define RNF_SCK       13


#define ONE_WIRE_PIN 5       // DS18b20

#define VENT_SPEED1_PIN 7
#define VENT_SPEED2_PIN 8
#define VENT_SPEED3_PIN 16 //A3

#define P1_PIN 5  //до фильтра
#define P2_PIN 4  //после фильтра

const byte ROOM_NUMBER = ROOM_VENT;

const uint32_t REFRESH_SENSOR_INTERVAL_S = 120;  //2 мин
const uint32_t READ_COMMAND_NRF_INTERVAL_S = 1;


elapsedMillis refreshSensors_ms = REFRESH_SENSOR_INTERVAL_S * 1000 + 1;
elapsedMillis readCommandNRF_ms = 0;


float t_out = 0.0f;
int p1_v = 0;    //давление до фильтра
int p2_v = 0;    //давление после фильтра

NRFResponse nrfResponse;
NRFRequest nrfRequest;


// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);
Adafruit_BMP085 bmp;

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
  radio.setChannel(ArRoomsChannelsNRF[ROOM_NUMBER]);
  radio.setRetries(0, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(CentralReadingPipe);     // Активация данных для отправки
  radio.openReadingPipe(1, RoomReadingPipe);   // Активация данных для чтения
  radio.startListening();

  radio.printDetails();

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, 12);

  bmp.begin();

  pinMode(VENT_SPEED1_PIN, OUTPUT);
  pinMode(VENT_SPEED2_PIN, OUTPUT);
  pinMode(VENT_SPEED3_PIN, OUTPUT);

  pinMode(P1_PIN, OUTPUT);
  pinMode(P2_PIN, OUTPUT);

  wdt_enable(WDTO_8S);
}

//room data
void PrepareCommandNRF()
{
  Serial.println("PrepareCommandNRF1");
  nrfResponse.Command = RSP_INFO;

  nrfResponse.roomNumber = ROOM_NUMBER;

  nrfResponse.tOut = t_out;

  radio.flush_tx();
  radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
}

void RefreshSensorData()
{

  if (refreshSensors_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    Serial.println("RefreshSensorData");

    sensors.requestTemperatures();
    t_out = sensors.getTempCByIndex(0);

    if (t_out == -127)
      t_out = 22.33;
    Serial.print("t_out=");
    Serial.println(t_out);

    digitalWrite(P1_PIN, HIGH);
    delay(10);
    bmp.begin();
    p1_v = 0.0075 * bmp.readPressure();
    digitalWrite(P1_PIN, LOW);

    digitalWrite(P2_PIN, HIGH);
    delay(10);
    bmp.begin();
    p2_v = 0.0075 * bmp.readPressure();
    digitalWrite(P2_PIN, LOW);

    PrepareCommandNRF();

    refreshSensors_ms = 0;
  }
}

void HandleInputNrfCommand()
{
  Serial.print("roomNumber= ");
  Serial.println(nrfRequest.roomNumber);
  VentControl();
}

void VentControl()
{
  Serial.print("ventSpeed= ");
  Serial.println(nrfRequest.ventSpeed);

  digitalWrite(VENT_SPEED1_PIN, nrfRequest.ventSpeed == 1);
  digitalWrite(VENT_SPEED2_PIN, nrfRequest.ventSpeed == 2);
  digitalWrite(VENT_SPEED3_PIN, nrfRequest.ventSpeed == 3);
}


//Get Command
void ReadCommandNRF()
{
  if (readCommandNRF_ms > READ_COMMAND_NRF_INTERVAL_S * 1000)
  {
    bool done = false;
    if (radio.available())
    {
      int cntAvl = 0;
      Serial.println("radio.available!!");
      while (!done) {
        done = radio.read(&nrfRequest, sizeof(nrfRequest));
        delay(20);
        Serial.println("radio.available: ");
        Serial.println(nrfRequest.tOut);

        cntAvl++;
        if (cntAvl > 10)
        {
          Serial.println("powerDown");
          _delay_ms(20);
          radio.powerDown();
          radio.powerUp();
        }
        if (nrfRequest.Command != RQ_NO) {
          HandleInputNrfCommand();
        };

        nrfResponse.Command == RSP_NO;
        nrfResponse.tOut = 99.9;
      }
    }
    readCommandNRF_ms = 0;
  }
}


void loop()
{
  ReadCommandNRF();
  //HandleInputNrfCommand();
  RefreshSensorData();
  wdt_reset();
}
