//Home_Central Control
//Arduino Mini
//Управление домом без GSM
//отправка Tout к устр-м отображения по NRFL
//включение вентилятора по NRFL от устр-в

// GND VCC CE  CSN MOSI  MISO  SCK
//          9  10  11    12    13
//  http://robotclass.ru/tutorials/arduino-radio-nrf24l01/
///   https://wiki.iarduino.ru/page/NRF24L01-trema/
//

#include <NrfCommands.h> // C:\Program Files (x86)\Arduino\libraries\NrfCommands
#include <SoftwareSerial.h>
#include <elapsedMillis.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include "sav_button.h" // Библиотека работы с кнопками
#include <RF24.h>
#include <RF24_config.h>
#include <Adafruit_BMP085.h> //давление Vcc – +5в; SDA – (A4);SCL - (A5)

#include <TimeLib.h>  //clock
#include <Wire.h> //clock
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t //clock

#include <avr/wdt.h>
#include <avr/power.h>
#include <util/delay.h>


#define ONE_WIRE_PIN 5    // DS18b20

//RNF SPI bus plus pins  9,10 для Уно или 9, 53 для Меги
#define CE_PIN 6
#define CSN_PIN 7
#define RNF_MOSI      51
#define RNF_MISO      50
#define RNF_SCK       52

//#define MIC_PIN 14        // активация микрофона(т.е. переключение на микрофон вместо MP3)
//#define REGISTRATOR_PIN 15// активация регистратора
//#define ADD_DEVICE_PIN 16 // дополнительное устройство

#define DHTTYPE DHT22

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

Adafruit_BMP085 bmp;
//DHT dht(DHT_PIN, DHTTYPE);
//
//Adafruit_SSD1306 display(OLED_RESET);

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(CE_PIN, CSN_PIN);


const unsigned long NRF_COMMUNICATION_INTERVAL_S = 10;
const unsigned long REFRESH_SENSOR_INTERVAL_S = 100;
const unsigned long BLOCK_UNKNOWN_PHONES_PERIOD_S = 1800; //30min
const unsigned long MAX_ON_PERIOD_ADD_DEVICE_S = 3600; //60min ограничение максимального времени работы доп устройства
const unsigned long ON_PERIOD_REGISTRATOR = 180; //3min времени работы регистратора
const unsigned long VENT_CORRECTION_PERIOD_S = 30; //5min
const unsigned long GET_EXTERNAL_DATA_INTERVAL_S = 600;

//Параметры комфорта
const float MIN_COMFORT_ROOM_TEMP_WINTER = 18.0;
const float MIN_COMFORT_ROOM_TEMP_SUMMER = 21.0;
const float MAX_COMFORT_ROOM_TEMP = 24.5;
const float BORDER_WINTER_SUMMER = 10; // +10c

const int arCO2Levels[3] = {450, 600, 1000};

const byte ADR_EEPROM_SCENARIO1_NAGREV = 100;   //начало адресов температур по комнатам для SCENARIO1_NAGREV; а последнем адресе - период работы сценария, часов
const byte ADR_EEPROM_SCENARIO2_NAGREV = 110;   //начало температур для SCENARIO2_NAGREV
const byte ADR_EEPROM_SCENARIO3_NAGREV = 120;   //начало температур для SCENARIO3_NAGREV

const byte ROOM_NUMBER_OUT_T1 = ROOM_VENT;
const byte ROOM_NUMBER_OUT_T2 = ROOM_SENSOR;

boolean AlarmMode = false;
boolean bNo220 = false;

enum EnModeVent { V_AUTO_OFF, V_AUTO_SPEED1, V_AUTO_SPEED2, V_AUTO_SPEED3, V_TO_AUTO, V_TO_SPEED1, V_TO_SPEED2, V_TO_SPEED3, V_SPEED1, V_SPEED2, V_SPEED3, V_TO_OFF, V_OFF };
//
//enum EnMP3Mode {
//  M_NO, M_ASK_DTMF, M_ASK_PASSWORD, M_RECALL_MODE_CHANGE, M_DTMF_RECOGN, M_DTMF_NO_RECOGN, M_DTMF_INCORRECT_PASSWORD, M_COMMAND_APPROVED,
//  M_NO_220, M_BREAK_220, M_ALARM1, M_ALARM2,
//} mp3Mode = M_NO;

