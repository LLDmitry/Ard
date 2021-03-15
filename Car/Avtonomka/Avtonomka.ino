//уходить в спящий режим
//Подача питания на автономку по кнопке по прерыванию на заданное короткое время (2ч). Не отключать питание если пошли импульсы на насос.
//Если в режиме подачи питания нажали кнопку долго и нет импульсов на насос, то выключить автономку, подождать заданное время(6ч) и затем подать питание на заданное время(8ч), если за это время импульсы не пошли, выключить автономку. Для дистанционного управления утром.
//Отключение питания через заданное время после прекращения импульсов на насос (время для продувки или повтороного включения при случайном выключении)
//Подача питания на датчик CO при начале работы насоса. Контроль CO через время (30мин) после начала подачи питания на датчик
//Сигнализация на превышение CO с отключением насоса.
//Сигнализация на падение напяжения с отключением насоса.
//Контроль температуры на корпусе с отключением насоса при перегреве.

#include "sav_button.h"
#include <elapsedMillis.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>

#define BTTN_PIN            2   //(обязательно = 2 т.к. attachInterrupt(0,...)
#define NASOS_SIGNAL_PIN    3   //(обязательно = 3 т.к. attachInterrupt(1,...) контроль импульсов питания насоса (признак работающей автономки), через сопротивление и стабилитрон
#define ONE_WIRE_PIN        11
#define BZZ_PIN             5
#define BAT_PIN             A0  //контроль напряжения батареи
#define CO_SIGNAL_PIN       A1  //контроль CO
#define NASOS_PIN           9   //на питание насоса, и на питание датчика CO
#define AVTONOMKA_PIN       10  //на питание автономки
#define LED_PIN             7   //LED индикации состояния


const unsigned long DELAY_CO_CHECK_M    = 20;     //задержка для разогрева датчика CO до начала приема его показаний
//const unsigned long ALARM_DELAY_S     = 60;     //задержка выдачи сигнала тревоги после первого появления тревоги (фильтр ошибочных показаний)
const float LOW_VALT                    = 11.5;
const float HIGH_TEMP                   = 60.0;   //max допустимая температура корпуса автономки
const unsigned long MAX_CO              = 350;
const unsigned long WAIT_PERIOD_M       = 2; //120;    //период ожидания включения, при этом подается питание автономки
const unsigned long WAIT_LONG_PERIOD_M  = 1; //720;    //долгий период ожидания включения, при этом не подается питание автономки
const unsigned long LONG_PERIOD_ON_M    = 3; //480;       //после долгого ожидания WAIT_LONG_PERIOD_M включаем на LONG_PERIOD_ON_M
const unsigned long ALARM_PERIOD_S      = 60;     //время подачи сигнала тревоги
//const unsigned long ALARM_CO_BZZ_S      = 3;      //длительность сигнала тревоги по CO
//const unsigned long ALARM_T_BZZ_S       = 2;      //длительность сигнала тревоги по T
//const unsigned long ALARM_V_BZZ_S       = 1;      //длительность сигнала тревоги по V
const unsigned long ALARM_PAUSE_S       = 10;      //пауза между повтором bzz сигнала тревоги
const unsigned long LED_PAUSE_S         = 10;      //пауза между повтором led сигнала режима работы
const unsigned long OFF_DELAY_PERIOD_S  = 120;    //время через которое отключится питание автономки после пропадания импульсов насоса
const unsigned long NASOS_IMPULSE_PERIOD_S  = 10;     //максимально возможный период импульсов насоса (для контроля включенной автономки)
const unsigned long CO_CHECK_PERIOD_S = 100; //если за это CO всегда High, выключить автономку
const unsigned long BATTERY_LOW_CHECK_PERIOD_S = 300; //если за это время баттарея всегда Low, выключить автономку (долго потому что еще может работать свеча)
const unsigned long CHECK_ALARM_PERIOD_S = 10;  //период проверки различных alarms (только при работающей автономке)


// резисторы делителя напряжения
const float R1 = 100000;        // 100K
const float R2 = 6700;          // 6.7K
const float VCC = 1.08345;      //  внутреннее опорное напряжение, необходимо откалибровать индивидуально  (м.б. 1.0 -- 1.2)

