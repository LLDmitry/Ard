
#include <SoftwareSerial.h>
#include <elapsedMillis.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <mp3TF.h>
#include <EEPROM.h>
#include <TinyGPS.h>

//Выводы Arduino, соответствующие аналоговым входам, имеют номера от 14 до 19. - pinMode(14, OUTPUT); digitalWrite(14, HIGH);
#define BZZ_PIN 1
#define SOFT_RX_PIN 2
#define SOFT_TX_PIN 3
#define MP3_BUSY_PIN 4    // пин от BUSY плеера
#define U_220_PIN 5       // контроль наличия 220в
#define ONE_WIRE_PIN 6    // DS18b20
#define ALARM_CHECK_PIN 7 // 1 при срабатывании сигнализации
#define IGNITION_PIN 8    // 1 при включении зажигания
#define ALARM_OUT_PIN 9   // включает тревогу
#define BTTN_PIN 10       // ручное управление командами
#define OPEN_DOOR_PIN 11  // открыть дверь
#define CLOSE_DOOR_PIN 12 // закрыть дверь
#define HEAT_PIN 13       // управление электроподогревом
#define ADD_DEVICE_PIN 14       // дополнительное устройство (pin A1)

#define CAR_VOLT_PIN A3   // измерение напряжения бортовой сети


SoftwareSerial mySerial(SOFT_RX_PIN, SOFT_TX_PIN);
TinyGPS gps;


const int T_COLD = -10;
const unsigned long HEAT_TIME1_M = 5; // > T_COLD C
const unsigned long HEAT_TIME2_M = 7; // < T_COLD C
const unsigned long REFRESH_SENSOR_INTERVAL_S = 100;
const unsigned long IN_UNKNOWN_ANSWER_PERIOD_S = 20;
const unsigned long IN_ANSWER_PERIOD_S = 5;
const unsigned long IN_UNKNOWN_CALL_TALK_PERIOD_S = 50;
const unsigned long IN_CALL_TALK_PERIOD_S = 50;
const unsigned long IN_DISCONNECT_PERIOD_S = 25;
const unsigned long OUT_DISCONNECT_PERIOD_S = 15;
const unsigned long OUT_INFORM_PERIOD_S = 8;
const unsigned long RECALL_PERIOD_S = 7;
const unsigned long BLOCK_UNKNOWN_PHONES_PERIOD_S = 1800; //30min

const byte INDEX_ALARM_PNONE = 1;               //index in phonesEEPROM[5]
const byte MAX_NUMBER_ATTEMPTS_UNKNOWN_PHONES = 3;  //После MAX_NUMBER_ATTEMPTS_UNKNOWN_PHONES неудачных попыток (с вводом неверного пароля) за последние 10 мин, блокируем (не берем трубку) звонки с любых неизвестных номеров на 30мин либо до звонка с известного номера (что раньше).
const byte ADR_EEPROM_RECALL_ME = 1;            //useRecallMeMode
const byte ADR_EEPROM_STORED_PHONES = 100;      //начало списка 7значных номеров телефонов (5шт по 11 байт)
const byte ADR_EEPROM_PASSWORD_UNKNOWN_PHONES = 10; //начало пароля для доступа неопознанных тел-в
const byte ADR_EEPROM_PASSWORD_ADMIN = 20;      //начало админского пароля

bool useRecallMeMode = false;          // true - will recall for receiing DTMF, false - will answer for receiing DTMF
byte incomingPhoneID;
String incomingPhone;
byte ringNumber;
byte numberAttemptsUnknownPhones;

byte stateDTMF;
boolean AlarmMode = false;
String resultFullDTMF = "";                                   // Переменная для хранения вводимых DTMF данных
String resultCommandDTMF = "";                                // Переменная для хранения введенной DTMF команды
String resultPasswordDTMF = "";                               // Переменная для хранения введенного пароля DTMF
String resultAdminPasswordDTMF = "";                          // Переменная для хранения введенного админского пароля DTMF


boolean alarmStatus = false;

String _response = "";              // Переменная для хранения ответов модуля

enum EnCallInform { CI_NO_220, CI_BREAK_220, CI_ALARM };

enum EnMP3Mode { M_WAIT, M_ASK_DTMF, M_ASK_PASSWORD, M_RECALL_MODE_CHANGE, M_DTMF_RECOGN, M_DTMF_NO_RECOGN, M_COMMAND_APPROVED } mp3Mode = M_WAIT;
enum EnInfoSpeach {
  S_ASK_DTMF, S_ASK_PASSWORD, S_RECALL_MODE_CHANGE, S_DTMF_RECOGN, S_DTMF_NO_RECOGN, S_COMMAND_APPROVED, S_CAR_ALARM,
  S_HEAT_FINISHED, S_NO_220, S_BREAK_220
};

