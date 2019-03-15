//Car_GSM_DTMF_MP3
#include <SoftwareSerial.h>
#include <elapsedMillis.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <mp3TF.h>
#include <EEPROM.h>
#include <TinyGPS.h>

#include "sav_button.h" // Библиотека работы с кнопками

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>

//Выводы Arduino, соответствующие аналоговым входам, имеют номера от 14 до 19. - pinMode(14, OUTPUT); digitalWrite(14, HIGH);

#define WAKE_UP_PIN 2
#define SOFT_RX_PIN 3
#define SOFT_TX_PIN 4
#define MP3_BUSY_PIN 9    // пин от BUSY плеера
#define U_220_PIN 5       // контроль наличия 220в, с конденсатором, чтобы не реагировать на иппульсы пропадания
#define ONE_WIRE_PIN 6    // DS18b20
#define ALARM_CHECK_PIN 7 // 1 при срабатывании сигнализации
#define IGNITION_PIN 8    // 1 при включении зажигания
#define ALARM_OUT_PIN 17   // включает тревогу
#define BTTN_PIN 10       // ручное управление командами
//#define OPEN_DOOR_PIN 11  // открыть дверь
//#define CLOSE_DOOR_PIN 12 // закрыть дверь
#define HEAT_PIN 13       // управление электроподогревом
#define ADD_DEVICE_PIN 20 // дополнительное устройство (pin A1)
#define MIC_PIN 15        // активация микрофона(т.е. переключение на микрофон вместо MP3) (pin A2)
#define REGISTRATOR_PIN 16// активация регистратора (pin A3)
#define BZZ_PIN 14        // (pin A7)

#define CAR_VOLT_PIN A4   // измерение напряжения бортовой сети

SButton btnControl(BTTN_PIN, 50, 700, 1500, 15000);

SoftwareSerial mySerialGSM(SOFT_RX_PIN, SOFT_TX_PIN);
SoftwareSerial mySerialMP3(11, 12);
TinyGPS gps;


const int T_COLD = -10;
const unsigned long HEAT_TIME1_M = 3; // > T_COLD C    времени работы нагревателя в зав-ти от Teng
const unsigned long HEAT_TIME2_M = 7; // < T_COLD C
const unsigned long REFRESH_SENSOR_INTERVAL_S = 100;
const unsigned long IN_UNKNOWN_ANSWER_PERIOD_S = 10;
const unsigned long IN_ANSWER_PERIOD_S = 5;
const unsigned long IN_UNKNOWN_CALL_TALK_PERIOD_S = 55;
const unsigned long IN_CALL_TALK_PERIOD_S = 40;
const unsigned long IN_DISCONNECT_PERIOD_S = 5;
const unsigned long OUT_TONE_DISCONNECT_PERIOD_S = 10;
const unsigned long OUT_NO_TONE_DISCONNECT_PERIOD_S = 15;
const unsigned long RECALL_NOANSWER_DISCONNECT_PERIOD_S = 20; //when recall without answer or without line
const unsigned long RECALL_TALK_DISCONNECT_PERIOD_S = 30; //when recall with answer
const unsigned long OUT_INFORM_PERIOD_S = 8;
const unsigned long OUTGOING_TALK_DISCONNECT_PERIOD_S = 30; //when recall with answer
const unsigned long RECALL_PERIOD_S = 5;
const unsigned long OUT_INFORM_PERIOD_1_S = 2;
const unsigned long OUT_INFORM_PERIOD_2_S = 3;
const unsigned long BLOCK_UNKNOWN_PHONES_PERIOD_S = 1800; //30min
const unsigned long MAX_ON_PERIOD_ADD_DEVICE_S = 3600; //60min ограничение максимального времени работы доп устройства
const unsigned long ON_PERIOD_REGISTRATOR = 180; //3min времени работы регистратора
const unsigned long AFTER_WAKE_UP_PERIOD_S = 15; //на принятие входящего звонка и определение номера

const byte INDEX_ALARM_PNONE = 1;               //index in phonesEEPROM[5]
const byte MAX_NUMBER_ATTEMPTS_UNKNOWN_PHONES = 3;  //После MAX_NUMBER_ATTEMPTS_UNKNOWN_PHONES неудачных попыток (с вводом неверного пароля) за последние 10 мин, блокируем (не берем трубку) звонки с любых неизвестных номеров на 30мин либо до звонка с известного номера (что раньше).
const byte ADR_EEPROM_RECALL_ME = 1;            //useRecallMeMode
const byte ADR_EEPROM_STORED_PHONES = 100;      //начало списка 7значных номеров телефонов (5шт по 11 байт)
const byte ADR_EEPROM_PASSWORD_UNKNOWN_PHONES = 10; //начало пароля для доступа неопознанных тел-в
const byte ADR_EEPROM_PASSWORD_ADMIN = 20;      //начало админского пароля

volatile boolean isActiveWork = false;  //true когда работаем с GSM или идет нагрев. Переменные, изменяемые в функции обработки прерывания, должным быть объявлены как volatile.
bool useRecallMeMode = false;          // true - will recall for receiing DTMF, false - will answer for receiing DTMF
byte incomingPhoneID;
String incomingPhone;
byte ringNumber;
byte numberAttemptsUnknownPhones;

byte stateDTMF;
boolean AlarmMode = false;
String resultFullDTMF = "";                                   // Переменная для хранения вводимых DTMF данных
String resultCommandDTMF = "";                                // Переменная для хранения введенной DTMF команды
String resultPasswordDTMF_1 = "";                               // Переменная для хранения введенного пароля DTMF
String resultPasswordDTMF_2 = "";                          // Переменная для хранения введенного админского пароля DTMF
int addDTMFParam = 0;
unsigned long onPeriodAddDevice_s;


boolean alarmStatus = false;
boolean bOutgoingCallToneStarted = false;

String _response = "";              // Переменная для хранения ответов модуля

enum EnCallInform { CI_NO_220, CI_BREAK_220, CI_ALARM };
enum EnWorkEEPROM { EE_PHONE_NUMBER, EE_PASSWORD_UNKNOWN_PHONES, EE_PASSWORD_ADMIN, EE_USE_RECALL_ME, EE_ADD_THIS_PHONE_TO_STORED};

enum EnMP3Mode {
  M_NO, M_ASK_DTMF, M_ASK_PASSWORD, M_RECALL_MODE_CHANGE, M_DTMF_RECOGN, M_DTMF_NO_RECOGN, M_DTMF_INCORRECT_PASSWORD, M_COMMAND_APPROVED,
  M_NO_220, M_BREAK_220, M_CAR_ALARM
} mp3Mode = M_NO;

enum EnDTMFCommandMode { DC_WAIT, DC_RECEIVING, DC_WRONG_COMMAND, DC_RECOGNISED_COMMAND, DC_WAIT_CONFIRMATION, DC_CONFIRMED, DC_EXECUTED, DC_REJECTED } dtmfCommandMode = DC_WAIT;

enum EnDTMF {
  D_SAY_NON_ADMIN_COMMANDS, D_SAY_ALL_SETTINGS, D_ON_HEAT, D_OFF_HEAT, D_INFO_HEAT, D_DO_ALARM, D_REQUEST_CAR_INFO, D_REQUEST_GPS_SMS,
  D_SWITCH_ON_REGISTRATOR, D_SWITCH_ON_MIC, D_SWITCH_ON_ADD_DEViCE, D_SWITCH_OFF_ADD_DEViCE, D_OPEN_DOOR,
  D_ADD_THIS_PHONE, D_CHANGE_UNP_PASSWORD, D_CHANGE_ADMIN_PASSWORD, D_CHANGE_RECALL_MODE
};