//enum EnDTMFCommandMode { DC_WAIT, DC_RECEIVING, DC_WRONG_COMMAND, DC_RECOGNISED_COMMAND, DC_WAIT_CONFIRMATION, DC_CONFIRMED, DC_EXECUTED, DC_REJECTED } dtmfCommandMode = DC_WAIT;

//enum EnDTMF {
//  D_SAY_NON_ADMIN_COMMANDS, D_SAY_ALL_SETTINGS, D_DO_ALARM, D_REQUEST_HOME_INFO,
//  D_SWITCH_ON_REGISTRATOR, D_SWITCH_ON_MIC, D_SWITCH_ON_ADD_DEViCE, D_SWITCH_OFF_ADD_DEViCE,
//  D_RESET_NAGREV, D_SCENARIO_NAGREV,
//  D_RESET_VENT, D_SCENARIO_VENT,
//  D_ADD_THIS_PHONE, D_CHANGE_UNP_PASSWORD, D_CHANGE_ADMIN_PASSWORD, D_CHANGE_RECALL_MODE
//} dtmfCommand;
//
//enum EnAddDeviceMode { ADD_DEVICE_OFF, REQUEST_ADD_DEVICE_ON, ADD_DEVICE_ON, REQUEST_ADD_DEVICE_OFF } addDeviceMode = ADD_DEVICE_OFF;
//
//enum EnNagrevMode { NAGREV_OFF, REQUEST_RESET_NAGREV, REQUEST_SCENARIO1_NAGREV, REQUEST_SCENARIO2_NAGREV, REQUEST_SCENARIO3_NAGREV, SCENARIO1_NAGREV, SCENARIO2_NAGREV, SCENARIO3_NAGREV } nagrevMode = NAGREV_OFF;
//enum EnVentMode { VENT_OFF, REQUEST_RESET_VENT, REQUEST_SCENARIO1_VENT, REQUEST_SCENARIO2_VENT, REQUEST_SCENARIO3_VENT, SCENARIO1_VENT, SCENARIO2_VENT, SCENARIO3_VENT } ventMode = VENT_OFF;
//
//enum EnMicMode { MIC_OFF, REQUEST_MIC_ON, MIC_ON } micMode = MIC_OFF;

//enum EnGsmMode {
//  WAIT, INCOMING_UNKNOWN_CALL_START, INCOMING_CALL_START, INCOMING_CALL_PROGRESS, INCOMING_UNKNOWN_CALL,
//  INCOMING_CALL_ANSWERED, INCOMING_UNKNOWN_CALL_ANSWERED,
//  INCOMING_CALL_DISCONNECTED, INCOMING_CALL_HANGUP,
//  TODO_CALL,
//  RECALL_DIALING, OUTGOING_CALL_PROGRESS, OUTGOING_CALL_PROGRESS, OUTGOING_TALK, RECALL_HANGUP, RECALL_NOANSWER, RECALL_BUSY,
//  WAIT_PSWD
//} gsmMode = WAIT_GSM;
//
//enum EnGSMSubMode {
//  WAIT_GSM_SUB,
//  INCOMING_UNKNOWN_CALL,
//  CONFIRM_CALL,
//  RECALL,
//  START_INFO_CALL,
//  FINISH_INFO_CALL,
//} gsmSubMode = WAIT_GSM_SUB;
//
//enum EnGSMMode {
//  WAIT_GSM, INCOMING_CALL_START, INCOMING_CALL_PROGRESS, INCOMING_CALL_DISCONNECTED, INCOMING_CALL_HANGUP, INCOMING_CALL_ANSWERED,
//  TODO_CALL, OUTGOING_CALL_PROGRESS, OUTGOING_CALL_HANGUP, OUTGOING_CALL_NOANSWER, OUTGOING_CALL_ANSWERED, OUTGOING_TALK, OUTGOING_CALL_BUSY
//} gsmMode = WAIT_GSM;
//
//enum EnDoAlarmMode { ALARM_OFF, REQUEST_ALARM_ON } doAlarmMode = ALARM_OFF;
//enum EnOpenDoorMode { OPEN_DOOR_OFF, REQUEST_OPEN_DOOR_ON } openDoorMode = OPEN_DOOR_OFF;
//enum EnRegistratorMode { REGISTRATOR_OFF, REQUEST_REGISTRATOR_ON, REGISTRATOR_ON } registratorMode = REGISTRATOR_OFF;
//enum EnSendSMSMode { SMS_NO, REQUEST_GPS_SMS, REQUEST_CAR_INFO_SMS } sendSMSMode = SMS_NO;
//enum EnWorkEEPROM { EE_PHONE_NUMBER, EE_PASSWORD_UNKNOWN_PHONES, EE_PASSWORD_ADMIN, EE_USE_RECALL_ME, EE_ADD_THIS_PHONE_TO_STORED, EE_SCENARIO_1_NAGREV, EE_SCENARIO_2_NAGREV, EE_SCENARIO_3_NAGREV, EE_SCENARIO_1_VENT, EE_SCENARIO_2_VENT, EE_SCENARIO_3_VENT };
//enum enOutCommand { OUT_NO, OUT_T_INFO, OUT_CLOSE_VODA_1, OUT_CLOSE_VODA_2, OUT_ALARM_SHORT, OUT_ALARM_LONG };
//enum enInCommand { IN_NO, IN_ROOM_INFO, IN_ROOM_COMMAND, IN_CENTRAL_COMMAND };
//enum enAlarmType { ALR_NO, ALR_VODA, ALR_DOOR };

