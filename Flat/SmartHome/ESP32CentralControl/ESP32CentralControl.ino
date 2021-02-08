#include <RF24.h>
#include <RF24_config.h>

#define BLYNK_PRINT Serial
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "time.h"
#include <ArduinoJson.h>
#include <Preferences.h>

#include <NrfCommandsESP32.h> // C:\Program Files (x86)\Arduino\libraries\NrfCommandsESP32
#include <elapsedMillis.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "sav_button.h" // Библиотека работы с кнопками
#include <RF24.h>
#include <Adafruit_BMP085.h> //давление Vcc – +5в; //esp32 SDA(SDI) – 21;SCL(SCK) - 22 //mega SDA – A4;SCL - A5

//#include <util/delay.h>

/* Set this to a bigger number, to enable sending longer messages */
//#define BLYNK_MAX_SENDBYTES 128


#define VP_MODE             V1 //Дома/Ушел/Гости
#define VP_ALARM_STATUS     V2
#define VP_ALARM_SOURCE     V3
#define VP_ALARM_DETAILS    V4
#define VP_ALARM_BTN        V5
#define VP_TMP_OUT          V6
#define VP_TMP_IN           V7
#define VP_TMP_BTN_REFRESH  V8
#define VP_POWER_HEATER_LEVEL      V9

#define VP_ROOM_SELECT      V20
#define VP_ROOM_TMP         V21
#define VP_ROOM_TMP_SET     V22
#define VP_ROOM_HEAT_BTN    V23
#define VP_ROOM_HEAT_STATUS V24
#define VP_LCD_ROW1         V25
#define VP_LCD_ROW2         V26
#define VP_ROOM_TEMP_SET2   V28

#define VP_DEVICE1_BTN      V30
#define VP_DEVICE2_BTN      V31
#define VP_DEVICE3_BTN      V32
#define VP_DEVICE1_TIMER    V33
#define VP_DEVICE2_TIMER    V34
#define VP_DEVICE3_TIMER    V35

#define VP_PASSWORD_TXT     V50
#define VP_TERMINAL         V51
#define VP_SHOW_SETTINGS_BTN    V56
#define VP_AUTO_NIGHT_BTN   V57
#define VP_AUTO_NIGHT_START V58
#define VP_AUTO_NIGHT_STOP  V59

#define interruptPin  25


#define U_220_PIN 10       // контроль наличия 220в, с конденсатором, чтобы не реагировать на импульсы пропадания
#define ONE_WIRE_PIN 9    // DS18b20
//RNF SPI bus plus pins  9,10 для Уно или 9, 53 для Меги
//NRF24L01 для ESP32 - ce-17, cs-5, sck-18, miso-19, mosi-23)
#define CE_PIN        17
#define CSN_PIN       5
#define RNF_MOSI      23
#define RNF_MISO      19
#define RNF_SCK       18

//#define CE_PIN        6
//#define CSN_PIN       7
//#define RNF_MOSI      51
//#define RNF_MISO      50
//#define RNF_SCK       52

#define BZZ_PIN 8

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(CE_PIN, CSN_PIN); //esp32 NRF24L01 - ce-17, cs-5, sck-18, miso-19, mosi-23)

const unsigned long NRF_COMMUNICATION_INTERVAL_S = 10;
const unsigned long REFRESH_SENSOR_INTERVAL_S = 100;
const unsigned long VENT_CORRECTION_PERIOD_S = 30; //5min
const unsigned long AUTO_REFRESH_DISPLAY_PERIOD_S = 10;
const unsigned long INPUT_COMMAND_DISPLAY_PERIOD_S = 60;
const unsigned long GET_EXTERNAL_DATA_INTERVAL_S = 600;

//Параметры комфорта
const float MIN_COMFORT_ROOM_TEMP_WINTER = 18.0;
const float MIN_COMFORT_ROOM_TEMP_SUMMER = 21.0;
const float MAX_COMFORT_ROOM_TEMP = 24.5;
const float BORDER_WINTER_SUMMER = 10; // +10c

const int arCO2Levels[3] = {450, 600, 900};

const byte INDEX_ALARM_PNONE = 1;                   //index in phonesEEPROM[5]
const byte MAX_NUMBER_ATTEMPTS_UNKNOWN_PHONES = 3;  //После MAX_NUMBER_ATTEMPTS_UNKNOWN_PHONES неудачных попыток (с вводом неверного пароля) за последние 10 мин, блокируем (не берем трубку) звонки с любых неизвестных номеров на 30мин либо до звонка с известного номера (что раньше).
const byte ADR_EEPROM_RECALL_ME = 1;                //useRecallMeMode
const byte ADR_EEPROM_STORED_PHONES = 100;          //начало списка 7значных номеров телефонов (5шт по 11 байт)
const byte ADR_EEPROM_PASSWORD_UNKNOWN_PHONES = 10; //начало пароля для доступа неопознанных тел-в
const byte ADR_EEPROM_PASSWORD_ADMIN = 20;          //начало админского пароля

const byte ADR_EEPROM_SCENARIO1_NAGREV = 100;   //начало адресов температур по комнатам для SCENARIO1_NAGREV; а последнем адресе - период работы сценария, часов
const byte ADR_EEPROM_SCENARIO2_NAGREV = 110;   //начало температур для SCENARIO2_NAGREV
const byte ADR_EEPROM_SCENARIO3_NAGREV = 120;   //начало температур для SCENARIO3_NAGREV

const byte ADR_EEPROM_SCENARIO1_VENT = 200;     //начало адресов вент по комнатам для SCENARIO1_VENT; а последнем адресе - период работы сценария, часов
const byte ADR_EEPROM_SCENARIO2_VENT = 210;     //начало вент для SCENARIO2_VENT
const byte ADR_EEPROM_SCENARIO3_VENT = 220;     //начало вент для SCENARIO3_VENT

const byte ROOM_NUMBER_OUT_T1 = ROOM_VENT;
const byte ROOM_NUMBER_OUT_T2 = ROOM_SENSOR;

boolean AlarmMode = false;
boolean bNo220 = false;

enum EnModeVent { V_AUTO_OFF, V_AUTO_SPEED1, V_AUTO_SPEED2, V_AUTO_SPEED3, V_TO_AUTO, V_TO_SPEED1, V_TO_SPEED2, V_TO_SPEED3, V_SPEED1, V_SPEED2, V_SPEED3, V_TO_OFF, V_OFF };
float t_inn[10];         //температура внутри, по комнатам
byte h[10];              //влажность внутри, по комнатам
int co2[10];             //co2 по комнатам
float t_set[10];         //желаемая температура по комнатам
float vent_set[10];      //желаемая вентиляция по комнатам
boolean nagrevStatus[10];//состояние батарей по комнатам (true/false)
EnModeVent modeVent[10]; //вентиляция по комнатам
byte alarmStatus[10];    //alarm, по комнатам  //0-none, 1-medium, 2-serious
byte alarmStatusNotification[10][2];    //alarm, раздать по комнатам: [10] номер комнаты куда послать; [10][0] статус//0-none, 1-medium, 2-serious; [10][1] AlarmFromRoom
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
elapsedMillis ventCorrectionPeriod_ms;
elapsedMillis lastGetExternalData_ms = GET_EXTERNAL_DATA_INTERVAL_S * 1000;;
elapsedMillis lastNrfCommunication_ms;