enum EnHeatMode {
  HEAT_OFF, CHECK_HEAT_POSSIBILITY, WAIT_CONF_HEAT_START,
  HEAT_TIME1_ON, HEAT_TIME2_ON, REQUEST_HEAT_OFF
} heatMode = HEAT_OFF;

enum EnAddDeviceMode { ADD_DEVICE_OFF, REQUEST_ADD_DEVICE_ON, ADD_DEVICE_ON, REQUEST_ADD_DEVICE_OFF } addDeviceMode = ADD_DEVICE_OFF;

enum EnMicMode { MIC_OFF, REQUEST_MIC_ON, MIC_ON } micMode = MIC_OFF;

//enum EnGsmMode {
//  WAIT, INCOMING_UNKNOWN_CALL_START, INCOMING_CALL_START, INCOMING_CALL_PROGRESS, INCOMING_UNKNOWN_CALL_PROGRESS,
//  INCOMING_CALL_ANSWERED, INCOMING_UNKNOWN_CALL_ANSWERED,
//  INCOMING_CALL_DISCONNECTED, INCOMING_CALL_HANGUP,
//  TODO_RECALL,
//  RECALL_DIALING, RECALL_CALLING, RECALL_ANSWERED, RECALL_TALK, RECALL_HANGUP, RECALL_NOANSWER, RECALL_BUSY,
//  WAIT_PSWD
//} gsmMode = WAIT_GSM;

enum EnGSMSubMode {
  WAIT_GSM_SUB,
  INCOMING_UNKNOWN_CALL,
  CONFIRM_CALL,
  RECALL,
  START_INFO_CALL,
  FINISH_INFO_CALL,
} gsmSubMode = WAIT_GSM_SUB;

enum EnGSMMode {
  WAIT_GSM, INCOMING_CALL_START, INCOMING_CALL_PROGRESS, INCOMING_CALL_DISCONNECTED, INCOMING_CALL_HANGUP, INCOMING_CALL_ANSWERED,
  TODO_CALL, OUTGOING_CALL_PROGRESS, OUTGOING_CALL_HANGUP, OUTGOING_CALL_NOANSWER, OUTGOING_CALL_ANSWERED, OUTGOING_TALK, OUTGOING_CALL_BUSY
} gsmMode = WAIT_GSM;

enum EnDoAlarmMode { ALARM_OFF, REQUEST_ALARM_ON } doAlarmMode = ALARM_OFF;
enum EnOpenDoorMode { OPEN_DOOR_OFF, REQUEST_OPEN_DOOR_ON } openDoorMode = OPEN_DOOR_OFF;
enum EnRegistratorMode { REGISTRATOR_OFF, REQUEST_REGISTRATOR_ON, REGISTRATOR_ON } registratorMode = REGISTRATOR_OFF;
enum EnSendSMSMode { SMS_NO, REQUEST_GPS_SMS, REQUEST_CAR_INFO_SMS } sendSMSMode = SMS_NO;

float t_eng;    //температура двигателя
float t_out;    //температура воздуха
float t_socket; //температура разъема 220v

elapsedMillis inCallReaction_ms;
elapsedMillis recallNoAnswerDisconnect_ms;
elapsedMillis recallTalkDisconnect_ms;
elapsedMillis recallPeriod_ms;
elapsedMillis inCallTalkPeriod_ms;
elapsedMillis heatTime_ms;
elapsedMillis lastRefreshSensor_ms;
elapsedMillis blockUnknownPhones_ms;
elapsedMillis onPeriodAddDevice_ms;
elapsedMillis onPeriodRegistrator_ms;
elapsedMillis afterWakeUp_ms;
elapsedMillis outCallStarted_ms;
elapsedMillis outToneStartedDisconnect_ms;

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress innerTempDeviceAddress;
DeviceAddress outerTempDeviceAddress;
DeviceAddress socketTempDeviceAddress;

mp3TF mp3tf = mp3TF();

unsigned long c = 0;
unsigned long ca = 0;

String sSoftSerialData = "";

