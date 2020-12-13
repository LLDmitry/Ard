//BALKON
#include "DHT.h"
#include <TM1637.h>          // Библиотека дисплея 
#include <elapsedMillis.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/wdt.h>

#define ONE_WIRE_PIN 5   // DS18b20
#define VENT_PIN 2     // вентилятор
#define CLK 3 // К этому пину подключаем CLK дисплея
#define DIO 4 // К этому пину подключаем DIO дисплея
#define TERMO_PIN 13     // нагревательный элемент

#define DHTTYPE DHT11

TM1637 tm1637(CLK, DIO);

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress innerTempDeviceAddress;
DeviceAddress outerTempDeviceAddress;

elapsedMillis lastVentOn_ms;
elapsedMillis lastVentOff_ms;
elapsedMillis lastChangeShowMode_ms = 999999;
elapsedMillis lastTermoOn_ms;
elapsedMillis lastTermoOff_ms;
elapsedMillis lastRefreshSensor_ms = 999999;
elapsedMillis lastStatistic_ms = 0;
elapsedMillis lastUpdStatistic_ms = 0;

int ShowMode = 1;     // 1-показать внутр T
// 2-показать внеш T
// 3-показать % работы нагрева от общего времени за последний час

int VentOn = 0;
int TermoOn = 0;  // 0/1
float k_Temperature = 1.0;  //для расчета времени нагрева от внешней T
volatile float t_in;
volatile float t_out;
volatile unsigned long totalStatisticNagrev = 0;
volatile unsigned long currentStatisticNagrev = 0;

const double MIN_T = 1.0; //диапозон температур внутри ящика
const double MAX_T = 2.0; //диапозон температур внутри ящика

const unsigned long SHOW_MODE_DELAY_S = 2; //sec
const unsigned long REFRESH_SENSOR_INTERVAL_S = 30;   //1 мин
const unsigned long NAGREV_AVERAGE_S = 30;
const unsigned long PAUSE_NAGREV_S = 30;
const unsigned long VENT_WORK_AFTER_NAGREV_S = 10;
const unsigned long PERIOD_VENT_S = 15;  // периодически (каждые PAUSE_VENT_S) включать на время PERIOD_VENT_S для выравнивания температуры внутри ящика
const unsigned long PAUSE_VENT_S = 3600;  //1 час  периодически (каждые PAUSE_VENT_S) включать на время PERIOD_VENT_S для выравнивания температуры внутри ящика
const unsigned long PERIOD_STATISTIC_S = 10800; //3ч период сбора анализа % работы нагрева

void setup() {
  // immediately disable watchdog timer so set will not get interrupted
  wdt_disable();

  pinMode(TERMO_PIN, OUTPUT);

  Serial.begin(9600);
  // Инициация дисплея
  tm1637.set(0);    // Устанавливаем яркость от 0 до 7
  tm1637.init(D4056A);
  tm1637.point(false);

  sensors.begin();
  sensors.getAddress(innerTempDeviceAddress, 1);
  sensors.getAddress(outerTempDeviceAddress, 0);
  sensors.setResolution(innerTempDeviceAddress, 11);  //9 bits 0.5°C ; 10 bits 0.25°C ; 11 bits 0.125°C ; 12 bits  0.0625°C
  sensors.setResolution(outerTempDeviceAddress, 11);

  // enable the watchdog timer. There are a finite number of timeouts allowed (see wdt.h).
  wdt_enable(WDTO_4S);
}

void TermoControl()
{
  if (TermoOn == 0 && t_in < MIN_T) //проверить и включить
  {
    // Serial.println(lastTermoOff_ms);
    if (lastTermoOff_ms > PAUSE_NAGREV_S * 1000)
    {
      //Serial.println("TermoControlON");
      TermoOn = 1;
      lastTermoOn_ms = 0;
    }
  }
  else if (TermoOn == 1 && (lastTermoOn_ms > NAGREV_AVERAGE_S * k_Temperature * 1000 || t_in > MAX_T))// проверить и выключить
  {
    //Serial.println("TermoControlOFF");
    Serial.println(k_Temperature);
    TermoOn = 0;
    lastTermoOff_ms = 0;
  }
  digitalWrite(TERMO_PIN, TermoOn);
}

void VentControl()
{
  if (TermoOn == 1 || lastTermoOff_ms < VENT_WORK_AFTER_NAGREV_S * 1000)  //при нагреве всегда сразу включать вент, а выключть через VENT_WORK_AFTER_NAGREV_S после окончания нагрева из-за инерции нагревателя
  {
    VentOn = 1;
    // Serial.println("TermoOn1");
    //lastVentOn_ms = 0;
  }
  else // даже без нагрева, периодически включать вентилятор для выравнивания температуры внутри ящика
  {
    if (VentOn == 1)
    {
      if (lastVentOn_ms > PERIOD_VENT_S * 1000)
      {
        VentOn = 0;
        lastVentOff_ms = 0;
      }
    }
    else
    {
      if (lastVentOff_ms > PAUSE_VENT_S * 1000)
      {
        Serial.println("TermoOn2");
        VentOn = 1;
        lastVentOn_ms = 0;
      }
    }
  }
  Serial.print("VentOn:");
  Serial.println(VentOn);
  digitalWrite(VENT_PIN, VentOn);
}

