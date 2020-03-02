//StopVoda+NRF
#include "sav_button.h" // Библиотека работы с кнопками
#include <elapsedMillis.h>
//#include "DHT.h"
//#include <Arduino.h>
#include <avr/wdt.h>

#define DHT_PIN       6
#define BTN_PIN       3
#define SOLENOID_PIN  8
#define BUZZ_PIN      4
#define VODA_PIN      2     //300Kom +5v
#define ONE_WIRE_PIN  5    // DS18b20


const boolean SOLENOID_NORMAL_OPENED = true;
const unsigned long MANUAL_OPEN_DURATION_SEC = 10800; //3 hours
const unsigned long MANUAL_CLOSE_DURATION_SEC = 10; //10s
const unsigned long CHECK_VODA_PERIOD_SEC = 3;
const unsigned long ALARM_INTERVAL_SEC = 2;

SButton btn(BTN_PIN, 50, 1000, 5000, 15000);

elapsedMillis CheckVoda_ms;
elapsedMillis manualOpenTime_ms;
elapsedMillis manualCloseTime_ms;
elapsedMillis alarmInterval_ms;


volatile float AvgVoda = 0;
volatile int Voda = 0;
volatile int Voda1 = 0;
volatile int Voda2 = 0;
volatile boolean closeVoda = false;
volatile int vodaMode = 0;  //0-open, 1-closed, 2-open temporary, 3-close temporary, for test
volatile boolean isAlarm = false;


void setup()
{
  // immediately disable watchdog timer so set will not get interrupted
  wdt_disable();

  //pinMode(VODA_PIN, INPUT_PULLUP);
  pinMode(VODA_PIN, INPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  Serial.begin(9600);   // Debugging only
  Serial.println("setup");

  // Инициация кнопок
  btn.begin();

  switchSolenoid(); // для открытия NormalOpen клапана

  // enable the watchdog timer. There are a finite number of timeouts allowed (see wdt.h).
  // Notes I have seen say it is unwise to go below 250ms as you may get the WDT stuck in a
  // loop rebooting.
  // The timeouts I'm most likely to use are:
  // WDTO_1S
  // WDTO_2S
  // WDTO_4S
  // WDTO_8S
  wdt_enable(WDTO_8S);
}


void CheckVoda()
{
  if (CheckVoda_ms > CHECK_VODA_PERIOD_SEC * 1000)
  {
    calcAvgVoda();
    if (vodaMode == 0 && AvgVoda == 1)
    {
      closeVoda = true;
      alarmInterval_ms = 0;
      vodaMode = 1;
      switchSolenoid();
    }
    CheckVoda_ms = 0;
  }
}

void VodaControl(int typeControl) //0 - auto check, 1-short click, 2-long click
{
  if (typeControl == 0)
  {
    if ((vodaMode == 2 && manualOpenTime_ms > MANUAL_OPEN_DURATION_SEC * 1000) || (vodaMode == 3 && manualCloseTime_ms > MANUAL_CLOSE_DURATION_SEC * 1000))
    {
      Serial.println(manualOpenTime_ms); //Reset
      vodaMode = 0;
      closeVoda = false;
      CheckVoda();
      switchSolenoid();
    }
  }
  else //manual
  {
    if (typeControl == 1)
    {
      if (vodaMode == 0)
      {
        vodaMode = 3; //temporary close voda (testing)
        manualCloseTime_ms = 0;
        closeVoda = true;
        alarmInterval_ms = 0;
        switchSolenoid();
      }
      else if (vodaMode == 1)
      {
        vodaMode = 2; //temporary open voda
        manualOpenTime_ms = 0;
        closeVoda = false;
        switchSolenoid();
      }
    }
    else // 2-long click, reset
    {
      vodaMode = 0;
      closeVoda = false;
      Voda1 = 0;
      Voda2 = 0;
      AvgVoda = 0;
      switchSolenoid();
    }
  }
}

void switchSolenoid()
{
  if (closeVoda)
    Serial.println("closeVoda");
  else
    Serial.println("openVoda");
  digitalWrite(SOLENOID_PIN, SOLENOID_NORMAL_OPENED ? !closeVoda : closeVoda);
}

//average voda for 3 last measures
void calcAvgVoda()
{
  Voda = !digitalRead(VODA_PIN);
  AvgVoda = (Voda2 + Voda1 + Voda) / 3;
  Voda2 = Voda1;
  Voda1 = Voda;
  Serial.print("Voda: ");
  Serial.println(Voda);
  Serial.print("VodaMode: ");
  Serial.println(vodaMode);
}

void AlarmControl()
{
  if (closeVoda)
  {
    if (alarmInterval_ms > ALARM_INTERVAL_SEC * 1000)
    {
      isAlarm = !isAlarm;
      alarmInterval_ms = 0;
    }
  }
  else
  {
    isAlarm = false;
  }
  digitalWrite(BUZZ_PIN, isAlarm);
}

void loop()
{
  switch (btn.Loop()) {
    case SB_CLICK:
      Serial.println("btnShort"); //temporary open voda if was closed, or close if was opened for testing
      VodaControl(1);
      break;
    case SB_LONG_CLICK:
      Serial.println("btnLong"); //Reset
      VodaControl(2);
      break;
  }
  VodaControl(0);
  CheckVoda();
  AlarmControl();
  wdt_reset();
}