float t_inn[10];         //температура внутри, по комнатам
byte h[10];              //влажность внутри, по комнатам
int co2[10];             //co2 по комнатам
float t_set[10];         //желаемая температура по комнатам
float vent_set[10];      //желаемая вентиляция по комнатам
boolean nagrevStatus[10];//состояние батарей по комнатам (true/false)
EnModeVent modeVent[10]; //вентиляция по комнатам
byte alarmStatus[10];    //alarm, по комнатам  //0-none, 1-medium, 2-serious
byte alarmStatusNotification[10][2];    //alarm, раздать по комнатам:  статус//0-none, 1-medium, 2-serious + номера комнат
//номера комнат подписчиков и поставщиков Alert bitRead(a, 0)
byte arRoomsAlarmNotification[5] = {
  0b00111010,
  0b00111010,
  0b00000000,
  0b10000000,
  0b10000001
};


float t_out1;   //температура снаружи место1
float t_out2 = 99.99;   //температура снаружи место2
float t_out;    //температура снаружи (минимальная место1 или место2)
float t_vent;   //температура внутри вентиляционной системы (в блоке разветвления воздуха) для расчета (t_vent - t_out)
float t_unit;   //температура блока управления (в самой горячей точке)
float t_bat;    //температура батареи отопления (ближайшей)
int p_v = 0;    //давление
byte h_out = 0; //влажность снаружи


elapsedMillis lastRefreshSensor_ms = REFRESH_SENSOR_INTERVAL_S * 1000;
elapsedMillis lastNrfCommunication_ms;
elapsedMillis ventCorrectionPeriod_ms;
elapsedMillis lastGetExternalData_ms = GET_EXTERNAL_DATA_INTERVAL_S * 1000;;

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress outer1TempDeviceAddress;

//mp3TF mp3tf = mp3TF();

unsigned long c = 0;
unsigned long ca = 0;

String sSoftSerialData = "";

//typedef struct {
//  enOutCommand Command;
//  byte roomNumber; //? или лучше менять address?
//  float tOut;
//  int p_v;
//  boolean nagrevStatus;
//  byte hours;           //1b
//  byte minutes;         //1b
//} nrfResponse;        //11b

//typedef struct {
//  enInCommand Command;
//  byte roomNumber;  //1,2,3,4,.. 0-центральное упр-е (Command=IN_CENTRAL_COMMAND)
//  float t;
//  int co2;
//  byte h;
//  enAlarmType alarmType;  //voda/door alarm/...
//  byte ventSpeed;     //0-not supported, 1-1st speed, 2-2nd speed, 3-3d speed, 10 - off, 100 - auto
//  float t_set;      //желаемая температура (-100 если не задано)
//byte servoDet;       //1b
//byte servoBed;       //1b
//byte servoGost;       //1b
//} NRFResponse;

NRFRequest nrfRequest;
NRFResponse nrfResponse;
boolean nrfCommandProcessing = false; //true when received nrf command

//ESP https://istarik.ru/blog/esp8266/29.html
//HardwareSerial & ESPport = Serial1; //ESP подключите к Serial1 (18, 19), скорость можно сделать 57600
#define BUFFER_SIZE 128
char esp_buffer[BUFFER_SIZE];
String vklotkl;

