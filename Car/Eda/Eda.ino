#include <Encoder.h>
#include "sav_button.h"
#include "Arduino.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include "TM1637.h" // Подключаем библиотеку дмсплея
#include <elapsedMillis.h>

#define CLK 10 // К этому пину подключаем CLK дисплея
#define DIO 11 // К этому пину подключаем DIO дисплея
#define ONE_WIRE_PIN 9   // DS18b20
#define LED_PIN 13      // the number of the LED pin
#define TERMO_PIN 7     // нагревательный элемент
#define BZ_PIN 1      // signal
#define LEFT_PIN 12
#define RIGHT_PIN 6
#define BTTN_PIN 8

unsigned long loopTimeEncoder = 0;

unsigned char encoder_Left;
unsigned char encoder_Right;
unsigned char encoder_Left_prev = 0;
unsigned char encoder_Btn;
unsigned char encoder_Btn_prev = 0;
int counter_encoder_Btn = 0;

int iShowMode = 1; //  1-T, 2-StartTime, 3-Duration
int iTemperSetCounter;
int iStartTimeSet;
int iDurationSet;
long oldPosition = -999;

boolean TermoOn = false;
boolean ActiveMode = false;

const int iTemperSetMin = 20;
const int iTemperSetMax = 50;
const int iStartTimeMin = 0;
const int iStartTimeMax = 500;
const int iDurationMin = 10;
const int iDurationMax = 30;
const int iDurationIncrement = 5;
const int iStartTimeIncrement = 10;

SButton button1(BTTN_PIN, 50, 1000, 4000, 1000);

elapsedMillis loopContolNagrev;
elapsedMillis thermoOn;
elapsedMillis thermoOff;

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);

TM1637 tm1637(CLK, DIO); //дисплей

DeviceAddress tempDeviceAddress;

const int addrLastTemper = 1;
const unsigned long periodControl = 30;  //sec

float TermoIns;  //температура датчика Inside
float TermoOut;  //температура датчика Outside

unsigned long loopTimeTermo = 0;
uint32_t startTimer_ms = 0;
uint32_t startNagrev_ms = 0;

Encoder myEnc(encoder_Left, encoder_Right);

void setup()
{
  Serial.begin(9600);
  Serial.println("STARTING ...");

  pinMode(LEFT_PIN, INPUT_PULLUP);
  pinMode(RIGHT_PIN, INPUT_PULLUP);
  pinMode(BTTN_PIN, INPUT_PULLUP);

  // Инициация дисплея
  tm1637.init(D4056A);
  tm1637.set(0);   // Устанавливаем яркость дисплея от 0 до 7


  // Инициация кнопок
  button1.begin();

  pinMode(TERMO_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BZ_PIN, OUTPUT);
  digitalWrite(TERMO_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, 10);

  iTemperSetCounter = EEPROM.read(addrLastTemper); //при подаче питания восстановим установку темпераратуры к-я была до выключения

  tm1637.display(iTemperSetCounter);
  tm1637.display(0, 10);
}


void loop()
{
  int iBtn;

  switch (button1.Loop()) {
  case SB_CLICK:
    Serial.println("Press button 1");
    ActionBtn(1);
    break;
  case SB_LONG_CLICK:
    Serial.println("Long press button 1");
    ActionBtn(2);
    break;
  case SB_AUTO_CLICK:
    Serial.println("Auto press button 1");
    ActionBtn(1);
    break;
  }

  uint32_t ms = millis();

  if (ms >= (loopTimeEncoder + 5)) // проверяем каждые 5мс (200 Гц)
  {
    iBtn = GetEncoderData();
    if (iBtn > 0)
    {
      ActionBtn(iBtn);
    }
    loopTimeEncoder = ms;
  }

  if (loopContolNagrev > periodControl * 1000)
  {
    TimeControl();
    if (ActiveMode)
    {
      TermoControl();
    }
    else
    {
      TermoOn = false;
      digitalWrite(TERMO_PIN, TermoOn);
    }
    loopContolNagrev = 0;
  }
}