void setup()
{
  // immediately disable watchdog timer so set will not get interrupted
  //wdt_disable();

  //Initialize serial ports for communication.
  Serial.begin(9600);
  mySerialGSM.begin(9600);
  mySerialMP3.begin(9600);

  Serial.println("Setup start");


  // первый раз, потом закоментарить
  //InitialEepromSettings();

  pinMode(ALARM_CHECK_PIN, INPUT_PULLUP);
  pinMode(IGNITION_PIN, INPUT);
  pinMode(U_220_PIN, INPUT);
  pinMode(BTTN_PIN, INPUT_PULLUP);
  pinMode(HEAT_PIN, OUTPUT);
  pinMode(BZZ_PIN, OUTPUT);
  pinMode(ALARM_OUT_PIN, OUTPUT);
  //  pinMode(OPEN_DOOR_PIN, OUTPUT);
  //  pinMode(CLOSE_DOOR_PIN, OUTPUT);
  pinMode(ADD_DEVICE_PIN, OUTPUT);
  pinMode(MIC_PIN, OUTPUT);
  pinMode(REGISTRATOR_PIN, OUTPUT);

  useRecallMeMode = (EEPROM.read(ADR_EEPROM_RECALL_ME) == 49);
  Serial.print("useRecallMeMode= ");
  if (useRecallMeMode)
    Serial.println("tr ue");
  else
    Serial.println("false");

  cellSetup();

  //mp3
  mp3tf.init(&mySerialMP3);
  _delay_ms(200);
  pinMode(MP3_BUSY_PIN, INPUT);
  mp3tf.volumeSet(15);
  _delay_ms(200);
  ozv(1, 1, false);

  // Инициация кнопок
  btnControl.begin();

  sensors.begin();
  sensors.getAddress(innerTempDeviceAddress, 0);
  sensors.getAddress(outerTempDeviceAddress, 1);
  sensors.getAddress(socketTempDeviceAddress, 2);
  sensors.setResolution(innerTempDeviceAddress, 10);
  sensors.setResolution(outerTempDeviceAddress, 10);
  sensors.setResolution(socketTempDeviceAddress, 10);

  digitalWrite(BZZ_PIN, HIGH);
  _delay_ms(2000);
  digitalWrite(BZZ_PIN, LOW);

  Serial.println("Setup done");

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

void cellSetup()
{
  //_delay_ms(15000);
  mySerialGSM.println("AT");
  _delay_ms(100);
  mySerialGSM.println("AT+DDET=1,100,0,0");
  _delay_ms(1000);
  mySerialGSM.println("AT+CLIP=1");
  _delay_ms(1000);
  mySerialGSM.println("AT+CSCLK=2");
  _delay_ms(1000);
  //установить длительность исходящих DTMF
  mySerialGSM.println("AT+VTD=3");
  _delay_ms(300);
  //скорость Serial port
  //mySerialGSM.println("AT+IPR=19200");
  //_delay_ms(300);
  //чувствительность микрофона
  //  mySerialGSM.println("AT+CMIC=0, 9");
  //  _delay_ms(300);

  //  //Брать время gsm сети (при перегистрации, но работает не у всех операторов)
  //  mySerialGSM.println("AT+CLTS=1");
  //  _delay_ms(300);
  //сохранить настройки
  //  mySerialGSM.println("AT&W");
  //  _delay_ms(300);

  SIMflush();

  mySerialGSM.println("AT");
  _delay_ms(100);
}

void SIMflush()
{
  Serial.println("SIMflush_1");
  while (mySerialGSM.available() != 0)
  {
    mySerialGSM.read();
  }
  Serial.println("SIMflush_2");
}

//1й старт, потом закоментарить
void InitialEepromSettings()
{
  //4 set recallMe mode=1
  workWithEEPROM(1, 4, 0, "1");
  //5 add my phone numbers
  workWithEEPROM(1, 5, 0, "79062656420");
  workWithEEPROM(1, 5, 1, "79990424298");
  //2 set unknown psswd
  workWithEEPROM(1, 2, 0, "1234");
  //3 set admin psswd
  workWithEEPROM(1, 3, 0, "4321");
}

void CheckAlarmLine()
{
  alarmStatus = digitalRead(ALARM_CHECK_PIN);  //store new condition
}


//mp3Mode
void sayInfo()
{
  Serial.print("sayInfo:");
  Serial.println(mp3Mode);
  //Serial.println(millis());
  ozv(1, mp3Mode, false);
  //Serial.println(millis());
  //Serial.println("sayInfoEnd");
  mp3Mode = M_NO;
}

//воспроизвести из фолдера № fld файл № file. Ждать окончания воспроизведения если playToEnd=true
void ozv(int fld, int file, boolean playToEnd)
{
  mp3tf.playFolder2(fld, file);
  mySerialMP3.println("AT");
  _delay_ms(100);
  while (playToEnd && !digitalRead(MP3_BUSY_PIN));
}

void sayCommandsList(byte typeCommands) //typeCommands = 2 - commands; 3 - settngs
{
  //  for (int i = 0; 10; i++)
  //  {
  //    mp3tf.playFolder2(typeCommands, i);
  //    _delay_ms(200);
  //    while (!digitalRead(MP3_BUSY_PIN));
  //  }
}

void InformCall(EnCallInform typeCallInform) //CI_NO_220, CI_BREAK_220, CI_ALARM
{
  DoCall(true);
  switch (typeCallInform)
  {
    case CI_NO_220:
      mp3Mode = M_NO_220;
      break;
    case CI_BREAK_220:
      mp3Mode = M_BREAK_220;
      break;
    case CI_ALARM:
      mp3Mode = M_CAR_ALARM;
      break;
  }
  sayInfo();
}

boolean CheckPhone(String str)
{
  Serial.print("CheckPh:" + str);
  //  "+CMT: "+79062656420",,"13/04/17,22:04:05+22"    //SMS
  //  "+CLIP: "79062656420",145,,,"",0"                //INCOMING CALL
  incomingPhoneID = 0;
  incomingPhone = "";
  if (str.indexOf("+CLIP:") > -1 || str.indexOf("+CMT:") > -1)
  {
    for (byte i = 0; i < 5; i++)
    {
      String sPhone = workWithEEPROM(0, 1, i, "");
      sPhone = sPhone.substring(0, 11);
      //sPhone = "79062656420";
      //      Serial.print("EEPROM phone #: ");
      //      Serial.print(i);
      //      Serial.println(" : " + sPhone);
      if (str.indexOf(sPhone) > -1)
      {
        incomingPhoneID = i + 1;
        incomingPhone = sPhone;
        Serial.print("Matched: ");
        Serial.println(incomingPhone);
        break;
      }
      else
      {
        Serial.print("Not matched: " + sPhone);
      }
    }
  }
  return (incomingPhoneID > 0);
}

void RefreshSensorData()
{
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    sensors.requestTemperatures();
    //float realTemper = sensors.getTempCByIndex(0);
    t_eng = sensors.getTempC(innerTempDeviceAddress);
    t_out = sensors.getTempC(outerTempDeviceAddress);
    t_socket = sensors.getTempC(socketTempDeviceAddress);

    lastRefreshSensor_ms = 0;
  }
}

//если в процессе нагрева  пропадет 220 (например выбъет предохранитель)
void CheckHeatAndU220()
{
  if ((heatMode == HEAT_TIME1_ON || heatMode == HEAT_TIME2_ON) && !digitalRead(U_220_PIN))
  {
    heatMode = HEAT_OFF;
    digitalWrite(HEAT_PIN, LOW);
    InformCall(CI_BREAK_220);
    Serial.println("CheckHeatAndU220");
  }
}

//при включении зажигания выдает звук, если осталось подключено 220
void CheckIgnitionAndU220()
{
  if (digitalRead(U_220_PIN) && digitalRead(IGNITION_PIN))
  {
    //tone(BZZ_PIN,2500,200);
    digitalWrite(BZZ_PIN, HIGH);
    _delay_ms(500);
    digitalWrite(BZZ_PIN, LOW);
    _delay_ms(200);
  }
}


String workWithEEPROM(byte mode, EnWorkEEPROM dataType, int addParam, byte* arrDataToSave)  //mode = 0- read, 1- write;
//sDataSave - string to save (for mode=1);
//dataType = 1 - телефонный номер (index = addParam), 2 - PasswordUnknownPhones, 3 - PasswordAdmin, 4 - change UseRecallMe, 5 - AddThisPhoneToStored
{
  //Serial.println("workWithEEPROM");
  byte* result;
  int adrEEPROM;
  int numSymbols;
  char buf[11];
  switch (dataType)
  {
    case EE_PHONE_NUMBER:
    case EE_ADD_THIS_PHONE_TO_STORED:
      adrEEPROM = ADR_EEPROM_STORED_PHONES;
      numSymbols = 11;
      break;
    case EE_PASSWORD_UNKNOWN_PHONES:
      adrEEPROM = ADR_EEPROM_PASSWORD_UNKNOWN_PHONES;
      numSymbols = 4;
      break;
    case EE_PASSWORD_ADMIN:
      adrEEPROM = ADR_EEPROM_PASSWORD_ADMIN;
      numSymbols = 4;
      break;
    case EE_USE_RECALL_ME:
      adrEEPROM = ADR_EEPROM_RECALL_ME;
      numSymbols = 1;
      break;
  }

  for (int k = 0; k < numSymbols; k++)
  {
    if (mode == 0)
    {
      buf[k] = EEPROM[k + adrEEPROM + addParam * numSymbols];
    }
    else
    {
      EEPROM.write(k + adrEEPROM + addParam * numSymbols, arrDataToSave[k]);
    }
  }
  return String(buf);
}

String GetGPSData()
{
  String sResult = "";
  float lat, lon;
  unsigned long age;

  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (mySerialGSM.available())
    {
      char c = mySerialGSM.read();
      gps.encode(c);
    }
    int year;
    byte month, day, hour, minutes, second, hundredths;

    // Получаем дату и время
    gps.crack_datetime(&year, &month, &day, &hour, &minutes, &second, &hundredths, &age);

    // Получаем координаты
    gps.f_get_position(&lat, &lon, &age);

    sResult = "Lat: " + String(lat) + " Lon: " + String(lon) + " Time: " + String(hour) + ":" + String(minutes) + ":" + String(second) + " Number Sat: " + String(gps.satellites());
  }

  return sResult;
}

String getCarInfo()
{
  String sResult = "";
  int carVolt = analogRead(CAR_VOLT_PIN);
  sResult = "U= " + carVolt / 1023;
  sResult += " Tout= " + String(t_out);
  sResult += " Teng= " + String(t_eng);
  return sResult;
}

