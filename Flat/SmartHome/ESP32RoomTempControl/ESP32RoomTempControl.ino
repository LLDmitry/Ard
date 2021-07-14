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
#include <Adafruit_ssd1306syp.h>
#include <SPI.h>

#define BTN_P_PIN       3
#define BTN_M_PIN       2
#define HEATER_PIN      8
#define BUZZ_PIN        9
#define PIR_SENSOR_PIN  10
#define ONE_WIRE_PIN    4    // DS18b20
#define IR_PIN          5

//RNF
#define RNF_CE_PIN      6  //9
#define RNF_CSN_PIN     7  //8
#define RNF_MOSI        11
#define RNF_MISO        12
#define RNF_SCK         13

#define SDA_PIN         A4  //(SDA) I2C
#define SCL_PIN         A5  //(SCK) I2C

//byte currentRoomNumber = ROOM_GOST; //up to TOTAL_ROOMS_NUMBER
byte currentRoomNumber = ROOM_BED;
bool alarmSignal = false;

const unsigned long ALARM_BUZZ_INTERVAL_S = 2;
const unsigned long REFRESH_SENSOR_INTERVAL_S = 60;  //1 мин
const unsigned long READ_COMMAND_NRF_INTERVAL_S = 10;
const unsigned long CHECK_ALARM_SENSOR_PERIOD_S = 10;
const unsigned long SHOW_TMP_INN_S = 10;
const unsigned long SHOW_TMP_OUT_S = 3;
const unsigned long SET_ROOM_MODE_S = 10;
const unsigned long SEND_RF_ON_NAGREV_PERIOD_S = 1;

const int EEPROM_ADR_SET_TEMP = 1023; //last address in eeprom for store tSet
const float T_SET_MIN = 1.0f;  //0.0f - off
const float T_SET_MAX = 25.0f;
const byte MAX_ALLOWED_MISS_CONNECTIONS = 5;

RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);

SButton btnP(BTN_P_PIN, 50, 1000, 5000, 30000);
SButton btnM(BTN_M_PIN, 50, 1000, 5000, 30000);

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress InnTempDeviceAddress;
DeviceAddress OutTempDeviceAddress;

Adafruit_ssd1306syp display(SDA_PIN, SCL_PIN);

elapsedMillis checkAlarmSensor_ms;
elapsedMillis alarmInterval_ms;
elapsedMillis lastRefreshSensor_ms = REFRESH_SENSOR_INTERVAL_S * 1000 + 1;
elapsedMillis readCommandNRF_ms = 0;
elapsedMillis displayMode_ms = 0;
elapsedMillis setRoomMode_ms = 0;
elapsedMillis buzzOff_ms = 0;
elapsedMillis buzzOn_ms = 0;
elapsedMillis sendRfOnNagrev_ms = 0;

bool alarmSensor = false;
float t_inn = 0.0f;
float t_out = 0.0f;
float t_outThisRoom = 0.0f;
byte t_outInt = 0;
byte t_outSign = '+';
byte t_outDec = 0;
boolean heaterStatus = false;
boolean isActiveAlarmFromCenter = false;
boolean t_setChangedInRoom = false;
byte alarmMaxStatusFromCenter = 0;
byte alarmMaxStatusRoomFromCenter = 0;
enum enDisplayMode { DISPLAY_AUTO, DISPLAY_INN_TMP, DISPLAY_OUT_TMP, DISPLAY_ALARM, SET_ROOM };
enDisplayMode displayMode = DISPLAY_OUT_TMP;
byte missConnectionsCounter = 0;
bool noNRFConnection = false;

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
  radio.setChannel(ArRoomsChannelsNRF[currentRoomNumber]);            // Установка канала вещания;
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
  //SaveToEEPROM('r'); //1st time

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
  EEPROM.get(EEPROM_ADR_SET_TEMP + 10, currentRoomNumber);
}

void PrepareCommandNRF()
{
  nrfResponse.Command = RSP_INFO;
  nrfResponse.roomNumber = currentRoomNumber;
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
}

