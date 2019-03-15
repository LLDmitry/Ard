/*
  Управление обогревом либо по IR либо по кнопке
  При временном пропадании напряжения восстанавливает режим нагрева
  Отключается если прошло MAX_TERMO_PERIOD_ON_S с последнего сигнала от MOTION_SENSOR_PIN
*/

# include <IRremote.h>
# include <OneWire.h>
# include <DallasTemperature.h>
# include <EEPROM.h>
# include <sav_button.h>
# include <elapsedMillis.h>

const int ONE_WIRE_PIN = 9;
const int RECV_PIN = 8;
const int LED_PIN = 13;      // the number of the LED pin
const int TERMO_PIN = 7;
const int BZ_PIN = 6;      // signal
const int BTTN_PIN = 4;
const int MOTION_SENSOR_PIN = 5;

const int ADDR_MODE_TERMO = 1;  //1 or 2 or 3
const int ADDR_ON_OFF = 2;      //0 or 1

const String T_DOWN_IR_CODE = "38863bc2";  //T Down
const String T_UP_IR_CODE = "38863bca";  //T Up
const String ON_OFF_IR_CODE = "38863bf4";  //AV/TV

const unsigned long MAX_TERMO_PERIOD_ON_S = 80000;  //24 ч с последнего сигнала от MOTION_SENSOR_PIN
const unsigned long PERIOD_TERMO_CONTROL_S = 180;
const unsigned long PERIOD_SHOW_MODE_S = 10;
const int Termo1StartA = 17;
const int Termo1StopA = 18;
const int Termo2StartA = 20;
const int Termo2StopA = 21;
const int Termo3StartA = 21;
const int Termo3StopA = 22;

float TermoA;  //температура датчика A

boolean OnNagrevA = false;


elapsedMillis loopTimeTermo_ms;
elapsedMillis loopTimeShowMode_ms;
elapsedMillis loopTimeFromMotionSensor_ms;

int modeTermo = 0;  // 0-выкл  1-Termo1  2 Termo2  3 Termo3

OneWire ds(ONE_WIRE_PIN);
DeviceAddress tempDeviceAddress;
DallasTemperature sensors(&ds);

IRrecv irrecv(RECV_PIN);

decode_results results;

SButton button1(BTTN_PIN, 50, 500, 10000, 1000);

void setup()
{
  Serial.begin(9600);
  irrecv.enableIRIn(); // Start the receiver
  pinMode(TERMO_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BZ_PIN, OUTPUT);
  pinMode(BTTN_PIN, INPUT_PULLUP);
  pinMode(MOTION_SENSOR_PIN, INPUT);

  digitalWrite(TERMO_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, 10);
  //  sensors.getAddress(tempDeviceAddress, 1);
  //  sensors.setResolution(tempDeviceAddress, 12);

  button1.begin();

  delay(1000); //для уменьшения скачка напряжения при включении питания
  RestoreTermoMode(false); //при подаче питания восстановим modeTermo к-й был до выключения
  if (modeTermo == 0)
  {
    BuzzerPulse(-1);
  }
  else
  {
    BuzzerPulse(modeTermo);
  }
}

void RestoreTermoMode(bool switchOn)
{
  if (!switchOn)
  {
    modeTermo = EEPROM.read(ADDR_ON_OFF);
  }

  if (switchOn || modeTermo != 0) //"on"
  {
    modeTermo = EEPROM.read(ADDR_MODE_TERMO);
  }

  if (modeTermo < 0 || modeTermo > 2)
  {
    modeTermo = 1;
  }
  TermoControl();
}

void loop()
{
  switch (button1.Loop())
  {
    case SB_CLICK:
      loopTimeFromMotionSensor_ms = 0;
      Serial.println("Short press button");
      ActionBtn('S');
      break;
    case SB_LONG_CLICK:
      Serial.println("Long press button");
      ActionBtn('L');
      break;
  }

  if (irrecv.decode(&results))
  {
    loopTimeFromMotionSensor_ms = 0;
    String res = String(results.value, HEX);
    Serial.println(res);
    if (res == ON_OFF_IR_CODE)
    {
      if (modeTermo > 0)
      {
        modeTermo = 0;
      }
      else
      {
        modeTermo = 2;
      }
    }
    else if (res == T_DOWN_IR_CODE && modeTermo < 3)
    {
      modeTermo = modeTermo + 1;
    }
    else if (res == T_UP_IR_CODE)
    {
      if (modeTermo > 1)
      {
        modeTermo = modeTermo - 1;
      }
      else
      {
        modeTermo = 1;
      }
    }

    //   else if (res != "0") //защита от подбора кода
    //   {
    //      delay(10000);
    //   }

    if (res == ON_OFF_IR_CODE || res == T_DOWN_IR_CODE || res == T_UP_IR_CODE)
    {
      EEPROM.write(ADDR_MODE_TERMO, modeTermo);
      Serial.println(String(modeTermo));
      if (modeTermo == 0)
      {
        BuzzerPulse(-1);
      }
      else
      {
        BuzzerPulse(modeTermo);
      }

      TermoControl();  // применим новую установку температуры сразу по нажатию пульта
    }

    irrecv.resume(); // Receive the next value
  }

  unsigned long diffTime = 0;


  if (loopTimeTermo_ms >= PERIOD_TERMO_CONTROL_S * 1000)
  {
    TermoControl();
    loopTimeTermo_ms = 0;
  }

  if (loopTimeShowMode_ms >= PERIOD_SHOW_MODE_S * 1000)
  {
    if (modeTermo == 0)
    {
      LCDPulse(-1);
    }
    else
    {
      LCDPulse(modeTermo);
    }
    loopTimeShowMode_ms = 0;
  }

  MotionControl();
}