void PrepareDtmfCommand(EnDTMF command, byte* arrDataToSave)  //enum EnDTMF { D_ON_HEAT, D_OFF_HEAT, D_INFO_HEAT, D_REQUEST_CAR_INFO, D_OPEN_DOOR, D_CHANGE_UNP_PASSWORD, D_CHANGE_ADMIN_PASSWORD, D_RECALL_MODE_CHANGE, D_REQUEST_GPS_SMS };
{
  switch (command)
  {
    case D_SAY_NON_ADMIN_COMMANDS:
      sayCommandsList(2);
      break;
    case D_SAY_ALL_SETTINGS:
      sayCommandsList(3);
      break;
    case D_REQUEST_CAR_INFO:
      sendSMSMode = REQUEST_CAR_INFO_SMS;
      dtmfCommandMode = DC_WAIT_CONFIRMATION;
      break;
    case D_REQUEST_GPS_SMS:
      dtmfCommandMode = DC_WAIT_CONFIRMATION;
      sendSMSMode = REQUEST_GPS_SMS;
      Serial.println("GPS sent");
      break;
    case D_ON_HEAT:
      heatMode = CHECK_HEAT_POSSIBILITY;
      Serial.println("CHECK_HEAT_PSSB");
      break;
    case D_OFF_HEAT:
      heatMode = REQUEST_HEAT_OFF;
      dtmfCommandMode = DC_WAIT_CONFIRMATION;
      Serial.print("dtmfCommandMode=");
      Serial.println(dtmfCommandMode);
      Serial.print("gsmMode=");
      Serial.println(gsmMode);
      Serial.println("RQ H_O");
      break;
    case D_SWITCH_ON_REGISTRATOR:
      registratorMode = REQUEST_REGISTRATOR_ON;
      dtmfCommandMode = DC_WAIT_CONFIRMATION;
      Serial.println("Reg");
      break;
    case D_SWITCH_ON_MIC:
      Serial.println("MIC_ON");
      dtmfCommandMode = DC_WAIT_CONFIRMATION;
      micMode = REQUEST_MIC_ON;
      break;
    case D_DO_ALARM:
      Serial.println("D_DO_ALARM");
      doAlarmMode = REQUEST_ALARM_ON;
      dtmfCommandMode = DC_WAIT_CONFIRMATION;
      break;
    case D_OPEN_DOOR:
      Serial.println("D_OPEN_DOOR");
      openDoorMode = REQUEST_OPEN_DOOR_ON;
      dtmfCommandMode = DC_WAIT_CONFIRMATION;
      break;
    case D_SWITCH_ON_ADD_DEViCE:
      addDeviceMode = REQUEST_ADD_DEVICE_ON;
      dtmfCommandMode = DC_WAIT_CONFIRMATION;
      break;
    case D_SWITCH_OFF_ADD_DEViCE:
      addDeviceMode = REQUEST_ADD_DEVICE_OFF;
      dtmfCommandMode = DC_WAIT_CONFIRMATION;
      break;
    case D_CHANGE_UNP_PASSWORD:
    case D_CHANGE_ADMIN_PASSWORD:
    case D_CHANGE_RECALL_MODE:
    case D_ADD_THIS_PHONE:
      workWithEEPROM(1, (command == D_CHANGE_UNP_PASSWORD ? 2 :
                         (command == D_CHANGE_ADMIN_PASSWORD ? 3 :
                          (command == D_CHANGE_RECALL_MODE ? 4 : 5))), 0, arrDataToSave);
      Serial.println("eeprom saved");
      break;
  }
}

// Виды команд DTMF
// 0 - проговорить все доступные DTMF команды (кроме настроечных)
// 1 - получить сообщение о состоянии ALARM, подогрева, температуры, напряжения,...,SPS,...
// 21/20 - включить/выключить подогрев (на время в зависимости от T). Если только 1 цифра - то поменять с выкл на вкл или наоборот
// 3 - сработать сигнализации
// 4*PWD# - открыть дверь и закрыть автоматически через 1 мин (при этом отключать сирену) (требуется пароль ADMIN_PWD между *и#)
// 5 - включить видеорегистратор (на 3 мин).
// 61, 60 - включить/выключить доп устройсво №1
// 7 - включить микрофон.
// 8 - прислать sms с GPS координатами
// 90*ADMIN_PWD# проговорить все доступные настроечные DTMF команды
// 91*ADMIN_PWD# добавить данный тел-н в группу "своих"
// 92#PWD#PWD_NEW# сменить пароль для unknown phones c PWD на PWD_NEW
// 93*ADMIN_PWD*ADMIN_PWD_NEW# сменить пароль c ADMIN_PWD на ADMIN_PWD_NEW
// 940/941*ADMIN_PWD# установить useRecallMeMode в False/True
//
// Parsing DTMF команд, вызов PrepareDtmfCommand()
void CheckDTMF(String symbol)
{
  Serial.println("Key: " + symbol);                         // Выводим в Serial для контроля, что ничего не потерялось
  ozv(1, 1, false);
  if (dtmfCommandMode == DC_WAIT_CONFIRMATION)
  {
    if (symbol == "#") //это пришла команда-подверждение - "#" (т.е. команда из единственного символа- "#")
    {
      ozv(1, 7, false);
      Serial.println("DC_CONFIRMED");
      dtmfCommandMode = DC_CONFIRMED;
    }
    else //ожидали команды-подверждения, а пришел не "#" - отбой
    {
      Serial.println("DC_REJECTED");
      dtmfCommandMode = DC_REJECTED;
    }
    resultFullDTMF = "";                                      // сбрасываем вводимую комбинацию
  }
  else if (symbol == "#")                                        //признак завершения ввода команды, начинаем парсинг и проверку
  {
    ozv(1, 6, false);

    bool correct = true;                                   // Для оптимизации кода, переменная корректности команды
    ParsingDTMF();
    // Serial.println("ParsingDTMF_end");
    byte* arrDataToSave = "";  //данные для сохранения в EEPROM
    EnDTMF command;
    //      Serial.print("resultCommandDTMF[0]: ");
    //      Serial.println((int)resultCommandDTMF[0] - 48);
    //      Serial.print("resultCommandDTMF[1]: ");
    //      Serial.println((int)resultCommandDTMF[1] - 48);
    switch ((int)resultCommandDTMF[0] - 48)
    {
      case 0:
        // Serial.println("0 - проговорить все доступные DTMF команды (кроме настроечных)");
        command = D_SAY_NON_ADMIN_COMMANDS;
        break;
      case 1:
        // Serial.println("1 - получить сообщение о состоянии ALARM, подогрева, температуры, напряжения,..");
        command = D_REQUEST_CAR_INFO;
        break;
      case 2:
        // Serial.println("21/20 - включить/выключить подогрев (на время в зависимости от T). Если только 1 цифра - то поменять с выкл на вкл или наоборот");
        command = (((int)resultCommandDTMF[1] - 48) == 1 ? D_ON_HEAT : D_OFF_HEAT);
        break;
      case 3:
        // Serial.println("3 - сработать сигнализации");
        command = D_DO_ALARM;
        break;
      case 4:
        // Serial.println("4*PWD# - открыть дверь и закрыть автоматически через 1 мин (при этом отключать сирену) (требуется пароль ADMIN_PWD между *и#)");
        command = D_OPEN_DOOR;
        break;
      case 5:
        //Serial.println("5 - включить видеорегистратор (на 3 мин).");
        command = D_SWITCH_ON_REGISTRATOR;
        break;
      case 6:
        //Serial.println("61, 60 - включить/выключить доп устройсво №1;
        // 62437 - включить доп устройсво на 437 минут (62 - начало команды с временем)
        switch ((int)resultCommandDTMF[1] - 48)
        {
          case 1:
            command = D_SWITCH_ON_ADD_DEViCE;
            break;
          case 2:
            //addDTMFParam = ((int)resultCommandDTMF[resultCommandDTMF.length - 3] - 48) * 1000 + ((int)resultCommandDTMF[resultCommandDTMF.length - 2] - 48) * 100 + ((int)resultCommandDTMF[resultCommandDTMF.length - 1] - 48);
            addDTMFParam = 3;
            command = D_SWITCH_ON_ADD_DEViCE;
            break;
          default:
            command = D_SWITCH_OFF_ADD_DEViCE;
            break;
        }
        break;
      case 7:
        //включить микрофон
        command = D_SWITCH_ON_MIC;
        break;
      case 8:
        //Serial.println("8 - прислать sms с GPS координатами");
        command = D_REQUEST_GPS_SMS;
        break;

      //Настроечные команды - resultCommandDTMF[0]=9, после команда* всегда идет AdminPWD, т.е. в resultPasswordDTMF_1
      case 9:
        switch (((int)resultCommandDTMF[1] - 48))
        {
          case 0:
            // Serial.println("0 - проговорить все доступные настроечные DTMF команды");
            command = D_SAY_ALL_SETTINGS;
            break;
          case 1:
            command = D_ADD_THIS_PHONE;
            incomingPhone.toCharArray(arrDataToSave, 11);
            break;
          case 2:
            command = D_CHANGE_UNP_PASSWORD;
            resultPasswordDTMF_2.toCharArray(arrDataToSave, 4);
            break;
          case 3:
            command = D_CHANGE_ADMIN_PASSWORD;
            resultPasswordDTMF_2.toCharArray(arrDataToSave, 4);
            break;
          case 4:
            command = D_CHANGE_RECALL_MODE;
            if (((int)resultCommandDTMF[2] - 48) == 1)
              arrDataToSave = "1";
            else
              arrDataToSave = "0";
            break;
          default:
            correct = false;
            break;
        }
        if (correct)
        {
          if (!CheckPassword(3, resultPasswordDTMF_1)) //пароль AP неверен
          {
            Serial.println("Incorrect APWD");
            mp3Mode = M_DTMF_INCORRECT_PASSWORD;
            dtmfCommandMode = DC_WRONG_COMMAND;
            correct = false;
          }
        }
      default:
        correct = false;
        break;
    }
    if (correct && gsmSubMode == INCOMING_UNKNOWN_CALL)
    {
      if (!CheckPassword(2, resultPasswordDTMF_1)) //пароль для UP неверен
      {
        Serial.println("Incorrect UPPWD");
        mp3Mode = M_DTMF_INCORRECT_PASSWORD;
        dtmfCommandMode = DC_WRONG_COMMAND;
        correct = false;
      }
    }
    if (correct)
    {
      Serial.println("RECOGNISED");
      dtmfCommandMode = DC_RECOGNISED_COMMAND;
      ozv(1, 8, false);
      mp3Mode = M_DTMF_RECOGN;
      PrepareDtmfCommand(command, arrDataToSave);
    }
    else // Если команда нераспознана, выводим сообщение
    {
      Serial.println("DC_WRONG_COMMAND");
      dtmfCommandMode = DC_WRONG_COMMAND;
      mp3Mode = M_DTMF_NO_RECOGN;
      // Serial.println("Wrong command: " + resultFullDTMF);
    }
    resultFullDTMF = "";                                      // После каждой решетки сбрасываем вводимую комбинацию
  }

  else  //"#" еще нет, продолжаем посимвольно собирать команду
  {
    resultFullDTMF += symbol;
    dtmfCommandMode = DC_RECEIVING;
  }
}

