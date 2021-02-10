//Для управления (с дисплеем и кнопками) температурой в каждой комнате и связи с CentralControl.
//Также передает сигнал тревоги в CentralControl. Для некоторых комнат включает сигнал тревоги
//попеременно показывает iInn+tSet и tOut

#include <EEPROM.h>
#include <NrfCommandsESP32.h> // C:\Program Files (x86)\Arduino\libraries\NrfCommandsESP32
#include "sav_button.h" // Библиотека работы с кнопками
#include <elapsedMillis.h>
#include <Arduino.h>
#include <avr/wdt.h>
#include <RF24.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

#include <Wire.h>
//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>
#include <Adafruit_ssd1306syp.h>
#include <SPI.h>

#define BTN_P_PIN       3
#define BTN_M_PIN       4
#define HEATER_PIN      8
#define BUZZ_PIN        9
#define PIR_SENSOR_PIN  2
#define ONE_WIRE_PIN    5    // DS18b20

//RNF
#define RNF_CE_PIN      6
#define RNF_CSN_PIN     7
#define RNF_MOSI        11
#define RNF_MISO        12
#define RNF_SCK         13

#define SDA_PIN         A4  //(SDA) I2C
#define SCL_PIN         A5  //(SCK) I2C

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

//const byte ROOM_NUMBER = ROOM_GOST;
const byte ROOM_NUMBER = ROOM_BED;

const unsigned long ALARM_INTERVAL_S = 2;
const unsigned long REFRESH_SENSOR_INTERVAL_S = 20;  //1 мин
const unsigned long READ_COMMAND_NRF_INTERVAL_S = 1;
const unsigned long CHECK_ALARM_SENSOR_PERIOD_S = 10;
const unsigned long SHOW_TMP_INN_S = 10;
const unsigned long SHOW_TMP_OUT_S = 3;

const int EEPROM_ADR_SET_TEMP = 1023; //last address in eeprom for store tSet
const float T_SET_MIN = 1.0f;  //0.0f - off
const float T_SET_MAX = 25.0f;

RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);

SButton btnP(BTN_P_PIN, 50, 1000, 5000, 15000);
SButton btnM(BTN_M_PIN, 50, 1000, 5000, 15000);

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress InnTempDeviceAddress;
DeviceAddress OutTempDeviceAddress;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_ssd1306syp display(SDA_PIN, SCL_PIN);
//Adafruit_SSD1306 display(-1);

elapsedMillis checkAlarmSensor_ms;
elapsedMillis alarmInterval_ms;
elapsedMillis lastRefreshSensor_ms = REFRESH_SENSOR_INTERVAL_S * 1000 + 1;
elapsedMillis readCommandNRF_ms = 0;
elapsedMillis displayMode_ms = 0;

bool alarmSensor = false;
float t_inn = 0.0f;
float t_out = 0.0f;
float t_outThisRoom = 0.0f;
byte t_outInt = 0;
byte t_outSign = '+';
byte t_outDec = 0;
boolean heaterStatus = false;
boolean isAlarmFromCenter = false;
boolean t_setChangedInRoom = false;
byte alarmMaxStatusFromCenter = 0;
byte alarmMaxStatusRoomFromCenter = 0;
enum enDisplayMode { DISPLAY_AUTO, DISPLAY_INN_TMP, DISPLAY_OUT_TMP, DISPLAY_ALARM };
enDisplayMode displayMode = DISPLAY_OUT_TMP;

NRFResponse nrfResponse;
NRFRequest nrfRequest;

bool t_set_on = false;
byte t_set = 4; //set_temp[4-1]
byte set_temp [5] = {3, 15, 21, 22, 23};