void MotionControl()
{
  if (modeTermo > 0 && digitalRead(MOTION_SENSOR_PIN))
  {
    loopTimeFromMotionSensor_ms = 0; //reset timer on each motion
  }

  if (modeTermo > 0 && loopTimeFromMotionSensor_ms > MAX_TERMO_PERIOD_ON_S * 1000)
  {
    modeTermo = 0;
    BuzzerPulse(-1);
    EEPROM.write(ADDR_ON_OFF, 0);
    TermoControl();
  }
}

void ActionBtn(char code)
{
  if (code == 'L') //long press, switch on/off
  {
    if (modeTermo == 0) //was off, do on
    {
      RestoreTermoMode(true);
      BuzzerPulse(modeTermo);
    }
    else //was on, do off
    {
      modeTermo = 0;
      BuzzerPulse(-1);
    }
    EEPROM.write(ADDR_ON_OFF, (modeTermo > 0 ? 1 : 0));
    TermoControl();
  }
  else if (modeTermo > 0) //short press, change T
  {
    modeTermo += 1;
    if (modeTermo > 3)
    {
      modeTermo = 1;
    }
    EEPROM.write(ADDR_MODE_TERMO, modeTermo);
    BuzzerPulse(modeTermo);
    TermoControl();
  }

}

void TermoControl()
{
  sensors.requestTemperatures();
  Serial.print("T1= ");
  //  TermoA = sensors.getTempCByIndx(0);
  //  Serial.println(String(TermoA);
  Serial.println(sensors.getTempCByIndex(0));
  //  Serial.print("T2= ");
  //sensors.getTempCByIndex(1) = sensors.getTempCByIndex(0);
  //  Serial.println(String(sensors.getTempCByIndex(1));
  //  Serial.println(sensors.getTempCByIndex(1));
  //  delay(600);

  switch (modeTermo)
  {
    case 0:
      OnNagrevA = false;
      break;
    case 1:
      OnNagrevA = (sensors.getTempCByIndex(0) < Termo1StartA || OnNagrevA && sensors.getTempCByIndex(0) < Termo1StopA);
      break;
    case 2:
      OnNagrevA = (sensors.getTempCByIndex(0) < Termo2StartA || OnNagrevA && sensors.getTempCByIndex(0) < Termo2StopA);
      break;
    case 3:
      OnNagrevA = (sensors.getTempCByIndex(0) < Termo3StartA || OnNagrevA && sensors.getTempCByIndex(0) < Termo3StopA);
      break;
  }
  digitalWrite(TERMO_PIN, OnNagrevA);
  digitalWrite(LED_PIN, (modeTermo > 0));
  //Serial.println(String(modeTermo));
  //Serial.println(String(OnNagrevA));
}

void BuzzerPulse(int bzMode)
{
  boolean longBz = (bzMode < 0);

  for (int i = 0; i < abs(bzMode); i++)
  {
    digitalWrite(BZ_PIN, HIGH);
    if (longBz)
    {
      delay(600);
    }
    else
    {
      delay(200);
    }
    digitalWrite(BZ_PIN, LOW);
    delay(400);
  }
}

void LCDPulse(int Mode)
{
  boolean longPulse = (Mode < 0);
  boolean reverse = (modeTermo > 0);

  for (int i = 0; i < abs(Mode); i++)
  {
    if (reverse)
    {
      digitalWrite(LED_PIN, LOW);
    }
    else
    {
      digitalWrite(LED_PIN, HIGH);
    }

    if (longPulse)
    {
      delay(1500);
    }
    else
    {
      delay(200);
    }
    if (reverse)
    {
      digitalWrite(LED_PIN, HIGH);
    }
    else
    {
      digitalWrite(LED_PIN, LOW);
    }
    delay(400);
  }
}
