//уходить в спящий режим
//Подача питания на автономку по кнопке по прерыванию на заданное короткое время (3 мин). Не отключать питание если за это время пошли импульсы на насос.
//Если в режиме подачи питания нажали кнопку долго и еще нет импульсов на насос, то подождать долгое время(WAIT_START_LONG 20ч)  если за это время импульсы не пошли, выключить автономку. Для дистанционного управления утром или запуска по таймеру
//Если в режиме подачи питания нажали кнопку коротко и еще нет импульсов на насос, то выключить автономку.
//Отключение питания через заданное время после прекращения импульсов на насос (время для продувки или повтороного включения при случайном выключении)
//Подача питания на датчик CO при начале работы насоса. Контроль CO через время (30мин) после начала подачи питания на датчик
//Сигнализация на превышение CO с отключением насоса. Выключение невозможно
//Сигнализация на падение напяжения с отключением насоса. Выключение невозможно
//Контроль температуры на корпусе с отключением насоса при перегреве. Выключение невозможно

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
#define ONE_WIRE_PIN        5
#define BZZ_PIN             8
#define BAT_PIN             A0  //контроль напряжения батареи
#define CO_SIGNAL_PIN       A1  //контроль CO
#define CO_SRC_PIN          9   //на питание датчика CO, reverse
#define NASOS_PIN           12  //на питание насоса
#define AVTONOMKA_PIN       6   //на реле включения питания автономки
#define LED_PIN             4   //LED индикации состояния

const float LOW_VALT                    = 11.5;
const float HIGH_TEMP_BODY              = 60.0;   //max допустимая температура корпуса автономки
const float HIGH_TEMP_VYHLOP            = 90.0;   //max допустимая температура в районе выхлопа
const unsigned long MAX_CO              = 300;
const unsigned long WAIT_PERIOD_M       = 2;      //короткий период ожидания включения (начала работы насоса), при этом подается питание автономки
const unsigned long WAIT_LONG_PERIOD_M  = 1200;   //долгий период ожидания включения, при этом подается питание автономки
const unsigned long ALARM_PERIOD_S      = 60;     //время подачи сигнала тревоги
const unsigned long ALARM_PAUSE_S       = 10;     //пауза между повтором bzz сигнала тревоги
const unsigned long SIGNAL_PAUSE_S      = 10;     //пауза между повтором led сигнала режима работы
const unsigned long OFF_DELAY_PERIOD_S  = 120;    //время через которое отключится питание автономки после пропадания импульсов насоса
const unsigned long MIN_NASOS_IMPULSE_PERIOD_MS   = 100; //игнорируем импульсы если между ними меньше этого времени
const unsigned long CHECK_NASOS_IMPULSE_PERIOD_S  = 5;  //период контроля импульсов насоса, подсчета импульсов внутри периода(для контроля включенной автономки)
const unsigned long CO_CHECK_PERIOD_S             = 30; //если за это время CO всегда High, подать сигнал, выключить автономку
const unsigned long CO_RAZOGREV_INIT_S            = 300; //время разогрева датчика после которого можно снимать показания
const unsigned long BATTERY_LOW_CHECK_PERIOD_S    = 300; //если за это время баттарея всегда Low, выключить автономку (долго потому что при старте работает свеча и напряжение будет низкое)
const unsigned long TEMPERATURE_HIGH_CHECK_PERIOD_S = 30; ////если за это T всегда High, подать сигнал, выключить автономку
const unsigned long CHECK_ALARM_PERIOD_S          = 10;  //период проверки различных alarms (только при работающей автономке)
const unsigned long CHECK_ALARM_DELAY_MS          = 200;  //задержка проверки различных alarms после импульса насоса, для правильного снятия U (только при работающей автономке)
const unsigned long SWITCH_ON_DELAY_MS            = 1000; //задержка всех проверок после подвчи питания на автономку
const unsigned int LOW_TONE                       = 1600; //нижняя частота зумера Гц
const unsigned int HIGH_TONE                      = 2200; //верхняя частота зумера Гц
const int NUMBER_NASOS_IMPULSES_WORK              = 5; //количество импульсов за CHECK_NASOS_IMPULSE_PERIOD_S означающее что автономка работает