OneWire ds(ONE_WIRE_PIN);
DallasTemperature tempSensors(&ds);
DeviceAddress outer1TempDeviceAddress;


//unsigned long c = 0;
//unsigned long ca = 0;

NRFRequest nrfRequest;
NRFResponse nrfResponse;
boolean nrfCommandProcessing = false; //true when received nrf command

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
// Go to the Project Settings (nut icon).
char blynkToken[] = "lxj-qafnfidshbCxIrGHQ4A12NZd2G4G";
// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "WFDV";
char pass[] = "31415926";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 3;
const int  daylightOffset_sec = 3600 * 3;

const char *weatherHost = "api.openweathermap.org";
String APIKEY = "c536de4ac23e5608ec8a62e5e0744ed8";        // Чтобы получить API ключ, перейдите по ссылке http://openweathermap.org/api
String weatherLang = "&lang=ru";
String cityID = "482443"; //Токсово

const byte ROOMS_NUMBER = 10;
const byte EEPROM_SIZE = 1;

int powerHeaters[ROOMS_NUMBER] = {800, 2000, 2000, 500, 800, 1000, 1000};
byte setsTemp[ROOMS_NUMBER][5] = {
  {2, 15, 20, 23, 25},
  {2, 15, 20, 23, 25},
  {2, 15, 20, 23, 25},
  {2, 15, 19, 21, 23},
  {2, 15, 20, 23, 25},
  {2, 12, 20, 23, 25},
  {2, 10, 15, 18, 21},
};

byte defaultTemp[5][ROOMS_NUMBER] = { //[дома/ушел/ночь][к1 к2 к3 к4 к5] setsTemp[room][i]; 0-выключить, 100-не менять
  {1, 0, 1, 0, 0, 0, 0},        //ушел
  {2, 3, 4, 3, 1, 1, 1},        //дома
  {1, 2, 1, 2, 100, 100, 100},  //ночь
  {1, 0, 3, 3, 4, 4, 4},        //гости
  {1, 0, 0, 0, 0, 0, 0},        //стоп
};

enum homeMode_enum {GUARD_MODE, HOME_MODE, NIGHT_MODE, GUESTS_MODE, STOP_MODE, SET_ROOM_TEMP_MODE, NONE_MODE};
homeMode_enum homeMode;
homeMode_enum homeModeBeforeNight;

Adafruit_BMP085 bmp;
BlynkTimer timer;
WidgetTerminal terminal(VP_TERMINAL);

Preferences prefs;

struct tm timeinfo;

bool isWiFiConnected = false;
int numTimerReconnect = 0;

bool allowChangeSettings;
bool motionDetected;
bool isSetup;

volatile byte heat_status_room[ROOMS_NUMBER]; //0-off, 1-on, 2-on in progress; 3-err
volatile byte heat_control_room[ROOMS_NUMBER]; //0-off, 1-on
volatile byte currentRoom;
volatile byte prevTempSet;

long timeStartNightSec;
long timeStopNightSec;
bool allowAuthoNight;
int tMinute;
int tHour;
bool freshTime;


//погода
int cntFailedWeather = 0;  // Счетчик отсутстия соединения с сервером погоды
String weatherMain = "";
String weatherDescription = "";
String weatherLocation = "";
String country;
int humidity;
int pressure;
float temp;
float tempMin, tempMax;
int clouds;
float windSpeed;
String date;
String currencyRates;
String weatherString;
int windDeg;
String windDegString;
String cloudsString;
String firstString;


void IRAM_ATTR detectsMotion() {
  motionDetected = true;
}

void backupDefaultTemp()
{
  Serial.println("backupDefaultTemp1Mode");
  Serial.println("backupDefaultTempAllModes");
  for (int s = 0; s < 5; s++) {
    backupDefaultTempOneMode(s);
  }
  //prefs.end();
}

void backupDefaultTempOneMode(int setHomeMode)
{
  for (int r = 0; r < ROOMS_NUMBER; r++) {
    char a[4];
    ("d" + (String)setHomeMode + (String)r).toCharArray(a, 4);
    Serial.print(a);
    Serial.print(": ");
    Serial.println(defaultTemp[setHomeMode][r]);
    prefs.putUInt(a, defaultTemp[setHomeMode][r]);
  }
}

void backupSetsTempOneRoom(byte room)
{
  Serial.print("    backupSetsTempOneRoom:");
  Serial.println(room);
  for (int s = 0; s < 5; s++) {
    char a[4];
    ("s" + (String)s + (String)room).toCharArray(a, 4);
    Serial.print(a);
    Serial.print(": ");
    Serial.println(setsTemp[room][s]);
    prefs.putUInt(a, setsTemp[room][s]);
  }
}

void restoreDefaultTemp()
{
  Serial.println("restoreDefaultTemp");
  for (int s = 0; s < 5; s++) {
    for (int r = 0; r < ROOMS_NUMBER; r++) {
      char a[4];
      ("d" + (String)s + (String)r).toCharArray(a, 4);
      //      Serial.print(a);
      //      Serial.print(": ");
      defaultTemp[s][r] = prefs.getUInt(a, 0);
      //      Serial.println(defaultTemp[s][r]);
    }
  }
  //prefs.end();
}

void restoreSetsTemp()
{
  Serial.println("restoreSetsTemp");
  for (int s = 0; s < 5; s++) {
    for (int r = 0; r < ROOMS_NUMBER; r++) {
      char a[4];
      ("s" + (String)s + (String)r).toCharArray(a, 4);
      //      Serial.print(a);
      //      Serial.print(": ");
      setsTemp[r][s] = prefs.getUInt(a, 0);
      //      Serial.println(setsTemp[r][s]);
    }
  }
  //prefs.end();
}

void backupSettings()
{
  Serial.println("backupSettingds");
  prefs.putUInt("nal", allowAuthoNight);
  prefs.putUInt("nst", timeStartNightSec);
  prefs.putUInt("nsp", timeStopNightSec);

  timeStartNightSec = prefs.getUInt("nst", 0);
  timeStopNightSec = prefs.getUInt("nsp", 0);
  Serial.print("  timeStartNightSec = ");
  Serial.println(timeStartNightSec);
  Serial.print("  timeStopNightSec = ");
  Serial.println(timeStopNightSec);
}

void restoreSettings()
{
  Serial.println("restoreSettings");
  allowAuthoNight = prefs.getUInt("nal", 0);
  timeStartNightSec = prefs.getUInt("nst", 0);
  timeStopNightSec = prefs.getUInt("nsp", 0);
  Serial.print("  timeStartNightSec ");
  Serial.println(timeStartNightSec);

  restoreDefaultTemp();
  restoreSetsTemp();
}

