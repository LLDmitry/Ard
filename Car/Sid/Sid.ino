//Sid
#include "sav_button.h"
#include <elapsedMillis.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

#define ONE_WIRE_PIN 11  // DS18b20
#define TERMO_PIN1 13 // нагревательный элемент sid1
#define TERMO_PIN2 12 // нагревательный элемент sid2
#define BZ_PIN 10   // signal
#define BTTN_PIN1 3 //чтобы работало прерывание 0
#define BTTN_PIN2 2 //чтобы работало прерывание 1

#define LED_B_PIN1 7  //sidMode1=1
#define LED_G_PIN1 8  //sidMode1=2
#define LED_R_PIN1 9  //sidMode1=3
#define LED_B_PIN2 4
#define LED_G_PIN2 5
#define LED_R_PIN2 6

#define SOCKET12_PIN 15 //A1

#define BAT_PIN A0

const float LOW_VALT_ALARM = 11.9;      //если измеренное напряжение меньше этого значения - выдать Alarm (моргнуть красными светодиодами), выключить нагрев и 12В
const float HALF_BATTERY = 12.1;        //50% заряда
const float ENGINE_WORK_VALT = 13.0;    //если измеренное напряжение больше этого значения - значит мотор работает
const float T_AUTO_START_NAGR1 = 5.0;  //для автоматического включения при старте режима нагрева, соответствующего температуре салона (только для sidNagrev1)
const float T_AUTO_START_NAGR2 = 1.0;
const float T_AUTO_START_NAGR3 = -3.0;

// резисторы делителя напряжения
const float R1 = 100000;         // 100K
const float R2 = 6700;          // 6.7K
const float VCC = 1.08345;        //  внутреннее опорное напряжение, необходимо откалибровать индивидуально  (м.б. 1.0 -- 1.2)

const unsigned long PAUSE_NAGREV_S = 10;
const unsigned long TIME_NAGREV1_S = 7;
const unsigned long TIME_NAGREV2_S = 20;
const unsigned long TIME_NAGREV3_S = 50;
const unsigned long PERIOD_CHECK_ENGINE_STATUS_S = 2;    //период опроса напряжения
const unsigned long TIME_STOP_ENGINE_DECISION_S = 20; //для отфильтровывания провалов напряжения и перезапусков двигателя
const unsigned long TIME_LOW_VOLTAGE_DECISION_S = 120; //для отфильтровывания провалов напряжения и возможности включения мощной нагрузки на это время при начальном напряжении немного превышающем Low
const unsigned long TIME_SID_WORK_WHEN_STOP_ENGINE_S = 1800; //30м - время работы обогрева при ручном включении при неработающем двигателе
const unsigned long TIME_12V_WORK_AFTER_STOP_ENGINE_S = 6; //6s - время работы прикуривателя после выключения двигателя
const unsigned long TIME_12V_WORK_AFTER_MANUAL_START_S = 7200; //2ч - время работы прикуривателя после последнего нажатия любой из кнопок обогрева
//const unsigned long TIME_REDUCE_NAGREV_S = 1800; //30м - время последнего изменения нагрева через которое при работающем двигателе sidMode автоматически переключится на 1 если было на 2 или 3
const unsigned long TIME_REDUCE_NAGREV1_S = 300; //50м - время последнего изменения нагрева через которое при работающем двигателе sidMode автоматически переключится на 1 если было на 2 или 3, при начальной температуре T1
const unsigned long TIME_REDUCE_NAGREV2_S = 900; //15м - время последнего изменения нагрева через которое при работающем двигателе sidMode автоматически переключится на 1 если было на 2 или 3, при начальной температуре T2
const unsigned long TIME_REDUCE_NAGREV3_S = 1600; //30м - время последнего изменения нагрева через которое при работающем двигателе sidMode автоматически переключится на 1 если было на 2 или 3, при начальной температуре T3

