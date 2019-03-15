//Kitchen
#include "DHT.h"
#define DHTTYPE DHT11   // DHT 11 
#include "sav_button.h" // Библиотека работы с кнопками
#include <elapsedMillis.h>
#include <Arduino.h>
#include <avr/wdt.h>

const int DHT_PIN = 2;
const int BTN_PIN = 7;
const int PIR_PIN = 3;
const int VENT_PIN = 13;
const int BUZZ_PIN = 4;
const int LAMP_PIN = 8;

const int ON_HUMIDITY_LEVEL = 80;
const int OFF_HUMIDITY_LEVEL = 60;
const int MIN_COMFORT_HUMIDITY = 35; //минимальный комфортный % в квартрире. Не будем включать вентиляцию если baseHumidityLevel < MIN_COMFORT_HUMIDITY - пусть повышается влажность в квартире от кухни

const unsigned long MANUAL_SHORT_ON_DURATION_SEC = 180; //3 minute
const unsigned long MANUAL_LONG_ON_DURATION_SEC = 1800; //30 minute
const unsigned long CHECK_HUMIDITY_PERIOD_SEC = 30;
const unsigned long PERIOD_BASE_LEVELS_SEC = 10000; //3 hours

SButton btnVent(BTN_PIN, 50, 700, 3000, 10000);
elapsedMillis ventOn_ms;
elapsedMillis checkHumidity_ms = CHECK_HUMIDITY_PERIOD_SEC * 1000;
elapsedMillis periodBaseLevels_ms = PERIOD_BASE_LEVELS_SEC * 1000;

int avgHumidity = 70;
int avgTemper = 20;
int baseHumidityLevel = 70; //minimum average for the last several hours
int baseTemperLevel = 30; //minimum average for the last several hours
int minAvgHumidityLevel = 70;
int minAvgTemperLevel = 30;
boolean ventOn = false;
int ventMode = 0;  //0-off, 1-humidityOn, 2-manualShortOn, 3-manualLongOn

DHT dht(DHT_PIN, DHTTYPE);

void setup()
{
  // immediately disable watchdog timer so set will not get interrupted
  wdt_disable();

  dht.begin();

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(VENT_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LAMP_PIN, OUTPUT);
  Serial.begin(9600);   // Debugging only
  Serial.println("setup");

  // Инициация кнопок
  btnVent.begin();

  digitalWrite(VENT_PIN, true);
  delay(500);
  Serial.println("000");
  digitalWrite(VENT_PIN, false);

  // enable the watchdog timer. There are a finite number of timeouts allowed (see wdt.h).
  // Notes I have seen say it is unwise to go below 250ms as you may get the WDT stuck in a
  // loop rebooting.
  // The timeouts I'm most likely to use are:
  // WDTO_1S
  // WDTO_2S
  // WDTO_4S
  // WDTO_8S
  wdt_enable(WDTO_4S);
}

void loop()
{
  switch (btnVent.Loop()) {
  case SB_CLICK:
    Serial.println("btnVentShort");
    VentControl(1);
    break;
  case SB_LONG_CLICK:
    Serial.println("btnVentLong");
    VentControl(2);
    break;
  case SB_AUTO_CLICK:
    Serial.println("btnVentAuto");
    VentControl(3);
    break;
  }
  VentControl(0);
  CheckHumidity();
  LampControl();
  wdt_reset();
}

void CheckHumidity()
{
  if (checkHumidity_ms > CHECK_HUMIDITY_PERIOD_SEC * 1000)
  {
    CalcAvg();
    SetBaseLevels();

    if (ventMode == 0 && baseHumidityLevel > MIN_COMFORT_HUMIDITY && avgHumidity > ON_HUMIDITY_LEVEL)
    {
      ventOn = true;
      ventMode = 1;
      switchVent();
    }
    else if (ventMode == 1 && (avgHumidity < baseHumidityLevel || avgHumidity <  OFF_HUMIDITY_LEVEL))
    {
      ventOn = false;
      ventMode = 0;
      switchVent();
    };
    checkHumidity_ms = 0;
  }
}

void VentControl(int typeControl) //0 - auto check, 1-short click, 2-long click, 3-auto click
{
  switch (typeControl)
  {
  case 0:
    if (ventMode == 2 || ventMode == 3)
    {
      if (ventMode == 2 && ventOn_ms > MANUAL_SHORT_ON_DURATION_SEC * 1000 || ventMode == 3 && ventOn_ms > MANUAL_LONG_ON_DURATION_SEC * 1000)
      {
        ventMode = 0;
        ventOn = false;
        switchVent();
      }
    }
    switchVent();
    break;
  case 1:
  case 2:
    if (ventMode == 0 || ventMode == 1) // off or humidity_on
    {
      ventMode = (typeControl == 1 ? 2 : 3);
      ventOn_ms = 0;
      ventOn = true;
    }
    else //выключить если был включен мануально
    {
      ventMode = 0;
      ventOn = false;
    }
    if (typeControl == 2)
    {
      digitalWrite(BUZZ_PIN, HIGH);
      delay(200);
      digitalWrite(BUZZ_PIN, LOW);
    }
    switchVent();
    break;
  case 3: //play beeps with base_humidity and current_humidity
    digitalWrite(BUZZ_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZ_PIN, LOW);
    ventMode = 0; //off vent for silent
    ventOn = false;
    switchVent();
    delay(1500);
    PlayHumidity();
    break;
  }
}

void switchVent()
{
  digitalWrite(VENT_PIN, ventOn);
}

//average humidity for 3 last measures
void CalcAvg()
{
  avgHumidity = 0; 
  avgTemper = 0;
  for (int i = 0; i < 3; i++)
  {
    avgHumidity += dht.readHumidity();
    avgTemper += dht.readTemperature();
  }
  avgHumidity = (avgHumidity) / 3;
  avgTemper = (avgTemper) / 3;
  if (avgHumidity < minAvgHumidityLevel)
  {
    minAvgHumidityLevel = avgHumidity;
  }
  if (avgTemper < minAvgTemperLevel)
  {
    minAvgTemperLevel = avgTemper;
  }
  Serial.println(avgHumidity);
}

//раз в 3ч устанавливаем baseHumidityLevel и baseTemperLevel = min average за прошедшие 3ч
void SetBaseLevels()
{
  if (periodBaseLevels_ms > PERIOD_BASE_LEVELS_SEC * 1000)
  {
    baseHumidityLevel = minAvgHumidityLevel;
    minAvgHumidityLevel = 100;
    minAvgTemperLevel = 40;
    periodBaseLevels_ms = 0;
  }
}

void PlayHumidity()
{
  PlayDigitBeeps(baseHumidityLevel);
  delay(1200);
  PlayDigitBeeps(avgHumidity);
}

void PlayDigitBeeps(int val)
{
  int dig1, dig2;
  dig1 = val / 10;
  dig2 = val % 10;

  //long
  for (int i = 1; i <= dig1; i++)
  {
    digitalWrite(BUZZ_PIN, HIGH);
    delay(600);
    digitalWrite(BUZZ_PIN, LOW);
    delay(300);
    wdt_reset();
  }
  delay(500);
  //short
  for (int i = 1; i <= dig2; i++)
  {
    digitalWrite(BUZZ_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZ_PIN, LOW);
    delay(300);
    wdt_reset();
  }
}

void LampControl()
{
  digitalWrite(LAMP_PIN, digitalRead(PIR_PIN));
}
