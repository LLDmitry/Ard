#include "sav_button.h" // Библиотека работы с кнопками
#include <EEPROM.h>
#include <elapsedMillis.h>
#include <ctype.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

#define BTN_PIN         2
#define LED_RED_PIN     3
#define LED_GREEN_PIN   4
#define LED_BLUE_PIN    5
#define TERMO_PIN       6     // нагревательный элемент полотенц
#define POL_PIN         7     // нагревательный элемент пол 

unsigned long ON_BLINK = 100;  // milliseconds
unsigned long SHORT_PAUSE_BLINK = 100;
unsigned long MEDIUM_PAUSE_BLINK = 500;
unsigned long LONG_PAUSE_BLINK = 800;

unsigned long THERMO_ON_PERIOD = 3;  //minutes
unsigned long THERMO_LOW_OFF_PERIOD = 3; //minutes
unsigned long THERMO_MED_OFF_PERIOD = 2;
unsigned long THERMO_HIGH_OFF_PERIOD = 0;
unsigned long POL_ON_PERIOD = 60; //minutes

SButton button(BTN_PIN, 50, 1000, 10000, 500);

elapsedMillis blinkOn;
elapsedMillis blinkOff;
elapsedMillis thermoOn;
elapsedMillis thermoOff;
elapsedMillis polOn;

enum Mode { OFF, SET, ON, POL } mode; //0-OFF, 1-SET, 2-ON, 3-TermoPol
byte TimeLevel = 0;
byte TermoLevel = 0;

int addressTermoLevel = 0;
bool TermoStatus = LOW;
bool ledOn = LOW;
int addressTermoLevel_2 = 0;
bool TermoStatus_2 = LOW;
bool ledOn_2 = LOW;

const int SettingsModeDelay_ms = 5000; //msec

const int ShortTime_hours = 1;
const int MediumTime_hours = 3;
const int LongTime_hours = 8;
const int PolTime_hours = 3;

unsigned long lastChangeSettings_ms = 0;
unsigned long lastChangeSettings_2_ms = 0;

void setup() {
  pinMode(TERMO_PIN, OUTPUT);
  pinMode(POL_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);

  Serial.begin(9600);

  // Инициация кнопки
  button.begin();

  // read a byte from the current address of the EEPROM
  TermoLevel = EEPROM.read(addressTermoLevel);
  Serial.println("Start");
  wdt_enable(WDTO_8S);
}

void ButtonClick()
{
  if (mode == 0)
  {
    mode = 1;
    TimeLevel = 1;
    lastChangeSettings_ms = millis();
  }
  else if (mode == 1)
  {
    TimeLevel += 1;
    if (TimeLevel > 3) TimeLevel = 0;
    lastChangeSettings_ms = millis();
    blinkOff = ON_BLINK + 1;
  }
  else if (mode == 2)
  {
    TimeLevel = 0;
    mode = 0;
  }
  else if (mode == 3)
  {
    mode = 0;
  }
  Serial.print("ButtonClick mode= ");
  Serial.println(mode);
}

void ButtonLongClick()
{
  if (TimeLevel > 0) //изменение мощности, только в режие нагрева
  {
    TermoLevel += 1;
    Serial.print("ButtonLongClick TermoLevel+= ");
    Serial.println(TermoLevel);
    if (TermoLevel > 3) TermoLevel = 1;

    blinkOff = ON_BLINK + 1;  //for set blink On immediately
    EEPROM.update(addressTermoLevel, TermoLevel);
  }
  else //on/off теплого пола
  {
    if (mode == 0)
      mode = 3;
    else if (mode == 3)
      mode = 0;
    if (mode == 3) polOn = 0;
  }
  Serial.print("ButtonLongClick TermoLevel= ");
  Serial.println(TermoLevel);
  Serial.print("ButtonLongClick PolStatus= ");
  Serial.println(mode);
}