void GetStatistic()
{
  if (lastStatistic_ms > PERIOD_STATISTIC_S * 1000)
  {
    totalStatisticNagrev = currentStatisticNagrev;
    currentStatisticNagrev = 0;
    lastStatistic_ms = 0;
  }
  if (TermoOn == 1 && lastUpdStatistic_ms > 1000) //update every sec
  {
    currentStatisticNagrev += 1;
    lastUpdStatistic_ms = 0;
  }
}

void AutoChangeShowMode(bool needRefresh)
{
  int modeChanged = 0;

  if (lastChangeShowMode_ms > SHOW_MODE_DELAY_S * (ShowMode == 1 ? 2 : 1) * 1000) //для внутр T  - в 2 р дольше
  {
    ShowMode += 1;
    if (ShowMode > 3) ShowMode = 1;
    lastChangeShowMode_ms = 0;

    tm1637.clearDisplay();
    switch (ShowMode)
    {
      case 1:     //внутр T
        if (TermoOn == 1)
          tm1637.set(7);    // яркость от 0 до 7
        else
          tm1637.set(1);    // яркость от 0 до 7
        tm1637.display(FormatDisplay(t_in));
        if (abs((int)(t_in * 10.0)) <= 9)  //<=0.9c
          tm1637.display(2, 0);
        tm1637.display(0, 10);
        if (t_in < 0)
          tm1637.display(1, 16);
        if (abs((int)(t_in * 10.0)) <= 99)  //<=9.9c
        {
          tm1637.display(3, 99); //мигнем последним разрядом - признак что это десятые градуса
          delay(200);
          tm1637.display(FormatDisplay(t_in));
          tm1637.display(0, 10);
          if (abs((int)(t_in * 10.0)) <= 9)  //<=0.9c
            tm1637.display(2, 0);
          if (t_in < 0)
            tm1637.display(1, 16);
        }
        break;
      case 2:     //внеш T
        tm1637.set(1);    // яркость от 0 до 7
        tm1637.display(FormatDisplay(t_out));
        if (abs((int)(t_out * 10.0)) <= 9)  //<=0.9c
          tm1637.display(2, 0);
        tm1637.display(0, 11);
        if (t_out < 0)
          tm1637.display(1, 16);
        if (abs((int)(t_out * 10.0)) <= 99) //<=9.9c
        {
          tm1637.display(3, 99); //мигнем последним разрядом - признак что это десятые градуса
          delay(200);
          tm1637.display(FormatDisplay(t_out));
          if (abs((int)(t_out * 10.0)) <= 9)  //<=0.9c
            tm1637.display(2, 0);
          tm1637.display(0, 11);
          if (t_out < 0)
            tm1637.display(1, 16);
        }
        break;
      case 3:     //% работы нагрева за предыдущие 3ч
        tm1637.set(1);    // яркость от 0 до 7
        Serial.print("totalStatisticNagrev ");
        Serial.println(totalStatisticNagrev * 100);
        tm1637.display(int((totalStatisticNagrev * 100) / PERIOD_STATISTIC_S ));
        tm1637.display(0, 12);
        break;
    }
  }
}

// когда <10, показываем десятые градуса
int FormatDisplay(float f)
{
  int i;
  if (abs((int)(f * 10.0)) <= 99)  //<=9.9c
    i = f * 10.0;
  else
    i = f;
  return abs(i);
}

void RefreshSensorData()
{
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    sensors.requestTemperatures();
    float realTemper = sensors.getTempCByIndex(0);
    t_in = sensors.getTempC(innerTempDeviceAddress);
    t_out = sensors.getTempC(outerTempDeviceAddress);
    //    Serial.println(lastRefreshSensor_ms);
    //    Serial.println("T=");
    //    Serial.println(t_in);
    //    Serial.println(t_out);

    //вычисление к-та времени нагрева от наружней температуры
    if (t_out < -10)
    {
      k_Temperature = 5.0;
    }
    else if (t_out < -5)
    {
      k_Temperature = 2.0;
    }
    else if (t_out < 0)
    {
      k_Temperature = 1.0;
    }
    else
    {
      k_Temperature = 0;
    }
    lastRefreshSensor_ms = 0;
  }
}

void loop()
{
  RefreshSensorData();
  TermoControl();
  VentControl();
  GetStatistic();

  AutoChangeShowMode(false);
  wdt_reset();
}