void setup()
{
  isSetup = true;
  // Debug console
  Serial.begin(9600);

  //Blynk.begin(blynkToken, ssid, pass);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.begin(ssid, pass);
  Blynk.config(blynkToken);
  if (Blynk.connect())
  {
    Serial.printf("[%8lu] setup: Blynk connected\r\n", millis());
  }
  else
  {
    Serial.printf("[%8lu] setup: Blynk no connected\r\n", millis());
  }
  Serial.printf("[%8lu] Setup: Start timer reconnected\r\n", millis());
  numTimerReconnect = timer.setInterval(60000, ReconnectBlynk);
  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), detectsMotion, RISING);
  // Clear the terminal content
  terminal.clear();
  // Send e-mail when your hardware gets connected to Blynk Server
  // Just put the recepient's "e-mail address", "Subject" and the "message body"
  Blynk.email("LLDmitry@yandex.ru", "Subject", "My Blynk project is online!.");
  // Get the NTP time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  everyHourTimer();
  everyMinTimer();
  getWeatherData();
  //String sTime = &timeinfo, "%A, %B %d %Y %H:%M:%S";
  Blynk.notify("Device started ");
  timer.setInterval(600000L, refreshAllTemperatures); //10minutes
  timer.setInterval(3600000L, everyHourTimer); //sync time
  timer.setInterval(60000L, everyMinTimer);    //incremet inner time
  timer.setInterval(10000L, every10SecTimer);    //checkAlarm
  //eeprom analog
  prefs.begin("nvs", false);
  //backupDefaultTemp(); //1st time, comment after

  restoreSettings();

  pinMode(BZZ_PIN, OUTPUT);
  RadioSetup();

  bool status;
  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  status = bmp.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  else
  {
    Serial.println("BME280 sensor connected");
  }

  tempSensors.begin();
  tempSensors.getAddress(outer1TempDeviceAddress, 1);
  tempSensors.setResolution(outer1TempDeviceAddress, 12);

  for (byte iRoom = 0; iRoom < 5; iRoom++)
  {
    modeVent[iRoom] = V_AUTO_OFF;
  }

  isSetup = false;
}

void RadioSetup()
{
  //RF24
  radio.begin();                          // Включение модуля;
  delay(20);
  radio.enableAckPayload();       //+
  radio.setPayloadSize(32);
  radio.setChannel(ArRoomsChannelsNRF[ROOM_SENSOR]);             // Установка канала вещания;
  radio.setRetries(10, 10);               // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);            // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_LOW);            // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(RoomReadingPipe);   // Активация данных для отправки
  radio.openReadingPipe(1, CentralReadingPipe);    // Активация данных для чтения
  radio.startListening();

  //radio.printDetails();
}

void ReadCommandNRF() //from reponse
{
  Serial.println("                                     ReadCommandNRF()");
  if ( radio.available() )
  {
    bool done = false;
    Serial.println("radio.available!!");
    radio.read(&nrfResponse, sizeof(nrfResponse));
    delay(20);
    radio.flush_rx();
    Serial.print("received data from room: ");
    Serial.println(nrfResponse.roomNumber);
    Serial.print("received Command: ");
    Serial.println(nrfResponse.Command);
    if (nrfResponse.Command != RSP_NO)
    {
      ParseAndHandleInputNrfCommand();
    }
  }
}

