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

//#include <NrfCommands.h>
#include <NrfCommandsESP32.h>

#include <TFT.h>
//#include "SoftwareSerial.h"
#include "DHT.h"
#include <elapsedMillis.h>
//#include <SPI.h>                 // Подключаем библиотеку SPI
#include <Wire.h>
#include <EEPROM.h>
#include "nRF24L01.h"
#include <RF24.h>
#include <RF24_config.h>
#include <stdio.h> // for function sprintf
#include <IRremote.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

#define DHTTYPE DHT22

#define CO2_TX        A0
#define CO2_RX        A1

//#define SQA           A4  //(SDA) I2C
//#define SCL           A5  //(SCK) I2C

//#define OLED_RESET  4

//RNF  SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
#define RNF_CE_PIN    6
#define RNF_CSN_PIN   7
//#define RNF_MOSI      11
//#define RNF_MISO      12
//#define RNF_SCK       13
#define RNF_MOSI      51
#define RNF_MISO      50
#define RNF_SCK       52

#define TFT_CS        10                  // Указываем пины cs
#define TFT_DC        9                   // Указываем пины dc (A0)
#define TFT_RST       8                   // Указываем пины reset

#define BZZ_PIN       2
#define LED_PIN       3
#define IR_RECV_PIN   4
#define DHT_PIN       5
#define BTTN_PIN      A2
#define LGHT_SENSOR_PIN      A3

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)



const byte ROOM_NUMBER = ROOM_BED;

const uint32_t REFRESH_SENSOR_INTERVAL_S = 20;  //1 мин
const uint32_t SAVE_STATISTIC_INTERVAL_S = 1800; //30мин
const uint32_t CHANGE_STATISTIC_INTERVAL_S = 3;
const uint32_t SET_LED_INTERVAL_S = 5;
const uint32_t READ_COMMAND_NRF_INTERVAL_S = 1;

const byte NUMBER_STATISTICS = 60;

const byte BASE_VAL_T_IN = 190;
const byte BASE_VAL_T_OUT = 0;  //-25
const byte BASE_VAL_HM_IN = 10;
const byte BASE_VAL_CO2 = 40;
const byte BASE_VAL_P = 210;
const byte TOP_VAL_T_IN = 300;
const byte TOP_VAL_T_OUT = 250; //+25
const byte TOP_VAL_HM_IN = 80;
const byte TOP_VAL_CO2 = 150;
const byte TOP_VAL_P = 260;

const byte DIFF_VAL_T_IN = 2;
const byte DIFF_VAL_T_OUT = 2;
const byte DIFF_VAL_HM_IN = 2;
const byte DIFF_VAL_CO2 = 5;
const byte DIFF_VAL_P = 2;

const int LEVEL1_CO2_ALARM = 600;
const int LEVEL2_CO2_ALARM = 900;
const int LEVEL3_CO2_ALARM = 1200;

const int LIGHT_LEVEL_DARK = 370;

const int EEPROM_ADR_INDEX_STATISTIC = 1023; //last address in eeprom for store indexStatistic

// Single radio pipe address for the 2 nodes to communicate.  Значение "трубы" передатчика и приемника ОБЯЗАНЫ быть одинаковыми.
//const uint64_t readingPipe = 0xE8E8F0F0AALL;  // д.б. свой для каждого блока
//const uint64_t writingPipe = 0xE8E8F0F0ABLL;  // д.б. один для всех

const String IR_CODE_VENT1 = "38863bc2";
const String IR_CODE_VENT2 = "38863bca";
const String IR_CODE_VENT_STOP = "38863bca";
const String IR_CODE_VENT_AUTO = "38863bca";

const byte W1 = 69;
const byte W2 = 69;
const byte H1 = 25;
const byte H2 = 25;
const byte H3 = 25;

elapsedMillis refreshSensors_ms = REFRESH_SENSOR_INTERVAL_S * 1000 + 1;
elapsedMillis saveStatistic_ms = (SAVE_STATISTIC_INTERVAL_S - REFRESH_SENSOR_INTERVAL_S * 3) * 1000;
elapsedMillis changeStatistic_ms = 0;
elapsedMillis setLed_ms = 0;
elapsedMillis readCommandNRF_ms = 0;


char* parameters[] = {"Temper In", "Temper Out", "Humidity", "CO2", "Pressure"};

byte h_v = 0;
float t_inn = 0.0f;
int ppm_v = 0;

byte Mode = 0;

byte values[NUMBER_STATISTICS];
byte indexStatistic = 0;

String alertedRooms = "";

NRFResponse nrfResponse;
NRFRequest nrfRequest;

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);

