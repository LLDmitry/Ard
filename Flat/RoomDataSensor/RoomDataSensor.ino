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
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

#define DHTTYPE DHT22



//RNF  SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
#define RNF_CE_PIN    6
#define RNF_CSN_PIN   7
#define RNF_MOSI      11  //SDA
#define RNF_MISO      12
#define RNF_SCK       13



#define DHT_PIN       5
#define LGHT_SENSOR_PIN      A3
#define ONE_WIRE_PIN 11  // DS18b20


//DHT dht(DHT_PIN, DHTTYPE);

const byte ROOM_NUMBER = 1; //1,2,3,4; 0 -main control (if exists)
const uint32_t REFRESH_SENSOR_INTERVAL_S = 60;  //1 мин


elapsedMillis refreshSensors_ms = REFRESH_SENSOR_INTERVAL_S * 1000 + 1;


float t = 0.0f;

NRFResponse nrfResponse;
NRFRequest nrfRequest;


// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);


OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);

DeviceAddress tempDeviceAddress;

// watchdog interrupt
ISR(WDT_vect)
{
  wdt_disable();  // disable watchdog
}  // end of WDT_vect

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

}

void RefreshSensorData()
{

  if (refreshSensors_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    Serial.println("RefreshSensorData");

    sensors.requestTemperatures();
    t = sensors.getTempCByIndex(0);
    //t = dht.readTemperature();

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

//send room data and set Vent On/Off
void PrepareCommandNRF(byte command, byte ventSpeed, float t_set, byte scenarioVent, byte scenarioNagrev)
{
  Serial.println("PrepareCommandNRF1");
  nrfResponse.Command = RSP_INFO;

  nrfResponse.roomNumber = ROOM_NUMBER;

  nrfResponse.tInn = t;


  radio.flush_tx();
  radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
}

void loop()
{
  ReadCommandNRF(); //each loop try read t_out and other info from central control
  //HandleInputNrfCommand();
  RefreshSensorData();
}