enum EnDTMF { D_SAY_ALL_COMMANDS, D_ON_HEAT, D_OFF_HEAT, D_INFO_HEAT, D_DO_ALARM, D_REQUEST_INFO_CAR, D_REQUEST_GPS_SMS,
              D_SWITCH_ON_REGISTRATOR, D_SWITCH_ON_ADD_DEViCE, D_SWITCH_OFF_ADD_DEViCE, D_OPEN_DOOR,
              D_SAY_ALL_SETTINGS, D_ADD_THIS_PHONE, D_CHANGE_PASSWORD, D_CHANGE_ADMIN_PASSWORD, D_CHANGE_RECALL_MODE,
              D_WAIT_CONFIRMATION, D_CONFIRMED,
            };

enum EnHeatMode { HEAT_OFF, CHECK_HEAT_POSSIBILITY, WAIT_CONF_HEAT_START, HEAT_START_CONFIRMED,
                  HEAT_TIME1_ON, HEAT_TIME2_ON, REQUEST_HEAT_OFF
                } heatMode = HEAT_OFF;
enum EnGSMMode {
  WAIT, INCOMING_UNKNOWN_CALL_START, INCOMING_CALL_START, INCOMING_CALL_PROGRESS, INCOMING_UNKNOWN_CALL_PROGRESS,
  INCOMING_CALL_DISCONNECTED, INCOMING_CALL_HANGUP,
  TODO_RECALL,
  RECALL_PROGRESS, RECALL_ANSWERED, RECALL_HANGUP, RECALL_NOANSWER, RECALL_BUSY,
  INCOMING_CALL_ANSWERED, INCOMING_UNKNOWN_CALL_ANSWERED,
  WAIT_PSWD
} gsmMode = WAIT;

float t_eng;    //температура двигателя
float t_out;    //температура воздуха
float t_socket; //температура разъема 220v

elapsedMillis inCallReaction_ms;
elapsedMillis outDisconnect_ms;
elapsedMillis recallPeriod_ms;
elapsedMillis inCallTalkPeriod_ms;
elapsedMillis heatTime_ms;
elapsedMillis lastRefreshSensor_ms;
elapsedMillis blockUnknownPhones_ms;

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress innerTempDeviceAddress;
DeviceAddress outerTempDeviceAddress;
DeviceAddress socketTempDeviceAddress;

mp3TF mp3tf = mp3TF();

unsigned long c = 0;

void setup()
{
  //Initialize serial ports for communication.
  Serial.begin(9600);
  mySerial.begin(9600);

  Serial.println("Setup start");

  pinMode(ALARM_CHECK_PIN, INPUT_PULLUP);
  pinMode(IGNITION_PIN, INPUT);
  pinMode(U_220_PIN, INPUT);
  pinMode(BTTN_PIN, INPUT);
  pinMode(HEAT_PIN, OUTPUT);
  pinMode(BZZ_PIN, OUTPUT);
  pinMode(ALARM_OUT_PIN, OUTPUT);
  pinMode(OPEN_DOOR_PIN, OUTPUT);
  pinMode(CLOSE_DOOR_PIN, OUTPUT);
  pinMode(ADD_DEVICE_PIN, OUTPUT);

  useRecallMeMode = EEPROM.read(0);

  cellSetup();

  // первый раз, потом закоментарить
 // InitialEepromSettings();

  //mp3
  mp3tf.init(&mySerial);
  delay(200);
  pinMode(MP3_BUSY_PIN, INPUT);
  mp3tf.volumeSet(15);

  sensors.begin();
  sensors.getAddress(innerTempDeviceAddress, 0);
  sensors.getAddress(outerTempDeviceAddress, 1);
  sensors.getAddress(socketTempDeviceAddress, 2);
  sensors.setResolution(innerTempDeviceAddress, 10);
  sensors.setResolution(outerTempDeviceAddress, 10);
  sensors.setResolution(socketTempDeviceAddress, 10);


  digitalWrite(BZZ_PIN, HIGH);
  delay(300);
  digitalWrite(BZZ_PIN, LOW);
  Serial.println("Setup done");
}

void cellSetup()
{
  delay(20000);
  mySerial.begin(9600);

  delay(1000);

  mySerial.print("AT+IPR?\r");
  delay(300);

  mySerial.print("AT+CMGF=1\r");  //    mySerial.print("AT+CCMGF=1\r");
  delay(300);

  mySerial.print("AT+IFC=1, 1\r");
  delay(300);

  mySerial.print("AT+CPBS=\"SM\"\r");
  delay(300);

  mySerial.print("AT+CNMI=1,2,2,1,0\r");
  delay(500);

  mySerial.print("AT+CLIP=1\r");  //разрешает показ инфо о входящем вызове номер
  delay(300);

  SIMflush();

  Serial.println("Setup done");

  digitalWrite(BZZ_PIN, HIGH);
  delay(300);
  digitalWrite(BZZ_PIN, LOW);
}