void setup()
{
  // immediately disable watchdog timer so set will not get interrupted
  wdt_disable();

  pinMode(PIR_SENSOR_PIN, INPUT);
  pinMode(BTN_P_PIN, INPUT_PULLUP);
  pinMode(BTN_M_PIN, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT);
  pinMode(HEATER_PIN, OUTPUT);
  Serial.begin(9600);   // Debugging only
  Serial.println("setup");

  // Инициация кнопок
  btnP.begin();
  btnM.begin();

  // RF24
  radio.begin();                          // Включение модуля;
  delay(2);
  radio.enableAckPayload();                     // Allow optional ack payloads
  //radio.enableDynamicPayloads();                // Ack payloads are dynamic payloads
  radio.setPayloadSize(32); //18
  radio.setChannel(ArRoomsChannelsNRF[ROOM_NUMBER]);            // Установка канала вещания;
  radio.setRetries(0, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(CentralReadingPipe);     // Активация данных для отправки
  radio.openReadingPipe(1, RoomReadingPipe);   // Активация данных для чтения
  radio.startListening();

  radio.printDetails();

  sensors.begin();

  sensors.getAddress(OutTempDeviceAddress, 0);
  sensors.getAddress(InnTempDeviceAddress, 1);

  sensors.setResolution(InnTempDeviceAddress, 12);
  sensors.setResolution(OutTempDeviceAddress, 12);

  //b  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64) 0x3C
  //b  display.clear();
  display.initialize();

  //SaveTSetsEEPROM(); //1st time
  RestoreFromEEPROM();

  wdt_enable(WDTO_8S);
}

void RestoreFromEEPROM()
{
  EEPROM.get(EEPROM_ADR_SET_TEMP, t_set);
  EEPROM.get(EEPROM_ADR_SET_TEMP + 1, t_set_on);
  for (int s = 1; s <= 5; s++)
  {
    EEPROM.get(EEPROM_ADR_SET_TEMP + 1 + s, set_temp[s - 1]);
  }
}

void PrepareCommandNRF()
{
  nrfResponse.Command = RSP_INFO;
  nrfResponse.roomNumber = ROOM_NUMBER;
  nrfResponse.alarmType = alarmSensor ? ALR_MOTION : ALR_NO;
  nrfResponse.tInnSet = t_set;

  float cc1;
  nrfResponse.tInnDec = modff(abs(t_inn), &cc1) * 10;
  nrfResponse.tInn = (int)cc1;
  nrfResponse.tInnSign = t_inn < 0 ? '-' : '+';

  // если подключен внешний датчик
  nrfResponse.tOutDec = modff(t_outThisRoom, &cc1);
  nrfResponse.tOut = (int)cc1;
  nrfResponse.tOutSign = t_outThisRoom < 0 ? '-' : '+';

  nrfResponse.co2 = random(200); // если подключен датчик
  //nrfResponse.addParam1 = t_hot; // если подключен датчик
  //nrfResponse.h= h_v;  // если подключен датчик

  uint8_t f = radio.flush_tx();
  radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
}

void RefreshSensorData()
{
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    Serial.println("RefreshSensorData");
    //    sensors.requestTemperatures();
    //    t_inn = sensors.getTempC(InnTempDeviceAddress);
    t_inn = (float)random(150) / 10.0; //debug
    //    t_outThisRoom = sensors.getTempC(OutTempDeviceAddress);
    //    //t_hot = sensors.getTempC(HotVodaTempDeviceAddress);
    //

    PrepareCommandNRF();
    HeaterControl();
    lastRefreshSensor_ms = 0;
  }
}

void CheckAlarmSensor()
{
  if (checkAlarmSensor_ms > CHECK_ALARM_SENSOR_PERIOD_S * 1000)
  {
    if (digitalRead(PIR_SENSOR_PIN))
      alarmSensor = true; //reset only after succesful send response.Alarm (чтобы не сбросить тревогу до отправки в CentralControl)
    checkAlarmSensor_ms = 0;
  }
}

void AlarmSignal()
{
  digitalWrite(BUZZ_PIN, isAlarmFromCenter);
}

//Get Command
void ReadCommandNRF()
{
  _delay_ms(10);
  if (readCommandNRF_ms > READ_COMMAND_NRF_INTERVAL_S * 1000)
  {
    _delay_ms(10);
    if (radio.available())
    {
      int cntAvl = 0;
      radio.read(&nrfRequest, sizeof(nrfRequest));
      delay(20);
      Serial.print("radio.read: ");
      Serial.println(nrfRequest.roomNumber);
      HandleInputNrfCommand();
      doAfterRead();
    }
    readCommandNRF_ms = 0;
  }
}