void NrfCommunication()
{
  ResetExternalData();
  if (lastNrfCommunication_ms > NRF_COMMUNICATION_INTERVAL_S * 1000)
  {
    //    radio.setChannel(ArRoomsChannelsNRF[ROOM_SENSOR]);
    //    radio.openWritingPipe(ArRoomsReadingPipes[ROOM_SENSOR]); //for confirm writes
    //ReadCommandNRF(); // from ROOM_SENSOR                                         пока отключил!!!
    Serial.println("                       radio.stopListening();");
    radio.stopListening();
    //d for (byte iRoom = 0; iRoom < ROOM_SENSOR; iRoom++)
    for (byte iRoom = 0; iRoom < 3; iRoom++)
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
          if (alarmStatus[iCheckRoom] > alarmStatusNotification[iNotifRoom][0]) //если в iCheckRoom более высокий приоритет Alarm чем был ранее
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
  //nrfRequest.tOut = t_out;
  nrfRequest.tOut = random(30);
  nrfRequest.tOutDec = random(9);
  Serial.print("roomNumber    ");
  Serial.println(roomNumber);
  Serial.println(nrfRequest.tOut);
  Serial.println(p_v);
  nrfRequest.p = (int)((p_v - 6000) / 10);
  nrfRequest.pDec = (p_v - 6000) % 10;
  nrfRequest.tInnSet = t_set[roomNumber];
  Serial.print("nrfRequest.tInnSet= ");
  Serial.println(nrfRequest.tInnSet);

  nrfRequest.tInnSetVal1 = setsTemp[roomNumber][0];
  nrfRequest.tInnSetVal2 = setsTemp[roomNumber][1];
  nrfRequest.tInnSetVal3 = setsTemp[roomNumber][2];
  nrfRequest.tInnSetVal4 = setsTemp[roomNumber][3];
  nrfRequest.tInnSetVal5 = setsTemp[roomNumber][4];

  //nrfRequest.nagrevStatus = nagrevStatus[roomNumber];
  if (roomNumber == ROOM_VENT)
    switch (modeVent[ROOM_BED])
    {
      case V_AUTO_SPEED1:
      case V_SPEED1:
        nrfRequest.ventSpeed = 1;
        break;
      case V_AUTO_SPEED2:
      case V_SPEED2:
        nrfRequest.ventSpeed = 2;
        break;
      case V_AUTO_SPEED3:
      case V_SPEED3:
        nrfRequest.ventSpeed = 3;
        break;
      default:
        nrfRequest.ventSpeed = 0;
    }
  nrfRequest.hours = tHour;
  nrfRequest.minutes = tMinute;
  nrfRequest.alarmMaxStatus = alarmStatusNotification[roomNumber][0];
  // nrfRequest.alarmMaxStatusRoom = alarmStatusNotification[roomNumber][1];
  if (roomNumber == 0)
  {
    Serial.println("time:");
    Serial.println(tHour);
    Serial.println(tMinute);
  }
}

void SendCommandNRF(byte roomNumber)
{
  PrepareRequestCommand(roomNumber);

  Serial.print("send to roomNumber: ");
  Serial.println(roomNumber);
  radio.setChannel(ArRoomsChannelsNRF[roomNumber]);
  if (radio.write(&nrfRequest, sizeof(nrfRequest)))
  {
    Serial.println("Success Send");
    delay(20);
    if (!radio.isAckPayloadAvailable() )   // Ждем получения..
      Serial.println(F("Empty response."));
    else
    {
      radio.read(&nrfResponse, sizeof(nrfResponse));
      delay(20);
      radio.flush_rx();
      Serial.print(F("RESPONSE! from "));
      Serial.println(nrfResponse.roomNumber);
      ParseAndHandleInputNrfCommand();
    }
  }
  else
  {
    Serial.println("Failed Send");
  }
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
    t_out1 = t_out1 + (nrfResponse.tOutDec / 10);
    if (nrfResponse.tOutSign == 1)
    {
      t_out1 = t_out1 * (-1);
    }
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
  //      h[nrfResponse.roomNumber] = nrfResponse.h;
  //      t_set[nrfResponse.roomNumber] = nrfResponse.t_set;
  //
  //      switch (nrfResponse.alarmType) // { ALR_NO, ALR_VODA, ALR_DOOR }
  //      {
  //        case ALR_VODA:
  //          InformCall(nrfResponse.roomNumber == ROOM_VANNA1 ? CI_VODA1 : CI_VODA2);
  //          break;
  //        case ALR_DOOR:
  //          InformCall(nrfResponse.roomNumber == ROOM_HALL ? CI_ALARM1 : CI_ALARM2);
  //          break;
  //      }
  //      break;
  //
  //    case IN_ROOM_COMMAND:
  //V_TO_AUTO, V_AUTO_SPEED1, V_AUTO_SPEED2, V_AUTO_OFF, V_TO_SPEED1, V_TO_SPEED2, V_SPEED1, V_SPEED2, V_TO_OFF, V_OFF
  //  switch (nrfResponse.ventSpeed) //0-not supported, 1,2,3: 1-3st speed, 10 - off, 100 - auto
  //  {
  //    case 10: //off
  //      if (modeVent[nrfResponse.roomNumber] != V_OFF)
  //        modeVent[nrfResponse.roomNumber] = V_TO_OFF;
  //      break;
  //    case 1: //1st speed
  //      if (modeVent[nrfResponse.roomNumber] != V_SPEED1)
  //        modeVent[nrfResponse.roomNumber] = V_TO_SPEED1;
  //      break;
  //    case 2: //2d speed
  //      if (modeVent[nrfResponse.roomNumber] != V_SPEED2)
  //        modeVent[nrfResponse.roomNumber] = V_TO_SPEED2;
  //      break;
  //    case 3: //3d speed
  //      if (modeVent[nrfResponse.roomNumber] != V_SPEED3)
  //        modeVent[nrfResponse.roomNumber] = V_TO_SPEED3;
  //      break;
  //    case 100: //auto
  //      if (modeVent[nrfResponse.roomNumber] != V_AUTO_SPEED1 &&
  //          modeVent[nrfResponse.roomNumber] != V_AUTO_SPEED2 &&
  //          modeVent[nrfResponse.roomNumber] != V_AUTO_SPEED3 &&
  //          modeVent[nrfResponse.roomNumber] != V_AUTO_OFF)
  //      {
  //        modeVent[nrfResponse.roomNumber] = V_TO_AUTO;
  //      }
  //      break;
  //  }
  //  Serial.print("modeVent= ");
  //  Serial.println(modeVent[nrfResponse.roomNumber]);
  //      break;
  //
  //    case IN_CENTRAL_COMMAND:
  //      switch (nrfResponse.scenarioVent) //0-reset, 1, 2, 3
  //      {
  //        case 0: //off
  //          ventMode = REQUEST_RESET_VENT;
  //          break;
  //        case 1: //1st scenario
  //          ventMode = REQUEST_SCENARIO1_VENT;
  //          break;
  //        case 2:
  //          ventMode = REQUEST_SCENARIO2_VENT;
  //          break;
  //        case 3:
  //          ventMode = REQUEST_SCENARIO3_VENT;
  //          break;
  //      }
  //      switch (nrfResponse.scenarioNagrev) //0-reset, 1, 2, 3
  //      {
  //        case 0: //off
  //          nagrevMode = REQUEST_RESET_NAGREV;
  //          break;
  //        case 1: //1st scenario
  //          nagrevMode = REQUEST_SCENARIO1_NAGREV;
  //          break;
  //        case 2:
  //          nagrevMode = REQUEST_SCENARIO2_NAGREV;
  //          break;
  //        case 3:
  //          nagrevMode = REQUEST_SCENARIO3_NAGREV;
  //          break;
  //      }
  //      break;
  //  }
}

void RefreshSensorData()
{
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {

    t_out = random(30);
    //    tempSensors.requestTemperatures();
    //    Serial.println("A1");
    //    t_out1 = tempSensors.getTempCByIndex(0);
    //    Serial.println("A2");
    //    t_inn[ROOM_GOST] = tempSensors.getTempCByIndex(1);
    //    Serial.println("A3");
    //t_inn = tempSensors.getTempC(innerTempDeviceAddress);
    //t_out1 = tempSensors.getTempC(outer1TempDeviceAddress);
    //t_vent = tempSensors.getTempC(ventTempDeviceAddress);
    //t_inn[ROOM_GOST] = tempSensors.getTempC(unitTempDeviceAddress);
    //    h[ROOM_GOST] = dht.readHumidity();
    //    t_inn[ROOM_GOST] = dht.readTemperature();
    //t_vent = t_out1;
    //t_out = (t_out2 < t_out1 || t_out1 < -100) && t_out2 > -100 ? t_out2 : t_out1;
    Serial.print("t_out= ");
    Serial.println(t_out);
    Serial.print("Temperature = ");
    Serial.print(bmp.readTemperature());
    Serial.println(" *C");

    p_v = 0.075 * bmp.readPressure();
    Serial.print("Pressure*10 = ");
    Serial.print(p_v);
    Serial.println(" мм Рт ст");
    lastRefreshSensor_ms = 0;
    Serial.println("                      DONE RefreshSensorData");
  }
}

// V_TO_AUTO, V_AUTO_SPEED1, V_AUTO_SPEED2, V_AUTO_OFF, V_TO_SPEED1, V_TO_SPEED2, V_SPEED1, V_SPEED2, V_TO_OFF, V_OFF
//void VentControl()
//{
//  if (ventCorrectionPeriod_ms > VENT_CORRECTION_PERIOD_S * 1000)
//  {
//    if (co2[ROOM_BED] == 0)
//      return;
//    float kT = 1;
//    if (t_out < -20)  //too cold
//      kT = 1.5;
//    else if (t_out < 30)
//      kT = (-4 * t_out + 525) / (float)arCO2Levels[1];
//    else  //too hot, >30
//      kT = 1.5;
//
//    //  Serial.print("modeVent=");
//    //  Serial.println(modeVent[ROOM_BED]);
//    //  Serial.print("t_vent=");
//    //  Serial.println(t_vent);
//    //  Serial.print("t_inn=");
//    //  Serial.println(t_inn[ROOM_BED]);
//    //  Serial.print("co2=");
//    //  Serial.println(co2[ROOM_BED]);
//
//    //for (byte i = 0; i < 5; i++)
//    switch (modeVent[ROOM_BED])
//    {
//      case V_TO_AUTO:
//      case V_AUTO_SPEED1:
//      case V_AUTO_SPEED2:
//      case V_AUTO_SPEED3:
//      case V_AUTO_OFF:
//        if (co2[ROOM_BED] > (arCO2Levels[3] * kT))
//          modeVent[ROOM_BED] = V_AUTO_SPEED2;
//        if (co2[ROOM_BED] > (arCO2Levels[2] * kT))
//          modeVent[ROOM_BED] = V_AUTO_SPEED2;
//        else if (co2[ROOM_BED] > (arCO2Levels[1] * kT))
//          modeVent[ROOM_BED] = V_AUTO_SPEED1;
//        else
//        {
//          Serial.print("      t_out=");
//          Serial.println(t_out);
//          Serial.print("      t_inn=");
//          Serial.println(t_inn[ROOM_BED]);
//          Serial.print("      kT=");
//          Serial.println(kT);
//          modeVent[ROOM_BED] = V_AUTO_OFF;
//        }
//        Serial.print("      CO2=");
//        Serial.println(co2[ROOM_BED]);
//
//        //reduce speed or off if too cold in room
//        if (t_inn[ROOM_BED] < (t_out < BORDER_WINTER_SUMMER ? MIN_COMFORT_ROOM_TEMP_WINTER : MIN_COMFORT_ROOM_TEMP_SUMMER) && t_inn[ROOM_BED] > t_vent)
//        {
//          if (modeVent[ROOM_BED] >= V_AUTO_SPEED1 && modeVent[ROOM_BED] > V_AUTO_SPEED1)
//            modeVent[ROOM_BED] = (EnModeVent)(modeVent[ROOM_BED] - 1);
//        }
//
//        //increase speed if too hot in room
//        if (t_inn[ROOM_BED] > MAX_COMFORT_ROOM_TEMP && t_inn[ROOM_BED] > t_out)
//        {
//          if (modeVent[ROOM_BED] >= V_AUTO_OFF && modeVent[ROOM_BED] < V_AUTO_SPEED2)
//            modeVent[ROOM_BED] = (EnModeVent)(modeVent[ROOM_BED] + 1);
//        }
//
//        break;
//
//      case V_TO_OFF:
//        modeVent[ROOM_BED] = V_OFF;
//        break;
//      case V_TO_SPEED1:
//        modeVent[ROOM_BED] = V_SPEED1;
//        break;
//      case V_TO_SPEED2:
//        modeVent[ROOM_BED] = V_SPEED2;
//        break;
//      case V_TO_SPEED3:
//        modeVent[ROOM_BED] = V_SPEED3;
//        break;
//    }
//
//    bool speed1 = (modeVent[ROOM_BED] == V_SPEED1 || modeVent[ROOM_BED] == V_AUTO_SPEED1);
//    bool speed2 = (modeVent[ROOM_BED] == V_SPEED2 || modeVent[ROOM_BED] == V_AUTO_SPEED2);
//
//    nrfRequest.Command = RQ_VENT;
//    nrfRequest.ventSpeed = speed1 ? 1 : speed2 ? 2 : 0;
//
//    ventCorrectionPeriod_ms = 0;
//
//    Serial.print("modeVent[ROOM_BED]");
//    Serial.println(modeVent[ROOM_BED]);
//  }
//}


BLYNK_CONNECTED()
{
  Blynk.syncAll();
  Blynk.virtualWrite(VP_ALARM_DETAILS, "Connected");
  Serial.println("BLYNK_CONNECTED");
}

BLYNK_WRITE(VP_MODE)
{
  changeHomeMode((homeMode_enum)(param.asInt() - 1), false);
}

void changeHomeMode(homeMode_enum newHomeMode, bool authoChange)
{
  Serial.print("changeHomeMode = ");
  homeMode = newHomeMode;
  Serial.println(homeMode);
  for (int i = 0; i <= ROOMS_NUMBER - 1; i++) {
    if (defaultTemp[homeMode][i] != 100 && t_set[i] != defaultTemp[homeMode][i])
    {
      t_set[i] = defaultTemp[homeMode][i];
      Serial.print("room ");
      Serial.println(i);
      Serial.print("set_temp_room=");
      Serial.println(t_set[i]);
      if (t_set[i] == 0) //0 -switch off heater
      {
        Serial.println("stop heat");
        heat_control_room[i] = 0;
      }
      else
        heat_control_room[i] = 1;
    }
    prevTempSet = 0; //reset on room select or mode select
  }

  setRoomHeatStatus(true, true);
  if (authoChange)
  {
    Blynk.virtualWrite(VP_MODE, homeMode + 1);
  }
  displayCurrentRoomHeaterTemperature();
  displayCurrentRoomHeatBtn();
}

void displayCurrentRoomHeaterTemperature()
{
  Serial.println("displayCurrentRoomHeaterTemperature");
  Blynk.virtualWrite(VP_ROOM_TMP_SET, t_set[currentRoom - 1]);
}

void displayCurrentRoomHeatBtn()
{
  Serial.print("displayCurrentRoomHeatBtn ");
  Serial.println(heat_control_room[currentRoom - 1]);
  Blynk.virtualWrite(VP_ROOM_HEAT_BTN, heat_control_room[currentRoom - 1]);
}

BLYNK_WRITE(VP_TMP_BTN_REFRESH)
{
  int btnVal = param.asInt();
  if (btnVal == HIGH)
  {
    refreshTemperature(true, true);
  }
}

void refreshAllTemperatures() {
  refreshTemperature(true, false);
}

void every10SecTimer() {
  //d  if (motionDetected && homeMode == GUARD_MODE)
  //  {
  //    // Blynk.virtualWrite(VP_ALARM_STATUS, HIGH);
  //    String lbl = "Тревога " + (String)tHour + ":" + (tMinute < 10 ? "0" : "") + (String)tMinute;
  //    Blynk.setProperty(VP_ALARM_BTN, "onLabel", lbl);
  //    Blynk.notify(lbl);
  //    Blynk.virtualWrite(VP_ALARM_BTN, HIGH);
  //  }
  motionDetected = false;
}

void everyMinTimer() {
  if (!freshTime)
  {
    Serial.println("             everyMinTimer");
    tMinute += 1;
    if (tMinute == 60)
    {
      tHour += 1;
      tMinute = 0;
    }
    if (tHour == 24)
    {
      tHour = 0;
    }
    Serial.println(tHour);
    Serial.println(tMinute);
  }

  checkTimer();
  freshTime = false;
}

void checkTimer() {
  Serial.println("checkTimer");
  long nowSec = 3600 * tHour + 60 * tMinute;
  Serial.println(nowSec);
  Serial.println(allowAuthoNight);
  Serial.println(homeMode);
  Serial.println(HOME_MODE);
  Serial.println(nowSec);
  Serial.println(timeStartNightSec);
  if (allowAuthoNight && (homeMode == HOME_MODE || homeMode == GUESTS_MODE) && nowSec == timeStartNightSec)
  {
    Serial.println("START NIGHT");
    homeModeBeforeNight = homeMode;
    changeHomeMode(NIGHT_MODE, true);
  }

  if (homeMode == NIGHT_MODE && nowSec == timeStopNightSec)
  {
    Serial.println("STOP NIGHT");
    changeHomeMode(homeModeBeforeNight, true);
  }
}

void everyHourTimer() {
  Serial.println("everyHourTimer");
  refreshLocalTime();
  freshTime = true;
}

//from application, every 2 minutes
BLYNK_READ(VP_TMP_OUT)
{
  Serial.println("                                                                              Blynk.virtualWrite(VP_TMP_OUT");
  //refreshTemperature(true, true);
  displayCurrentData();
}

void displayCurrentData()
{
  Serial.println("displayCurrentData");
  Blynk.virtualWrite(VP_TMP_IN, t_inn[0]);
  Blynk.virtualWrite(VP_TMP_OUT, t_out);
  Blynk.virtualWrite(VP_TMP_BTN_REFRESH, LOW);

  displayCurrentRoomInfo();
  displayPowerHeatersLevel();
}

void refreshTemperature(bool allRooms, bool displayData) {
  Serial.println("refreshTemperatures");

  if (allRooms)
  {
    for (int i = 0; i <= ROOMS_NUMBER - 1; i++) {
      t_inn[i] = random(-5, 35);
    }
    t_out = random(-20, 35);
  }
  else
  {
    t_inn[currentRoom - 1] = random(-5, 35);
  }

  setRoomHeatStatus(allRooms, false);
  if (displayData)
  {
    displayCurrentData();
  }
}

BLYNK_WRITE(VP_ROOM_SELECT)
{
  Serial.println("VP_ROOM_SELECT");
  byte prevCurrentRoom = currentRoom;
  currentRoom = param.asInt();
  displayCurrentRoomInfo();
  displayCurrentRoomTemperatuteSet(prevCurrentRoom);
  displayCurrentRoomHeaterTemperature();
  displayCurrentRoomHeatBtn();
  Serial.println("Room=" + currentRoom);
  prevTempSet = 0; //reset on room select or mode select
}

void displayCurrentRoomTemperatuteSet(byte prevCurrentRoom)
{
  if (setsTemp[prevCurrentRoom - 1][0] != setsTemp[currentRoom - 1][0] ||
      setsTemp[prevCurrentRoom - 1][1] != setsTemp[currentRoom - 1][1] ||
      setsTemp[prevCurrentRoom - 1][2] != setsTemp[currentRoom - 1][2] ||
      setsTemp[prevCurrentRoom - 1][3] != setsTemp[currentRoom - 1][3] ||
      setsTemp[prevCurrentRoom - 1][4] != setsTemp[currentRoom - 1][4]
     )
  {
    Blynk.setProperty(VP_ROOM_TMP_SET, "labels",
                      setsTemp[currentRoom - 1][0],
                      setsTemp[currentRoom - 1][1],
                      setsTemp[currentRoom - 1][2],
                      setsTemp[currentRoom - 1][3],
                      setsTemp[currentRoom - 1][4]
                     );
  }
}

BLYNK_WRITE(VP_ROOM_HEAT_BTN)
{

  //  if (param.asInt() == 1 && t_set[currentRoom - 1] == 0) //try to push but T was not selected before -> unpush
  //  {
  //    Blynk.virtualWrite(VP_ROOM_HEAT_BTN, LOW);
  //    return;
  //  }
  //
  //  heat_control_room[currentRoom - 1] = param.asInt();
  //setRoomHeatStatus(false, true);
  if (param.asInt() == 0)
  {
    Serial.println("VP_ROOM_HEAT_BTN=0");
    prevTempSet = t_set[currentRoom - 1]; // for restoring in future
    roomTempSet(0);
  }
  else
  {
    Serial.println("VP_ROOM_HEAT_BTN=1");
    roomTempSet(prevTempSet > 0 ? prevTempSet : 1); //restore
  }
  displayCurrentRoomHeaterTemperature();
}

BLYNK_WRITE(VP_TERMINAL)
{
  String command = param.asStr();

  terminal.print("You said:");
  terminal.write(param.getBuffer(), param.getLength());
  terminal.println();

  execTerminalCommands(command);

  // Ensure everything is sent
  terminal.flush();
}

BLYNK_WRITE(VP_SHOW_SETTINGS_BTN)
{
  if (param.asInt())
  {
    terminal.println("Next settings");
    getWeatherData();
    terminal.flush();
  }
}

BLYNK_WRITE(VP_ALARM_BTN)
{
  Blynk.virtualWrite(VP_ALARM_STATUS, 0);
}

BLYNK_WRITE(VP_AUTO_NIGHT_BTN)
{
  if (allowChangeSettings || isSetup)
  {
    Serial.println("ALLOW");
    allowAuthoNight = param.asInt();
    backupSettings();
    if (allowAuthoNight)
      terminal.println("VP_AUTO_NIGHT_BTN On");
    else
      terminal.println("VP_AUTO_NIGHT_BTN Off");
    terminal.flush();

    Blynk.setProperty(VP_AUTO_NIGHT_START, "color", allowAuthoNight ? "#6b8e23" : "#877994");
    Blynk.setProperty(VP_AUTO_NIGHT_STOP, "color", allowAuthoNight ? "#6b8e23" : "#877994");
  }
  else
  {
    //reset changes
    Serial.println("NOT ALLOW");
    Blynk.virtualWrite(VP_AUTO_NIGHT_BTN, allowAuthoNight);
  }
}

BLYNK_WRITE(VP_AUTO_NIGHT_START)
{
  if (allowChangeSettings || isSetup)
  {
    Serial.println("ALLOW");
    timeStartNightSec = param[0].asLong();
    allowAuthoNight = true;
    Blynk.virtualWrite(VP_AUTO_NIGHT_BTN, 1);
    Serial.println(timeStartNightSec);
    backupSettings();
  }
  else
  {
    //reset changes
    Serial.println("NOT ALLOW");
    char tz[] = "Europe/Moscow";
    char days[] = "1";
    Blynk.virtualWrite(VP_AUTO_NIGHT_START, timeStartNightSec, 0, tz, days);
  }
}

BLYNK_WRITE(VP_AUTO_NIGHT_STOP)
{
  if (allowChangeSettings || isSetup)
  {
    Serial.println("ALLOW");
    timeStopNightSec = param[0].asLong();
    allowAuthoNight = true;
    Blynk.virtualWrite(
      VP_AUTO_NIGHT_BTN, 1);
    Serial.println(timeStopNightSec);
    backupSettings();
  }
  else
  {
    //reset changes
    Serial.println("NOT ALLOW");
    char tz[] = "Europe/Moscow";
    char days[] = "1";
    Blynk.virtualWrite(VP_AUTO_NIGHT_STOP, timeStopNightSec, 0, tz, days);
  }
}

void setRoomHeatStatus(bool allRooms, bool dispalyData)
{
  Serial.println("setRoomHeatStatus");
  if (allRooms)
  {
    Serial.println("allRooms");
    for (int i = 0; i <= ROOMS_NUMBER - 1; i++) {
      heat_status_room[i] = heat_control_room[i];
      if (checkHeatSwitch(i + 1))
      {
        heat_status_room[i] = 2;
      }
    }
  }
  else
  {
    heat_status_room[currentRoom - 1] = heat_control_room[currentRoom - 1];
    if (checkHeatSwitch(currentRoom))
    {
      heat_status_room[currentRoom - 1] = 2;
    }
  }
  if (dispalyData)
  {
    displayCurrentRoomInfo();
    displayPowerHeatersLevel();
  }
}

bool checkHeatSwitch(byte room)
{
  //  if (room == 1)
  //  {
  //    Serial.println("checkHeatSwitch");
  //    Serial.print("t_inn[ ");
  //    Serial.println(t_inn[room - 1]);
  //    Serial.print("set_temp_room ");
  //    Serial.println(t_set[room - 1]);
  //  }
  //return (heat_status_room[room - 1] == 1 && t_inn[room - 1] < setsTemp[room - 1][t_set[room - 1] - 1]); //включить нагрев
  return true;
}

void displayPowerHeatersLevel()
{
  Serial.println("displayPowerHeatersLevel");
  //calc maxPowerHeater;
  float totalPowerHeaters = 0;
  float maxPowerHeater = 0.0;
  for (int i = 0; i <= ROOMS_NUMBER - 1; i++) {
    maxPowerHeater += powerHeaters[i];
    if (heat_status_room[i] == 2)
      totalPowerHeaters += powerHeaters[i];
  }
  byte calcTo100TotalPowerHeaters = (float)(100 * (totalPowerHeaters / maxPowerHeater));
  Serial.println(totalPowerHeaters);
  Serial.println(maxPowerHeater);
  Serial.println(calcTo100TotalPowerHeaters);
  Blynk.virtualWrite(VP_POWER_HEATER_LEVEL, calcTo100TotalPowerHeaters);
}

BLYNK_WRITE(VP_ROOM_TMP_SET)
{
  Serial.println("VP_ROOM_TMP_SET");
  roomTempSet(param.asInt());
}

void roomTempSet(byte tSet)
{
  Serial.print("roomTempSet() ");
  Serial.println(tSet);
  t_set[currentRoom - 1] = tSet;
  defaultTemp[homeMode][currentRoom - 1] = t_set[currentRoom - 1];
  char a[3];
  ((String)homeMode + (String)(currentRoom - 1)).toCharArray(a, 3);
  prefs.putUInt(a, defaultTemp[homeMode][currentRoom - 1]);

  heat_control_room[currentRoom - 1] = (tSet > 0);
  setRoomHeatStatus(false, true);
  displayCurrentRoomHeatBtn();
}

void displayCurrentRoomInfo()
{
  Serial.print("displayCurrentRoomInfo ");
  Serial.println(heat_status_room[currentRoom - 1]);
  Blynk.virtualWrite(VP_ROOM_TMP, t_inn[currentRoom - 1]);
  switch (heat_status_room[currentRoom - 1])
  {
    case 0:
      Blynk.setProperty(VP_ROOM_TMP, "color", "#0047AB");
      Blynk.virtualWrite(VP_ROOM_HEAT_BTN, LOW);
      break;
    case 1:
      Blynk.setProperty(VP_ROOM_TMP, "color", "#FFB317");
      break;
    case 2:
      Blynk.setProperty(VP_ROOM_TMP, "color", "#D3435C");
      break;
    case 3:
      Blynk.setProperty(VP_ROOM_TMP, "color", "#FF0000");
      Blynk.virtualWrite(VP_ROOM_HEAT_BTN, LOW);
      break;
  }
  Blynk.virtualWrite(VP_ROOM_TMP, t_inn[currentRoom - 1]);
}

BLYNK_WRITE(VP_PASSWORD_TXT)
{
  if (param.asInt() == 123)
  {
    Serial.println("        PASSWORD CORRECT!!!");
    Blynk.virtualWrite(VP_PASSWORD_TXT, "***");
    allowChangeSettings = true;
    timer.setTimeout(30000L, stopAllowChangeSettingsTimer);    //stop ability to change admin settings
  }
  else
    Serial.println("        PASSWORD NOT CORRECT");
}

void refreshLocalTime()
{
  for (int i = 1; i <= 3; i++) {
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      if (i == 3)
        return;
    }
    else
      break;
  }

  time_t now;
  time(&now);
  long g = now;
  Serial.println(now);

  Serial.println(timeinfo.tm_hour);
  Serial.println(timeinfo.tm_min);
  Serial.println(timeinfo.tm_sec);
  tHour = timeinfo.tm_hour;
  tMinute = timeinfo.tm_min;
}