void ParsingDTMF()
{
  byte posEndCommand;
  byte posEndPassword_1;
  byte posEndPassword_2;
  //Serial.println("ParsingDTMF: " + resultFullDTMF);
  if (resultFullDTMF.indexOf("*") == -1) //only command, without password
  {
    posEndCommand = resultFullDTMF.indexOf("#");
  }
  else  //command and password(s)
  {
    posEndCommand = resultFullDTMF.indexOf("*");
    if (resultFullDTMF.substring(posEndCommand + 1).indexOf("*") != -1) // 2 passwords
    {
      posEndPassword_1 = posEndCommand + 1 + resultFullDTMF.substring(posEndCommand + 1).indexOf("*");
      posEndPassword_2 = posEndPassword_1 + 1 + resultFullDTMF.substring(posEndPassword_1 + 1).indexOf("#");
    }
    else  //only 1 password
    {
      posEndPassword_1 = posEndCommand + 1 + resultFullDTMF.substring(posEndCommand + 1).indexOf("#");
    }
  }

  addDTMFParam = 0;
  resultCommandDTMF = resultFullDTMF.substring(0, posEndCommand);
  if (posEndPassword_1 > 0) resultPasswordDTMF_1 = resultFullDTMF.substring(posEndCommand + 1, posEndPassword_1);
  if (posEndPassword_2 > 0) resultPasswordDTMF_2 = resultFullDTMF.substring(posEndPassword_1 + 1, posEndPassword_2);
  Serial.println("CMD: " + resultCommandDTMF);
  Serial.println("PWD1: " + resultPasswordDTMF_1);
  Serial.println("PWD2: " + resultPasswordDTMF_2);
}

void GetSoftSerialData()
{
  sSoftSerialData = "";
  if (mySerialGSM.available()) //если модуль что-то послал
  {
    char ch = ' ';
    unsigned long start_timeout = millis();            // Start the timer
    const unsigned int time_out_length = 3000; //ms
    while (mySerialGSM.available() && ((millis() - start_timeout) < time_out_length))
    {
      start_timeout = millis();
      ch = mySerialGSM.read();
      if ((int)ch != 17 && (int)ch != 19)
      {
        sSoftSerialData.concat(ch);
      }
      _delay_ms(2);
    }

    sSoftSerialData.trim();
    if (sSoftSerialData != "")
    {
      Serial.print("talk> ");
      Serial.println(sSoftSerialData);
    }
    if ((millis() - start_timeout) < time_out_length)
    {
      Serial.println("timeout");
    }
  }
}