void doAfterRead()
{
  radio.flush_rx();
  if (alarmSensor)
    alarmSensor = false;
}

void HandleInputNrfCommand()
{
  if (nrfRequest.roomNumber == ROOM_NUMBER) //вообще-то проверка не нужна - из-за разных каналов должно придти только этой Room
  {
    //    Serial.println(600 + nrfRequest.p);
    //    Serial.println(nrfRequest.Command);
    //    Serial.println(nrfRequest.minutes);
    //    Serial.println(nrfRequest.tOut);
    //    Serial.println(nrfRequest.tInnSet);

    t_outSign = nrfRequest.tOutSign;
    t_outInt = nrfRequest.tOut;
    t_outDec = nrfRequest.tOutDec;
    t_out = t_outInt + ((float)t_outDec / 10.0f);
    if (t_outSign == '-')
      t_out = -t_out;

    if (nrfRequest.tInnSet < 100 && t_set != nrfRequest.tInnSet && !t_setChangedInRoom) //if t_setChangedInRoom, will ignore obsolete data from centralControl
    {
      t_set = nrfRequest.tInnSet;
      t_set_on = t_set > 0;
      SaveTSetEEPROM();
      HeaterControl();
    }
    t_setChangedInRoom = false; //сбросим признак чтобы в следующий прием данных принять их от centralControl
    //    Serial.print("t_out= ");
    //    Serial.println(t_out);
    //    Serial.print("t_set= ");
    //    Serial.println(t_set);
    //    Serial.print("tInnSetVal1= ");
    //    Serial.println(nrfRequest.tInnSetVal1);

    if (nrfRequest.tInnSetVal1 <= 30 && set_temp[0] != nrfRequest.tInnSetVal1 ||
        nrfRequest.tInnSetVal2 <= 30 && set_temp[1] != nrfRequest.tInnSetVal2 ||
        nrfRequest.tInnSetVal3 <= 30 && set_temp[2] != nrfRequest.tInnSetVal3 ||
        nrfRequest.tInnSetVal4 <= 30 && set_temp[3] != nrfRequest.tInnSetVal4 ||
        nrfRequest.tInnSetVal5 <= 30 && set_temp[4] != nrfRequest.tInnSetVal5)
    {
      if (nrfRequest.tInnSetVal1 <= 30)
        set_temp[0] = nrfRequest.tInnSetVal1;
      if (nrfRequest.tInnSetVal2 <= 30)
        set_temp[1] = nrfRequest.tInnSetVal2;
      if (nrfRequest.tInnSetVal3 <= 30)
        set_temp[2] = nrfRequest.tInnSetVal3;
      if (nrfRequest.tInnSetVal4 <= 30)
        set_temp[3] = nrfRequest.tInnSetVal4;
      if (nrfRequest.tInnSetVal5 <= 30)
        set_temp[4] = nrfRequest.tInnSetVal5;

      SaveTSetsEEPROM();
      HeaterControl();
    }

    if (nrfRequest.alarmMaxStatus > 0) //central send it to this room
    {
      isAlarmFromCenter = true;  //reset only by button
      alarmMaxStatusFromCenter = nrfRequest.alarmMaxStatus;
      alarmMaxStatusRoomFromCenter = nrfRequest.alarmMaxStatusRoom;
      DisplayData(DISPLAY_ALARM);
    }
  }
}


byte ConvertToByte(byte param, float val) //0..255
{
  switch (param)
  {
    case 1: //T set от 0 до +25
      return ((byte)(val * 10));
      break;
    case 2: //T inn  от -32 до +32
      return ((byte)((32 + val) * 4));
      break;
  }
}

float ConvertFromByte(byte param, byte val)
{
  switch (param)
  {
    case 1: //T set от 0 до +25
      return ((float)(val / 10));
      break;
  }
}