unsigned long timeReduceNagrevLimit = 0;
boolean isEngineWork = false;
boolean isLowVoltage = false;
boolean isHalfBattery = false;
boolean isManualNagrevWork = false;      //запуск нагрева при выключенном двигателе
boolean isJustStartedEngine = false;  //выставляется при старте и сбрасывается после обработки

int rawX, rawY, rawZ;
float scaledX, scaledY, scaledZ; // Scaled values for each axis
volatile byte old_ADCSRA;

SButton button1(BTTN_PIN1, 100, 1000, 10000, 1000);
SButton button2(BTTN_PIN2, 100, 1000, 10000, 1000);

boolean sidNagrev1 = false;
boolean sidNagrev2 = false;
int sidMode1 = 0;   //0 - off, 1, 2, 3
int sidMode2 = 0;
float t_inn;  //температура датчика Inside
boolean socket12Mode = false;
volatile int buttonState = 0;

elapsedMillis thermoOn1_ms;
elapsedMillis thermoOn2_ms;
elapsedMillis thermoOff1_ms;  //выключаем в разное время в зав-ти от sidMode1,2
elapsedMillis thermoOff2_ms;
elapsedMillis checkEngineStatus_ms;
elapsedMillis stopEngineDecision_ms;
elapsedMillis lowVoltageDecision_ms;
elapsedMillis manualWorkStopEngine_ms;
elapsedMillis timeReduceNagrevSid1_ms;
elapsedMillis timeReduceNagrevSid2_ms;
int numberLowVoltageSleeps = 0;
int numberStopEngineSleeps = 0;
int numberNoActionsSleeps = 0;

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);

DeviceAddress tempDeviceAddress;

// watchdog interrupt
ISR(WDT_vect)
{
  wdt_disable();  // disable watchdog
}  // end of WDT_vect

void setup()
{
  Serial.begin(9600);
  Serial.println("STARTING ...");

  analogReference(INTERNAL);  // DEFAULT: стандартное опорное напряжение 5 В (на платформах с напряжением питания 5 В) или 3.3 В (на платформах с напряжением питания 3.3 В)
  //INTERNAL: встроенное опорное напряжение около 1.1 В на микроконтроллерах ATmega168 и ATmega328, и 2.56 В на ATmega8.
  //EXTERNAL : внешний источник опорного напряжения, подключенный к выводу AREF

  pinMode(BTTN_PIN1, INPUT_PULLUP);
  pinMode(BTTN_PIN2, INPUT_PULLUP);

  pinMode(TERMO_PIN1, OUTPUT);
  pinMode(TERMO_PIN2, OUTPUT);
  pinMode(LED_B_PIN1, OUTPUT);
  pinMode(LED_G_PIN1, OUTPUT);
  pinMode(LED_R_PIN1, OUTPUT);
  pinMode(LED_B_PIN2, OUTPUT);
  pinMode(LED_G_PIN2, OUTPUT);
  pinMode(LED_R_PIN2, OUTPUT);
  pinMode(BZ_PIN, OUTPUT);
  pinMode(SOCKET12_PIN, OUTPUT);

  // Инициация кнопок
  button1.begin();
  button2.begin();

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, 10);

  _delay_ms(1000);
}