void setup()
{
  Serial.begin(9600);
  Serial.println("Setup start");

  RadioSetup();
  bmp.begin();

  sensors.begin();
  sensors.getAddress(outer1TempDeviceAddress, 1);
  sensors.setResolution(outer1TempDeviceAddress, 12);
  _delay_ms(2000);
  for (byte iRoom = 0; iRoom < 5; iRoom++)
  {
    modeVent[iRoom] = V_AUTO_OFF;
  }

  setSyncProvider(RTC.get);   // the function to get the time from the RTC

  Serial.println("Setup done");

  wdt_enable(WDTO_8S);
}

void RadioSetup()
{
  //RF24
  radio.begin();                          // Включение модуля;
  _delay_ms(2);
  radio.enableAckPayload();       //+
  radio.setPayloadSize(32);
  radio.setChannel(ArRoomsChannelsNRF[ROOM_SENSOR]);             // Установка канала вещания;
  radio.setRetries(10, 10);               // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);            // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);            // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(RoomReadingPipe);   // Активация данных для отправки
  radio.openReadingPipe(1, CentralReadingPipe);    // Активация данных для чтения
  radio.startListening();

  radio.printDetails();
}

void ReadCommandNRF() //from reponse
{
  if ( radio.available() )
  {
    bool done = false;
    Serial.println("radio.available!!");
    while (!done)
    {
      done = radio.read(&nrfResponse, sizeof(nrfResponse));
      _delay_ms(20);
      //radio.stopListening();
      Serial.print("received data from room: ");
      Serial.println(nrfResponse.roomNumber);
    }
    ParseAndHandleInputNrfCommand();
  }
}

void NrfCommunication()
{
  ResetExternalData();
  if (lastNrfCommunication_ms > NRF_COMMUNICATION_INTERVAL_S * 1000)
  {
    //
    //    radio.setChannel(ArRoomsChannelsNRF[ROOM_SENSOR]);
    //    radio.openWritingPipe(ArRoomsReadingPipes[ROOM_SENSOR]); //for confirm writes
    ReadCommandNRF(); // from ROOM_SENSOR
    radio.stopListening();
    for (byte iRoom = 0; iRoom < ROOM_SENSOR; iRoom++)
    {
      SendCommandNRF(iRoom);
    }
    radio.setChannel(ArRoomsChannelsNRF[ROOM_SENSOR]);
    radio.startListening();
    lastNrfCommunication_ms = 0;
    FillAlarmStatuses();
  }
}

void ResetExternalData()
{
  if (lastGetExternalData_ms > GET_EXTERNAL_DATA_INTERVAL_S * 1000)
    t_out2 = 88.99;
}

void FillAlarmStatuses()
{
  memset(alarmStatusNotification, 0, 5); // обнуляем массив
  for (byte iCheckRoom = 0; iCheckRoom <= 5; iCheckRoom++)
  {
    if (alarmStatus[iCheckRoom] > 0)
    {
      for (byte iNotifRoom = 0; iNotifRoom <= 5; iNotifRoom++)
      {
        if (bitRead(arRoomsAlarmNotification[iNotifRoom], iCheckRoom))
        {
          if (alarmStatus[iCheckRoom] > alarmStatusNotification[iNotifRoom][0])
          {
            alarmStatusNotification[iNotifRoom][0] = alarmStatus[iCheckRoom];
            alarmStatusNotification[iNotifRoom][1] = iCheckRoom;
          }
        }
      }
    }
  }
}

void PrepareRequestCommand(byte roomNumber)
{
  nrfRequest.Command = RQ_T_INFO;
  nrfRequest.roomNumber = roomNumber;
  nrfRequest.tOut = t_out;
  nrfRequest.p_v = p_v;
  nrfRequest.nagrevStatus = nagrevStatus[roomNumber];
  if (roomNumber == ROOM_VENT)
  {
    nrfRequest.Command = RQ_VENT;
    nrfRequest.ventSpeed = PrepareVentParams();
  }
  nrfRequest.hours = hour();
  nrfRequest.minutes = minute();
  nrfRequest.alarmMaxStatus = alarmStatusNotification[roomNumber][0];
  nrfRequest.alarmRooms = alarmStatusNotification[roomNumber][1];
  if (roomNumber == 0)
  {
    Serial.println("time:");
    Serial.println(hour());
    Serial.println(minute());
  }
}

