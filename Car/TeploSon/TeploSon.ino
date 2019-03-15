//#include "SoftwareSerial.h"
#include "DHT.h"
#include "sav_button.h" // Библиотека работы с кнопками
#include "TM1637.h"          // Библиотека дисплея 

//SoftwareSerial mySerial(A0, A1); // A0 - к TX сенсора, A1 - к RX
//byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
//char response[9];

#define A_PIN 1
#define CLK 12 // К этому пину подключаем CLK дисплея
#define DIO 11 // К этому пину подключаем DIO дисплея
#define TERMO_PIN 13     // нагревательный элемент
#define BTTN_PIN 4
#define DHT_PIN 2
#define DHTTYPE DHT11

TM1637 tm1637(CLK, DIO);

SButton button1(BTTN_PIN, 50, 1000, 10000, 500);

DHT dht(DHT_PIN, DHTTYPE);

int Mode = 0;     // 1-показать режим нагрева
// 2-показать текущую T
// 3-показать текущую Влажность
// 4-показать суммарный расход тока
// 5-показать текущее напряжение
// 6-изменение режима нагрева

int TermoOn = 0;  // 0/1
int TermoLevel = 0;     // 0..5 или 999(oversource)
const int ShowModeDelay_ms = 5000; //msec
const unsigned long RefreshSensorInterval_ms = 60000;   //1 мин
const unsigned long TokNagreva_mA = 4000;
const int TotalTokLimit_Ach = 20;
const unsigned long  ColdStartPeriod_ms = 300000;

// резисторы делителя напряжения
const float r1 = 99700;  // 100K
const float r2 = 9870;  // 10K

const float Vcc = 1.12345;  //  эту константу необходимо откалибровать индивидуально    1.0 -- 1.2
float vReal;

unsigned long lastChangeShowMode_ms = 0;
unsigned long lastTermoOn_ms = 0;
unsigned long lastTermoOff_ms = 0;
unsigned long lastRefreshSensor_ms = 0;
unsigned long last5ModeOn_ms = 0;
int h = 0;
int t = 0;
unsigned long totalTok_mACh = 0;
uint32_t coldStart_ms = 0;
boolean coldStart;

void setup() {

  pinMode(TERMO_PIN, OUTPUT);

  Serial.begin(9600);
  // Инициация дисплея
  tm1637.set(0);    // Устанавливаем яркость от 0 до 7
  tm1637.init(D4056A);
  // Инициация кнопки
  button1.begin();

  // mySerial.begin(9600);
  dht.begin();
  coldStart = true;

  analogReference(INTERNAL); // выбираем внутреннее опорное напряжение 1.1В
}

void ButtonClick()
{
  if (Mode == 6)
  {
    if (TermoLevel >= 0)
    {
      TermoLevel = TermoLevel + 1;
      if (TermoLevel > 5) TermoLevel = 0;
      tm1637.display(TermoLevel);
      tm1637.display(0, 10);
    }
  }
  else
  {
    AutoChangeShowMode(true);
  }
}

void ButtonLongClick()
{
  if (Mode == 6)
  {
    tm1637.set(0);
    Mode = 6 - 1;
    AutoChangeShowMode(true);
  }
  else
  {
    if (TermoLevel < 999)
    {
      Mode = 6;
      last5ModeOn_ms = millis();
    }
    tm1637.set(7);
    tm1637.display(TermoLevel);
    tm1637.display(0, 10);
  }
}

void TermoControl()
{
  unsigned long pauseNagrev;
  unsigned long ms = millis();

  if (TermoOn == 0) //проверить и включить
  {
    switch (TermoLevel)
    {
      case 999:
      case 0: //off
        pauseNagrev = 999999999;
        break;
      case 1:
        pauseNagrev = coldStart ? 10000 : 90000;
        break;
      case 2:
        pauseNagrev = coldStart ? 5000 : 60000;
        break;
      case 3:
        pauseNagrev = coldStart ? 1000 : 30000;
        break;
      case 4:
        pauseNagrev = coldStart ? 10 : 15000;
        break;
      case 5:
        pauseNagrev = coldStart ? 10 : 5000;
        break;
    }
    if ((ms - lastTermoOff_ms) > pauseNagrev)
    {
      GetVoltage();  //напряжение без нагрузки

      TermoOn = 1;
      lastTermoOn_ms = ms;

      if (coldStart && coldStart_ms == 0)
      {
        coldStart_ms = ms;
      }
    }
  }
  else  // проверить и выключить
  {
    if (TermoLevel == 0)
    {
      TermoOn = 0;
    }
    else
    {
      if ((ms - lastTermoOn_ms) > 30000)
      {
        TermoOn = 0;
        CalcTotalTok((ms - lastTermoOn_ms) / 1000);
        lastTermoOff_ms = ms;

        if (coldStart)
        {
          if ((ms - coldStart_ms) > ColdStartPeriod_ms)
          {
            coldStart = false;
          }
        }
      }
    }
  }
  digitalWrite(TERMO_PIN, TermoOn);
}