void cellSetup1()
{
  delay(15000);

  mySerial.println("AT");
  mySerial.println("AT+DDET=1,1000,0,0");
  delay(1000);
  mySerial.println("AT+CLIP=1");
  delay(1000);
  mySerial.println("AT+CSCLK=2");
  delay(1000);
  //  mySerial.println("AT+IPR?");
  //  delay(300);
  //
  //  mySerial.println("AT+CMGF=1");
  //  delay(300);
  //
  //  mySerial.println("AT+IFC=1, 1");
  //  delay(300);
  //
  //  mySerial.print("AT+CPBS=\"SM\"\r");
  //  delay(300);
  //
  //  mySerial.println("AT+CNMI=1,2,2,1,0");
  //  delay(500);
  //
  //  mySerial.println("AT+CLIP=1");  //разрешает показ инфо о входящем вызове номер
  //  delay(300);
  //
  //  mySerial.println("AT+DDET=1,100,0,0");  //разрешает прием DTMF (2й параметр - минимальная пауза милисек к-я д.б. между тонами)
  //  delay(300);
  //
  //  mySerial.println("AT+CSCLK=2"); //спящий режим
  //  delay(300);
  //
  //  //установить длительность исходящих DTMF
  //  mySerial.println("AT+VTD=3");
  //  delay(300);
  //
  //  //Брать время gsm сети (при перегистрации, но работает не у всех операторов)
  //  mySerial.println("AT+CLTS=1");
  //  delay(300);
  //  mySerial.println("AT&W");
  //  delay(300);

  SIMflush();
}

void SIMflush()
{
  while (mySerial.available() != 0)
  {
    mySerial.read();
  }
}

//1й старт, потом закоментарить
void InitialEepromSettings()
{
  //4 set recallMe mode=1
  workWithEEPROM(1, 4, 0, "1");
  //5 add my phone number
  workWithEEPROM(1, 5, 0, "79062656420");
  //2 set unknown psswd
  workWithEEPROM(1, 2, 0, "1111");
  //3 set admin psswd
  workWithEEPROM(1, 3, 0, "1234");
}

void CheckAlarmLine()
{
  alarmStatus = digitalRead(ALARM_CHECK_PIN);  //store new condition
}

void SendSMS(String text, String phone)
{
  Serial.println("SMS send started");
  mySerial.print("AT+CMGS=\"");
  mySerial.print("+" + phone);
  mySerial.println("\"");
  delay(1000);
  mySerial.print(text);

  delay(500);

  mySerial.write(0x1A);
  mySerial.write(0x0D);
  mySerial.write(0x0A);
  Serial.println("SMS send finish");
}

void DoCall()
{
  SIMflush();
  Serial.println("call");
  mySerial.println("ATD+" + incomingPhone + ";"); //позвонить
  delay(100);
  /*ATD + 790XXXXXXXX; Позвонить на номер + 790XXXXXXXX;
    NO DIALTONE Нет сигнала
    BUSY  Если вызов отклонён
    NO CARRIER Повесили трубку
    NO ANSWER Нет ответа*/
}

void sayInfo(EnInfoSpeach commandInfo)  //EnInfoSpeach { ASK_DTMF, ASK_PASSWORD, DTMF_RECOGN, DTMF_NO_RECOGN, COMMAND_APPROVED, CAR_ALARM, HEAT_FINISHED, NO_220, BREAK_220 };
{
  { mp3tf.playFolder2(1, commandInfo);
    delay(200);
    while (!digitalRead(MP3_BUSY_PIN));
  }
}

void InformCall(EnCallInform typeCallInform) //CI_NO_220, CI_BREAK_220, CI_ALARM
{
  DoCall();
  switch (typeCallInform)
  {
    case CI_NO_220:
      sayInfo(S_NO_220);
      break;
    case CI_BREAK_220:
      sayInfo(S_BREAK_220);
      break;
    case CI_ALARM:
      sayInfo(S_CAR_ALARM);
      break;
  }
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
    delay(500);
    digitalWrite(BZZ_PIN, LOW);
    delay(200);
  }
}