void CheckSoftwareSerialData()
{
  //Serial.println("CheckGSMModule");
  //If a character comes in from the cellular module...
  if (sSoftSerialData != "")
  {
    //    if (sSoftSerialData.indexOf("RING") > -1)
    //    {
    //      ringNumber += 1;
    //    }

    if (gsmMode == WAIT_GSM && sSoftSerialData.indexOf("+CLIP") > -1) //если пришел входящий вызов
    {
      Serial.println("IR!");
      gsmMode = INCOMING_CALL_START;
      if (CheckPhone(sSoftSerialData))  // из текущей строки выберем тел номер. если звонящий номер есть в списке доступных, можно действовать
      {
        Serial.println("IP: " + incomingPhone + " " + incomingPhoneID);
      }
      else
      {
        gsmSubMode = INCOMING_UNKNOWN_CALL;
        Serial.println("UNP");
      }
    }
    else if ((gsmMode == INCOMING_CALL_PROGRESS || gsmMode == INCOMING_CALL_ANSWERED) && sSoftSerialData.indexOf("NO CARRIER") > -1) //если входящий вызов сбросили не дождавшись ответа блока или повесили трубку не дождавшись выполнения команды
    {
      Serial.println("IP NC");
      gsmMode = INCOMING_CALL_HANGUP;
    }
    //else if ((gsmMode == RECALL_DIALING || gsmMode == OUTGOING_CALL_PROGRESS || gsmMode == OUTGOING_CALL_ANSWERED || gsmMode == OUTGOING_TALK) && sSoftSerialData.indexOf("NO CARRIER") > -1) //если исходящий от блока вызов сбросили
    //{
    //  Serial.println("WAIT");
    //  gsmMode = WAIT_GSM;
    //}
    else if (gsmMode == OUTGOING_TALK || gsmMode == INCOMING_CALL_ANSWERED)
    {
      Serial.println("Check DTMF");
      int i = sSoftSerialData.indexOf("MF: "); //DTMF:
      if (i > -1)
      {
        CheckDTMF(sSoftSerialData.substring(i + 4, i + 5));                     // Логику выносим для удобства в отдельную функцию
      }
      //else if (gsmMode == RECALL_DIALING && GetOutgoingCallStatus(sSoftSerialData) == "3") //если пошел исходящмй гудок
      //{
      //  Serial.println("RECALLING");
      //  gsmMode = OUTGOING_CALL_PROGRESS;
      //}
      else if (gsmMode == OUTGOING_CALL_PROGRESS && sSoftSerialData.indexOf("BUSY") > -1)
      {
        Serial.println("Outgoing call is hang up");
        gsmMode = OUTGOING_CALL_HANGUP;
      }
      else if (gsmMode == OUTGOING_CALL_PROGRESS && sSoftSerialData.indexOf("NO ANSWER") > -1) //если на исходящий вызов линия  нет ответа
      {
        Serial.println("Outgoing call - line no answer");
        gsmMode = OUTGOING_CALL_NOANSWER;
      }
      else if (gsmMode == OUTGOING_CALL_PROGRESS && sSoftSerialData.indexOf("NO DIALTONE") > -1) //если нет сигнала
      {
        Serial.println("Outgoing call - line no dialstone");
        gsmMode = OUTGOING_CALL_NOANSWER;
      }


      else if (gsmMode == OUTGOING_CALL_PROGRESS && GetOutgoingCallStatus(sSoftSerialData) == "0") //если на исходящий от блока вызов ответили
      {
        Serial.println("ANSWERED");
        gsmMode = OUTGOING_CALL_ANSWERED;
      }
      else if (gsmMode == OUTGOING_CALL_PROGRESS && !bOutgoingCallToneStarted)
      {
        Serial.println("GetOutgoingCallStatus");
        if (GetOutgoingCallStatus(sSoftSerialData) == "3") //если пошел исходящмй гудок
        {
          Serial.println("Tone");
          bOutgoingCallToneStarted = true;
          outToneStartedDisconnect_ms = 0;
        }
      }
      //}
    }
  }
}


//для ATWIN и для SIM800 алгоритмы определения статуса разные
//SIM800
String GetOutgoingCallStatus(String sRespond) //2 - набираем номер, 3 - идет исходящий вызов, ждем ответа, 0 - сняли трубку, идет разговор
{
  String result = "9";

  sRespond.trim();
  int i = sRespond.indexOf("+CLCC:");
  if (i > -1)
  {
    result = sRespond.substring(i + 11, i + 12);
  }
  Serial.print("Status=");
  Serial.println(result);
  return result;
}

void WorkflowGSM()
{
  switch (gsmMode)
  {
    case WAIT_GSM:
      break;
    case INCOMING_CALL_START:
      inCallReaction_ms = 0;
      gsmMode = INCOMING_CALL_PROGRESS;
      break;
    case INCOMING_CALL_PROGRESS:
      // отвечаем в случае useRecallMeMode-FALSE или звонке с неизвестного номера (через IN_UNKNOWN_ANSWER_PERIOD_S), иначе - перезваниваем
      if (useRecallMeMode && gsmSubMode != INCOMING_UNKNOWN_CALL && inCallReaction_ms > IN_DISCONNECT_PERIOD_S * 1000)
      {
        Serial.println("IN Discon");
        BreakCall();
        gsmMode = INCOMING_CALL_DISCONNECTED;
        recallPeriod_ms = 0;
      }
      else if ((!useRecallMeMode && inCallReaction_ms > IN_ANSWER_PERIOD_S * 1000) || (gsmSubMode == INCOMING_UNKNOWN_CALL && inCallReaction_ms > IN_UNKNOWN_ANSWER_PERIOD_S * 1000))
      {
        Serial.println("IN Answ");
        AnswerCall();
        if (gsmSubMode == INCOMING_UNKNOWN_CALL)
          mp3Mode = M_ASK_PASSWORD;
        else
          mp3Mode = M_ASK_DTMF;
        gsmMode = INCOMING_CALL_ANSWERED;
        inCallTalkPeriod_ms = 0;
      }
      break;
    case INCOMING_CALL_DISCONNECTED:  //only when set useRecallMeMode
      if (recallPeriod_ms > RECALL_PERIOD_S * 1000)
      {
        gsmSubMode = RECALL;
        gsmMode = TODO_CALL;
      }
      break;

    case TODO_CALL:
      switch (gsmSubMode)
      {
        case CONFIRM_CALL:
          Serial.println("CONFIRM_CALL");
          DoCall(false);
          break;
        case START_INFO_CALL:
          Serial.println("START_INFO_CALL");
          DoCall(false);
          break;
        case FINISH_INFO_CALL:
          Serial.println("FINISH_INFO_CALL");
          DoCall(false);
          break;
      }
      break;

    case OUTGOING_CALL_PROGRESS:
      if (!bOutgoingCallToneStarted && outCallStarted_ms > OUT_NO_TONE_DISCONNECT_PERIOD_S * 1000)  //если вызов не пошел (сбой сети или вне зоны)
      {
        Serial.println("OUT without tone signal. Disconnection");
        BreakCall();
        gsmSubMode = WAIT_GSM_SUB;
        gsmMode = WAIT_GSM;
        bOutgoingCallToneStarted = false;
      }
      else
        switch (gsmSubMode)
        {
          case START_INFO_CALL:
          case FINISH_INFO_CALL:
            if (bOutgoingCallToneStarted && outToneStartedDisconnect_ms > gsmSubMode == START_INFO_CALL ? OUT_INFORM_PERIOD_1_S : OUT_INFORM_PERIOD_2_S * 1000)
            {
              Serial.println("Disconnect after 1/2 ring");
              BreakCall();
              gsmSubMode = WAIT_GSM_SUB;
              gsmMode = WAIT_GSM;
              bOutgoingCallToneStarted = false;
            }
            break;
          default:
            if (bOutgoingCallToneStarted && outToneStartedDisconnect_ms > OUT_TONE_DISCONNECT_PERIOD_S * 1000) ////разрываем связь через 20с
            {
              Serial.println("OUT without answer. Disconnection");
              BreakCall();
              //gsmSubMode = WAIT_GSM_SUB;
              gsmMode = OUTGOING_CALL_NOANSWER;
              bOutgoingCallToneStarted = false;
            }
            break;
        }

      if (gsmMode == OUTGOING_CALL_PROGRESS && !bOutgoingCallToneStarted)   //пока не ответили и не истекло время ожидания ответа, вызываем запрос состояния "AT+CLCC" чтобы поймать момент ответа
      {
        mySerialGSM.println("AT");
        _delay_ms(200);
        Serial.println("AT+CLCC");
        mySerialGSM.println("AT+CLCC");
        _delay_ms(500);
      }
      break;
    case OUTGOING_CALL_ANSWERED: //как только ответили, выдадим DTMF
      mp3Mode = M_ASK_DTMF;
      gsmMode = OUTGOING_TALK;
      recallTalkDisconnect_ms = 0;

      /*_delay_ms(1000);
        mySerialGSM.println("AT");
        _delay_ms(100);
        mySerialGSM.println("AT+VTS=\"1\"");
        _delay_ms(1000);*/

      break;

    case INCOMING_CALL_ANSWERED:
      if ((dtmfCommandMode == DC_EXECUTED || dtmfCommandMode == DC_REJECTED) || inCallTalkPeriod_ms > (gsmSubMode == INCOMING_UNKNOWN_CALL ? IN_UNKNOWN_CALL_TALK_PERIOD_S : IN_CALL_TALK_PERIOD_S) * 1000)
      {
        Serial.println("Inc call Discn");
        BreakCall();
        FinalResetStatuses();
      }
      break;

    case OUTGOING_TALK:
      if ((dtmfCommandMode == DC_EXECUTED || dtmfCommandMode == DC_REJECTED) || recallTalkDisconnect_ms > OUTGOING_TALK_DISCONNECT_PERIOD_S * 1000)
      {
        Serial.println("Recall talk Discn");
        BreakCall();
        FinalResetStatuses();
      }
      break;

    case OUTGOING_CALL_HANGUP:
    case OUTGOING_CALL_NOANSWER:
    case OUTGOING_CALL_BUSY:
    case INCOMING_CALL_HANGUP:
      //case INCOMING_CALL_DISCONNECTED:
      FinalResetStatuses();
      break;
  }
}