void ActionBtn(int btn, int state) //btn: 1 or 2; state: 1-short, 2-long
{
  switch (state)
  {
    case 1:
      //Serial.println("btn");
      //  digitalWrite(BZ_PIN, true);
      //_delay_ms(200);
      digitalWrite(BZ_PIN, false);

      if (!isLowVoltage)
      {
        if (!isEngineWork)
        {
          numberNoActionsSleeps = 0;
          if (sidMode1 == 0 && btn == 1)
            thermoOff1_ms = PAUSE_NAGREV_S * 1000 + 1;
          if (sidMode2 == 0 && btn == 2)
            thermoOff2_ms = PAUSE_NAGREV_S * 1000 + 1;
        }

        if (btn == 1)
        {
          sidMode1 -= 1;
          if (sidMode1 < 0)
            sidMode1 = 3;
          if (isEngineWork && sidMode1 != 0)
            timeReduceNagrevSid1_ms = 0;
        }
        else
        {
          sidMode2 -= 1;
          if (sidMode2 < 0)
            sidMode2 = 3;
          if (isEngineWork && sidMode2 != 0)
            timeReduceNagrevSid2_ms = 0;
        }

        //        Serial.println(isEngineWork);
        //        Serial.println(sidMode1);
        //        Serial.println(sidMode2);
        //        _delay_ms(100);
        //if (!isEngineWork && (sidMode1 != 0 || sidMode2 != 0))
      }
      break;
    case 2:
      //Serial.println("btn long");
      digitalWrite(BZ_PIN, true);
      _delay_ms(500);
      digitalWrite(BZ_PIN, false);
      if (btn == 1)
        sidMode1 = 0;
      else
        sidMode2 = 0;
      break;
      numberNoActionsSleeps = TIME_12V_WORK_AFTER_MANUAL_START_S / 8 + 1; //switch off immediatly
  }
  if (!isEngineWork)
  {
    if (sidMode1 == 0 && sidMode2 == 0)
    {
      isManualNagrevWork = false;
    }
    else
    {
      isManualNagrevWork = true;
      manualWorkStopEngine_ms = 0;
    }
  }
  ShowMode();
  //  Serial.print("sidMode1=");
  //  Serial.println(sidMode1);
  //  _delay_ms(10);
}

void CheckEngineStatus()
{
  if (checkEngineStatus_ms > PERIOD_CHECK_ENGINE_STATUS_S * 1000)
  {
    boolean isPrevEngineWork = isEngineWork;

    float curAnalogData = 0.0;
    float v_bat = 0.0;
    for (int i = 0; i < 3; i++) {
      curAnalogData = curAnalogData + analogRead(BAT_PIN);
      //Serial.println(curAnalogData);
      _delay_ms(10);
    }
    curAnalogData = curAnalogData / 3;
    v_bat = (curAnalogData * VCC) / 1024.0 / (R2 / (R1 + R2));

    //    Serial.print("bat=");
    //    Serial.println(v_bat);
    //    _delay_ms(100);

    if (v_bat < (ENGINE_WORK_VALT - (sidNagrev1 ? 0.3 : 0) - (sidNagrev2 ? 0.3 : 0)))
    {
      if (isPrevEngineWork && (stopEngineDecision_ms > TIME_STOP_ENGINE_DECISION_S * 1000))
      {
        isEngineWork = false;
        numberNoActionsSleeps = TIME_12V_WORK_AFTER_MANUAL_START_S / 8 + 1;
      }

      if (v_bat < LOW_VALT_ALARM)
      {
        if (!isLowVoltage)
        {
          isLowVoltage = ((lowVoltageDecision_ms > TIME_LOW_VOLTAGE_DECISION_S * 1000) || (numberLowVoltageSleeps * 8 > TIME_LOW_VOLTAGE_DECISION_S));
        }

        if (isLowVoltage)
        {
          numberNoActionsSleeps = TIME_12V_WORK_AFTER_MANUAL_START_S / 8 + 1;
          numberStopEngineSleeps = TIME_12V_WORK_AFTER_STOP_ENGINE_S / 8 + 1;
          StopNagrev();
          socket12Mode = false;
          LowVoltageAlarm();
        }
      }
      else //Engine stop but battery not less LOW_VALT
      {
        lowVoltageDecision_ms = 0;
        numberLowVoltageSleeps = 0;
        isLowVoltage = false;
        isHalfBattery = (v_bat < HALF_BATTERY);
      }
      if (!isLowVoltage && sidMode1 == 0 && sidMode2 == 0)
      {
        InfoSignal();
      }
    }
    else //not stop engine
    {
      isEngineWork = true;
      isLowVoltage = false;
      isHalfBattery = false;
      stopEngineDecision_ms = 0;
      lowVoltageDecision_ms = 0;
      numberLowVoltageSleeps = 0;
      numberStopEngineSleeps = 0;
      numberNoActionsSleeps = 0;
      socket12Mode = true;
    }

    isJustStartedEngine = (isEngineWork && !isPrevEngineWork);
    checkEngineStatus_ms = 0;
  }
  //  _delay_ms(100);
  //  Serial.println("");
  //  Serial.print("NAS=");
  //  Serial.println(numberNoActionsSleeps);
  //  Serial.print("SES=");
  //  Serial.println(numberStopEngineSleeps);
  //  _delay_ms(100);
}