void RefreshSensorData()
{
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    Serial.println("RefreshSensorData");
    sensors.requestTemperatures();
    t_inn = sensors.getTempC(InnTempDeviceAddress);
    t_outThisRoom = sensors.getTempC(OutTempDeviceAddress);
    //    t_hot = sensors.getTempC(HotVodaTempDeviceAddress);
    //    Serial.print("t_inn: ");
    //    Serial.println(t_inn);
    //    Serial.print("t_outThisRoom: ");
    //    Serial.println(t_outThisRoom);

    // t_inn = (float)random(150) / 10.0; //debug

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
  if (isActiveAlarmFromCenter)
  {
    if (!alarmSignal && buzzOff_ms > ALARM_BUZZ_INTERVAL_S * 1000)
    {
      alarmSignal = true;
      buzzOn_ms = 0;
    }
    else if (alarmSignal && buzzOn_ms > ALARM_BUZZ_INTERVAL_S * 1000)
    {
      alarmSignal = false;
      buzzOff_ms = 0;
    }
  }
  else
    alarmSignal = false;
  digitalWrite(BUZZ_PIN, alarmSignal);
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
      uint8_t f = radio.flush_tx();
      radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1

      missConnectionsCounter = 0;
      int cntAvl = 0;
      radio.read(&nrfRequest, sizeof(nrfRequest));
      delay(20);
      Serial.print("radio.read: ");
      Serial.println(nrfRequest.roomNumber);
      HandleInputNrfCommand();
      doAfterRead();
    }
    else
      missConnectionsCounter = missConnectionsCounter + (missConnectionsCounter < 250 ? 1 : 0);

    noNRFConnection = missConnectionsCounter > MAX_ALLOWED_MISS_CONNECTIONS;
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
  //    Serial.println(600 + nrfRequest.p);
  //    Serial.println(nrfRequest.Command);
  //    Serial.println(nrfRequest.minutes);
  Serial.println("HandleInputNrfCommand:");
  Serial.println(nrfRequest.tOut);
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
    SaveToEEPROM('t');
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

    SaveToEEPROM('s');
    HeaterControl();
  }

  if (nrfRequest.alarmMaxStatus > 0) //central send it to this room
  {
    Serial.print("Alarm! ");
    Serial.println(nrfRequest.alarmMaxStatus);
    isActiveAlarmFromCenter = true;  //will be reset only by button
    alarmMaxStatusFromCenter = nrfRequest.alarmMaxStatus;
    alarmMaxStatusRoomFromCenter = nrfRequest.alarmMaxStatusRoom;
    DisplayData(DISPLAY_ALARM);
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

void SaveToEEPROM(char command)
{
  Serial.print("SaveToEEPROM:");
  Serial.println(command);
  Serial.println(t_set);
  if (command == 't')
  {
    EEPROM.put(EEPROM_ADR_SET_TEMP, t_set);
    EEPROM.put(EEPROM_ADR_SET_TEMP + 1, t_set_on);
  }
  else if (command == 's')
  {
    for (int s = 1; s <= 5; s++)
    {
      EEPROM.put(EEPROM_ADR_SET_TEMP + 1 + s, set_temp[s - 1]);
    }
  }
  else if (command == 'r')
  {
    EEPROM.put(EEPROM_ADR_SET_TEMP + 10, currentRoomNumber);
  }
}

void HeaterControl()
{
  Serial.println("HeaterControl");
  if (!heaterStatus)
  {
   sendRfOnNagrev_ms = (SEND_RF_ON_NAGREV_PERIOD_S + 1) * 1000; //когда heaterStatus станет true, сразу отправим RF
  }
  heaterStatus = (t_set_on && set_temp[t_set - 1] > t_inn);
  digitalWrite(HEATER_PIN, heaterStatus);
  if (heaterStatus)
  {
    SendRfCommandOnNagrev();
  }
  //DisplayData(DISPLAY_INN_TMP);
}

void HandleBtnClick(char command)
{
  if (isActiveAlarmFromCenter)  //reset alarm info by any button
  {
    isActiveAlarmFromCenter = false;
    alarmMaxStatusFromCenter = 0;
    alarmMaxStatusRoomFromCenter = 0;
    exit;
  }

  Serial.print("command=");
  Serial.println(command);

  if (command == 'p')
  {
    if (displayMode == SET_ROOM)
    {
      if (currentRoomNumber < TOTAL_ROOMS_NUMBER - 2)
        currentRoomNumber += 1;
      else
        currentRoomNumber = 0;
      SaveToEEPROM('r');
      DisplayData(SET_ROOM);
      setRoomMode_ms = 0;
    }
    else
    {
      t_set_on = true;
      if (t_set < T_SET_MAX)
        t_set += 1;
    }
  }

  else if (command == 'm')
  {
    if (displayMode == SET_ROOM)
    {
      if (currentRoomNumber > 0)
        currentRoomNumber -= 1;
      else
        currentRoomNumber = TOTAL_ROOMS_NUMBER - 1;
      SaveToEEPROM('r');
      DisplayData(SET_ROOM);
      setRoomMode_ms = 0;
    }
    else
    {
      if (t_set > 1)
      {
        t_set_on = true;
        t_set -= 1;
      }
      else
        t_set_on = false;
    }
  }

  else if (command == '!')
    if (displayMode == SET_ROOM)
      displayMode = DISPLAY_AUTO;
    else
      t_set_on = !t_set_on;

  else if (command == 'A')
  {
    Serial.println("command == A");
    setRoomMode_ms = 0;
    DisplayData(SET_ROOM);
  }

  if (displayMode != SET_ROOM)
  {
    t_setChangedInRoom = true;
    SaveToEEPROM('t');
    HeaterControl();
    DisplayData(DISPLAY_INN_TMP);
    PrepareCommandNRF();
  }
}

void CheckButtons()
{
  switch (btnP.Loop()) {
    case SB_CLICK:
      Serial.println("    btn + Short");
      HandleBtnClick('p');
      break;
    case SB_LONG_CLICK:
      Serial.println("btnPLong");
      HandleBtnClick('!');
      break;
    case SB_AUTO_CLICK:
      Serial.println("btnAuto");
      HandleBtnClick('A');
      break;
  }
  switch (btnM.Loop()) {
    case SB_CLICK:
      Serial.println("    btn - Short");
      HandleBtnClick('m');
      break;
    case SB_LONG_CLICK:
      Serial.println("btnMLong");
      HandleBtnClick('!');
      break;
    case SB_AUTO_CLICK:
      Serial.println("btnAuto");
      HandleBtnClick('A');
      break;
  }
}

void DisplayData(enDisplayMode toDisplayMode)
{
  if (isActiveAlarmFromCenter)
    toDisplayMode = DISPLAY_ALARM;

  if (toDisplayMode != DISPLAY_AUTO ||
      displayMode == DISPLAY_INN_TMP && displayMode_ms > SHOW_TMP_INN_S * 1000 ||
      displayMode == DISPLAY_OUT_TMP && displayMode_ms > SHOW_TMP_OUT_S * 1000 ||
      displayMode == SET_ROOM && setRoomMode_ms > SET_ROOM_MODE_S * 1000)
  {
    if (toDisplayMode == DISPLAY_AUTO)
    {
      if (displayMode == DISPLAY_INN_TMP)
        displayMode = DISPLAY_OUT_TMP;
      else if (displayMode == DISPLAY_OUT_TMP)
        displayMode = DISPLAY_INN_TMP;
      else if (displayMode == SET_ROOM)
      {
        Serial.println("Reset SET_ROOM");
        displayMode = DISPLAY_INN_TMP;
        displayMode_ms = 0;
      }
    }
    else
      displayMode = toDisplayMode;

    if (toDisplayMode == DISPLAY_AUTO)
      displayMode_ms = 0;

    Serial.print("displayMode= ");
    Serial.println(displayMode);

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
      display.drawLine(81, 1, 81, display.height() - 2, WHITE);
      if (t_set_on)
      {
        byte x, y;
        if ((int)set_temp[t_set - 1] < 10)
          x = 100;
        else
          x = 91;
        display.setTextSize(3);
        if (t_inn < (float)set_temp[t_set - 1] - 0.5)
          y = 6;
        else if (t_inn > (float)set_temp[t_set - 1] + 0.5)
          y = 40;
        else
          y = 24;
        display.setCursor(x, y);
        display.println((int)set_temp[t_set - 1]);
      }
      else
      {
        display.setCursor(92, 46);
        display.println("OFF");
      }
    }
    else if (displayMode == DISPLAY_OUT_TMP)
    {
      if (noNRFConnection)
      {
        display.setTextSize(3);
        display.setCursor(48, 10);
        display.print("NO");
        display.setCursor(0, 40);
        display.print("CONNECT");
      }
      else
      {
        //внешняя температура - внутри рамки
        display.setTextSize(4);
        byte i = 1;
        display.drawRect(i, i, display.width() - 2 * i, display.height() - 2 * i, WHITE);
        byte lngth = CalculateIntSymbolsLength(t_outInt, t_outSign);
        //      Serial.print("lngth= ");
        //      Serial.println(lngth);
        byte startX = CalculateStartX(lngth, false);
        //      Serial.print("startX= ");
        //      Serial.println(startX);
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
    }
    else if (displayMode == DISPLAY_ALARM)
    {
      display.setTextSize(3);
      display.setCursor(10, 10);
      display.println("ALARM");
      display.setCursor(10, 25);
      display.println(alarmMaxStatusFromCenter);
      display.setCursor(10, 50);
      display.println(alarmMaxStatusRoomFromCenter);
    }
    else if (displayMode == SET_ROOM)
    {
      display.setTextSize(3);
      display.drawCircle(64, 32, 30, WHITE);
      display.setCursor((currentRoomNumber < 9 ? 58 : 48), 22);

      display.println(currentRoomNumber + 1);
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
    case 9:
      return isInnerTemp ? 9 : 22;
    case 10:
      return isInnerTemp ? 7 : 20;
    case 11:
      return isInnerTemp ? 5 : 18;
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

  //  Serial.print("text= ");
  //  Serial.println(text);
  for (int i = 0; i < 3; i++)
  {
    if (text[i] == '-' )
      rslt = rslt + 3;
    if (text[i] == '1')
      rslt = rslt + 5;
    else if (text[i] == '2' || text[i] == '3' || text[i] == '4' || text[i] == '5' || text[i] == '6' || text[i] == '7' || text[i] == '8' || text[i] == '9' || text[i] == '0')
      rslt = rslt + 6;
  }
  return rslt;
}

void SendRfCommandOnNagrev()
{
  //send every random period 1-2s
  if (sendRfOnNagrev_ms > SEND_RF_ON_NAGREV_PERIOD_S * 1000 + random(0, 1000))
  {
    sendRfOnNagrev_ms = 0;
    // send HEATER_PIN
  }
}

void loop()
{
  CheckButtons();
  RefreshSensorData();
  CheckAlarmSensor();
  ReadCommandNRF();
  DisplayData(DISPLAY_AUTO);
  AlarmSignal();
  wdt_reset();
}