bool CheckPassword(byte passwordType, String pwd) //passwordType 2 - for UnknownPhones, 3 - Admin
{
  return (workWithEEPROM(0, passwordType, 0, "") == pwd);
}

void WorkflowMain(byte mode) //0-auto(from loop), 1-manual
{
  switch (btnControl.Loop()) {
    case SB_CLICK:
      Serial.println("btnShort");
      if (heatMode == HEAT_TIME1_ON || heatMode == HEAT_TIME2_ON) //выкл нагрев
      {
        heatMode = REQUEST_HEAT_OFF;
        dtmfCommandMode = DC_CONFIRMED;
      }
      else //позвонить на свой номер
      {
        DoCall(true);
      }
      break;
    case SB_LONG_CLICK: //вкл нагрев
      Serial.println("btnLong");
      heatMode = WAIT_CONF_HEAT_START;
      dtmfCommandMode = DC_CONFIRMED;
      break;
  }

  switch (heatMode)
  {
    case CHECK_HEAT_POSSIBILITY:
      Serial.println("WF CHECK_HEAT_POSSIBILITY");
      // if (digitalRead(U_220_PIN))
      if (1 == 1)
      {
        dtmfCommandMode = DC_WAIT_CONFIRMATION;
        heatMode = WAIT_CONF_HEAT_START;
        Serial.println("heatMode = WAIT_CONF_HEAT_START");
      }
      else // нет 220
      {
        mp3Mode = M_NO_220;
        sayInfo();
        mp3Mode = M_NO;
        dtmfCommandMode = DC_WAIT;
        heatMode = HEAT_OFF;
        Serial.println("S_NO_220");

        //разрываем связь
        BreakCall();
        FinalResetStatuses();
      }
      break;
    case WAIT_CONF_HEAT_START:
      //Serial.println("WF WAIT_CONF_HEAT_START");
      if (dtmfCommandMode == DC_CONFIRMED)
      {
        Serial.println("H_1");
        heatTime_ms = 0;
        heatMode = (t_eng > T_COLD ? HEAT_TIME1_ON : HEAT_TIME2_ON);
        dtmfCommandMode = DC_EXECUTED;
        digitalWrite(HEAT_PIN, HIGH);
      }
      break;

    case HEAT_TIME1_ON: //включен на HEAT_TIME1_M
    case HEAT_TIME2_ON: //включен на HEAT_TIME2_M
      if (heatTime_ms > (heatMode == HEAT_TIME1_ON ? HEAT_TIME1_M : HEAT_TIME2_M) * 1000 * 60)
      {
        heatMode = HEAT_OFF;
        digitalWrite(HEAT_PIN, LOW);
        //  gsmMode = TODO_FINISH_INFO_CALL;
        Serial.println("HEAT OFF");
      }
      break;
    case REQUEST_HEAT_OFF:
      if (dtmfCommandMode == DC_CONFIRMED)
      {
        Serial.println("H_O DONE");
        heatMode = HEAT_OFF;
        // gsmMode = TODO_FINISH_INFO_CALL;
        dtmfCommandMode = DC_EXECUTED;
        digitalWrite(HEAT_PIN, LOW);
      }
      //      else
      //      {
      //        Serial.println("H_O wait conf");
      //      }
      break;
  }

  if (mp3Mode != M_NO)
  {
    sayInfo();
  }

  if (dtmfCommandMode == DC_WRONG_COMMAND)
  {
    BreakCall();
    FinalResetStatuses();
  }

  switch (addDeviceMode) //ADD_DEVICE_OFF, REQUEST_ADD_DEVICE_ON, ADD_DEVICE_ON, REQUEST_ADD_DEVICE_OFF
  {
    case REQUEST_ADD_DEVICE_ON:
      if (dtmfCommandMode == DC_CONFIRMED)
      {
        onPeriodAddDevice_ms = 0;
        digitalWrite(ADD_DEVICE_PIN, HIGH);
        addDeviceMode = ADD_DEVICE_ON;
        dtmfCommandMode = DC_EXECUTED;
      }
      break;
    case REQUEST_ADD_DEVICE_OFF:
      if (dtmfCommandMode == DC_CONFIRMED)
      {
        digitalWrite(ADD_DEVICE_PIN, LOW);
        addDeviceMode = ADD_DEVICE_OFF;
        if (addDTMFParam == 0)
          onPeriodAddDevice_s = MAX_ON_PERIOD_ADD_DEVICE_S;
        else
          onPeriodAddDevice_s = addDTMFParam;
        dtmfCommandMode = DC_EXECUTED;
      }
      break;
    case ADD_DEVICE_ON:
      if (onPeriodAddDevice_ms > onPeriodAddDevice_s * 1000)
      {
        digitalWrite(ADD_DEVICE_PIN, LOW);
        addDeviceMode = ADD_DEVICE_OFF;
        break;
      }
  }

  switch (registratorMode) //REGISTRATOR_OFF, REQUEST_REGISTRATOR_ON, REGISTRATOR_ON
  {
    case REQUEST_REGISTRATOR_ON:
      if (dtmfCommandMode == DC_CONFIRMED)
      {
        onPeriodRegistrator_ms = 0;
        digitalWrite(REGISTRATOR_PIN, HIGH);
        registratorMode = REGISTRATOR_ON;
        dtmfCommandMode = DC_EXECUTED;
      }
      break;
    case REGISTRATOR_ON:
      if (onPeriodRegistrator_ms > ON_PERIOD_REGISTRATOR * 1000)
      {
        digitalWrite(REGISTRATOR_PIN, LOW);
        registratorMode = REGISTRATOR_OFF;
        break;
      }
  }

  //MIC_OFF, REQUEST_MIC_ON, MIC_ON
  if (micMode == REQUEST_MIC_ON && dtmfCommandMode == DC_CONFIRMED)
  {
    digitalWrite(MIC_PIN, HIGH);
    micMode = MIC_ON;
    dtmfCommandMode = DC_EXECUTED;
  }
  else if (micMode == MIC_ON && gsmMode == WAIT_GSM)
  {
    digitalWrite(MIC_PIN, LOW);
    micMode = MIC_OFF;
  }

  //SMS_NO, REQUEST_GPS_SMS, REQUEST_CAR_INFO_SMS
  if ((sendSMSMode == REQUEST_GPS_SMS || sendSMSMode == REQUEST_CAR_INFO_SMS) && dtmfCommandMode == DC_CONFIRMED)
  {
    String str = "";
    if (sendSMSMode == REQUEST_GPS_SMS)
      str = GetGPSData();
    else if (sendSMSMode == REQUEST_CAR_INFO_SMS)
      str = getCarInfo();
    SendSMS(str, incomingPhone);
    sendSMSMode = SMS_NO;
    dtmfCommandMode = DC_EXECUTED;
  }

  //ALARM_OFF, REQUEST_ALARM_ON
  if (doAlarmMode == REQUEST_ALARM_ON && dtmfCommandMode == DC_CONFIRMED)
  {
    digitalWrite(ALARM_OUT_PIN, HIGH);
    _delay_ms(5000);
    digitalWrite(ALARM_OUT_PIN, LOW);
    doAlarmMode = ALARM_OFF;
    dtmfCommandMode = DC_EXECUTED;
  }

  //OPEN_DOOR_OFF, REQUEST_OPEN_DOOR_ON
  if (openDoorMode == REQUEST_OPEN_DOOR_ON && dtmfCommandMode == DC_CONFIRMED)
  {
    //digitalWrite(OPEN_DOOR_PIN, HIGH);
    _delay_ms(100);
    //    digitalWrite(OPEN_DOOR_PIN, LOW);
    _delay_ms(60000);
    //  digitalWrite(CLOSE_DOOR_PIN, HIGH);
    _delay_ms(100);
    //    digitalWrite(CLOSE_DOOR_PIN, LOW);
    openDoorMode = OPEN_DOOR_OFF;
    dtmfCommandMode = DC_EXECUTED;
  }
}