//уст
//  к1=22
//  ночь к1т=19,к2т=2,к5т=21
//  ночь к1=19,к2=2,к5=21
//  дома к1т=22,к2т=21,к5т=21
//  гости к1=19,к2=2,k4=22,к5=21
// уст ночь к1т9 к2т2 к5т21
// уст дома к1т19 к2т2 к3т22 к5т21

// к2 т3 т15 т22 т23 т24
// уст к2 т3 т15 т22 т23 т24

//инф
//  ком
//=> к1 гостинная
//   к2 кухня
// ....
//  к1
//> тем=8 нагрев=22

const String setWord = "уст";
const String infWord = "инф";

const String waitWord = "ушел";
const String stopWord = "стоп";
const String homeWord = "дома";
const String guestsWord = "гости";
const String nightWord = "ночь";

const String roomWord = "к";

void execTerminalCommands(String command)
{
  Serial.print("defaultTemp prev");
  Serial.println(defaultTemp[2][1]);

  int str_len = command.length() + 1;
  char char_array[str_len];
  char *str;

  String typeComnand;
  homeMode_enum keyCommand;
  byte keyRoom;

  command.toCharArray(char_array, str_len);
  char *p = char_array;
  int i = 0;
  while ((str = strtok_r(p, " ", &p)) != NULL)
  {
    if (str == " ") {
      continue;
    }
    i++;
    Serial.println(str);
    if (i == 1)
    {
      typeComnand = str;
      typeComnand.toLowerCase();
    }
    else
    {
      if (i == 2)
      {
        if (typeComnand == setWord)
        {
          keyCommand = getKeyCommand(str);
          if (keyCommand == SET_ROOM_TEMP_MODE)
          {
            keyRoom = getRoom(str); //для команд вида    уст к2 т3 т15 т22 т23 т24
            Serial.print("keyRoom: ");
            Serial.println(keyRoom);
          }
        }
      }
      else
      {
        Serial.print("keyCommand: ");
        Serial.println(keyCommand);
        Serial.print("getRoom(str): ");
        Serial.println(getRoom(str));
        Serial.print("getValue(str): ");
        Serial.println(getValue(str));
        Serial.print("keyRoom: ");
        Serial.println(keyRoom);

        if (keyCommand == SET_ROOM_TEMP_MODE) //настройка шкалы температуры в комнате
        {
          setsTemp[keyRoom][i - 3] = getValue(str);
          //prefs.putUInt(a, defaultTemp[keyCommand][getRoom(str)]);
        }
        else  // установка значений температуры для режима по комнатам
        {
          defaultTemp[keyCommand][getRoom(str)] = getValue(str);
          if (keyCommand == homeMode)
          {
            changeHomeMode(homeMode, false); //чтобы стразу применилось если текущий mode = настраиваемому
          }
        }
      }
    }
  }

  if (keyCommand == SET_ROOM_TEMP_MODE) //установка шкалы температуры в комнате
  {
    // backup шкалы температуры в настраиваемой комнате
    backupSetsTempOneRoom(keyRoom);
  }
  else  // backup установленных значений температуры для настраиваемого режима по комнатам
    backupDefaultTempOneMode(keyCommand);

  //  if (command.startsWith(setWord))
  //  {
  //    if (command.startsWith(nightWord))
  //    {
  //
  //    }
  //    else if (command.startsWith(homeWord))
  //    {
  //
  //    }
  //  }
  //  else if (command.startsWith(infWord))
  //  {
  //
  //  }
  Serial.print("defaultTemp post");
  Serial.println(defaultTemp[2][1]);
}