// refresh only immediatly after start (isJustStartedEngine=true)
void RefreshSensors()
{
  sensors.requestTemperatures();
  t_inn = sensors.getTempCByIndex(0);
  //  Serial.print("T1= ");
  //  Serial.println(t_inn);
}

void Socket12Control()
{
  //  _delay_ms(100);
  //  Serial.println("");
  //  Serial.print("NAS=");
  //  Serial.println(numberNoActionsSleeps);
  //  Serial.print("SES=");
  //  Serial.println(numberStopEngineSleeps);
  //  _delay_ms(100);
  socket12Mode = !isLowVoltage && (isEngineWork || isManualNagrevWork ||
                                   (!isEngineWork && (numberNoActionsSleeps * 8 < TIME_12V_WORK_AFTER_MANUAL_START_S) ||
                                    (!isEngineWork && (numberStopEngineSleeps * 8 < TIME_12V_WORK_AFTER_STOP_ENGINE_S))));
  digitalWrite(SOCKET12_PIN, socket12Mode);
}

void StopNagrev()
{
  isManualNagrevWork = false;
  //выключить нагрев
  sidNagrev1 = false;
  sidNagrev2 = false;
  sidMode1 = 0;
  sidMode2 = 0;
}

void NagrevControl()
{
  if (!isEngineWork)
  {
    if (!isManualNagrevWork || manualWorkStopEngine_ms > TIME_SID_WORK_WHEN_STOP_ENGINE_S * 1000)
    {
      //Serial.print("manualWorkStopEngine_ms ");
      //Serial.println(manualWorkStopEngine_ms);
      //_delay_ms(100);
      StopNagrev();
    }
  }
  else //isEngineWork
  {
    if (isJustStartedEngine)
    {
      isManualNagrevWork = false;
      //preset sidMode1 (if was not yet manual mode)
      if (sidMode1 == 0)
      {
        RefreshSensors();

        //        if (t_inn < T_AUTO_START_NAGR3)
        //          sidMode1 = 3;
        //        else if (t_inn < T_AUTO_START_NAGR2)
        //          sidMode1 = 2;
        //        else if (t_inn < T_AUTO_START_NAGR1)
        //          sidMode1 = 1;

        if (t_inn < T_AUTO_START_NAGR3)
          timeReduceNagrevLimit =  TIME_REDUCE_NAGREV3_S;
        else if (t_inn < T_AUTO_START_NAGR2)
          timeReduceNagrevLimit =  TIME_REDUCE_NAGREV2_S;
        else
          timeReduceNagrevLimit =  TIME_REDUCE_NAGREV1_S;

        if (t_inn < T_AUTO_START_NAGR1)
          sidMode1 = 3;

        sidNagrev1 = false;
        sidNagrev2 = false;
        thermoOff1_ms = PAUSE_NAGREV_S * 1000 + 1;
        thermoOff2_ms = PAUSE_NAGREV_S * 1000 + 1;
        timeReduceNagrevSid1_ms = 0;
        timeReduceNagrevSid2_ms = 0;
      }
    }
    else
    {
      if (sidMode1 > 1 && timeReduceNagrevSid1_ms > timeReduceNagrevLimit * 1000)
      {
        sidMode1 = 1;
      }
      if (sidMode2 > 1 && timeReduceNagrevSid2_ms > timeReduceNagrevLimit * 1000)
      {
        sidMode2 = 1;
      }
    }
  }

  if (sidNagrev1 && (sidMode1 == 0 || thermoOn1_ms > (sidMode1 == 1 ? TIME_NAGREV1_S : sidMode1 == 2 ? TIME_NAGREV2_S : TIME_NAGREV3_S) * 1000))
  {
    //Serial.println("sidNagrev1 = false");
    sidNagrev1 = false;
    thermoOff1_ms = 0;
  }
  else if (sidMode1 > 0 && !sidNagrev1 && thermoOff1_ms > PAUSE_NAGREV_S * 1000)
  {
    //Serial.println("sidNagrev1 = true");
    sidNagrev1 = true;
    thermoOn1_ms = 0;
  }

  if (sidNagrev2 && (sidMode2 == 0 || thermoOn2_ms > (sidMode2 == 1 ? TIME_NAGREV1_S : sidMode2 == 2 ? TIME_NAGREV2_S : TIME_NAGREV3_S) * 1000))
  {
    sidNagrev2 = false;
    thermoOff2_ms = 0;
  }
  else if (sidMode2 > 0 && !sidNagrev2 && thermoOff2_ms > PAUSE_NAGREV_S * 1000)
  {
    sidNagrev2 = true;
    thermoOn2_ms = 0;
  }

  //  Serial.println("");
  //  Serial.print("sidMode1=");
  //  Serial.println(sidMode1);
  //  Serial.print("thermoOn1_ms=");
  //  Serial.println(thermoOn1_ms);
  //  Serial.print("thermoOff1_ms=");
  //  Serial.println(thermoOff1_ms);
  //  Serial.print("isEngineWork=");
  //  Serial.println(isEngineWork);
  //  Serial.print("isManualNagrevWork=");
  //  Serial.println(isManualNagrevWork);
  //  Serial.print("sidNagrev1=");
  //  Serial.println(sidNagrev1);

  digitalWrite(TERMO_PIN1, sidNagrev1);
  digitalWrite(TERMO_PIN2, sidNagrev2);
}