void DoCall(boolean callToMyPhone)
{
  String sPhone;
  if (callToMyPhone)
    sPhone = workWithEEPROM(0, EE_PHONE_NUMBER, 0, "");
  else
    sPhone = incomingPhone;
  SIMflush();
  Serial.println("Call to " + sPhone);
  mySerialGSM.println("AT");
  _delay_ms(100);
  mySerialGSM.println("ATD+" + sPhone + ";"); //позвонить
  _delay_ms(500);

  outCallStarted_ms = 0;
  bOutgoingCallToneStarted = false;
  gsmMode = OUTGOING_CALL_PROGRESS;
  /*ATD + 790XXXXXXXX; Позвонить на номер + 790XXXXXXXX;
    NO DIALTONE Нет сигнала
    BUSY  Если вызов отклонён
    NO CARRIER Повесили трубку
    NO ANSWER Нет ответа*/
}

void SendSMS(String text, String phone)
{
  Serial.println("SMS send started");
  Serial.println("phone:" + phone);
  Serial.println("text:" + text);
  /*mySerialGSM.print("AT+CMGS=\"");
    mySerialGSM.print("+" + phone);
    mySerialGSM.println("\"");
    _delay_ms(1000);
    mySerialGSM.print(text);

    _delay_ms(500);

    mySerialGSM.write(0x1A);
    mySerialGSM.write(0x0D);
    mySerialGSM.write(0x0A);*/
  Serial.println("SMS send finish");
}

//for incoming and outgoing calls
void BreakCall()
{
  mySerialGSM.println("AT");
  _delay_ms(200);

  mySerialGSM.println("ATH");
  _delay_ms(500);
}

void AnswerCall()
{
  mySerialGSM.println("AT");
  _delay_ms(200);
  mySerialGSM.println("ATA");  //ответ
  _delay_ms(200);
}

//Reset all statuses in the end
void FinalResetStatuses()
{
  Serial.println("FinalReset");
  if (heatMode == CHECK_HEAT_POSSIBILITY || heatMode == WAIT_CONF_HEAT_START)
    heatMode = HEAT_OFF;
  if (heatMode == REQUEST_HEAT_OFF)
    heatMode = HEAT_TIME1_ON;
  if (addDeviceMode == REQUEST_ADD_DEVICE_ON)
    addDeviceMode = ADD_DEVICE_OFF;
  if (addDeviceMode == REQUEST_ADD_DEVICE_OFF)
    addDeviceMode = ADD_DEVICE_ON;
  if (registratorMode == REQUEST_REGISTRATOR_ON)
    registratorMode = REGISTRATOR_OFF;
  micMode = MIC_OFF;
  sendSMSMode = SMS_NO;
  doAlarmMode = ALARM_OFF;
  if (openDoorMode == REQUEST_OPEN_DOOR_ON)
    openDoorMode = OPEN_DOOR_OFF;
  addDTMFParam = 0;

  dtmfCommandMode = DC_WAIT;
  gsmMode = WAIT_GSM;
  mySerialGSM.println("AT");
  _delay_ms(100);
  mySerialGSM.println("AT");
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
  pinMode(WAKE_UP_PIN, INPUT_PULLUP);
  attachInterrupt(0, WakeUp, FALLING);

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
  // power_adc_enable();    // ADC converter
  // power_spi_enable();    // SPI
  //power_usart0_enable(); // Serial (USART)
  // power_timer0_enable(); // Timer 0
  // power_timer1_enable(); // Timer 1
  // power_timer2_enable(); // Timer 2
  // power_twi_enable();    // TWI (I2C)

  // запрещаем прерывания
  cli();

  isActiveWork = true;
  afterWakeUp_ms = 0;
}

void loop()
{
  GetSoftSerialData();
  CheckSoftwareSerialData();
  WorkflowGSM();
  WorkflowMain(0);
  //CheckIgnitionAndU220();
  //CheckHeatAndU220();
  //RefreshSensorData();
  //CheckAlarmLine();


  //    if (mySerialGSM.available())
  //      Serial.write(mySerialGSM.read());
  //    if (Serial.available())
  //      mySerialGSM.write(Serial.read());

  //_delay_ms(100);

  if (millis() - c > (1000.0 * 60))
  {
    digitalWrite(BZZ_PIN, HIGH);
    _delay_ms(100);
    digitalWrite(BZZ_PIN, LOW);
    //
    Serial.println("------");
    c = millis();
    //    ozv(1, 5, false);
    //    Serial.println(c);
    //    Serial.println(gsmMode);
    //    Serial.println(dtmfCommandMode);
    //    Serial.println(heatMode);
    mySerialGSM.println("AT");
    _delay_ms(100);
    mySerialGSM.println("AT");
    _delay_ms(100);
    //    //    mySerialGSM.println("AT+CBC");
    //    //    _delay_ms(100);
    //    //    mySerialGSM.println("AT+CSQ");
    //    //    _delay_ms(100);
  }

  if (gsmMode == WAIT_GSM && heatMode == HEAT_OFF && afterWakeUp_ms > AFTER_WAKE_UP_PERIOD_S * 1000)
    isActiveWork = false; //reset flag in the end of work

  if (!isActiveWork)
  {
    PrepareSleep();
    Serial.println("DoSleep");
    _delay_ms(20);
    DoSleep();
    //Serial.flush();
    _delay_ms(100);
    Serial.println("ExitSleep");
    mySerialGSM.println("AT");
    _delay_ms(100);
    wdt_enable(WDTO_8S);
  }
  wdt_reset();
}