homeMode_enum getKeyCommand(String key)
{
  key.toLowerCase();
  if (key.startsWith(waitWord)) return GUARD_MODE;
  if (key.startsWith(stopWord)) return STOP_MODE;
  if (key.startsWith(homeWord)) return HOME_MODE;
  if (key.startsWith(guestsWord)) return GUESTS_MODE;
  if (key.startsWith(nightWord)) return NIGHT_MODE;
  if (key.startsWith(roomWord)) return SET_ROOM_TEMP_MODE;
}

byte getRoom(String key) //key=  к3  или к3т23
{
  //Serial.print("getRoom ");
  //Serial.println(key.length());
  //Serial.println(key);
  int r = key.substring(2, 3).toInt() - 1;
  //Serial.print("r: ");;
  //Serial.println(r);
  return r;
}

byte getValue(String key)
{ 
  key.toLowerCase();
  Serial.print("getValue ");
  Serial.println(key.length());
  Serial.println(key);
  byte val;
  if (key.substring(3, 4) == "т")   //key = к3т19
    val = key.substring(4).toInt();
  else                              //key = т19
    val = key.substring(2).toInt();
  Serial.print("val= ");
  Serial.println(val);
  return val;
}

void getWeatherData() //client function to send/receive GET request data.
{
  String result = "";
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(weatherHost, httpPort)) {
    Serial.println("connection to openweather failed");
    cntFailedWeather++;
    return;
  }
  else {
    Serial.println("connection to openweather ok");
    cntFailedWeather = 0;
  }
  // We now create a URI for the request
  String url = "/data/2.5/weather?id=" + cityID + "&units=metric&cnt=1&APPID=" + APIKEY + weatherLang;

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + weatherHost + "\r\n" +
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      client.stop();
      return;
    }
  }

  // Read all the lines of the reply from server
  while (client.available()) {
    result = client.readStringUntil('\r');
  }

  result.replace('[', ' ');
  result.replace(']', ' ');

  char jsonArray [result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';

  StaticJsonBuffer<1024> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if (!root.success())
  {
    Serial.println("parseObject() failed");
  }

  weatherMain = root["weather"]["main"].as<String>();
  weatherDescription = root["weather"]["description"].as<String>();
  weatherDescription.toLowerCase();
  weatherLocation = root["name"].as<String>();
  country = root["sys"]["country"].as<String>();
  temp = root["main"]["temp"];
  humidity = root["main"]["humidity"];
  pressure = root["main"]["pressure"];

  windSpeed = root["wind"]["speed"];

  clouds = root["main"]["clouds"]["all"];
  String deg = String(char('~' + 25));
  weatherString =  "  Температура " + String(temp, 1) + "'C ";

  weatherString += " Влажность " + String(humidity) + "% ";
  weatherString += " Давление " + String(int(pressure / 1.3332239)) + "ммРтСт ";



  weatherString += " Ветер " + String(windSpeed, 1) + "м/с   ";

  if (clouds <= 10) cloudsString = "   Ясно";
  if (clouds > 10 && clouds <= 30) cloudsString = "   Малооблачно";
  if (clouds > 30 && clouds <= 70) cloudsString = "   Средняя облачность";
  if (clouds > 70 && clouds <= 95) cloudsString = "   Большая облачность";
  if (clouds > 95) cloudsString = "   Пасмурно";

  weatherString += cloudsString;

  Serial.println(weatherString);

  terminal.print("Weather: ");
  terminal.println(weatherString);

}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  isWiFiConnected = true;
  Serial.printf("[%8lu] Interrupt: Connected to AP, IP: ", millis());
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  isWiFiConnected = false;
  Serial.printf("[%8lu] Interrupt: Disconnected to AP!\r\n", millis());
}