void ShowMode()
{
  switch (sidMode1)
  {
    case 0:
      digitalWrite(LED_B_PIN1, false);
      digitalWrite(LED_G_PIN1, false);
      digitalWrite(LED_R_PIN1, false);
      break;
    case 1:
      digitalWrite(LED_B_PIN1, true);
      digitalWrite(LED_G_PIN1, false);
      digitalWrite(LED_R_PIN1, false);
      break;
    case 2:
      digitalWrite(LED_B_PIN1, false);
      digitalWrite(LED_G_PIN1, true);
      digitalWrite(LED_R_PIN1, false);
      break;
    case 3:
      digitalWrite(LED_B_PIN1, false);
      digitalWrite(LED_G_PIN1, false);
      digitalWrite(LED_R_PIN1, true);
      break;
  }
  switch (sidMode2)
  {
    case 0:
      digitalWrite(LED_B_PIN2, false);
      digitalWrite(LED_G_PIN2, false);
      digitalWrite(LED_R_PIN2, false);
      break;
    case 1:
      digitalWrite(LED_B_PIN2, true);
      digitalWrite(LED_G_PIN2, false);
      digitalWrite(LED_R_PIN2, false);
      break;
    case 2:
      digitalWrite(LED_B_PIN2, false);
      digitalWrite(LED_G_PIN2, true);
      digitalWrite(LED_R_PIN2, false);
      break;
    case 3:
      digitalWrite(LED_B_PIN2, false);
      digitalWrite(LED_G_PIN2, false);
      digitalWrite(LED_R_PIN2, true);
      break;
  }
}

void Wake()
{
  // cancel sleep as a precaution
  sleep_disable();
  // precautionary while we do other stuff
  detachInterrupt(0);
  detachInterrupt(1);
  // enable ADC
  ADCSRA = old_ADCSRA;

  buttonState = digitalRead(BTTN_PIN2) * 2 + digitalRead(BTTN_PIN1);
}