// резисторы делителя напряжения
const float R1 = 45000;        // 45K
const float R2 = 3300;          // 3.3K
const float VCC = 1.222;      //  внутреннее опорное напряжение, необходимо откалибровать индивидуально  (м.б. 1.0 -- 1.2)

enum EnAlarmStatuses { NONE, DETECTED, ALARM};
enum EnMode { SLEEP, WAIT_START, WAIT_START_LONG, WORK, STOPPING, ALARM_AND_STOPPING};

boolean isProcessBtnCode;
boolean isCoReadyToCheck;
boolean isRazogrevCO;
boolean isFirstRasogrevCO;
EnAlarmStatuses lowBatteryStatus = NONE;
EnAlarmStatuses coStatus = NONE;
EnAlarmStatuses highTemperatureStatus = NONE;
EnMode mode = SLEEP;
float t_inn_body;
float t_inn_vyhlop;
int nasosImpulsesCounter;

elapsedMillis waitPeriod_ms;
elapsedMillis waitLongPeriod_ms;
elapsedMillis alarmDelayPeriod_ms;
elapsedMillis alarmPeriod_ms;
elapsedMillis offDelayPeriod_ms;
elapsedMillis batteryLowCheckPeriod_ms;
elapsedMillis coCheckPeriod_ms;
elapsedMillis coRazogrevPeriod_ms;
elapsedMillis highTempCheckPeriod_ms;
elapsedMillis checkAlarmPeriod_ms;
elapsedMillis longPeriodOn_ms;
elapsedMillis alarmBzzPause_ms;
elapsedMillis signalPause_ms;
elapsedMillis checkAlarmDelay_ms;
elapsedMillis nasosImpulsePeriod_ms;
elapsedMillis checkNasosImpulsePeriod_ms;

SButton button1(BTTN_PIN, 50, 1000, 10000, 1000);

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress tempDeviceAddress;

void setup()
{
  wdt_disable();
  Serial.begin(9600);
  pinMode(BZZ_PIN, OUTPUT);
  stopBuzzer();
  pinMode(AVTONOMKA_PIN, OUTPUT);
  pinMode(CO_SRC_PIN, OUTPUT);
  pinMode(NASOS_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  pinMode(BTTN_PIN, INPUT_PULLUP);
  pinMode(NASOS_SIGNAL_PIN, INPUT_PULLUP);
  digitalWrite(CO_SRC_PIN, HIGH); //для закрытия pnp
  pinMode(CO_SIGNAL_PIN, INPUT);


  analogReference(INTERNAL);  // DEFAULT: стандартное опорное напряжение 5 В (на платформах с напряжением питания 5 В) или 3.3 В (на платформах с напряжением питания 3.3 В)
  //INTERNAL: встроенное опорное напряжение около 1.1 В на микроконтроллерах ATmega168 и ATmega328, и 2.56 В на ATmega8.
  //EXTERNAL : внешний источник опорного напряжения, подключенный к выводу AREF

  button1.begin();

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, 10);

  attachInterrupt(1, NasosImpulseHandler, FALLING);

  mode = SLEEP;
  lowBatteryStatus = NONE;
  coStatus = NONE;
  highTemperatureStatus = NONE;

  Serial.println("Setup");

  _delay_ms(1000);

  wdt_enable(WDTO_8S);
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
    if (mode == WAIT_START || mode == WAIT_START_LONG)
    {
      mode = SLEEP;
    }
    else if (mode == SLEEP)
    {
      resetAlarm();
      waitPeriod_ms = 0;
      checkNasosImpulsePeriod_ms = 0;
      mode = WAIT_START;
    }
    else if (mode == ALARM_AND_STOPPING)
    {
      resetAlarm();
      Serial.println("STOPPING_1");
      mode = STOPPING;
      offDelayPeriod_ms = 0;
    }
    else if (mode == WORK)
    {
      Serial.println("STOPPING_2");
      mode = STOPPING;
      offDelayPeriod_ms = 0;
    }
  }
  else if (typeClick == 'l')
  {
    playDigitSignal(1, 0, 'a');
    if (mode == SLEEP || mode == WAIT_START)
    {
      resetAlarm();
      waitLongPeriod_ms = 0;
      checkNasosImpulsePeriod_ms = 0;
      mode = WAIT_START_LONG;
    }
    else if (mode == WORK)
    {
      Serial.println("STOPPING_22");
      mode = STOPPING;
      offDelayPeriod_ms = 0;
    }
  }
  Serial.print("mode4=");
  Serial.println(mode);
}