enum EnAlarmStatuses { NONE, DETECTED, ALARM};
enum EnMode { SLEEP, WAIT_START, WAIT_START_LONG_OFF, WAIT_START_LONG_ON, WORK, STOPPING, ALARM_AND_STOPPING};

boolean isProcessBtnCode;
EnAlarmStatuses lowBatteryStatus = NONE;
EnAlarmStatuses coStatus = NONE;
EnAlarmStatuses highTemperatureStatus = NONE;
EnMode mode = SLEEP;
float t_inn;

elapsedMillis waitPeriod_ms;
elapsedMillis waitLongPeriod_ms;
elapsedMillis alarmDelayPeriod_ms;
elapsedMillis alarmPeriod_ms;
elapsedMillis offDelayPeriod_ms;
elapsedMillis nasosImpulsePeriod_ms;
elapsedMillis batteryLowCheckPeriod_ms;
elapsedMillis coCheckPeriod_ms;
elapsedMillis checkAlarmPeriod_ms;
elapsedMillis longPeriodOn_ms;
elapsedMillis alarmBzzPause_ms;
//elapsedMillis ledOn_ms;
elapsedMillis ledPause_ms;

SButton button1(BTTN_PIN, 100, 3000, 10000, 1000);

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress tempDeviceAddress;

void setup()
{
  wdt_disable();
  Serial.begin(9600);
  pinMode(BZZ_PIN, OUTPUT);
  pinMode(AVTONOMKA_PIN, OUTPUT);
  pinMode(NASOS_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  pinMode(BTTN_PIN, INPUT_PULLUP);
  pinMode(NASOS_SIGNAL_PIN, INPUT);


  analogReference(INTERNAL);  // DEFAULT: стандартное опорное напряжение 5 В (на платформах с напряжением питания 5 В) или 3.3 В (на платформах с напряжением питания 3.3 В)
  //INTERNAL: встроенное опорное напряжение около 1.1 В на микроконтроллерах ATmega168 и ATmega328, и 2.56 В на ATmega8.
  //EXTERNAL : внешний источник опорного напряжения, подключенный к выводу AREF

  button1.begin();

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, 10);

  attachInterrupt(1, NasosWorks, RISING);

  mode = SLEEP;
  lowBatteryStatus = NONE;
  coStatus = NONE;
  highTemperatureStatus = NONE;

  Serial.println("Setup");

  _delay_ms(1000);

  //wdt_enable(WDTO_8S);
}

void ActionBtn(byte typeClick) //'s' or 'l'
{
  Serial.print("ActionBtn:");
  Serial.println(typeClick);
  Serial.print("mode3=");
  Serial.println(mode);
  if (typeClick == 's')
  {
    playDigitSignal(0, 1, 'a');
    if (mode == WAIT_START || mode == WAIT_START_LONG_ON)
    {
      mode = SLEEP;
    }
    else if (mode == SLEEP || mode == WAIT_START_LONG_OFF)
    {
      mode = WAIT_START;
      waitPeriod_ms = 0;
    }
    else if (mode == ALARM_AND_STOPPING)
    {
      resetAlarm();
      mode = STOPPING;
      offDelayPeriod_ms = 0;
    }
    else if (mode == WORK)
    {
      mode = STOPPING;
      offDelayPeriod_ms = 0;
    }
  }
  else if (typeClick == 'l')
  {
    playDigitSignal(1, 0, 'a');
    if (mode == WAIT_START)
    {
      waitLongPeriod_ms = 0;
      mode = WAIT_START_LONG_OFF;
    }
  }
  Serial.print("mode4=");
  Serial.println(mode);
}

void PrepareSleep()
{
  pinMode(BTTN_PIN, INPUT_PULLUP);
  attachInterrupt(0, WakeUp, FALLING);

  wdt_disable();
}

void NasosWorks()
{
  mode = WORK;
  nasosImpulsePeriod_ms = 0;
}

void DoSleep()
{
  // отключаем АЦП
  ADCSRA = 0;
  // отключаем всю периферию
  power_all_disable();
  // устанавливаем режим сна - самый глубокий, здоровый сон :)
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  //set_sleep_mode(SLEEP_MODE_STANDBY);
  // разрешаем спящий режим
  sleep_enable();
  // разрешаем прерывания
  sei();
  // собственно засыпаем
  sleep_cpu();
}