void GoSleep()
{
  // clear various "reset" flags
  MCUSR = 0;
  // allow changes, disable reset
  WDTCSR = bit(WDCE) | bit(WDE);
  // set interrupt mode and an interval
  //WDTCSR = bit(WDIE) | bit(WDP2) | bit(WDP1);    // set WDIE, and 1 second delay
  WDTCSR = bit(WDIE) | bit(WDP3) | bit(WDP0);    // set WDIE, and 8 seconds delay
  wdt_reset();  // pat the dog

  // disable ADC
  old_ADCSRA = ADCSRA;
  ADCSRA = 0;

  // turn off various modules
  byte old_PRR = PRR;
  PRR = 0xFF;

  // timed sequence coming up
  noInterrupts();

  // will be called when pin D2 goes low
  attachInterrupt(0, Wake, FALLING ); //FALLING RISING
  EIFR = bit(INTF0);  // clear flag for interrupt 0

  // will be called when pin D3 goes low
  attachInterrupt(1, Wake, FALLING);
  EIFR = bit(INTF1);  // clear flag for interrupt 1

  // ready to sleep
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // turn off brown-out enable in software
  MCUCR = bit(BODS) | bit(BODSE);
  MCUCR = bit(BODS);
  interrupts(); // guarantees next instruction executed
  sleep_cpu();

  // cancel sleep as a precaution
  sleep_disable();
  PRR = old_PRR;
  ADCSRA = old_ADCSRA;
}


//each wake up
void LowVoltageAlarm()
{
  digitalWrite(LED_R_PIN1, HIGH);
  digitalWrite(LED_R_PIN2, HIGH);
  _delay_ms(200);
  digitalWrite(LED_R_PIN1, LOW);
  digitalWrite(LED_R_PIN2, LOW);
}

void InfoSignal()
{
  //  Serial.print("isHalfBattery=");
  //  Serial.println(isHalfBattery);
  digitalWrite(isHalfBattery ? LED_B_PIN1 : LED_G_PIN1, HIGH);
  digitalWrite(isHalfBattery ? LED_B_PIN2 : LED_G_PIN2, HIGH);
  _delay_ms(200);
  digitalWrite(isHalfBattery ? LED_B_PIN1 : LED_G_PIN1, LOW);
  digitalWrite(isHalfBattery ? LED_B_PIN2 : LED_G_PIN2, LOW);
}

void loop()
{
  if (buttonState > 0)
  {
    if (buttonState & 2 >= 2)
    {
      ActionBtn(2, 1);
    }
    else
    {
      ActionBtn(1, 1);
    }
    buttonState = 0;
    _delay_ms(100);
  }
  else
  {
    switch (button1.Loop()) {
      case SB_CLICK:
        //Serial.println("Press button 1");
        ActionBtn(1, 1);
        break;
      case SB_LONG_CLICK:
        //Serial.println("Long press button 1");
        ActionBtn(1, 2);
        break;
    }
    switch (button2.Loop()) {
      case SB_CLICK:
        //Serial.println("Press button 2");
        ActionBtn(2, 1);
        break;
      case SB_LONG_CLICK:
        //Serial.println("Long press button 2");
        ActionBtn(2, 2);
        break;
    }
  }

  CheckEngineStatus();
  NagrevControl();
  ShowMode();
  Socket12Control();
  isJustStartedEngine = false;


  if (!isEngineWork && !isManualNagrevWork)
  {
    GoSleep(); //8 sec sleep
    // cancel sleep as a precaution
    sleep_disable();
    numberLowVoltageSleeps += 1;
    numberStopEngineSleeps += 1;
    numberNoActionsSleeps += 1;
    checkEngineStatus_ms = PERIOD_CHECK_ENGINE_STATUS_S * 1000 + 1; //as result, will check if engine still stop each sleep loop
  }
}