void SendCommandNRF(byte roomNumber)
{
  PrepareRequestCommand(roomNumber);

  Serial.print("roomNumber: ");
  Serial.println(roomNumber);
  //radio.openWritingPipe(ArRoomsReadingPipes[roomNumber]);
  radio.setChannel(ArRoomsChannelsNRF[roomNumber]);
  if (radio.write(&nrfRequest, sizeof(nrfRequest)))
  {
    Serial.println("Success Send");
    _delay_ms(10);
    if (!radio.isAckPayloadAvailable() )   // Ждем получения..
      Serial.println(F("Empty response."));
    else
    {
      //  Serial.println(F("RESPONSE!."));
      //ReadCommandNRF();

      bool done = false;
      //  Serial.println("radio.available!!");
      while (!done)
      {
        done = radio.read(&nrfResponse, sizeof(nrfResponse));
        _delay_ms(20);
        //radio.stopListening();
        Serial.print("received data from room: ");
        Serial.println(nrfResponse.roomNumber);
      }
      ParseAndHandleInputNrfCommand();
    }
  }
  // else
  // Serial.println("Failed Send");
}

void ParseAndHandleInputNrfCommand()
{
  nrfCommandProcessing = true;
  Serial.print("roomNumber= ");
  Serial.println(nrfResponse.roomNumber);
  Serial.print("Tinn= ");
  Serial.println(nrfResponse.tInn);

  alarmStatus[nrfResponse.roomNumber] = nrfResponse.alarmType;
  t_inn[nrfResponse.roomNumber] = nrfResponse.tInn;
  co2[nrfResponse.roomNumber] = nrfResponse.co2;

  if (nrfResponse.roomNumber == ROOM_BED)
  {
    Serial.print("              CO2= ");
    Serial.println(co2[ROOM_BED]);
  }
  if (nrfResponse.roomNumber == ROOM_VENT)
  {
    t_out1 = nrfResponse.tOut;
    Serial.print("            Tout1= ");
    Serial.println(nrfResponse.tOut);
  }
  if (nrfResponse.roomNumber == ROOM_SENSOR)
  {
    t_out2 = nrfResponse.tOut;
    lastGetExternalData_ms = 0;
    Serial.print("              Tout2= ");
    Serial.println(nrfResponse.tOut);
  }
}

void RefreshSensorData()
{
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    Serial.println("RefreshSensorData");
    sensors.requestTemperatures();
    //    //float realTemper = sensors.getTempCByIndex(0);
    //    //t_inn = sensors.getTempC(innerTempDeviceAddress);
    //t_out1 = sensors.getTempC(outer1TempDeviceAddress);
    //    t_vent = sensors.getTempC(ventTempDeviceAddress);
    t_vent = t_out1;
    //    t_unit = sensors.getTempC(unitTempDeviceAddress);
    t_out = (t_out2 < t_out1 || t_out1 < -100) && t_out2 > -100 ? t_out2 : t_out1;
    Serial.print("t_out= ");
    Serial.println(t_out);

    p_v = 0.075 * bmp.readPressure();
    Serial.print("P= ");
    Serial.println(p_v);

    lastRefreshSensor_ms = 0;
  }
}

void PrepareVentParams()
{
  //  if (ventCorrectionPeriod_ms > VENT_CORRECTION_PERIOD_S * 1000)
  //  {
  VentControlByRoom(ROOM_BED);
  VentControlByRoom(ROOM_GOST);
  if (modeVent[ROOM_GOST] == V_SPEED2 || modeVent[ROOM_GOST] == V_AUTO_SPEED2 || modeVent[ROOM_GOST] == V_SPEED3 || modeVent[ROOM_GOST] == V_AUTO_SPEED3) //предполагаем что гости
  {
    modeVent[ROOM_DET] = modeVent[ROOM_GOST];
  }
  else  if (modeVent[ROOM_BED] == V_AUTO_OFF) //предполагаем что день
  {
    modeVent[ROOM_DET] = modeVent[ROOM_GOST];
  }
  else  if (modeVent[ROOM_GOST] == V_SPEED2) //предполагаем что сон
  {
    modeVent[ROOM_DET] = modeVent[ROOM_BED];
  }

  //сосчитаем итоговую скорость вентилятора
  if (GetVentLevel(ROOM_BED) == 3 || GetVentLevel(ROOM_GOST) == 3)
  {
    nrfRequest.ventSpeed = 3;
  }
  else     if (GetVentLevel(ROOM_BED) == 2 || GetVentLevel(ROOM_GOST) == 2)
  {
    nrfRequest.ventSpeed = 2;
  }
  else     if (GetVentLevel(ROOM_BED) == 1 || GetVentLevel(ROOM_GOST) == 1)
  {
    nrfRequest.ventSpeed = 1;
  }
  else
  {
    nrfRequest.ventSpeed = 0;
  }
  SetServoPosition(ROOM_BED);
  SetServoPosition(ROOM_GOST);
  SetServoPosition(ROOM_DET);

  //    ventCorrectionPeriod_ms = 0;
  //  }
}