void ReconnectBlynk(void)
{
  if (!Blynk.connected())
  {
    if (Blynk.connect())
    {
      Serial.printf("[%8lu] ReconnectBlynk: Blynk reconnected\r\n", millis());
    }
    else
    {
      Serial.printf("[%8lu] ReconnectBlynk: Blynk not reconnected\r\n", millis());
    }
  }
  else
  {
    Serial.printf("[%8lu] ReconnectBlynk: Blynk connected\r\n", millis());
  }
}

void BlynkRun(void)
{
  if (isWiFiConnected)
  {
    if (Blynk.connected())
    {
      if (timer.isEnabled(numTimerReconnect))
      {
        timer.disable(numTimerReconnect);
        Serial.printf("[%8lu] BlynkRun: Stop timer reconnected\r\n", millis());
      }
      Blynk.run();
    }
    else
    {
      if (!timer.isEnabled(numTimerReconnect))
      {
        timer.enable(numTimerReconnect);
        Serial.printf("[%8lu] BlynkRun: Start timer reconnected\r\n", millis());
      }
    }
  }
}

void stopAllowChangeSettingsTimer()
{
  Blynk.virtualWrite(VP_PASSWORD_TXT, "###");
  allowChangeSettings = false;
}

void loop()
{

  BlynkRun();
  timer.run();

  RefreshSensorData();
  NrfCommunication(); //read / send / read response
  //VentControl();
  //wdt_reset();
}


