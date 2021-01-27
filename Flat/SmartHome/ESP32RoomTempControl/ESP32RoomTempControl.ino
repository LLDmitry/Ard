#include <Adafruit_ssd1306syp.h>

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

#include <Wire.h>
//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>

#include <SPI.h>

#define BTN_P_PIN     3
#define BTN_M_PIN     4
#define HEATER_PIN    8
#define BUZZ_PIN      9
#define PIR_SENSOR_PIN     2
#define ONE_WIRE_PIN  5    // DS18b20

//RNF
#define RNF_CE_PIN    6
#define RNF_CSN_PIN   7
#define RNF_MOSI      11
#define RNF_MISO      12
#define RNF_SCK       13

#define SDA_PIN           A4  //(SDA) I2C
#define SCL_PIN           A5  //(SCK) I2C

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

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
byte t_outInt = 0;
byte t_outDec = 0;
boolean heaterStatus = false;
volatile boolean isAlarm = false;
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
  nrfResponse.tInn = t_inn;
  nrfResponse.tOut = t_out;
  nrfResponse.co2 = random(100);
  //nrfResponse.t1 = t_hot;
  //nrfResponse.h = h_v;

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
    t_inn = 15.6; //debug
    //    t_out = sensors.getTempC(OutTempDeviceAddress);
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
    alarmSensor = true;
    checkAlarmSensor_ms = 0;
  }
}

void AlarmSignal()
{
  digitalWrite(BUZZ_PIN, isAlarm);
}

//Get Command
void ReadCommandNRF()
{
  _delay_ms(10);
  if (readCommandNRF_ms > READ_COMMAND_NRF_INTERVAL_S * 1000)
  {
    Serial.println("ReadCommandNRF_START");
    _delay_ms(10);
    if (radio.available())
    {
      int cntAvl = 0;
      Serial.println("radio.available!!");
      radio.read(&nrfRequest, sizeof(nrfRequest));
      delay(20);
      radio.flush_rx();
      _delay_ms(20);
      Serial.println("radio.read: ");
      Serial.println(nrfRequest.roomNumber);
      Serial.println(600 + nrfRequest.p);
      Serial.println(nrfRequest.Command);
      Serial.println(nrfRequest.minutes);
      Serial.println(nrfRequest.tOut);
      Serial.println(nrfRequest.tInnSet);
      _delay_ms(20);

      HandleInputNrfCommand();


      nrfResponse.Command == RSP_NO;
      //      nrfResponse.tOut = 99.9;
      nrfResponse.tInn = t_inn;
      nrfResponse.tInnSet = t_set;
      //nrfResponse.alarmSensor = alarmSensor;
      //}
    }
    readCommandNRF_ms = 0;
  }
}

