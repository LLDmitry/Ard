//BALKON
#include "DHT.h"
#include "TM1637.h"          // Библиотека дисплея 
#include <elapsedMillis.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_PIN 9   // DS18b20
#define CLK 12 // К этому пину подключаем CLK дисплея
#define DIO 11 // К этому пину подключаем DIO дисплея
#define TERMO_PIN 13     // нагревательный элемент
#define VENT_PIN 10     // вентилятор
#define DHT_PIN 2
#define DHTTYPE DHT11

TM1637 tm1637(CLK, DIO);

DHT dht(DHT_PIN, DHTTYPE);

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

int ShowMode = 1;     // 1-показать внутр T
// 2-показать внеш T
// 3-показать внутр Влажность

int VentOn = 0;
int TermoOn = 0;  // 0/1
unsigned long k_Temperature = 1;  //для расчета времени нагрева от внешней T
double t_in;
double h_in;
double t_out;

const int SHOW_MODE_DELAY_S = 1; //sec
const unsigned long REFRESH_SENSOR_INTERVAL_S = 600;   //10 мин
const unsigned long NAGREV_AVERAGE_S = 30;
const unsigned long PAUSE_NAGREV_S = 60;
const unsigned long VENT_WORK_AFTER_NAGREV_S = 60;
const unsigned long PERIOD_VENT_S = 15;  // периодически (каждые PAUSE_VENT_S) включать на время PERIOD_VENT_S для выравнивания температуры внутри ящика
const unsigned long PAUSE_VENT_S = 3600;  //1 час
const double MIN_T = 1.5; //диапозон температур внутри ящика
const double MAX_T = 2.5; //диапозон температур внутри ящика

void setup() {

  pinMode(TERMO_PIN, OUTPUT);

  Serial.begin(9600);
  // Инициация дисплея
  tm1637.set(0);    // Устанавливаем яркость от 0 до 7
  tm1637.init(D4056A);

  dht.begin();

  sensors.begin();
  sensors.getAddress(innerTempDeviceAddress, 0);
  sensors.getAddress(outerTempDeviceAddress, 1);
  sensors.setResolution(innerTempDeviceAddress, 10);
  sensors.setResolution(outerTempDeviceAddress, 10);
}

void TermoControl()
{
  if (TermoOn == 0 && t_in < MIN_T) //проверить и включить
  {
    if (lastTermoOff_ms > PAUSE_NAGREV_S * 1000)
    {
      TermoOn = 1;
      lastTermoOn_ms = 0;
    }
  }
  else  // проверить и выключить
  {
    if (lastTermoOn_ms > NAGREV_AVERAGE_S * k_Temperature * 1000 || t_in > MAX_T)
    {
      TermoOn = 0;
      lastTermoOff_ms = 0;
    }
  }
  digitalWrite(TERMO_PIN, TermoOn);
}

void VentControl()
{
  if (TermoOn == 1 || lastTermoOff_ms > VENT_WORK_AFTER_NAGREV_S * 1000)  //при нагреве всегда сразу включать вент, а выключть через VENT_WORK_AFTER_NAGREV_S после окончания нагрева
  {
    VentOn = 1;
    lastVentOn_ms = 0;
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
        VentOn = 1;
        lastVentOn_ms = 0;
      }
    }
  }
  digitalWrite(VENT_PIN, VentOn);
}

void AutoChangeShowMode(bool needRefresh)
{
  int modeChanged = 0;

  if (lastChangeShowMode_ms > SHOW_MODE_DELAY_S * 1000 * (ShowMode == 3 ? 3 : 1)) //для внутр T  - в 3 р дольше
  {
    ShowMode += 1;
    if (ShowMode > 3) ShowMode = 1;
    lastChangeShowMode_ms = 0;

    RefreshSensorData();

    switch (ShowMode)
    {
      case 1:     //внутр T
        tm1637.display(t_in);
        tm1637.display(0, 10);
        break;
      case 2:     //внеш T
        tm1637.display(t_out);
        tm1637.display(0, 11);
        break;
      case 3:     //Влажн
        tm1637.display(h_in);
        tm1637.display(0, 12);
        break;
    }
  }
}

void RefreshSensorData()
{
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    lastRefreshSensor_ms = 0;
    sensors.requestTemperatures();
    //float realTemper = sensors.getTempCByIndex(0);
    float t_in = sensors.getTempC(innerTempDeviceAddress);
    float t_out = sensors.getTempC(outerTempDeviceAddress);
    //t_in = dht.readTemperature();
    //t_out = dht.readTemperature();

    h_in = dht.readHumidity();

    //вычмсление к-та времени нагрева от наружней температуры
    if (t_out < -10)
    {
      k_Temperature = 3;
    }
    else if (t_out < -5)
    {
      k_Temperature = 1.5;
    }
    else if (t_out < 0)
    {
      k_Temperature = 1;
    }
    else if (t_out < 3)
    {
      k_Temperature = 0.3;
    }
    else
    {
      k_Temperature = 0;
    }
  }
}


void loop()
{
  TermoControl();
  VentControl();

  AutoChangeShowMode(false);
}