String GetSoftSerialData()
{ // Функция ожидания ответа и возврата полученного результата
  Serial.println("GetSoftSerialData 1");
  String _resp = "";                                  // Переменная для хранения результата
  //  long _timeout = millis() + 10000;                   // Переменная для отслеживания таймаута (10 секунд)
  //  while (!mySerial.available() && millis() < _timeout)  {}; // Ждем ответа 10 секунд, если пришел ответ или наступил таймаут, то...
  //  if (mySerial.available()) {                           // Если есть, что считывать...
  //    _resp = mySerial.readString();                      // ... считываем и запоминаем
  //  }
  //  else {                                              // Если пришел таймаут, то...
  //    Serial.println("Timeout...");                     // ... оповещаем об этом и...
  //  }
  char ch = ' ';
  while (mySerial.available())
  {
    ch = mySerial.read();
    _resp += char(ch); //собираем принятые символы в строку
    delay(5);
  }
  Serial.println("GetSoftSerialData 2");
  return _resp;                                       // ... возвращаем результат. Пусто, если проблема
}


String workWithEEPROM(byte mode, byte dataType, byte addParam, byte* arrDataToSave)  //mode = 0- read, 1- write;
//sDataSave - string to save (for mode=1);
//dataType = 1 - телефонный номер (index = addParam), 2 - PasswordUnknownPhones, 3 - PasswordAdmin, 4 - change UseRecallMe, 5 - AddThisPhoneToStored
{
  byte* result;
  byte adrEEPROM;
  byte numSymbols;
  switch (dataType)
  {
    case 1:
    case 5:
      adrEEPROM = ADR_EEPROM_STORED_PHONES;
      numSymbols = 11;
      break;
    case 2:
      adrEEPROM = ADR_EEPROM_PASSWORD_UNKNOWN_PHONES;
      numSymbols = 4;
      break;
    case 3:
      adrEEPROM = ADR_EEPROM_PASSWORD_ADMIN;
      numSymbols = 4;
      break;
    case 4:
      adrEEPROM = ADR_EEPROM_RECALL_ME;
      numSymbols = 1;
      break;
  }

  for (byte k = 0; k < numSymbols; k++)
  {
    if (mode == 0)
      result[k] = EEPROM.read(k + adrEEPROM + addParam * numSymbols);
    else
    {
      //      Serial.print("k + ");
      //      Serial.print(k + adrEEPROM + addParam * numSymbols);
      //      Serial.print("k: ");
      //      Serial.print(k);
      //      Serial.print(" arrDataToSave[k]: ");
      //      Serial.println(arrDataToSave[k]);
      EEPROM.write(k + adrEEPROM + addParam * numSymbols, arrDataToSave[k]);
    }
  }
  return String((char*)result);
}


boolean checkPhone(String str)
{
  Serial.println("CheckPhone");
  //  "+CMT: "+79062656420",,"13/04/17,22:04:05+22"    //SMS
  //  "+CLIP: "79062656420",145,,,"",0"                //INCOMING CALL
  incomingPhoneID = 0;
  incomingPhone = "";
  if (str.indexOf("+CLIP:") > -1 || str.indexOf("+CMT:") > -1)
  {
    for (byte i = 0; i < 5; i++)
    {
      String sPhone = workWithEEPROM(0, 1, i, "");
      Serial.print("EEPROM phone: ");
      Serial.print(i);
      Serial.println(" : " + sPhone);
      if (str.indexOf(sPhone) > -1)
      {
        incomingPhoneID = i + 1;
        incomingPhone = sPhone;
        Serial.print("incomingPhone: ");
        Serial.println(incomingPhone);
        break;
      }
    }
  }
  if (incomingPhone == "")
    Serial.println("unknown phone");
  return (incomingPhoneID > 0);
}