void SetServoPositionByRoom(byte room)
{
  switch (room)
  {
    case ROOM_BED:
      nrfRequest.servoBed = GetServoPosition(room);
      break;
    case ROOM_GOST:
      nrfRequest.servoGost = GetServoPosition(room);
      break;
    case ROOM_DET:
      nrfRequest.servoDet = GetServoPosition(room);
      break;
  }

}

byte GetServoPosition(byte room)
{
  if (modeVent[room] ==  V_SPEED1 || modeVent[room] == V_AUTO_SPEED1)
    return SERVO_1;
  else   if (modeVent[room] ==  V_SPEED2 || modeVent[room] == V_AUTO_SPEED2)
    return SERVO_2;
  else   if (modeVent[room] ==  V_SPEED3 || modeVent[room] == V_AUTO_SPEED3)
    return SERVO_3;
  else
    return SERVO_0;
}

byte GetVentLevel(byte room)
{
  byte rslt = 0;
  if (modeVent[room] == V_SPEED3 || modeVent[room] == V_AUTO_SPEED3)
  {
    rslt = 3;
  }
  else if (modeVent[room] == V_SPEED2 || modeVent[room] == V_AUTO_SPEED2)
  {
    rslt = 2;
  }
  else if (modeVent[room] == V_SPEED1 || modeVent[room] == V_AUTO_SPEED1)
  {
    rslt = 1;
  }
  return rslt;
}

void VentControlByRoom (byte room)
{
  if (co2[room] == 0)
    return;
  float kT = 1;
  if (t_out < -20)  //too cold
    kT = 1.5;
  else if (t_out < 30)
    kT = (-4 * t_out + 525) / (float)arCO2Levels[1];
  else  //too hot, >30
    kT = 1.5;

  switch (modeVent[room])
  {
    case V_TO_AUTO:
    case V_AUTO_SPEED1:
    case V_AUTO_SPEED2:
    case V_AUTO_SPEED3:
    case V_AUTO_OFF:
      if (co2[room] > (arCO2Levels[3] * kT))
        modeVent[room] = V_AUTO_SPEED3;
      if (co2[room] > (arCO2Levels[2] * kT))
        modeVent[room] = V_AUTO_SPEED2;
      else if (co2[room] > (arCO2Levels[1] * kT))
        modeVent[room] = V_AUTO_SPEED1;
      else
      {
        Serial.print("      t_out=");
        Serial.println(t_out);
        Serial.print("      t_inn=");
        Serial.println(t_inn[room]);
        Serial.print("      kT=");
        Serial.println(kT);
        modeVent[room] = V_AUTO_OFF;
      }
      Serial.print("      CO2=");
      Serial.println(co2[room]);

      //reduce speed or off if too cold in room
      if (t_inn[room] < (t_out < BORDER_WINTER_SUMMER ? MIN_COMFORT_ROOM_TEMP_WINTER : MIN_COMFORT_ROOM_TEMP_SUMMER) && t_inn[room] > t_vent)
      {
        if (modeVent[room] >= V_AUTO_SPEED1 && modeVent[room] > V_AUTO_SPEED1)
          modeVent[room] = modeVent[room] - 1;
      }

      //increase speed if too hot in room
      if (t_inn[room] > MAX_COMFORT_ROOM_TEMP && t_inn[room] > t_out)
      {
        if (modeVent[room] >= V_AUTO_OFF && modeVent[room] < V_AUTO_SPEED2)
          modeVent[room] = modeVent[room] + 1;
      }

      break;

    case V_TO_OFF:
      modeVent[room] = V_OFF;
      break;
    case V_TO_SPEED1:
      modeVent[room] = V_SPEED1;
      break;
    case V_TO_SPEED2:
      modeVent[room] = V_SPEED2;
      break;
    case V_TO_SPEED3:
      modeVent[room] = V_SPEED3;
      break;
  }

  Serial.println("room: " + room);
  Serial.print("modeVent[room]: ");
  Serial.println(modeVent[room]);
}

void loop()
{

  RefreshSensorData();
  NrfCommunication(); //read / send / read response
  wdt_reset();
}