void TermoControl()
{
  unsigned long delta;
  unsigned long ms = millis();

  if (mode == 2)
  {
    switch (TimeLevel)
    {
      case 1:
        delta = ShortTime_hours;  // 1 hour
        break;
      case 2:
        delta = MediumTime_hours;  // 4 hour
        break;
      case 3:
        delta = LongTime_hours;  //8 hours
        break;
    }

    if ((ms - lastChangeSettings_ms) > (delta * 3600 * 1000))  //stop
    {
      TermoStatus == LOW;
      mode = 0;
      Serial.println("lastChangeSettings_ms reset mode");
    }
    else  //continue work
    {
      if (TermoStatus == LOW) //проверить и включить
      {
        unsigned long pauseTermo = (TermoLevel == 1 ? THERMO_LOW_OFF_PERIOD : TermoLevel == 2 ? THERMO_MED_OFF_PERIOD : TermoLevel == 3 ? THERMO_HIGH_OFF_PERIOD : 0) * 60 * 1000;
        if (thermoOff > pauseTermo)
        {
          TermoStatus = HIGH;
          thermoOn = 0;
        }
      }
      else  // проверить и выключить
      {
        if (thermoOn > (THERMO_ON_PERIOD * 60 * 1000))
        {
          TermoStatus = LOW;
          thermoOff = 0;
        }
      }
    }
  }
  else
  {
    if (mode == 0)
    {
      TermoStatus = LOW;
    }
  }

  if (mode == 3 && polOn > (POL_ON_PERIOD * 60 * 1000))
  {
    mode = 0;
  }

  Serial.print("TermoStatus ");
  Serial.println(TermoStatus);
  //_delay_ms(20);
  digitalWrite(TERMO_PIN, TermoStatus);
  digitalWrite(POL_PIN, mode == 3);
}

void ShowMode()
{
  Serial.print("ledOn=");
  Serial.println(ledOn);
  Serial.print("mode=");
  //_delay_ms(50);
  Serial.println(mode);
  // Serial.println("blinkOn=" + blinkOn);
  //_delay_ms(50);
  Serial.print("TimeLevel=");
  Serial.println(TimeLevel);
  //_delay_ms(50);

  if (mode == 3)
  {
    ledOn = HIGH;
    digitalWrite(LED_BLUE_PIN, LOW);
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_RED_PIN, HIGH);
  }
  else
  {
    if (ledOn)
    {
      if (mode == 0 || blinkOn > ON_BLINK)
      {
        blinkOff = 0;
        ledOn = LOW;

        digitalWrite(LED_BLUE_PIN, LOW);
        digitalWrite(LED_GREEN_PIN, LOW);
        digitalWrite(LED_RED_PIN, LOW);
      }
    }
    else if (mode > 0)
    {
      unsigned long calcBlinkOff = (TermoLevel == 1 ? LONG_PAUSE_BLINK : TermoLevel == 2 ? MEDIUM_PAUSE_BLINK : TermoLevel == 3 ? SHORT_PAUSE_BLINK : 0);
      if (TimeLevel > 0 && blinkOff > calcBlinkOff)
      {
        blinkOn = 0;
        ledOn = HIGH;
        switch (TimeLevel)
        {
          case 1:
            digitalWrite(LED_BLUE_PIN, ledOn);
            Serial.println("LED_BLUE_PIN");
            //_delay_ms(20);
            digitalWrite(LED_GREEN_PIN, LOW);
            digitalWrite(LED_RED_PIN, LOW);
            break;
          case 2:
            digitalWrite(LED_BLUE_PIN, LOW);
            digitalWrite(LED_GREEN_PIN, ledOn);
            Serial.println("LED_GREEN_PIN");
            //_delay_ms(20);
            digitalWrite(LED_RED_PIN, LOW);
            break;
          case 3:
            digitalWrite(LED_BLUE_PIN, LOW);
            digitalWrite(LED_GREEN_PIN, LOW);
            digitalWrite(LED_RED_PIN, ledOn);
            Serial.println("LED_RED_PIN");
            //_delay_ms(20);
            break;
        }
      }
    }
  }
}

void loop()
{
  wdt_reset();
  switch (button.Loop()) {
    case SB_CLICK:
      Serial.println("SB_CLICK++++++++++++++++");
      //_delay_ms(20);
      ButtonClick();
      break;
    case SB_LONG_CLICK:
      Serial.println("ButtonLongClick+++++++++++++");
      //_delay_ms(20);
      ButtonLongClick();
      break;
      //case SB_AUTO_CLICK:
      ////Serial.println("");
      ////Serial.println("Auto press button 1");
      //ButtonClick();
      //break;
  }

  //Reset SettingMode in 5 sec
  if (mode == 1)
  {
    if (millis() - lastChangeSettings_ms > 5000)
    {
      if (TimeLevel == 0)
      {
        Serial.println("TimeLevel reset mode");
        mode = 0;
      }
      else
      {
        mode = 2;
        thermoOff = 99999999;
      }
    }
  }

  TermoControl();
  ShowMode();
  ////_delay_ms(200);
}