void setup()
{
  Serial.begin(9600);


  // RF24
  radio.begin();                          // Включение модуля;
  _delay_ms(10);
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

  //  radio.printDetails();

  pinMode(BZZ_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode( LGHT_SENSOR_PIN, INPUT);

  Serial.print("ROOM_NUMBER=");
  Serial.println(ROOM_NUMBER);
  _delay_ms(10);

}

void(* resetFunc) (void) = 0; // объявляем функцию reset с адресом 0

//void AutoChangeShowMode()
//{
//  if (showMode_ms > SHOW_MODE_INTERVAL_S * 1000)
//  {
//    //    //Serial.print("showMode_ms ");
//    //    //Serial.println(millis());
//    //    Mode += 1;
//    //    if (Mode > 5) Mode = 1;
//    //
//    showMode_ms = 0;
//    Mode = 1; //!!!test
//
//  }
//}

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
    //    //Serial.print("startrefreshSensors_ms ");
    //    //Serial.println(millis());
    //    h_v = dht.readHumidity();
    //    //Serial.println("readHumidity ");
    //    t_inn = dht.readTemperature();
    //    t_inn = t_inn - 0.5;
    t_inn = random(30);
    ppm_v = random(100);
    //    //Serial.println("readTemperature ");
    //
    //    mySerial.write(cmd, 9);
    //    memset(response, 0, 9);
    //    mySerial.readBytes(response, 9);
    //
    //    int i;
    //    byte crc = 0;
    //    for (i = 1; i < 8; i++) crc += response[i];
    //    crc = 255 - crc;
    //    crc++;
    //
    //    if ( !(response[0] == 0xFF && response[1] == 0x86 && response[8] == crc) )
    //    {
    //      Serial.println("CRC error: " + String(crc) + " / " + String(response[8]));
    //    } else
    //    {
    //      unsigned int responseHigh = (unsigned int) response[2];
    //      unsigned int responseLow = (unsigned int) response[3];
    //      ppm_v = (256 * responseHigh) + responseLow;
    //      if (ppm_v < 400) ppm_v = 400;
    //      Serial.print("co2= ");
    //      Serial.println(ppm_v);
    //    }
    //
    //    if (ppm_v == 0)
    //    {
    //      Serial.println("RESET in 3 sec");
    //      _delay_ms(3000);
    //      resetFunc(); //вызов reset
    //    }

    PrepareCommandNRF(RSP_INFO, 100, -100, 99, 99);

    refreshSensors_ms = 0;
  }
}


//Get T out, Pressure and Command
void ReadCommandNRF()
{
  if (readCommandNRF_ms > READ_COMMAND_NRF_INTERVAL_S * 1000)
  {
    bool done = false;
    if ( radio.available() )
    {
      int cntAvl = 0;
      Serial.println("radio.available!!");
      _delay_ms(20);
      //while (!done) {                            // Упираемся и
      radio.read(&nrfRequest, sizeof(nrfRequest)); // по адресу переменной nrfRequest функция записывает принятые данные
      _delay_ms(20);
      //radio.flush_rx();
      Serial.println("radio.read: ");
      _delay_ms(20);
      Serial.println(nrfRequest.Command);
      Serial.println(nrfRequest.minutes);
      Serial.println(nrfRequest.tOut);
      _delay_ms(20);
      cntAvl++;
      if (cntAvl > 10)
      {
        Serial.println("powerDown");
        _delay_ms(20);
        radio.powerDown();
        radio.powerUp();
      }
      //}
      if (nrfRequest.Command != RQ_NO) {
        HandleInputNrfCommand();
      };
      //radio.startListening();   // Now, resume listening so we catch the next packets.
      nrfResponse.Command == RSP_NO;
      //nrfResponse.ventSpeed = 0;
    }
    readCommandNRF_ms = 0;
  }
}

void HandleInputNrfCommand()
{
  alertedRooms = "";
  if (nrfRequest.alarmMaxStatus > 0)
  {
    for (byte iCheckRoom = 0; iCheckRoom <= 5; iCheckRoom++)  //сформируем строку с номерами комнат с тревогой
    {
      //      if (bitRead(nrfRequest.alarmRooms, iCheckRoom))
      //      {
      //        alertedRooms = alertedRooms + (String)(iCheckRoom + 1);
      //      }
    }
    switch (nrfRequest.alarmMaxStatus)
    {
      case 1:
        Alarm(1);
        break;
      case 2:
        Alarm(2);
        break;
    }
  }

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
  //nrfResponse.ventSpeed = ventSpeed;  //auto
  //nrfResponse.t_set = t_set;   //auto
  //  nrfResponse.scenarioVent = scenarioVent;
  //  nrfResponse.scenarioNagrev = scenarioNagrev;
  nrfResponse.tInn = t_inn;
  nrfResponse.co2 = ppm_v;
  nrfResponse.h = h_v;

  uint8_t f = radio.flush_tx();
  radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
}

//Alarm alarmType== - внутренний, разовый; alarmType=1,2 - при каждом новом request от CentralControl
void Alarm(byte alarmType)  //0 - 0.2s short signal; 1 - 1s signal; 2 - 5s signal
{
  digitalWrite(BZZ_PIN, HIGH);
  delay(alarmType == 0 ? 200 : alarmType == 1 ? 1000 : 5000);
  digitalWrite(BZZ_PIN, LOW);
}


void loop()
{
  ReadCommandNRF(); //each loop try read t_out and other info from central control

  RefreshSensorData();

  wdt_reset();
}