void SaveTSetEEPROM()
{
  Serial.print("SaveTSetEEPROM:");
  Serial.println(t_set);
  EEPROM.put(EEPROM_ADR_SET_TEMP, t_set);
  EEPROM.put(EEPROM_ADR_SET_TEMP + 1, t_set_on);
}

void SaveTSetsEEPROM()
{
  for (int s = 1; s <= 5; s++)
  {
    EEPROM.put(EEPROM_ADR_SET_TEMP + 1 + s, set_temp[s - 1]);
  }
}

void HeaterControl()
{
  Serial.println("HeaterControl");
  heaterStatus =  (t_set_on && set_temp[t_set - 1] > t_inn);
  digitalWrite(HEATER_PIN, heaterStatus);
  //DisplayData(DISPLAY_INN_TMP);
}

void ChangeSetTemp(int sign)
{

  if (isAlarmFromCenter)  //reset alarm info by any button
  {
    isAlarmFromCenter = false;
    alarmMaxStatusFromCenter = 0;
    alarmMaxStatusRoomFromCenter = 0;
    exit;
  }

  if (sign == 1)
  {
    t_set_on = true;
    if (t_set < T_SET_MAX)
      t_set += 1;
  }
  else if (sign == -1)
  {
    if (t_set > 1)
    {
      t_set_on = true;
      t_set -= 1;
    }
    else
      t_set_on = false;
  }
  else if (sign == "!")
    t_set_on = !t_set_on;

  t_setChangedInRoom = true;
  SaveTSetEEPROM();
  HeaterControl();
  PrepareCommandNRF();
}

void CheckButtons()
{
  switch (btnP.Loop()) {
    case SB_CLICK:
      Serial.println("btnPShort");
      ChangeSetTemp(1);
      break;
    case SB_LONG_CLICK:
      Serial.println("btnPLong");
      ChangeSetTemp("!");
      break;
  }
  switch (btnM.Loop()) {
    case SB_CLICK:
      Serial.println("btnMShort");
      ChangeSetTemp(-1);
      break;
    case SB_LONG_CLICK:
      Serial.println("btnMLong");
      ChangeSetTemp("!");
      break;
  }
}