int GetEncoderData()  // 0-nothing; 3-left; 4-right
{
  int iResult = 0;


  long newPosition = myEnc.read();
  if (newPosition != oldPosition && newPosition % 4 == 0)
  {
    oldPosition = newPosition;
    if (newPosition > oldPosition)
      iResult = 3;
    else
      iResult = 4;

    // Serial.println(newPosition);
  }



  //    encoder_Left = digitalRead(LEFT_PIN);     // считываем состояние выхода А энкодера
  //    encoder_Right = digitalRead(RIGHT_PIN);     // считываем состояние выхода B энкодера
  //    encoder_Btn = !digitalRead(BTTN_PIN);     // считываем состояние выхода Btn энкодера и инвертируем т.к. 0-нажата 1-отпущена
  //    if((!encoder_Left) && (encoder_Left_prev)){    // если состояние изменилось с положительного к нулю
  //      if(encoder_Right){         // выход В в полож. сост., значит вращение по часовой стрелке
  //         iResult = 3;
  //      }
  //      else {         // выход В в 0 сост., значит вращение против часовой стрелки
  //         iResult = 4;
  //      }
  //    }
  //    encoder_Left_prev = encoder_Left;     // сохраняем значение А для следующего цикла
  //
  //    if(encoder_Btn)         // нажата кнопка энкодера
  //    {
  //      //encoder_Btn_result = 0;
  //      counter_encoder_Btn = counter_encoder_Btn + 1;
  //      if (counter_encoder_Btn == 200)
  //      {
  //        iResult = 2;
  //      }
  //    }
  //    else
  //    {
  //      if(encoder_Btn_prev) // если кнопку отпустили
  //      {
  //        if (counter_encoder_Btn < 200)
  //        {
  //         iResult = 1;
  //        }
  //      }
  //      counter_encoder_Btn = 0;
  //    }
  //    encoder_Btn_prev = encoder_Btn;
  //
  return iResult;
}



void ActionBtn(int iBtn) //1-short;2-long;3-left;4-right
{
  switch (iBtn)
  {
  case 1:
    Serial.println("btn");
    iShowMode += 1;
    if (iShowMode > 3)
    {
      iShowMode = 1;
    }
    break;
  case 2:
    Serial.println("btn long");
    if (iShowMode == 1)
      EEPROM.write(addrLastTemper, iTemperSetCounter);
    startTimer_ms = millis();
    break;
  case 3:
    Serial.println("left");
    switch (iShowMode)
    {
    case 1:
      if (iTemperSetCounter > iTemperSetMin + 1)
      {
        iTemperSetCounter -= 1;
      }
      else
      {
        iTemperSetCounter = iTemperSetMax;
      }
      break;
    case 2:
      if (iStartTimeSet > iStartTimeMin + iStartTimeIncrement)
      {
        iStartTimeSet -= iStartTimeIncrement;
      }
      else
      {
        iStartTimeSet = iStartTimeMax;
      }
      break;
    case 3:
      if (iDurationSet > iDurationMin + iDurationIncrement)
      {
        iDurationSet -= iDurationIncrement;
      }
      else
      {
        iDurationSet = iDurationMax;
      }
      break;
    }
    break;
  case 4:
    Serial.println("right");
    switch (iShowMode)
    {
    case 1:
      if (iTemperSetCounter > iTemperSetMax - 1)
      {
        iTemperSetCounter += 1;
      }
      else
      {
        iTemperSetCounter = iTemperSetMin;
      }
      break;
    case 2:
      if (iStartTimeSet > iStartTimeMax - iStartTimeIncrement)
      {
        iStartTimeSet += iStartTimeIncrement;
      }
      else
      {
        iStartTimeSet = iStartTimeMin;
      }
      break;
    case 3:
      if (iDurationSet > iDurationMax - iDurationIncrement)
      {
        iDurationSet += iDurationIncrement;
      }
      else
      {
        iDurationSet = iDurationMin;
      }
      break;
    }

  }
  switch (iShowMode)
  {
  case 1:
    tm1637.display(iTemperSetCounter);
    tm1637.display(0, 10);
    tm1637.point(POINT_OFF);
    break;
  case 2:
    tm1637.display(1, iStartTimeSet / 60);
    tm1637.display(2, iStartTimeSet % 60);
    tm1637.display(0, 11);
    tm1637.point(POINT_ON);
    break;
  case 3:
    tm1637.display(iDurationSet);
    tm1637.display(0, 12);
    tm1637.point(POINT_OFF);
    break;
  }

}

void TermoControl()
{
  uint32_t ms = millis();

  sensors.requestTemperatures();
  float realTemper = sensors.getTempCByIndex(0);
  Serial.print("T1= ");
  Serial.println(realTemper);
  if (realTemper > iTemperSetCounter)
  {
    TermoOn = false;
  }
  else  //включаем-выключаем для импульсного нагрева
  {
    TermoOn = !TermoOn;
  }

  digitalWrite(TERMO_PIN, TermoOn);
}

void TimeControl()
{
  uint32_t ms = millis();
  if ((ms - startTimer_ms) >= iStartTimeSet * 60 * 1000)
  {
    if (!ActiveMode)
    {
      ActiveMode = true;
      startNagrev_ms = 0;
    }
  }
  if (ActiveMode)
  {
    if ((ms - startNagrev_ms) >= iDurationSet * 60  * 1000)
    {
      tm1637.display(2, iDurationSet - startNagrev_ms / 60 / 1000);
      tm1637.display(0, 12);
      tm1637.point(POINT_OFF);
    }
    else
    {
      ActiveMode = false;
      tm1637.display(1, (iStartTimeSet - startTimer_ms / 60 / 1000) / 60);
      tm1637.display(2, (iStartTimeSet - startTimer_ms / 60 / 1000) % 60);
      tm1637.display(0, 11);
      tm1637.point(POINT_ON);
    }
  }   
}