void WakeUp()
{
  // запрещаем режим сна
  sleep_disable();
  // включаем все внутренние блоки ЦП
  power_all_enable();
  // запрещаем прерывания
  cli();

  attachInterrupt(1, NasosWorks, RISING);
}

bool CheckCO()
{
  float curAnalogData = 0.0;
  curAnalogData = curAnalogData + analogRead(CO_SIGNAL_PIN);
  if (curAnalogData > MAX_CO)
  {
    if (coStatus == NONE)
    {
      coStatus = DETECTED;
      coCheckPeriod_ms = 0;
    }
    if (coCheckPeriod_ms >= CO_CHECK_PERIOD_S * 1000)
    {
      coStatus = ALARM;
    }
  }
  else
  {
    coStatus == NONE;
  }
  return (coStatus == ALARM);
}

bool CheckTemp()
{
  sensors.requestTemperatures();
  t_inn = sensors.getTempCByIndex(0);
  //  Serial.print("T1= ");
  //  Serial.println(t_inn);
  if (t_inn > HIGH_TEMP)
  {
    if (highTemperatureStatus == NONE)
    {
      highTemperatureStatus = DETECTED;
      batteryLowCheckPeriod_ms = 0;
    }
    if (highTemperatureStatus == DETECTED && batteryLowCheckPeriod_ms >= BATTERY_LOW_CHECK_PERIOD_S * 1000)
    {
      highTemperatureStatus = ALARM;
    }
  }
  else
  {
    highTemperatureStatus = NONE;
  }
  return (highTemperatureStatus == ALARM);
}

bool CheckBattery()
{
  float curAnalogData = 0.0;
  float v_bat = 0.0;
  curAnalogData = curAnalogData + analogRead(BAT_PIN);
  //Serial.println(curAnalogData);
  v_bat = (curAnalogData * VCC) / 1024.0 / (R2 / (R1 + R2));
  if (v_bat < LOW_VALT)
  {
    if (lowBatteryStatus == NONE)
    {
      lowBatteryStatus = DETECTED;
      batteryLowCheckPeriod_ms = 0;
    }
    if (lowBatteryStatus == DETECTED && batteryLowCheckPeriod_ms >= BATTERY_LOW_CHECK_PERIOD_S * 1000)
    {
      lowBatteryStatus = ALARM;
    }
  }
  else
  {
    lowBatteryStatus = NONE;
  }
  return (lowBatteryStatus == ALARM);
}

bool checkNasosImpulses()
{
  boolean bRslt = false;
  if (nasosImpulsePeriod_ms >= NASOS_IMPULSE_PERIOD_S * 1000)
  {
    bRslt = true;
    if (mode == WORK)
    {
      mode = STOPPING;
      offDelayPeriod_ms = 0;
    }
  }
  return bRslt;
}

bool checkAlarms()
{
  boolean alarm = false;
  if (mode == WORK && checkAlarmPeriod_ms >= CHECK_ALARM_PERIOD_S * 1000)
  {
    checkAlarmPeriod_ms = 0;
    if (CheckCO() || CheckBattery() || CheckTemp())
    {
      mode = ALARM_AND_STOPPING;
      alarmPeriod_ms = 0;
      alarm = true;
    }
  }
  return alarm;
}

void checkButtons()
{
  switch (button1.Loop())
  {
    case SB_CLICK:
      Serial.println("Press button");
      ActionBtn('s');
      break;
    case SB_LONG_CLICK:
      Serial.println("Long press button");
      ActionBtn('l');
      break;
  }
}

void GoSleep()
{
  PrepareSleep();
  Serial.println("DoSleep");
  _delay_ms(50);
  DoSleep();
  //Serial.flush();
  _delay_ms(100);
  Serial.println("ExitSleep");
  _delay_ms(50);
  //wdt_enable(WDTO_8S);

  wdt_reset();
}