void PrepareSleep()
{
  pinMode(BTTN_PIN, INPUT_PULLUP);
  attachInterrupt(0, WakeUpByBttn, FALLING);

  wdt_disable();
}

void NasosImpulseHandler()
{
  Serial.println("NasosImpulse_0");
  if (nasosImpulsePeriod_ms > MIN_NASOS_IMPULSE_PERIOD_MS)
  {
    Serial.println("NasosImpulse_1");
    Serial.print("mode=");
    Serial.println(mode);
    nasosImpulsesCounter ++;
  }
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

void WakeUpByBttn()
{
  // запрещаем режим сна
  sleep_disable();
  // включаем все внутренние блоки ЦП
  power_all_enable();
  // запрещаем прерывания
  cli();

  attachInterrupt(1, NasosImpulseHandler, FALLING);
  Serial.println("WakeUp button");
  ActionBtn('s');
}

void ControlSourceCO()
{
  if (mode == WORK)
  {
    if (isRazogrevCO && coRazogrevPeriod_ms >= CO_RAZOGREV_INIT_S * 1000)
    {
      isRazogrevCO = false;
    }
  }
  else
  {
    isRazogrevCO = false;
  }
}

bool CheckCO()
{
  ControlSourceCO();
  if (mode == WORK && !isRazogrevCO)
  {
    if (digitalRead(!CO_SIGNAL_PIN)) //reverse
    {
      if (coStatus == NONE)
      {
        coStatus = DETECTED;
        coCheckPeriod_ms = 0;
      }
      if (coStatus == DETECTED && coCheckPeriod_ms >= CO_CHECK_PERIOD_S * 1000)
      {
        coStatus = ALARM;
        Serial.println("coStatus = ALARM");
      }
    }
    else
    {
      coStatus == NONE;
    }
  }
  return (coStatus == ALARM);
}

bool CheckTemp()
{
  sensors.requestTemperatures();
  t_inn_body = sensors.getTempCByIndex(0);
  t_inn_vyhlop = sensors.getTempCByIndex(1);
  Serial.print("T1= ");
  Serial.println(t_inn_body);
  Serial.print("T2= ");
  Serial.println(t_inn_vyhlop);
  if (t_inn_body > HIGH_TEMP_BODY || t_inn_vyhlop > HIGH_TEMP_VYHLOP)
  {
    if (highTemperatureStatus == NONE)
    {
      highTemperatureStatus = DETECTED;
      highTempCheckPeriod_ms = 0;
    }
    if (highTemperatureStatus == DETECTED && highTempCheckPeriod_ms >= TEMPERATURE_HIGH_CHECK_PERIOD_S * 1000)
    {
      highTemperatureStatus = ALARM;
      Serial.println("highTemperatureStatus = ALARM");
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
  Serial.print("CheckBattery=");
  Serial.println(curAnalogData);
  v_bat = (curAnalogData * VCC) / 1024.0 / (R2 / (R1 + R2));
  Serial.println(v_bat);
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
      Serial.println("lowBatteryStatus = ALARM");
    }
  }
  else
  {
    lowBatteryStatus = NONE;
  }
  return (lowBatteryStatus == ALARM);
}

void checkNasosImpulses()
{
  if (checkNasosImpulsePeriod_ms >= CHECK_NASOS_IMPULSE_PERIOD_S * 1000)
  {
    if (nasosImpulsesCounter >= NUMBER_NASOS_IMPULSES_WORK)
    {
      if (mode == WAIT_START || mode == WAIT_START_LONG)
      {
        mode = WORK;
        coRazogrevPeriod_ms = 0;
        isRazogrevCO = true;
      }
      checkAlarmDelay_ms = 0;
    }
    else
    {
      if (mode == WORK)
      {
        Serial.println("STOPPING_3");
        mode = STOPPING;
        offDelayPeriod_ms = 0;
      }
    }
    nasosImpulsesCounter = 0;
    checkNasosImpulsePeriod_ms = 0;
  }
}

bool checkAlarms()
{
  boolean alarm = false;
  //DL if (mode == WORK && checkAlarmPeriod_ms >= CHECK_ALARM_PERIOD_S * 1000 && checkAlarmDelay_ms >= CHECK_ALARM_DELAY_MS)
  if ((mode == WORK || mode == WAIT_START || mode == WAIT_START_LONG) && checkAlarmPeriod_ms >= CHECK_ALARM_PERIOD_S * 1000) //DL
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
  wdt_enable(WDTO_8S);
}

void ModeControl()
{
  if (mode == WAIT_START && waitPeriod_ms > WAIT_PERIOD_M * 60000 || mode == WAIT_START_LONG && waitLongPeriod_ms > WAIT_LONG_PERIOD_M * 60000)
  {
    mode = SLEEP;
  }
  else if (mode == STOPPING && offDelayPeriod_ms > OFF_DELAY_PERIOD_S * 1000)
  {
    mode = SLEEP;
  }
  else if (mode == ALARM_AND_STOPPING && alarmPeriod_ms > ALARM_PERIOD_S * 1000)
  {
    resetAlarm();
    offDelayPeriod_ms = 0;
    Serial.print("STOPPING_4");
    mode = STOPPING;
  }
  else if (mode == STOPPING && offDelayPeriod_ms > OFF_DELAY_PERIOD_S * 1000)
  {
    mode = SLEEP;
  }

  digitalWrite(NASOS_PIN, (mode == WORK));
  digitalWrite(CO_SRC_PIN, mode != WORK); // invert
  digitalWrite(AVTONOMKA_PIN, mode != SLEEP);
}

void resetAlarm()
{
  lowBatteryStatus = NONE;
  coStatus = NONE;
}

//typeSignal 'l' or 'b' or 'a'  -  led or buzzer or all
void playDigitSignal(byte numberLowToneSignals, byte numberHighToneSignals, char typeSignal)
{
  Serial.println("playDigitSignal: ");
  Serial.println(numberLowToneSignals);
  Serial.println(numberHighToneSignals);
  Serial.println(typeSignal);
  //long
  for (int i = 1; i <= numberLowToneSignals; i++)
  {
    if (typeSignal == 'l' || typeSignal == 'a')
      digitalWrite(LED_PIN, HIGH);
    if (typeSignal == 'b' || typeSignal == 'a')
      tone(BZZ_PIN, LOW_TONE);
    _delay_ms(600);
    if (typeSignal == 'l' || typeSignal == 'a')
      digitalWrite(LED_PIN, LOW);
    if (typeSignal == 'b' || typeSignal == 'a')
      stopBuzzer();
    _delay_ms(300);
    wdt_reset();
  }
  if (numberLowToneSignals > 0 && numberHighToneSignals >  0)
    _delay_ms(200);

  //short
  for (int i = 1; i <= numberHighToneSignals; i++)
  {
    if (typeSignal == 'l' || typeSignal == 'a')
      digitalWrite(LED_PIN, HIGH);
    if (typeSignal == 'b' || typeSignal == 'a')
      tone(BZZ_PIN, HIGH_TONE);
    _delay_ms(200);
    if (typeSignal == 'l' || typeSignal == 'a')
      digitalWrite(LED_PIN, LOW);
    if (typeSignal == 'b' || typeSignal == 'a')
      stopBuzzer();
    _delay_ms(200);
    wdt_reset();
  }
}

void stopBuzzer()
{
  noTone(BZZ_PIN);
  digitalWrite(BZZ_PIN, HIGH); //для закрытия p-chanel mosfet
}

void modeSignal()
{
  if (signalPause_ms > SIGNAL_PAUSE_S * 1000)
  {
    signalPause_ms = 0;
    if (mode == WAIT_START)
      playDigitSignal(0, 1, 'l');
    else if (mode == WAIT_START_LONG)
      playDigitSignal(1, 0, 'l');
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
  checkButtons();
  checkAlarms();
  checkNasosImpulses();
  ModeControl();
  modeSignal();
  alarmSignal();
  wdt_reset();
}