//void receiveJsonDataBySerial()
//{
//  if (Serial.available() > 0)
//  {
//    StaticJsonBuffer<650> jsonBuffer;
//    JsonObject& root = jsonBuffer.parseObject(Serial);
//    temperatureInside = root["tempIn"];
//    temperatureOutside = root["tempOut"];
//    temperatureWater = root["tempW"];
//    const char* _currentDate = root["date"];
//    const char* _currentTime = root["time"];
//    currentDate = String(_currentDate);
//    currentTime = String(_currentTime);
//    manualModeSetPoint = root["manSetPoint"];
//    heaterStatus = root["heatSt"];
//    modeNumber = root["modeNumber"];
//    daySetPoint = root["daySetPoint"];
//    nightSetPoint = root["nightSetPoint"];
//    outsideLampMode = root["outLampMode"];
//    waterSetPoint = root["waterSetPoint"];
//    panicMode = root["panicMode"];
//  }
//}

//WiFi.begin(ssid, pass);
//delay(2000);
//if (WiFi.status() == WL_CONNECTED)
//{
//    Blynk.config(auth, IPAddress(89,31,107,158));
//    Blynk.connect();
//
//  Serial.print("\nIP address: ");
//    Serial.println(WiFi.localIP());
//
//    ESP.deepSleep(60000000*10); // Сон на 10 Минут
//}
//else
//{
//    Serial.print("HET WIFI, COH HA 1M");
//    ESP.deepSleep(60000000*10); // Сон на 10 Минут
//}

// void emailOnButtonPress()
// {
//   // *** WARNING: You are limited to send ONLY ONE E-MAIL PER 5 SECONDS! ***

//   // Let's send an e-mail when you press the button
//   // connected to digital pin 2 on your Arduino

//   int isButtonPressed = !digitalRead(17); // Invert state, since button is "Active LOW"

//   if (isButtonPressed) // You can write any condition to trigger e-mail sending
//   {
//     Serial.println("Button is pressed."); // This can be seen in the Serial Monitor

//     count++;

//     String body = String("You pushed the button ") + count + " times.";

//     Blynk.email("LLDmitry@yandex.ru", "Subject: Button Logger2", body);

//     // Or, if you want to use the email specified in the App (like for App Export):
//     //Blynk.email("Subject: Button Logger", "You just pushed the button...");
//   }
// }
