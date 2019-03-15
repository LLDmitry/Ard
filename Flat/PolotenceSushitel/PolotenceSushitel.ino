#include "sav_button.h" // Библиотека работы с кнопками
#include <EEPROM.h>
#include <elapsedMillis.h>
#include <ctype.h>

#define TERMO_PIN 13     // нагревательный элемент
#define BTTN_PIN 9
#define LED_RED_PIN 10
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12

unsigned long ON_BLINK = 100;  // milliseconds
unsigned long SHORT_PAUSE_BLINK = 100;
unsigned long MEDIUM_PAUSE_BLINK = 500;
unsigned long LONG_PAUSE_BLINK = 800;

unsigned long THERMO_ON_PERIOD = 3;  //minutes
unsigned long THERMO_LOW_OFF_PERIOD = 3; //minutes
unsigned long THERMO_MED_OFF_PERIOD = 2;
unsigned long THERMO_HIGH_OFF_PERIOD = 0;

SButton button1(BTTN_PIN, 50, 1000, 10000, 500);

elapsedMillis blinkOn;
elapsedMillis blinkOff;
elapsedMillis thermoOn;
elapsedMillis thermoOff;

enum Mode { OFF, SET, ON } mode;
byte TimeLevel = 0;
byte TermoLevel = 0;

int addressTermoLevel = 0;

bool TermoStatus = LOW;
bool ledOn = LOW;

const int SettingsModeDelay_ms = 5000; //msec

const int ShortTime_hours = 1;
const int MediumTime_hours = 3;
const int LongTime_hours = 8;

unsigned long lastChangeSettings_ms = 0;

void setup() {
  pinMode(TERMO_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);

  Serial.begin(9600);

  // Инициация кнопки
  button1.begin();

  // read a byte from the current address of the EEPROM
  TermoLevel = EEPROM.read(addressTermoLevel);
}

void ButtonClick()
{
  if (mode == 0)
  {
    mode = 1;
  }

  if (mode == 2)
  {
    TimeLevel = 0;
    mode = 0;
  }

  if (mode == 1)
  {
    TimeLevel = TimeLevel + 1;
    if (TimeLevel > 3) TimeLevel = 0;
    lastChangeSettings_ms = millis();
    blinkOff = ON_BLINK + 1;
  }
}

void ButtonLongClick() //изменение мощности, только в режие нагрева
{
  if (TimeLevel > 0)
  {
    TermoLevel = TermoLevel + 1;
    if (TermoLevel > 3) TermoLevel = 1;

    blinkOff = ON_BLINK + 1;  //for set blink On immediately
    EEPROM.update(addressTermoLevel, TermoLevel);
  }    
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
      delta = 1;  // 1 hour
      break;
    case 2:
      delta = 4;  // 4 hour
      break;
    case 3:
      delta = 8;  //8 hours
      break;
    }

    if ((ms - lastChangeSettings_ms) > (delta * 3600 * 1000))  //stop
    {
      TermoStatus == LOW;
      mode = 0;
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

  digitalWrite(TERMO_PIN, TermoStatus);
}

void ShowMode()
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
        digitalWrite(LED_GREEN_PIN, LOW);
        digitalWrite(LED_RED_PIN, LOW);
        break;
      case 2:
        digitalWrite(LED_BLUE_PIN, LOW);
        digitalWrite(LED_GREEN_PIN, ledOn);
        digitalWrite(LED_RED_PIN, LOW);
        break;
      case 3:
        digitalWrite(LED_BLUE_PIN, LOW);
        digitalWrite(LED_GREEN_PIN, LOW);
        digitalWrite(LED_RED_PIN, ledOn);
        break;
      }
    }
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
}
