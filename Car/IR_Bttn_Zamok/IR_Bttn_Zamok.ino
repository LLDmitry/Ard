
//IR_Bttn_Zamok

// принимает сигналы при включенном зажигании и в течении 5 мин после выключения. Остальное время спит. При зависании на 8 сек, делаем reset
// управление по IR или по кнопке (BTN_OPEN_CODE, BTN_CLOSE_CODE где < 500ms короткое нажатие; >500 - длинное)
// при приеме неправильного кода - пауза 5сек
#include <IRremote.h>
#include "sav_button.h"
#include <elapsedMillis.h>

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>

const int IGNITION_PIN = 2; //(обязательно = 2 т.к. attachInterrupt(0,...)
const int BTTN_PIN = 4;
const int BZZ_PIN = 5;
const int OPEN_PIN = 6;
const int CLOSE_PIN = 7;
const int IR_RECV_PIN = 9;

const String IR_OPEN_CODE = "38863bc2";
const String IR_CLOSE_CODE = "38863bca";
const String BTN_OPEN_CODE = "sls"; //s-short, l - long click
const String BTN_CLOSE_CODE = "lss"; //s-short, l - long click
const String BTN_SETUP_CODE = "sss"; //s-short, l - long click
const int IMPULSE_ZAMOK_MS = 500;

const unsigned long DELAY_PERIOD_S = 1;
const unsigned long PERIOD_READ_BTTN_S = 5; //c первого click в течении этого времени набираем код, затем проверяем
const unsigned long AFTER_SWITCH_OFF_PERIOD_S = 300; //на прием сигналов управления после отключения зажигания

boolean isProcessBtnCode;
boolean isActiveWork = true;
boolean prevIgnitionStatus = false;
String resultBtnCode;
boolean setupMode = false;
String setupPart1 = "";
String setupPart2 = "";

elapsedMillis readBttn_ms;
elapsedMillis afterSwitchOff_ms;

IRrecv irrecv(IR_RECV_PIN);

decode_results results;

SButton button1(BTTN_PIN, 50, 500, 10000, 1000);

void setup()
{
  Serial.begin(9600);
  irrecv.enableIRIn(); // Start the receiver
  pinMode(BZZ_PIN, OUTPUT);
  pinMode(OPEN_PIN, OUTPUT);
  pinMode(CLOSE_PIN, OUTPUT);
  pinMode(BTTN_PIN, INPUT_PULLUP);
  pinMode(IGNITION_PIN, INPUT_PULLUP);

  button1.begin();

  _delay_ms(1000);
  //ZamokClose(); // при подаче напряжения не меняем состояние

  wdt_enable(WDTO_8S);
}


void PrepareSleep()
{
  // все пины на выход и в низкий уровень (закоментарил чтобы после просыпания работал SoftSerial)
  //  for (byte i = 0; i <= A7; i++) {
  //    pinMode(i, OUTPUT);
  //    digitalWrite(i, LOW);
  //  }
  // установливаем на пине с кнопкой подтяжку к VCC
  // устанавливаем обработчик прерывания INT0
  pinMode(BTTN_PIN, INPUT_PULLUP);
  pinMode(IGNITION_PIN, INPUT_PULLUP);
  attachInterrupt(0, WakeUp, RISING);

  wdt_disable();
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

  isActiveWork = true;
}