void DisplayData(enDisplayMode toDisplayMode)
{
  if (isAlarmFromCenter)
    toDisplayMode = DISPLAY_ALARM;

  if (toDisplayMode != DISPLAY_AUTO || displayMode == DISPLAY_INN_TMP && displayMode_ms > SHOW_TMP_INN_S * 1000 || displayMode == DISPLAY_OUT_TMP && displayMode_ms > SHOW_TMP_OUT_S * 1000)
  {
    if (toDisplayMode == DISPLAY_AUTO)
    {
      if (displayMode == DISPLAY_INN_TMP)
        displayMode = DISPLAY_OUT_TMP;
      else
        displayMode = DISPLAY_INN_TMP;
    }
    else
    {
      if (toDisplayMode = DISPLAY_INN_TMP)
      {
        displayMode = DISPLAY_INN_TMP;
      }
    }
    if (toDisplayMode == DISPLAY_AUTO)
      displayMode_ms = 0;

    //display.setFont(&FreeSerif9pt7b);
    display.clear();
    display.update();
    _delay_ms(10);

    if (displayMode == DISPLAY_INN_TMP)
    {
      byte lngth = CalculateIntSymbolsLength((int)t_inn, t_inn < 0 ? '-' : '+');
      byte startX = CalculateStartX(lngth, true);
      if (t_inn < 0)
      {
        display.setTextSize(1);
        display.setCursor(startX, 30);
        display.println("-");
      }

      display.setTextSize(4);
      display.setTextColor(WHITE);

      display.setCursor(CalculateIntX(startX, t_inn < 0 ? '-' : '+'), 20);
      display.print(abs((int)t_inn));
      display.setCursor(CalculateDecX(startX, lngth, t_inn < 0 ? '-' : '+'), 33);
      display.setTextSize(2);
      display.print(".");
      display.println(abs((int)((t_inn - (int)t_inn) * 10)));

      //display set temp
      display.drawLine(81, 0, 81, display.height() - 1, WHITE);
      if (t_set_on)
      {
        byte x, y;
        if ((int)set_temp[t_set - 1] < 10)
          x = 100;
        else
          x = 88;
        display.setTextSize(3);
        if (t_inn < (float)set_temp[t_set - 1] - 0.5)
          y = 6;
        else if (t_inn > (float)set_temp[t_set - 1] + 0.5)
          y = 42;
        else
          y = 22;
        display.setCursor(x, y);
        display.println((int)set_temp[t_set - 1]);
      }
      else
      {
        display.setCursor(90, 35);
        display.println("OFF");
      }
    }
    else if (displayMode == DISPLAY_OUT_TMP)
    {
      //внешняя температура - внутри рамки
      display.setTextSize(4);
      byte i = 1;
      display.drawRect(i, i, display.width() - 2 * i, display.height() - 2 * i, WHITE);
      byte lngth = CalculateIntSymbolsLength(t_outInt, t_outSign);
      Serial.print("lngth= ");
      Serial.println(lngth);
      byte startX = CalculateStartX(lngth, false);
      Serial.print("startX= ");
      Serial.println(startX);
      if (t_outSign == '-')
      {
        display.setCursor(startX, 30);
        display.setTextSize(2);
        display.print("-");
      }
      display.setTextSize(4);
      display.setCursor(CalculateIntX(startX, t_outSign), 20);
      display.print(abs(t_outInt));
      display.setTextSize(3);
      display.setCursor(CalculateDecX(startX, lngth, t_outSign), 26);
      display.print(".");
      display.println(t_outDec);
    }
    else if (displayMode == DISPLAY_ALARM)
    {
      display.setTextSize(3);
      display.setCursor(100, 10);
      display.println("ALARM");
      display.setCursor(100, 25);
      display.println(alarmMaxStatusFromCenter);
      display.setCursor(100, 50);
      display.println(alarmMaxStatusRoomFromCenter);
    }
    display.update();
    delay(200);
  }
}

byte CalculateStartX(byte lngth, bool isInnerTemp)
{
  switch (lngth) {
    case 5:
      return isInnerTemp ? 24 : 40;
    case 6:
      return isInnerTemp ? 22 : 38;
    case 7:
      return isInnerTemp ? 15 : 30;
    case 8:
      return isInnerTemp ? 10 : 22;
    case 10:
      return isInnerTemp ? 7 : 18;
    case 11:
      return isInnerTemp ? 5 : 16;
    case 12:
      return isInnerTemp ? 3 : 14;
    case 13:
      return isInnerTemp ? 2 : 12;
    case 14:
      return isInnerTemp ? 1 : 10;
  }
}

byte CalculateIntX(byte startX, char sign)
{
  return startX + (sign == '-' ? 15 : 0);
}

byte CalculateDecX(byte startX, byte lngth, char sign)
{
  return CalculateIntX(startX, sign) + (sign == '-' ? lngth - 1 : lngth) * 4 ;
}

byte CalculateIntSymbolsLength(byte tInt, char tSign)
{
  byte rslt = 0;
  char text[3] = "   ";
  if (tSign == '-')
    sprintf(text, "-%d", tInt);
  else
    sprintf(text, "%d", tInt);

  Serial.print("text= ");
  Serial.println(text);
  for (int i = 0; i < 3; i++)
  {
    if (text[i] == '-' )
      rslt = rslt + 2;
    if (text[i] == '1')
      rslt = rslt + 5;
    else if (text[i] == '2' || text[i] == '3' || text[i] == '4' || text[i] == '5' || text[i] == '6' || text[i] == '7' || text[i] == '8' || text[i] == '9' || text[i] == '0')
      rslt = rslt + 6;
  }
  return rslt;
}

void loop()
{
  CheckButtons();
  RefreshSensorData();
  CheckAlarmSensor();
  ReadCommandNRF();
  DisplayData(DISPLAY_AUTO);
  wdt_reset();
}