void ModeControl()
{
  Serial.print("ModeControl_1=");
  Serial.println(mode);
  if (mode == WAIT_START && waitPeriod_ms > WAIT_PERIOD_M * 60000 || mode == WAIT_START_LONG_ON && longPeriodOn_ms > LONG_PERIOD_ON_M * 60000)
  {
    mode = SLEEP;
  }
  else if (mode == WAIT_START_LONG_OFF && waitLongPeriod_ms > WAIT_LONG_PERIOD_M * 60000)
  {
    longPeriodOn_ms = 0;
    mode = WAIT_START_LONG_ON;
  }
  else if (mode == STOPPING && offDelayPeriod_ms > OFF_DELAY_PERIOD_S * 1000)
  {
    mode = SLEEP;
  }
  else if (mode == ALARM_AND_STOPPING && alarmPeriod_ms > ALARM_PERIOD_S * 1000)
  {
    resetAlarm();
    offDelayPeriod_ms = 0;
    mode = STOPPING;
  }
  else if (mode == STOPPING && offDelayPeriod_ms > OFF_DELAY_PERIOD_S * 1000)
  {
    mode = SLEEP;
  }

  digitalWrite(NASOS_PIN, (mode == WORK));
  digitalWrite(AVTONOMKA_PIN, (mode == WORK || mode == STOPPING));
  Serial.print("ModeControl_2=");
  Serial.println(mode);
}

void resetAlarm()
{
  lowBatteryStatus = NONE;
  coStatus = NONE;
}

//typeSignal 'l' or 'b' or 'a'  -  led or buzzer or all
void playDigitSignal(byte numberLongSignals, byte numberShortSignals, char typeSignal)
{
  //long
  for (int i = 1; i <= numberLongSignals; i++)
  {
    if (typeSignal == 'l' || typeSignal == 'a')
      digitalWrite(LED_PIN, HIGH);
    if (typeSignal == 'b' || typeSignal == 'a')
      digitalWrite(BZZ_PIN, HIGH);
    _delay_ms(600);
    if (typeSignal == 'l' || typeSignal == 'a')
      digitalWrite(LED_PIN, LOW);
    if (typeSignal == 'b' || typeSignal == 'a')
      digitalWrite(BZZ_PIN, LOW);
    _delay_ms(300);
    wdt_reset();
  }
  if (numberLongSignals > 0 && numberShortSignals >  0)
    _delay_ms(500);

  //short
  for (int i = 1; i <= numberShortSignals; i++)
  {
    if (typeSignal == 'l' || typeSignal == 'a')
      digitalWrite(LED_PIN, HIGH);
    if (typeSignal == 'b' || typeSignal == 'a')
      digitalWrite(BZZ_PIN, HIGH);
    _delay_ms(200);
    if (typeSignal == 'l' || typeSignal == 'a')
      digitalWrite(LED_PIN, LOW);
    if (typeSignal == 'b' || typeSignal == 'a')
      digitalWrite(BZZ_PIN, LOW);
    _delay_ms(300);
    wdt_reset();
  }
}

void modeSignal()
{
  if (ledPause_ms > LED_PAUSE_S * 1000)
  {
    ledPause_ms = 0;
    if (mode == WAIT_START)
      playDigitSignal(0, 1, 'l');
    else if (mode == WAIT_START_LONG_OFF)
      playDigitSignal(1, 0, 'l');
    else if (mode == WAIT_START_LONG_ON)
      playDigitSignal(1, 1, 'l');
    else if (mode == STOPPING)
      playDigitSignal(3, 0, 'l');
  }
}

void alarmSignal()
{
  if (mode == ALARM_AND_STOPPING)
  {
    if (alarmBzzPause_ms > ALARM_PAUSE_S * 1000)
    {
      alarmBzzPause_ms = 0;
      if (lowBatteryStatus == ALARM)
        playDigitSignal(1, 1, 'a');
      else if (highTemperatureStatus == ALARM)
        playDigitSignal(1, 2, 'a');
      else if (coStatus == ALARM)
        playDigitSignal(1, 3, 'a');
    }
  }
}

void loop()
{
  Serial.print("mode1=");
  Serial.println(mode);
  checkButtons();
  checkAlarms();
  checkNasosImpulses();
  ModeControl();
  modeSignal();
  alarmSignal();
  Serial.print("mode2=");
  Serial.println(mode);
  _delay_ms(50);
  if (mode == SLEEP)
    GoSleep();
}
