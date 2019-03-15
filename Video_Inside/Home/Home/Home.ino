// HOME
#include <RF24.h>
#include <RF24_config.h>
#include "sav_button.h" // Библиотека работы с кнопками
#include <elapsedMillis.h>
#include <SPI.h>
#include <stdint.h>

// GND VCC CE  CSN MOSI  MISO  SCK
//          9  10  11    12    13
//  http://robotclass.ru/tutorials/arduino-radio-nrf24l01/
//

#define HOME_BTTN_OPEN_DOOR_PIN 2
#define HOME_BTTN_INCREASE_SENSOR_LEVEL_PIN 4
#define HOME_BTTN_STOP_ALARM_PIN 5

#define CAMERA1_PIN 6     // Camera1 at home
#define CAMERA2_PIN 7     // Camera2 at home
#define ALERT_PIN 8     // LED ot Buzzer


//RNF
#define CE_PIN 9
#define CSN_PIN 10

const unsigned long TIME_CAMERA1 = 4;  // sec
const unsigned long TIME_CAMERA2 = 6;  // sec

elapsedMillis blinkOn_ms;
elapsedMillis camera1On_ms;
elapsedMillis camera2On_ms;

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(CE_PIN, CSN_PIN);
// Single radio pipe address for the 2 nodes to communicate.  Значение "трубы" передатчика и приемника ОБЯЗАНЫ быть одинаковыми.
const uint64_t readingPipe = 0xE8E8F0F0AALL;
const uint64_t writingPipe = 0xE8E8F0F0ABLL;
const uint8_t channelNRF = 0x60;

enum ModeCamera { CAMERA_OFF, CAMERA1_ON, CAMERA2_ON, CAMERA1_NEW, CAMERA2_NEW } modeCamera;
enum EnINNRFCommand { IN_H_NO, CAMERA1_H_ON, CAMERA2_H_ON, CAMERA_H_OFF, SENSOR_ALARM } inNRFCommand = IN_H_NO;

unsigned long lastChangeSettings_ms = 0;

int miNoConnectM = 0;

void setup()
{
  delay(2000);

  Serial.begin(9600);

  Serial.println("setup_1");

  pinMode(CAMERA1_PIN, OUTPUT);
  pinMode(CAMERA2_PIN, OUTPUT);

  //RF24
  //стоило переключить CS с 9 ножки в + и все бодренько заработало

  radio.begin();                          // Включение модуля;
  delay(2);
  //radio.enableAckPayload();       //+
  radio.setPayloadSize(8);
  radio.setChannel(channelNRF);            // Установка канала вещания;
  radio.setRetries(10, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(writingPipe);     // Активация данных для отправки
  radio.openReadingPipe(1, readingPipe);   // Активация данных для чтения
  radio.startListening();

  radio.printDetails();
}

void ReadCommandNRF()
{
  //Serial.println("ReadCommandNRF");
  inNRFCommand = IN_H_NO;
  if (radio.available())
  {
    Serial.println("radio.available!!");
    while (radio.available()) // While there is data ready
    {
      radio.read(&inNRFCommand, sizeof(inNRFCommand)); // по адресу переменной inNRFCommand функция записывает принятые данные
      delay(20);
      Serial.print("radio.available: ");
      Serial.println(inNRFCommand);

    }
    radio.startListening();                                // Now, resume listening so we catch the next packets.
  }
}

void ParseAndHandleInputCommand()
{
  switch (inNRFCommand) {
    case CAMERA1_H_ON:
      modeCamera = CAMERA1_NEW;
      break;
    case CAMERA2_H_ON:
      modeCamera = CAMERA2_NEW;
      break;
    case CAMERA_H_OFF:
      modeCamera = CAMERA_OFF;
      break;
  }
}

void CameraControl()
{
  switch (modeCamera) {
    case CAMERA1_NEW:
      camera1On_ms = 0;
      modeCamera = CAMERA1_ON;
      break;
    case CAMERA2_NEW:
      camera2On_ms = 0;
      modeCamera = CAMERA2_ON;
      break;
    case CAMERA1_ON:
      if (camera1On_ms > (TIME_CAMERA1 * 1000))
      {
        modeCamera = CAMERA_OFF;
      }
      break;
    case CAMERA2_ON:
      if (camera2On_ms > (TIME_CAMERA2 * 1000))
      {
        modeCamera = CAMERA_OFF;
      }
      break;
  }

  digitalWrite(CAMERA1_PIN, modeCamera == CAMERA1_ON);
  digitalWrite(CAMERA2_PIN, modeCamera == CAMERA2_ON);
}


void loop()
{

  ReadCommandNRF();

  ParseAndHandleInputCommand();

  CameraControl();

  //delay(200);
}


