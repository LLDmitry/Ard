//CAR

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

#define CAR_BTTN_PIN 3
#define MONITOR_PIN 5     // Monitor at car
#define SENT_INFORM_PIN 8     // LED or Buzzer

//RNF
#define CE_PIN 9
#define CSN_PIN 10

const unsigned long TIME_CAMERA1 = 10;  // sec
const unsigned long TIME_CAMERA2 = 8;  // sec

SButton buttonInCar(CAR_BTTN_PIN, 50, 700, 3000, 15000);

elapsedMillis blinkOn_ms;
elapsedMillis camera1On_ms;
elapsedMillis camera2On_ms;
elapsedMillis lastSend_ms;

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(CE_PIN, CSN_PIN);
// Single radio pipe address for the 2 nodes to communicate.  Значение "трубы" передатчика и приемника ОБЯЗАНЫ быть одинаковыми.
const uint64_t readingPipe = 0xE8E8F0F0ADLL;
const uint64_t writingPipe = 0xE8E8F0F0AALL;  // or 0xE8E8F0F0ABLL  for B
const uint8_t channelNRF = 0x60;
const char My_Car = 'A';  // or 'B'

enum modeCamera { OFF, CAMERA1, CAMERA2 } modeCamera;

enum EnOutNRFCommand { OUT_NO, OUT_CAMERA1_ON, OUT_CAMERA2_ON, OUT_CAMERA_OFF, OUT_ALARM } outNRFCommand = OUT_NO;  // 0 - Nothing to do; 1 - Camera1 On; 2 - Camera2 On; 2 - All Camera Off

void setup()
{
  delay(2000);

  Serial.begin(9600);

  pinMode(MONITOR_PIN, OUTPUT);
  pinMode(SENT_INFORM_PIN, OUTPUT);

  // Инициация кнопки
  buttonInCar.begin();

  //RF24
  radio.begin();                          // Включение модуля;
  delay(2);
  // radio.enableAckPayload(); //+
  radio.setPayloadSize(8);
  radio.setChannel(channelNRF);            // Установка канала вещания;
  radio.setRetries(10, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(writingPipe);     // Активация данных для отправки
  radio.openReadingPipe(1, readingPipe);   // Активация данных для чтения
  radio.startListening();
}


void ButtonClick()
{
  Serial.print("modeCamera: ");
  Serial.println(modeCamera);

  if (modeCamera == CAMERA1)
  {
    modeCamera = CAMERA2;
  }
  else
  {
    modeCamera = CAMERA1;
  }

  MonitorControl(HIGH);
}

void ButtonLongClick() // выкл камеры и монитор
{
  modeCamera = OFF;
  MonitorControl(HIGH);
}

void SendCommandNRF()
{
  if (outNRFCommand != OUT_NO)
  {
    Serial.print("SendCommandNRF: ");
    Serial.println(outNRFCommand);
    radio.startListening();
    radio.stopListening();

    if (radio.write(&outNRFCommand, sizeof(outNRFCommand)))
    {
      Serial.println("Success Send");
      lastSend_ms = 0;
      if (outNRFCommand != OUT_ALARM) {
        SentInform(HIGH);
      }
    }
    else
    {
      Serial.println("Failed Send");
      if (outNRFCommand != OUT_ALARM) {
        SentInform(LOW);
      }
    }
  }
  //delay(100);
  outNRFCommand = OUT_NO;
}

void SentInform(bool success)
{
  digitalWrite(SENT_INFORM_PIN, HIGH);
  if (success)
    delay(200);
  else
    delay(1000);
  digitalWrite(SENT_INFORM_PIN, LOW);
}

void MonitorControl(bool manualControl)
{
  if (manualControl)
  {
    switch (modeCamera) {
      case CAMERA1:
        camera1On_ms = 0;
        outNRFCommand = OUT_CAMERA1_ON;
        break;
      case CAMERA2:
        camera2On_ms = 0;
        outNRFCommand = OUT_CAMERA2_ON;
        break;
      case OFF:
        outNRFCommand = OUT_CAMERA_OFF;
        break;
    }
    SendCommandNRF();
  }
  else
  {
    if (modeCamera != OFF)
    {
      if (modeCamera == CAMERA1)
      {
        if (camera1On_ms > (TIME_CAMERA1 * 1000))
        {
          modeCamera = OFF;
          outNRFCommand = OUT_CAMERA_OFF;
          SendCommandNRF();
        }
      }
      else  //CAMERA2
      {
        if (camera2On_ms > (TIME_CAMERA2 * 1000))
        {
          modeCamera = OFF;
          outNRFCommand = OUT_CAMERA_OFF;
          SendCommandNRF();
        }
      }
    }
  }
  digitalWrite(MONITOR_PIN, modeCamera != OFF);
}

void loop()
{
  switch (buttonInCar.Loop()) {
    case SB_CLICK:
      Serial.println("ButtonClick");
      ButtonClick();
      break;
    case SB_LONG_CLICK:
      ButtonLongClick();
      break;
  }

  MonitorControl(LOW);

  delay(200);
}