void HandleInputNrfCommand()
{
  Serial.print("                           roomNumber= ");
  Serial.println(nrfRequest.roomNumber);
  if (nrfRequest.roomNumber == ROOM_NUMBER)
  {
    //Serial.print("tSet= ");
    //Serial.println(nrfRequest.tSet);
    t_outInt = nrfRequest.tOut;
    t_outDec = nrfRequest.tOutDec;
    t_out = t_outInt + ((float)nrfRequest.tOutDec / 10.0f);
    //d if (nrfRequest.tOutSign == '-')
    t_out = -t_out;
    if (nrfRequest.tInnSet < 100)
    {
      t_set = nrfRequest.tInnSet;
      t_set_on = t_set > 0;
      SaveTSetEEPROM();
    }
    Serial.print("t_out= ");
    Serial.println(t_out);
    Serial.print("t_set= ");
    Serial.println(t_set);
    Serial.print("tInnSetVal1= ");
    Serial.println(nrfRequest.tInnSetVal1);

    if (nrfRequest.tInnSetVal1 <= 30 || nrfRequest.tInnSetVal2 <= 30 || nrfRequest.tInnSetVal3 <= 30 || nrfRequest.tInnSetVal4 <= 30 || nrfRequest.tInnSetVal5 <= 30)
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
    }
    HeaterControl();
    //isAlarm = nrfRequest.isAlarm; //central send it to one room
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

  SaveTSetEEPROM();
  HeaterControl();
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
  if (toDisplayMode != DISPLAY_AUTO || displayMode == DISPLAY_INN_TMP && displayMode_ms > SHOW_TMP_INN_S * 1000 || (displayMode == DISPLAY_OUT_TMP || displayMode == DISPLAY_ALARM) && displayMode_ms > SHOW_TMP_OUT_S * 1000)
  {
    Serial.println("DD_1");
    Serial.print("displayMode = ");
    Serial.println(displayMode);
    _delay_ms(10);
    if (toDisplayMode == DISPLAY_AUTO)
    {
      if (displayMode == DISPLAY_INN_TMP)
        displayMode = DISPLAY_OUT_TMP;
      else
        displayMode = DISPLAY_INN_TMP;
    }
    else
    {
      if (toDisplayMode = DISPLAY_ALARM)
      {
      }
      else if (toDisplayMode = DISPLAY_INN_TMP)
      {
        displayMode = DISPLAY_INN_TMP;
      }
    }
    displayMode_ms = 0;

    Serial.println("DD2");
    _delay_ms(10);

    //display.setFont(&FreeSerif9pt7b);
    display.clear();
    Serial.println("DD21");
    display.update();
    _delay_ms(10);
    Serial.println("DD3");

    if (displayMode == DISPLAY_INN_TMP)
    {
      Serial.println("DD4");
      if ((int)t_inn < 0)
      {
        display.setTextSize(2);
        display.setCursor(1, 30);
        display.println("-");
      }

      display.setTextSize(5);
      display.setTextColor(WHITE);
      display.setCursor(15, 18);
      display.println(abs((int)t_inn));

      //Serial.print("t_set_on ");
      //Serial.println(t_set_on);
      Serial.print("t_set ");
      Serial.println(t_set);
      Serial.print("t_set2 ");
      _delay_ms(10);
      Serial.println(set_temp[t_set - 1]);


      if (t_set_on)
      {
        byte x, y;
        if ((int)set_temp[t_set - 1] < 10)
          x = 100;
        else
          x = 88;
        display.setTextSize(3);
        if ((int)t_inn < (int)set_temp[t_set - 1])
          y = 8;
        else if ((int)t_inn > (int)set_temp[t_set - 1])
          y = 42;
        else
          y = 20;
        display.setCursor(x, y);
        display.println((int)set_temp[t_set - 1]);
      }
      else
      {
        display.setCursor(100, 20);
        display.println("OFF");
      }
    }
    else if (displayMode == DISPLAY_OUT_TMP)
    {
      Serial.println("DD5");
      //внешняя температура - внутри рамки
      display.setTextSize(4);
      byte i = 1;
      display.drawRect(i, i, display.width() - 2 * i, display.height() - 2 * i, WHITE);
      if (t_out < 0)
      {
        display.setCursor(10, 30);
        display.setTextSize(2);
        display.println("-");
      }
      display.setTextSize(4);
      display.setCursor(25, 20);
      display.println(abs(t_outInt));
      display.setTextSize(3);
      display.setCursor(80, 28);
      display.println(".");
      display.setCursor(95, 28);
      display.println(abs(t_outDec));

      //      char buf[15];
      //      dtostrf(t_out, -10, 1, buf); //"-" omits leading blanks
      //      display.println(buf);  //prints 26.5
    }
    else if (displayMode == DISPLAY_ALARM)
    {
      display.setCursor(100, 10);
      display.println("ALARM");
      //display.setCursor(100, 30);
      //display.println(alarmMaxStatus);
      //display.setCursor(100, 60);
      //display.println(alarmMaxStatusRoom);
    }
    display.update();
    delay(200);
  }
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