void loop()
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

  if (irrecv.decode(&results))
  {
    String res = String(results.value, HEX);
    Serial.println(res);
    if (setupMode)
    {
      if (res == "38863bda") // && setupPart1 == "" && setupPart2 == "")
      {
        Serial.println("YES!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        Serial.println(setupPart1);
        Serial.println(setupPart2);
        digitalWrite(BZZ_PIN, HIGH);
        _delay_ms(500);
        digitalWrite(BZZ_PIN, LOW);
        setupPart1 = "";
        setupPart2 = "";
      }
      else
      {
        setupPart1 = setupPart2;
        setupPart2 = res;
      }
      if (res == IR_OPEN_CODE)
      {
        ZamokOpen();
        ChangeSetupMode(); //проверим и сразу выйдем из сетапа
      }
    }
    else //normal mode
    {
      if (res == IR_OPEN_CODE)
      {
        _delay_ms(DELAY_PERIOD_S * 1000);
        ZamokOpen();
      }
      else if (res == IR_CLOSE_CODE)
      {
        _delay_ms(DELAY_PERIOD_S * 1000);
        ZamokClose();
      }
      else if (res != "0") //wrong code, do pause
      {
        _delay_ms(5000);
      }
    }

    irrecv.resume(); // Receive the next value
  }

  //analise resulting bttn code
  if (isProcessBtnCode && readBttn_ms > PERIOD_READ_BTTN_S * 1000)
  {
    if (resultBtnCode == BTN_CLOSE_CODE)
      ZamokClose();
    else if (resultBtnCode == BTN_OPEN_CODE)
      ZamokOpen();
    else if (resultBtnCode == BTN_SETUP_CODE)
      ChangeSetupMode();
    else//wrong code, do pause
    {
      Serial.println("wrong");
      digitalWrite(BZZ_PIN, HIGH);
      _delay_ms(5000);
      digitalWrite(BZZ_PIN, LOW);
    }
    isProcessBtnCode = false;
  }

  if (digitalRead(IGNITION_PIN))
  {
    isActiveWork = true;
    prevIgnitionStatus = true;
    Serial.println("IGNITION ON");
  }
  else
  {
    if (prevIgnitionStatus)
    {
      Serial.println("IGNITION Off");
      prevIgnitionStatus = false;
      afterSwitchOff_ms = 0;
    }
    if (afterSwitchOff_ms > AFTER_SWITCH_OFF_PERIOD_S * 1000)
      isActiveWork = false; //reset flag in the end of work
  }

  if (!isActiveWork)
  {
    PrepareSleep();
    Serial.println("DoSleep");
    _delay_ms(20);
    DoSleep();
    //Serial.flush();
    _delay_ms(100);
    Serial.println("ExitSleep");
    wdt_enable(WDTO_8S);
  }
  wdt_reset();
}

void ActionBtn(char code)
{
  if (!isProcessBtnCode)
  {
    isProcessBtnCode = true;
    readBttn_ms = 0;
    resultBtnCode = "";
  }
  resultBtnCode += code;
  Serial.println(resultBtnCode);
}

void ZamokClose()
{
  Serial.println("ZamokClose");
  digitalWrite(CLOSE_PIN, HIGH);
  _delay_ms(IMPULSE_ZAMOK_MS);
  digitalWrite(CLOSE_PIN, LOW);

  digitalWrite(BZZ_PIN, HIGH);
  _delay_ms(600);
  digitalWrite(BZZ_PIN, LOW);
}

void ZamokOpen()
{
  Serial.println("ZamokOpen");
  digitalWrite(OPEN_PIN, HIGH);
  _delay_ms(IMPULSE_ZAMOK_MS);
  digitalWrite(OPEN_PIN, LOW);

  digitalWrite(BZZ_PIN, HIGH);
  _delay_ms(300);
  digitalWrite(BZZ_PIN, LOW);
  _delay_ms(300);
  digitalWrite(BZZ_PIN, HIGH);
  _delay_ms(300);
  digitalWrite(BZZ_PIN, LOW);
}


void ChangeSetupMode()
{
  Serial.println("ZamokClose");
  digitalWrite(CLOSE_PIN, HIGH);
  _delay_ms(IMPULSE_ZAMOK_MS);
  digitalWrite(CLOSE_PIN, LOW);
  setupMode = !setupMode;

  digitalWrite(BZZ_PIN, HIGH);
  if (setupMode)
    _delay_ms(1000);
  else
    _delay_ms(200);
  digitalWrite(BZZ_PIN, LOW);
}