String GetGPSData()
{
  String sResult = "";
  float lat, lon;
  unsigned long age;

  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (mySerial.available())
    {
      char c = mySerial.read();
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

void ExecuteDtmfCommand(EnDTMF command, byte* arrDataToSave)  //enum EnDTMF { D_ON_HEAT, D_OFF_HEAT, D_INFO_HEAT, D_REQUEST_INFO_CAR, D_OPEN_DOOR, D_CHANGE_PASSWORD, D_CHANGE_ADMIN_PASSWORD, D_RECALL_MODE_CHANGE, D_REQUEST_GPS_SMS };
{
  String str = "";
  switch (command)
  {
    case D_REQUEST_INFO_CAR:
      str = getCarInfo();
      SendSMS(str, incomingPhone);
      break;
    case D_REQUEST_GPS_SMS:
      str = GetGPSData();
      SendSMS(str, incomingPhone);
      Serial.println("GPS sent");
      break;
    case D_ON_HEAT:
      heatMode = CHECK_HEAT_POSSIBILITY;
      break;
    case D_OFF_HEAT:
      heatMode = REQUEST_HEAT_OFF;
      break;
    case D_SWITCH_ON_REGISTRATOR:
      Serial.println("Reg");
      break;
    case D_DO_ALARM:
      Serial.println("D_DO_ALARM");
      digitalWrite(ALARM_OUT_PIN, HIGH);
      delay(5000);
      digitalWrite(ALARM_OUT_PIN, LOW);
      break;
    case D_OPEN_DOOR:
      Serial.println("D_OPEN_DOOR");
      digitalWrite(OPEN_DOOR_PIN, HIGH);
      delay(100);
      digitalWrite(OPEN_DOOR_PIN, LOW);
      delay(60000);
      digitalWrite(CLOSE_DOOR_PIN, HIGH);
      delay(100);
      digitalWrite(CLOSE_DOOR_PIN, LOW);
      break;
    case D_SWITCH_ON_ADD_DEViCE:
      digitalWrite(ADD_DEVICE_PIN, HIGH);
      break;
    case D_SWITCH_OFF_ADD_DEViCE:
      digitalWrite(ADD_DEVICE_PIN, LOW);
      break;
    case D_CHANGE_PASSWORD:
    case D_CHANGE_ADMIN_PASSWORD:
    case D_CHANGE_RECALL_MODE:
    case D_ADD_THIS_PHONE:
      workWithEEPROM(1, (command == D_CHANGE_PASSWORD ? 2 :
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
// Parsing DTMF команд, вызов ExecuteDtmfCommand()
void ProcessingDTMF(String symbol)
{
  Serial.println("Key: " + symbol);                         // Выводим в Serial для контроля, что ничего не потерялось
  if (symbol == "#")                                        //признак завершения ввода команды
  {
    bool correct = false;                                   // Для оптимизации кода, переменная корректности команды
    //    if (resultFullDTMF.length() == 2)
    //    {
    //int ledIndex = ((String)resultFullDTMF[0]).toInt();     // Получаем первую цифру команды - адрес устройства (1-3)
    //int ledState = ((String)resultFullDTMF[1]).toInt();     // Получаем вторую цифру команды - состояние (0 - выкл, 1 - вкл)

    EnDTMF command;


    if (correct)
    {
      ParsingDTMF();
      //  resultAdminPasswordDTMF
      byte* arrDataToSave;
      switch (resultCommandDTMF[0])
      {
        case 0:
          // Serial.println("0 - проговорить все доступные DTMF команды (кроме настроечных)");
          command = D_SAY_ALL_COMMANDS;
          break;
        case 1:
          // Serial.println("1 - получить сообщение о состоянии ALARM, подогрева, температуры, напряжения,..");
          command = D_REQUEST_INFO_CAR;
          break;
        case 2:
          // Serial.println("21/20 - включить/выключить подогрев (на время в зависимости от T). Если только 1 цифра - то поменять с выкл на вкл или наоборот");
          command = (resultCommandDTMF[1] == 1 ? D_ON_HEAT : D_OFF_HEAT);
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
          command = (resultCommandDTMF[1] == 1 ? D_SWITCH_ON_ADD_DEViCE : D_SWITCH_OFF_ADD_DEViCE);
          break;
        case 8:
          //Serial.println("8 - прислать sms с GPS координатами");
          command = D_REQUEST_GPS_SMS;
          break;

        //Настроечные команды - resultCommandDTMF[0]=9
        case 9:
          switch (resultCommandDTMF[1])
          {
            case 0:
              // Serial.println("0 - проговорить все доступные настроечные DTMF команды");
              command = D_SAY_ALL_SETTINGS;
              break;
            case 1:
              command = D_ADD_THIS_PHONE;
              break;
            case 2:
              command = D_CHANGE_PASSWORD;
              //arrDataToSave = resultAdminPasswordDTMF;
              break;
            case 3:
              command = D_CHANGE_ADMIN_PASSWORD;
              //arrDataToSave = resultAdminPasswordDTMF;
              break;
            case 4:
              command = D_CHANGE_RECALL_MODE;
              if (resultCommandDTMF[2] == 1)
                arrDataToSave = 1;
              else
                arrDataToSave = 0;
              break;
            default:
              correct = false;
              break;
          }
        default:
          correct = false;
          break;
      }
      ExecuteDtmfCommand(command, arrDataToSave);
    }
    else
      Serial.println("Wrong command: " + resultFullDTMF);     // Если команда некорректна, выводим сообщение
    resultFullDTMF = "";                                      // После каждой решетки сбрасываем вводимую комбинацию
  }
  else  //"#" еще нет, продолжаем собирать команду
  {
    if (symbol == "*")  //команда введена, дальше будет пароль
    {
    }
    else
    {
      resultFullDTMF += symbol;
    }
  }
}

String ParsingDTMF()
{
  byte posEndCommand;
  byte posEndPassword;
  byte posEndAdminPassword;
  if (resultFullDTMF.indexOf("*") == -1) //only command, without password
  {
    posEndCommand = resultFullDTMF.indexOf("#");
  }
  else  //command and password(s)
  {
    posEndCommand = resultFullDTMF.indexOf("*");
    if (resultFullDTMF.substring(posEndCommand + 1).indexOf("*") != -1) //only 1 password
    {
      posEndPassword = resultFullDTMF.substring(posEndCommand + 1).indexOf("*");
      posEndAdminPassword = resultFullDTMF.substring(posEndPassword + 1).indexOf("#");
    }
    else  //only 1 password
    {
      posEndPassword = resultFullDTMF.substring(posEndCommand + 1).indexOf("#");
    }
  }

  resultCommandDTMF = resultFullDTMF.substring(0, posEndCommand);
  resultPasswordDTMF = resultFullDTMF.substring(posEndCommand + 1, posEndPassword);
  resultAdminPasswordDTMF = resultFullDTMF.substring(posEndPassword + 1, posEndAdminPassword);
}

void CheckGSMModule()
{
  //Serial.println("CheckGSMModule");
  //If a character comes in from the cellular module...
  if (mySerial.available()) //если модуль что-то послал
  {
    char ch = ' ';
    String currStr = "";
    while (mySerial.available())
    {
      ch = mySerial.read();
      currStr += char(ch); //собираем принятые символы в строку
      delay(5);
    }
    Serial.print("GSM module talk> ");
    Serial.println(currStr);

    if (currStr.indexOf("RING") > -1)
    {
      ringNumber += 1;
    }

    //currStr.startsWith("+CLIP")
    if (gsmMode == WAIT && currStr.indexOf("+CLIP") > -1) //если пришел входящий вызов
    {
      Serial.println("INCOMING RING!");
      if (checkPhone(currStr))  // из текущей строки выберем тел номер. если звонящий номер есть в списке доступных, можно действовать
      {
        Serial.println("Call from Phone: " + incomingPhone + " " + incomingPhoneID);
        gsmMode = INCOMING_CALL_START;
      }
      else
      {
        gsmMode = INCOMING_UNKNOWN_CALL_START;
        Serial.println("Call from Phone: UNKNOWN");
      }
    }
    else if (gsmMode == INCOMING_CALL_PROGRESS && currStr.indexOf("NO CARRIER") > -1) //если входящий вызов сбросили не дождавшись ответа
    {
      Serial.println("Incoming call is hang up");
      gsmMode = INCOMING_CALL_HANGUP;
    }
  }
}

void CheckGSMModule1()
{
  //Serial.println("CheckGSMModule");
  //If a character comes in from the cell module...
  if (mySerial.available())
  { // Если модем, что-то отправил...
    Serial.println(millis());
    char ch = ' ';
    String submsg = "";
    while (mySerial.available())
    {
      ch = mySerial.read();
      submsg += char(ch); //собираем принятые символы в строку
      delay(5);
    }
    // _response = GetSoftSerialData();                       // Получаем ответ от модема для анализа
    //    String submsg = "";
    //    submsg = _response;


    //    Serial.println(">" + _response);                  // Выводим полученную пачку сообщений
    //    int index = -1;
    //    do
    //    { // Перебираем построчно каждый пришедший ответ
    //      index = _response.indexOf("\r\n");              // Получаем идекс переноса строки
    //      String submsg = "";
    //      if (index > -1)
    //      { // Если перенос строки есть, значит
    //        submsg = _response.substring(0, index);       // Получаем первую строку
    //        _response = _response.substring(index + 2);   // И убираем её из пачки
    //      }
    //      else
    //      { // Если больше переносов нет
    //        submsg = _response;                           // Последняя строка - это все, что осталось от пачки
    //        _response = "";                               // Пачку обнуляем
    //      }


    submsg.trim();                                  // Убираем пробельные символы справа и слева
    if (submsg != "")
    { // Если строка значимая (не пустая), то распознаем уже её
      //      Serial.println("GSM > ");
      //      Serial.println(submsg);
      //      Serial.println("<GSM");
      //      Serial.print("indexOf(+CLIP) = ");
      //      Serial.println(submsg.indexOf("+CLIP"));
      //      Serial.println(millis());

      //        if (submsg.indexOf("RING") > -1)  //входящий (исходящий?)
      //        {
      //          ringNumber += 1;
      //          Serial.print("ringNumber > ");
      //          Serial.println(ringNumber);
      //        }

      //        Serial.print("submsg = ");
      //        Serial.println(submsg);
      //        Serial.print("indexOf(+CLIP) = ");
      //        Serial.println(submsg.indexOf("+CLIP"));

      if (gsmMode == WAIT && submsg.indexOf("+CLIP") > -1) //если пришел входящий вызов
      {
        Serial.println("INC RING!");
        if (checkPhone(submsg))  // из текущей строки выберем тел номер. если звонящий номер есть в списке доступных, можно действовать
        {
          Serial.println("Call from Phone: " + incomingPhone + " " + incomingPhoneID);
          gsmMode = INCOMING_CALL_START;

          mySerial.println("ATH");
          delay(100);
          mySerial.println("ATD+79062656420;"); //позвонить
          delay(100);
        }
        else
        {
          gsmMode = INCOMING_UNKNOWN_CALL_START;
          Serial.println("Call from Phone UNKN");
        }
      }
      //      else if (gsmMode == INCOMING_CALL_PROGRESS && submsg.indexOf("NO CARRIER") > -1) //если входящий вызов сбросили не дождавшись ответа
      //      {
      //        Serial.println("Inc call hang up");
      //        gsmMode = INCOMING_CALL_HANGUP;
      //      }
      //      else if (gsmMode == RECALL_PROGRESS && submsg.indexOf("NO CARRIER") > -1) //если исходящий вызов сбросили
      //      {
      //        Serial.println("Recall is hang up");
      //        gsmMode = RECALL_HANGUP;
      //      }
      //      else if (gsmMode == RECALL_PROGRESS && submsg.indexOf("BUSY") > -1) //если на исходящий вызов линия занята
      //      {
      //        Serial.println("Recall - line busy");
      //        gsmMode = RECALL_BUSY;
      //      }
      //      else if (gsmMode == RECALL_PROGRESS && submsg.indexOf("NO ANSWER") > -1) //если на исходящий вызов линия  нет ответа
      //      {
      //        Serial.println("Recall - line no answer");
      //        gsmMode = RECALL_NOANSWER;
      //      }
      //      else if (gsmMode == RECALL_PROGRESS && submsg.indexOf("NO DIALTONE") > -1) //если нет сигнала
      //      {
      //        Serial.println("Recall - line no dialtone");
      //        gsmMode = RECALL_NOANSWER;
      //      }
      //      else if (gsmMode == RECALL_PROGRESS && submsg.indexOf("OKKKK") > -1) //если на исходящий вызов ответили
      //      {
      //        Serial.println("Recall - answered");
      //        gsmMode = RECALL_ANSWERED;
      //      }
      //      else if (gsmMode == INCOMING_CALL_PROGRESS && (submsg.startsWith("+DTMF:"))) // Если ответ начинается с "+DTMF:" тогда:
      //      {
      //        String symbol = submsg.substring(7, 8);     // Выдергиваем символ с 7 позиции длиной 1 (по 8)
      //        ProcessingDTMF(symbol);                     // Логику выносим для удобства в отдельную функцию
      //      }
      //else if (submsg.indexOf("+CMT") > -1) //если пришло SMS
      //{
      //if (checkPhone(submsg))
      //}
    }
    //    } while (index > -1);                             // Пока индекс переноса строки действителен
  }
}

void WorkflowGSM()
{
  switch (gsmMode)
  {
    case WAIT:
      break;
    case INCOMING_CALL_START:
      inCallReaction_ms = 0;
      gsmMode = INCOMING_CALL_PROGRESS;
      break;
    case INCOMING_UNKNOWN_CALL_START: //wait 20c and answer for receiving dtmf pswd
      inCallReaction_ms = 0;
      gsmMode = INCOMING_UNKNOWN_CALL_PROGRESS;
      break;
    case INCOMING_CALL_PROGRESS:
    case INCOMING_UNKNOWN_CALL_PROGRESS:
      // отвечаем в случае useRecallMeMode-FALSE или звонке с неизвестного номера, иначе - перезваниваем
      if (inCallReaction_ms > (gsmMode == INCOMING_UNKNOWN_CALL_PROGRESS ? IN_UNKNOWN_ANSWER_PERIOD_S :
                               (useRecallMeMode ? IN_DISCONNECT_PERIOD_S : IN_ANSWER_PERIOD_S)) * 1000)
      {
        if (useRecallMeMode && gsmMode == INCOMING_CALL_PROGRESS)
        {
          Serial.println("IN Discon");
          mySerial.println("ATH");  //разрываем связь
        }
        else
        {
          Serial.println("IN Answ");
          mySerial.println("ATA");  //ответ
        }
        delay(100);

        if (useRecallMeMode && gsmMode == INCOMING_CALL_PROGRESS)
        {
          gsmMode = INCOMING_CALL_DISCONNECTED;
          recallPeriod_ms = 0;
        }
        else if (gsmMode == INCOMING_UNKNOWN_CALL_PROGRESS)
        {
          gsmMode = INCOMING_UNKNOWN_CALL_ANSWERED;
          mp3Mode = M_ASK_PASSWORD;
          inCallTalkPeriod_ms = 0;
        }
        else if (gsmMode == INCOMING_CALL_PROGRESS)
        {
          gsmMode = INCOMING_CALL_ANSWERED;
          mp3Mode = M_ASK_DTMF;
          inCallTalkPeriod_ms = 0;
        }
      }
      break;
    case INCOMING_CALL_HANGUP:
      gsmMode = WAIT;
      break;
    case INCOMING_CALL_DISCONNECTED:  //only when set useRecallMeMode
      if (recallPeriod_ms > RECALL_PERIOD_S * 1000)
      {
        Serial.println("Do ReCall");
        gsmMode = TODO_RECALL;
      }
      break;
    case INCOMING_CALL_ANSWERED:
    case INCOMING_UNKNOWN_CALL_ANSWERED:
      if (inCallTalkPeriod_ms > (gsmMode == INCOMING_CALL_ANSWERED ? IN_CALL_TALK_PERIOD_S : IN_UNKNOWN_CALL_TALK_PERIOD_S) * 1000)
      {
        Serial.println("INC_CALL_ANSW");
        gsmMode = WAIT;
      }
      break;

    case TODO_RECALL:
      Serial.println("TODO_RECALL");
      outDisconnect_ms = 0;
      DoCall();
      gsmMode = RECALL_PROGRESS;
      break;

    case RECALL_PROGRESS:
      if (outDisconnect_ms > OUT_DISCONNECT_PERIOD_S * 1000)
      {
        // Serial.println("OUT without answ. Discn");
        mySerial.println("ATH");  //разрываем связь через 20с
        delay(100);
        gsmMode = WAIT;
      }
      else
      {
        mySerial.println("AT+CLCC"); //???
      }
      break;
    case RECALL_ANSWERED:
      mp3Mode = M_ASK_DTMF;
      gsmMode = WAIT;
      break;

    case RECALL_HANGUP:
    case RECALL_NOANSWER:
    case RECALL_BUSY:
      gsmMode = WAIT;
      break;
  }
}

bool CheckPassword(byte passwordType, String pwd) //passwordType 2 - for UnknownPhones, 3 - Admin
{
  return (workWithEEPROM(0, passwordType, 0, "") == pwd);
}

void WorkflowMain(byte mode) //0-auto(from loop), 1-manual
{
  switch (heatMode)
  {
    case CHECK_HEAT_POSSIBILITY:
      if (digitalRead(U_220_PIN))
      {
        heatMode = WAIT_CONF_HEAT_START;
        Serial.println("heatMode = WAIT_CONF_HEAT_START");
      }
      else // нет 220
      {
        sayInfo(S_NO_220);
        heatMode = HEAT_OFF;
        Serial.println("heatMode = HEAT_OFF");
      }
      break;
    case HEAT_START_CONFIRMED:
      heatTime_ms = 0;
      heatMode = (t_eng > T_COLD ? HEAT_TIME1_ON : HEAT_TIME2_ON);
      digitalWrite(HEAT_PIN, HIGH);
      Serial.println("HEAT ON");
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
      heatMode = HEAT_OFF;
      digitalWrite(HEAT_PIN, LOW);
      // gsmMode = TODO_FINISH_INFO_CALL;
      Serial.println("HEAT OFF DONE");
      break;
  }

  switch (mp3Mode) //M_WAIT, M_ASK_DTMF, M_ASK_PASSWORD, M_RECALL_MODE_CHANGE, M_DTMF_RECOGN, M_DTMF_NO_RECOGN, M_COMMAND_APPROVED
  {
    case M_ASK_DTMF:
      sayInfo(S_ASK_DTMF);
      break;
    case M_ASK_PASSWORD:
      sayInfo(S_ASK_PASSWORD);
      CheckPassword(2, "");
      break;
    case M_DTMF_RECOGN:
      sayInfo(S_DTMF_RECOGN);
      break;
    case M_COMMAND_APPROVED:
      sayInfo(S_COMMAND_APPROVED);
      break;
  }
}

void loop()
{
  //CheckAlarmLine();
  CheckGSMModule();
  //WorkflowGSM();
  //CheckIgnitionAndU220();
  //CheckHeatAndU220();
  //RefreshSensorData();
  //WorkflowMain(0);

  //checkDTMF();

  if (millis() - c > (1000.0 * 60))
  {
    Serial.println("----------------");
    c = millis();
    mySerial.println("AT");
    delay(1000);
    mySerial.println("AT+CBC");
    delay(1000);
    mySerial.println("AT+CSQ");
    delay(1000);
  }

  //if (mySerial.available())
  //    Serial.write(mySerial.read());
  //if (Serial.available())
  //  mySerial.write(Serial.read());
}