void GetVoltage()
{
  float curAnalogData = 0.0;
  curAnalogData = analogRead(A_PIN); // читаем значение на аналоговом входе
  delay(30);
  curAnalogData = curAnalogData + analogRead(A_PIN); // читаем значение на аналоговом входе
  delay(30);
  curAnalogData = curAnalogData + analogRead(A_PIN); // читаем значение на аналоговом входе
  curAnalogData = curAnalogData / 3;
  float curVoltage = (curAnalogData * Vcc) / 1024.0;
  vReal = curVoltage / (r2 / (r1 + r2));
  //Vcc = readVcc();
  //curVoltage = 0.0;
  //for (i = 0; i < COUNT; i++) {
  //  curVoltage = curVoltage + analogRead(A_PIN);
  //  delay(10);
  //}
  //curVoltage = curVoltage / COUNT;
  //float v = (curVoltage * Vcc) / 1024.0;
  //float v2 = v / (r2 / (r1 + r2));
}

void AutoChangeShowMode(bool needRefresh)
{
  int modeChanged = 0;
  unsigned long ms = millis();
  int rsltI = 0;

  if (((ms - lastChangeShowMode_ms) > ShowModeDelay_ms || needRefresh) && Mode != 6)
  {
    Mode = Mode + 1;
    if (Mode > 6) Mode = 1;
    lastChangeShowMode_ms = ms;
    modeChanged = 1;
  }

  if (modeChanged == 1)
  {
    switch (Mode)
    {
      case 1:
        tm1637.display(TermoLevel);
        tm1637.display(0, 10);
        break;
      case 2:     //T
        RefreshSensorData();
        tm1637.display(t);
        tm1637.display(0, 11);
        break;
      case 3:     //Влажн
        tm1637.display(h);
        tm1637.display(0, 12);
        break;
      case 4:     //Суммарный расход тока
        rsltI = totalTok_mACh / 1000;
        tm1637.display(rsltI);
        tm1637.display(0, 13);
        break;
      case 5:      //Current Voltage
        tm1637.display(vReal - 10);
        tm1637.display(0, 14);
        break;
    }
          // CO2
      //  mySerial.write(cmd, 9);
      //  memset(response, 0, 9);
      //  mySerial.readBytes(response, 9);
      //  int responseHigh = (int) response[2];
      //  int responseLow = (int) response[3];
      //  int ppm = 256*(responseHigh + (responseLow<0?1:0)) + responseLow;
      //  tm1637.display(ppm/10); // Выводим значение
      //  tm1637.display(0,10);

  }
  if (Mode == 6 && ms - last5ModeOn_ms > 20000)
  {
    ButtonLongClick(); //exit from 5 Mode in 10sec automatically
  }
}

void RefreshSensorData()
{
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  unsigned long ms = millis();
  if (ms - lastRefreshSensor_ms > RefreshSensorInterval_ms || lastRefreshSensor_ms == 0)
  {
    h = dht.readHumidity();
    t = dht.readTemperature();
    lastRefreshSensor_ms = ms;
  }
}

void CalcTotalTok(unsigned long periodOn_sec)
{
  totalTok_mACh = totalTok_mACh + (TokNagreva_mA * periodOn_sec) / 3600;

  if (totalTok_mACh / 1000 >= TotalTokLimit_Ach)
  {
    TermoLevel = 999;
  }
}


void loop()
{
  switch (button1.Loop()) {
    case SB_CLICK:
      ButtonClick();
      break;
    case SB_LONG_CLICK:
      ButtonLongClick();
      break;
    case SB_AUTO_CLICK:
      ButtonClick();
      break;
  }

  TermoControl();

  AutoChangeShowMode(false);
}
