#include "DHT.h"
//#include <IRremote.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <Wire.h>
#include <DS1307.h>  //uno a4a5; mega 20 21
#include <math.h>
#include <AllSensorsUtils.h>  // contains typedef struct SmsCommand  - D:\Arduino\Setup\arduino-1.0.2\libraries\AllSensorsUtils
#include <avr/wdt.h>
#include <SoftwareSerial.h>
#include <SoftEasyTransfer.h>

#define RS485Transmit    HIGH
#define RS485Receive     LOW

#define DHTTYPE DHT22   // DHT 22  (AM2302)

const int IR_RECV_PIN = 7; //1 - out 2 - gnd 3 - vcc
const int DIST_TRIG_PIN = 5;
const int DIST_ECHO_PIN = 6;
const int IR_ALARM_PIN = 4;
const int SoilAnalogPIN = 0;
const int SVET_ANALOG_PIN = 1;
const int CELL_SOFT_RX_PIN = 10;  //  2 for UNO; 10 for Mega (connect GSM pin 2 with GSM pin 10, disconnect GSM pin 2 and Mega pin 2)
const int CELL_SOFT_TX_PIN = 3;
const int RX485_SOFT_RX_PIN = 15;
const int RX485_SOFT_TX_PIN = 16;
const int RX485_TX_CONTROL_PIN = 17;  //RS485 Direction control
const int DHT_PIN = 8;
const int ONE_WIRE_PIN = 12;
const int BZZ_PIN = 11;
const int LED_PIN = 13;

const byte MasterRoomsNagrevPins[8] = {20,21,22};
const byte MasterRoomsLightPins[8] = {23,24,25};
const int MasterRoomsCount = 3;
const int RoomsCount = 8;  //total= MasterRoomsCount + other rooms

const float DELTA_START_TEMP = 0.5;
const float DELTA_STOP_TEMP = 0.5;
const float ZERO_TEMPERATURE = 2.5; //for Zero nagrev operation
const int MAX_NAGREV_PERIOD_MINUTES = 20;//480; //will use if other period was not sent in command
const int MAX_ZERO_PERIOD_DAYS = 60;       //will use if other period was not sent in command
const float ControlDistanceSm = 60;

const unsigned long periodSetCommandsStatusSec = 30;
const unsigned long periodTermoPreControlSec = 120;
const unsigned long periodTermoControlSec = 1;
const unsigned long periodWait485Answer = 40; // millisec
const unsigned long period485ReSend = 50; // millisec


const int addrOper = 10;  //начальный адрес SmsCommands в EEPROM
const int CountCommandElements = 25;  //кол-во элементов в SmsCommand
const int addrTemperatureHours = 270;  //начальный адрес TemperatureHours в EEPROM
const int addrTemperatureDays = 280;  //начальный адрес TemperatureDays в EEPROM
const byte EMPTY_TEMP = '-';
const char RS485MyID = 'A';
const int MaxCountResend485 = 3;
const int Resolution18b20 = 11;

String incomingPhone;
int IncomingPhoneID;
String smsFromPnone;
boolean isStringMessage; // Переменная принимает значение True, если текущая строка является сообщением  
String currStr;
boolean flStart;
boolean FlNeedThermoControl;
boolean FlNewNagrevCommand;
unsigned long currTime;
unsigned long diffTime;
unsigned long loopTimeTermoPre;
unsigned long loopTimeTermo;
unsigned long loopTimeSetCommandStatus;
unsigned long loopTimeSaveTemperatureHistoryHours;
unsigned long loopTimeSaveTemperatureHistoryDays;
unsigned long loopTimeSend485Command;
unsigned long loopTimeRead485Answer;
//unsigned long delay18b20;

float Upit = 4.9;
float Temperature;
float Humidity;
float Svet;
float Soil;
int sensorValue;
float DistanceSm;

SmsCommand SmsCommands[10];
boolean RoomsLights[8];
float SetRoomsTemperatures[8];
byte MasterRoomsNagrevStatus[8];  //0-выключен; 1 - включен; 2 - выключить; 3 - включить
byte RoomsToSend[8];  //массив rooms для очередной отправки команд по 485
int NumberRoomsToSend;
int CounterRoomToSend;

byte CurrHour;
byte CurrMinute;
byte CurrSec;
byte CurrDay;
byte CurrMonth;
byte CurrYear;

byte PossibleMinutesLateStart;

char incoming_char;      //Will hold the incoming character from the Serial Port.
char serial_char;

//char Send485Status = 'R'; // 'R' - ready, 'W' - ожидание ответа, 'S' ожидание переотправки
boolean IsWaitingAnswer485;
int CountResend485;

DHT dht(DHT_PIN, DHTTYPE);
OneWire  ds(ONE_WIRE_PIN);
DeviceAddress tempDeviceAddress;
DallasTemperature sensors(&ds);

SoftwareSerial cell(CELL_SOFT_RX_PIN, CELL_SOFT_TX_PIN);  //Create a 'fake' serial port. Pin 2 is the Rx pin, pin 3 is the Tx pin.
SoftwareSerial RS485Serial(RX485_SOFT_RX_PIN, RX485_SOFT_TX_PIN); // RX, TX

char* phones[5]={"79062656420", "79217512416", "79217472342"};
boolean AlarmMode = false;
const int AlarmPnone = 1;  //number in phones[5]

boolean FlSendCmndReceivedInfo;

SoftEasyTransfer ET_Command;

typedef struct{
  char SrcID;
  char DstID;
  char Command;
  char Attribute;
  int Data1;
  float Data2;
  unsigned long Data3;
} CommandStruct;
CommandStruct Comm;

void setup()
{
  Serial.begin(9600);

  pinMode(BZZ_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(DIST_TRIG_PIN, OUTPUT); 
  pinMode(DIST_ECHO_PIN, INPUT);
  pinMode(IR_ALARM_PIN, INPUT);
  
  Serial.println("                      Start");
  
  dht.begin();
                                                    cellSetup();
  
  flStart = true;
  FlNeedThermoControl = false;
  incoming_char=0;
  serial_char=0;
  incomingPhone = "";    
  smsFromPnone = "";
  isStringMessage = false;
  IncomingPhoneID = 0;
  FlSendCmndReceivedInfo = false;
  
//  RTC.stop();
//  RTC.set(DS1307_SEC,00);        //set the seconds
//  RTC.set(DS1307_MIN,28);     //set the minutes
//  RTC.set(DS1307_HR,20);       //set the hours
//  RTC.set(DS1307_DOW,7);       //set the day of the week
//  RTC.set(DS1307_DATE,26);       //set the date
//  RTC.set(DS1307_MTH,1);        //set the month
//  RTC.set(DS1307_YR,14);         //set the year
//  RTC.start();

  // clear eeprom
    for (int i = 0; i < 512; i++)
      EEPROM.write(i, 0);
  
  
    GetDateTime();
    ShowTime();
    currStr = "";
//    currStr = "!G /T 24  /K 1,2 !I /G  /S 21:22 !Z /K 1,3";  // !I /T h !N /p1m";
//        currStr = "!G /T 24  /K 1,2 /P 1m /C !I /G  /S 19:42 !Z /K 1,3";  // !I /T h !N /p1m";
    
    
    sensors.begin(); 
    for (int i = 0; i<MasterRoomsCount; i++)
    {
      sensors.getAddress(tempDeviceAddress, i);
      sensors.setResolution(tempDeviceAddress, Resolution18b20);
    }    
    //delay18b20 = 750/ (1 << (12-Resolution18b20));
    sensors.setWaitForConversion(false);  // makes it async
    sensors.requestTemperatures();  // for preparing Temp for 1st read
    
    pinMode(RX485_TX_CONTROL_PIN, OUTPUT);    
    digitalWrite(RX485_TX_CONTROL_PIN, RS485Receive);  // Init Transceiver   
    RS485Serial.begin(9600);   // set the data rate 
    ET_Command.begin(details(Comm), &RS485Serial);

                                                                    //EepromRead();                                                                    
    
    wdt_enable(WDTO_8S);  //если в loop не делать wdt_reset, то каждые 8сек будет перезагрузка
    wdt_reset();
}

void cellSetup()
{
  delay(30000);
    cell.begin(9600);
   
    delay(1000);
    
    cell.print("AT+IPR?\r");  
    delay(300);

    cell.print("AT+CMGF=1\r");  //    cell.print("AT+CCMGF=1\r");  
    delay(300);  

    cell.print("AT+IFC=1, 1\r");  
    delay(300);  

    cell.print("AT+CPBS=\"SM\"\r");  
    delay(300);  

    cell.print("AT+CNMI=1,2,2,1,0\r");  
    delay(500); 


    cell.print("AT+CLIP=1\r");  //разрешает показ инфо о входящем вызове номер  
    delay(300);  

    
//    Serial.println("Starting ATWIN   Communication...");
    Serial.println(" ATWIN ");

digitalWrite(BZZ_PIN, HIGH);
delay(100);
digitalWrite(BZZ_PIN, LOW);

//      cell.print("AT+CMGS=\"+79062656420\"\r");
//      delay(1000);
//      cell.print("Test1\r");
    
//    delay(1000);
//  cell.print(26,BYTE);  
//  cell.write(26);
  
}

void EepromRead()
{
  SmsCommand NewSmsCommand;
  
  for (int i = 9; i>=0; i--)
  {
    NewSmsCommand.Oper = EEPROM.read(addrOper + CountCommandElements*i);
    NewSmsCommand.CreateDateYear =  EEPROM.read(addrOper + 1 + CountCommandElements*i);
    NewSmsCommand.CreateDateMonth =  EEPROM.read(addrOper + 2 + CountCommandElements*i);
    NewSmsCommand.CreateDateDay =  EEPROM.read(addrOper + 3 + CountCommandElements*i);
    NewSmsCommand.CreateDateHour =  EEPROM.read(addrOper + 4 + CountCommandElements*i);
    NewSmsCommand.CreateDateMinute =  EEPROM.read(addrOper + 5 + CountCommandElements*i);
    NewSmsCommand.CreateDateSec =  EEPROM.read(addrOper + 6 + CountCommandElements*i);
    NewSmsCommand.StartOnNextDay =  EEPROM.read(addrOper + 7 + CountCommandElements*i);
    NewSmsCommand.Repeat =  EEPROM.read(addrOper + 8 + CountCommandElements*i);
    NewSmsCommand.Inform =  EEPROM.read(addrOper + 9 + CountCommandElements*i);
    NewSmsCommand.StartTimeHour =  EEPROM.read(addrOper + 10 + CountCommandElements*i);
    NewSmsCommand.StartTimeMinute =  EEPROM.read(addrOper + 11 + CountCommandElements*i);
    NewSmsCommand.StartTimeSec =  EEPROM.read(addrOper + 12 + CountCommandElements*i);
    NewSmsCommand.DurationDays =  EEPROM.read(addrOper + 13 + CountCommandElements*i);
    NewSmsCommand.DurationHours =  EEPROM.read(addrOper + 14 + CountCommandElements*i);
    NewSmsCommand.DurationMinutes =  EEPROM.read(addrOper + 15 + CountCommandElements*i);
    NewSmsCommand.DurationSeconds =  EEPROM.read(addrOper + 16 + CountCommandElements*i);
    NewSmsCommand.Temperature =  EEPROM.read(addrOper + 17 + CountCommandElements*i);
    NewSmsCommand.Rooms =  EEPROM.read(addrOper + 18 + CountCommandElements*i);
    NewSmsCommand.Status =  EEPROM.read(addrOper + 19 + CountCommandElements*i);
    NewSmsCommand.IncomingPhoneID =  EEPROM.read(addrOper + 20 + CountCommandElements*i);
    
    SaveSmsCommand(NewSmsCommand, false); 
  }
}

void loop() {

//  if (flStart)
//  {
//      ParseAndSaveCommands();
//      flStart = false;
//  }

  currTime = millis();
  
  CheckIncomingGSM();
  
  RS485ExchangeData();
  
  //checkDistanceAlarm();
  checkIRAlarm();

  if (TimeSaveTemperatureHistoryHours())
  {
    SaveTemperatureHistory(0);
    if (TimeSaveTemperatureHistoryDays())
    {
      SaveTemperatureHistory(1);
    } 
  }

  if (currTime-loopTimeSetCommandStatus >= periodSetCommandsStatusSec * 1000)
  {
    loopTimeSetCommandStatus = currTime;
    
    GetDateTime();
    ShowTime();
    SetCommandsStatus();
    ExecuteCommands();
  } 
    

  //fill MasterRoomsNagrevStatus array
   if (currTime-loopTimeTermoPre >= periodTermoPreControlSec * 1000 || FlNewNagrevCommand == true)
   {
     GetDateTime();
     ShowTime();
     Serial.println("                    TermoPreControl");
     loopTimeTermoPre = currTime;
     FlNewNagrevCommand = false;          
    
     TemperaturePreControl();     
   }
  
    if (FlNeedThermoControl)
    {
      //every 1 sec
     if (currTime-loopTimeTermo >= periodTermoControlSec * 1000)
     {
       Serial.println("                        TermoControl");
         TemperatureControl();
         loopTimeTermo = currTime;
     }
    }
  
    cli(); //запрещает прерывания
    wdt_reset(); //сброс собаки, если не сбросить - вызовется перезагрузка процессора
    sei(); //разрешает прерывания
}

void CheckIncomingGSM()
{ 
  //Serial.println("CheckIncomingGSM");
  //If a character comes in from the cellular module...
  if(cell.available() >0)    
  {
//    Serial.println("cell.available");
    incoming_char=cell.read();    //Get the character from the cellular serial port.
    Serial.print(incoming_char);  //Print the incoming character to the terminal.
    
    if ('\r' == incoming_char) {  
        Serial.println("");
        if (isStringMessage) 
        {
            isStringMessage = false;    
            Serial.println("currStr: " + currStr);
            //если текущая строка - SMS-сообщение,  
            //отреагируем на него соответствующим образом
            if (IncomingPhoneID > 0)
            {
              if (currStr.indexOf("SMS")>=0) 
              {  
                  Serial.println("Return sms command to " + smsFromPnone);
                  incomingPhone = smsFromPnone;
                  SendSMS("okk",0);                
              }
              else
              {
                Serial.println("ParseAndSaveCommands");
                ParseAndSaveCommands();
              }
            }  
        } else 
          {  
            if (currStr.startsWith("+CMT")) 
            {  
                //если текущая строка начинается с "+CMT",  
                //то следующая строка является sms сообщением, а из текущей строки выберем тел номер и дату-время
                isStringMessage = true;
                smsFromPnone = GetPhoneNumber(currStr);
                Serial.println("IncomingsmsFromPnone: " + smsFromPnone);
                incomingPhone = smsFromPnone;                
                if (checkPhone(incomingPhone))
                {
                  Serial.println("smsFromPnone: " + incomingPhone);                    
                }
                else  //from unknown phone, ignore
                {
                  Serial.println("smsFromUnknownPnone");                  
                }
            }
            else if (currStr.startsWith("+CLIP")) 
            {  
                //если текущая строка начинается с "+CLIP",  
                //то идет входящий вызов, из текущей строки выберем тел номер
                incomingPhone = GetPhoneNumber(currStr);
                Serial.println("incomingPhone: " + incomingPhone);  
                if (checkPhone(incomingPhone))
                {
                    delay(1000);
                    cell.print("ATH\r");  //занято
                    //                        cell.print("ATA\r");  //снять трубку 
                    delay(2000);
                    Serial.println("bussssy");
                }
            }
        }  
        currStr = "";  
    } else if ('\n' != incoming_char) {  
        currStr += String(incoming_char);  
    }    
  }
}

//                    Serial.println("call bak");
//                    digitalWrite(BZZ_PIN, HIGH);
//                    delay(500);
//                    digitalWrite(BZZ_PIN, LOW);
                    // cell.print("ATD" + incomingPhone + ";\r");  //перезвонить 
                    //      SendSms(true);
//                    ShowTime();
                    //                                       cell.print("AT+CEER\r");  //показать последние ошибки

//uuu
//cell.print("AT+CSQ\r");  // +CSQ: 8,99    1st shows signal quality 0-31
//delay(1000);
////cell.print("AT+CCID\r");  // +ccid:       display SIM card: ESN разнный почему-то
////cell.print("AT+CMGR=1\r");
//cell.print("AT+CMGL=\"SENT\"\r");
//delay(1000);
                  

void GetDateTime()
{
  CurrHour = RTC.get(DS1307_HR,true); //read the hour and also update all the values by pushing in true
  CurrMinute = RTC.get(DS1307_MIN,false);//read minutes without update (false)
  CurrSec = RTC.get(DS1307_SEC,false);//read seconds
  CurrDay = RTC.get(DS1307_DATE,false);//read date
  CurrMonth = RTC.get(DS1307_MTH,false);//read month
  CurrYear = RTC.get(DS1307_YR,false) - 2000; //read year
}

void ShowTime()
{
  Serial.print(CurrHour); //read the hour and also update all the values by pushing in true
  Serial.print(":");
  Serial.print(CurrMinute);//read minutes without update (false)
  Serial.print(":");
  Serial.print(CurrSec);//read seconds
  Serial.print("      ");                 // some space for a more happy life
  Serial.print(CurrDay);//read date
  Serial.print("/");
  Serial.print(CurrMonth);//read month
  Serial.print("/");
  Serial.print(CurrYear); //read year
  Serial.println();
}

boolean CheckCommandStartTime(byte CommandCreateDateDay, byte StartOnNextDay, byte CommadStartTimeHour, byte CommadStartTimeMinute, byte CommandStartTimeSec)
{
//    delay(500);
//  digitalWrite(13, HIGH);   // turn the LED on (HIGH is the voltage level)
//  delay(500);               // wait for a second
//  digitalWrite(13, LOW);    // turn the LED off by making the voltage LOW
//    delay(500);
    
  // Serial.println("CheckCommandStartTime");
  boolean bResult = false;
  if (CurrHour > CommadStartTimeHour || 
      CurrHour == CommadStartTimeHour && CurrMinute > CommadStartTimeMinute || 
      CurrHour == CommadStartTimeHour && CurrMinute == CommadStartTimeMinute && CurrSec >= CommandStartTimeSec)
  {
    bResult = (StartOnNextDay == true && CurrDay != CommandCreateDateDay) || StartOnNextDay == false;
  }
  else
  {
    bResult = (StartOnNextDay == false && CurrDay != CommandCreateDateDay);
  }
  
  return bResult;
}

boolean CheckCommandFinishTime(byte CommandCreateDateYear, byte CommandCreateDateMonth, byte CommandCreateDateDay, 
                              byte StartOnNextDay, byte CommadStartTimeHour, byte CommadStartTimeMinute, byte CommandStartTimeSec,
                              byte DurationDays, byte DurationHours, byte DurationMinutes, byte DurationSeconds)
{
  boolean bResult = false;
  int Year2 = CommandCreateDateYear;
  int Month2 = CommandCreateDateMonth;
  int Day2 = CommandCreateDateDay;
  int Hour2 = CommadStartTimeHour;
  int Minute2 = CommadStartTimeMinute;
  int Sec2 = CommandStartTimeSec;
  
 // Serial.println("CheckCommandFinishTime");
      
  if (DurationSeconds > 0)
  {
      Sec2 = Sec2 + DurationSeconds%60;
      Minute2 = Minute2 + DurationSeconds/60;      
  }
  
  if (DurationMinutes > 0)
  {
      Minute2 = Minute2 + DurationMinutes%60;
      Hour2 = Hour2 + DurationMinutes/60;      
  }
  
  if (DurationHours > 0)
  {
      Hour2 = Hour2 + DurationHours%24;
      Day2 = Day2 + DurationHours/24;
  }
  
  if (Hour2 > 24)
  {
    Hour2 = Hour2 - 24;
    Day2 = Day2 + 1;    
  }
  
  if (DurationDays > 0)
  {
      Day2 = Day2 + DurationDays;
  }
  if (StartOnNextDay)
  {
      Day2 = Day2 + 1;
  }

  int DaysInMonth;
  while (1==1)
  {
    DaysInMonth = GetDaysInMonth(Year2, Month2);
    if (Day2 > DaysInMonth)
    {
      Day2 = Day2 - DaysInMonth;
      Month2 = Month2 + 1;
      if (Month2 > 12)
      {
        Month2 = 1;
        Year2 = Year2 + 1;
      }
    }
    else
    {
      break;
    }
  }
  
  bResult = (CurrYear > Year2 || 
      CurrMonth > Month2 && CurrYear == Year2 || 
      CurrDay > Day2 && CurrMonth == Month2 && CurrYear == Year2 ||
      CurrHour > Hour2 && CurrDay == Day2 && CurrMonth == Month2 && CurrYear == Year2 ||
      CurrMinute > Minute2 && CurrHour == Hour2 && CurrDay == Day2 && CurrMonth == Month2 && CurrYear == Year2 ||
      CurrSec >= Sec2 && CurrMinute == Minute2 && CurrHour == Hour2 && CurrDay == Day2 && CurrMonth == Month2 && CurrYear == Year2);
  
  return bResult;
}

int GetDaysInMonth(int Year, int Month)
{
  int ResDays;
  
  if (Month==1 || Month==3 || Month==5 || Month==7 || Month==8 || Month==10 || Month==12)
  {
    ResDays = 31;
  }
  else if (Month=2 && (Year % 4 == 0))
  {
     ResDays = 27;
  }
  else if (Month==2 && (Year % 4 != 0))
  {
     ResDays = 28;
  }
  else
  {
     ResDays = 30;
  }
  
  return ResDays;
}

void SetCommandsStatus()
{
//  Serial.println("CreateDateDay " + String(SmsCommands[1].CreateDateDay));
//  Serial.println("StartOnNextDay " + String(SmsCommands[1].StartOnNextDay));
//  Serial.println("StartTimeHour " + String(SmsCommands[1].StartTimeHour));
//  Serial.println("StartTimeMinute " + String(SmsCommands[1].StartTimeMinute));
  
   Serial.println("SetCommandsStatus");
   
   for (int i = 9; i>=0; i--)
  { 
//    Serial.println("");
//    Serial.println(String(i));
//    Serial.println(String(SmsCommands[i].Status));
//    Serial.println(String(SmsCommands[i].StartTimeHour));
//    Serial.println(String(SmsCommands[i].StartTimeMinute));
    
    if (SmsCommands[i].Status == 0 && SmsCommands[i].Oper != 0) //in queue
    {
      if (CheckCommandStartTime(SmsCommands[i].CreateDateDay, SmsCommands[i].StartOnNextDay, SmsCommands[i].StartTimeHour, SmsCommands[i].StartTimeMinute, SmsCommands[i].StartTimeSec))
      {
        SmsCommands[i].Status = 1; //ready for execute
        if (SmsCommands[i].Oper == 'G'|| SmsCommands[i].Oper == 'Z')
        {
          FlNewNagrevCommand = true;
        }        
                Serial.println("START " + String(i));
      }
    }
    else if (SmsCommands[i].Status == 1 || SmsCommands[i].Status == 2) //ready for execute, or in progress
    {    
      if (CheckCommandFinishTime(SmsCommands[i].CreateDateYear, SmsCommands[i].CreateDateMonth, SmsCommands[i].CreateDateDay, 
          SmsCommands[i].StartOnNextDay, SmsCommands[i].StartTimeHour, SmsCommands[i].StartTimeMinute, SmsCommands[i].StartTimeSec,
          SmsCommands[i].DurationDays, SmsCommands[i].DurationHours, SmsCommands[i].DurationMinutes, SmsCommands[i].DurationSeconds))
      {
        SmsCommands[i].Status = 3; // done
        if (SmsCommands[i].Oper == 'G' || SmsCommands[i].Oper == 'Z')
        {
          RoomsTemperatureReload();
          FlNewNagrevCommand = true;
        } 
        Serial.println("DONE " + String(i) + " " + SmsCommands[i].Oper);
      }
    }
  }  
}

//parse sms and save new commands to array and eeprom
void ParseAndSaveCommands()
{
  String str;
  GetDateTime();
  
  int pos_new_cmd = 0;
  int pos_next_cmd = 0;
Serial.println(currStr);
  currStr.toUpperCase();
  currStr.trim();
  str = currStr;
  
  while (str.indexOf("!")>-1)
   {    
      if (pos_new_cmd==0) {pos_new_cmd = str.indexOf("!");}
      str = str.substring(pos_new_cmd+1);
      pos_next_cmd = str.indexOf("!");
      
      if (pos_next_cmd==-1) {pos_next_cmd = str.length()+1;}
      
      GetCmd(str.substring(0, pos_next_cmd-1));
      pos_new_cmd = pos_next_cmd;
    }
    
    if (FlSendCmndReceivedInfo)
    {
      SendSmsInfo(5);
    }
    currStr = "";
}

void GetCmd(String cmd)
{
  Serial.println("");
  Serial.println("GetCmd");
    Serial.println(String(cmd));
  delay(100);
  
  char cmdType;
  byte oper_char;
  byte start_hour = -1;
  byte start_minute = -1;
  byte duration_minute = -1;
  
  cmd.trim();
  
  cmdType = cmd[0];

  SmsCommand NewSmsCommand;
 
   //clear
  NewSmsCommand.Status = 0;
  NewSmsCommand.DurationDays = 0;
  NewSmsCommand.DurationHours = 0;
  NewSmsCommand.DurationMinutes = 0;
  NewSmsCommand.DurationSeconds = 0;
  NewSmsCommand.Rooms = 0;
  NewSmsCommand.Temperature = 0;
  NewSmsCommand.Repeat = 0;
  NewSmsCommand.Inform = 0;
  NewSmsCommand.IncomingPhoneID = 0;
 
  NewSmsCommand.Oper = cmdType;
  if (NewSmsCommand.Oper == 'G')
  {
    NewSmsCommand.DurationMinutes = MAX_NAGREV_PERIOD_MINUTES;
  }
  if (NewSmsCommand.Oper == 'Z')
  {
    NewSmsCommand.Temperature = ZERO_TEMPERATURE;
    NewSmsCommand.DurationDays = MAX_ZERO_PERIOD_DAYS;
  } 
  NewSmsCommand.IncomingPhoneID = IncomingPhoneID;
  
  NewSmsCommand.CreateDateYear = CurrYear;
  NewSmsCommand.CreateDateMonth = CurrMonth;
  NewSmsCommand.CreateDateDay = CurrDay;
  NewSmsCommand.CreateDateHour = CurrHour;
  NewSmsCommand.CreateDateMinute = CurrMinute;
  NewSmsCommand.CreateDateSec = CurrSec;
  NewSmsCommand.StartOnNextDay = false;
  
  NewSmsCommand.StartTimeHour = CurrHour;
  NewSmsCommand.StartTimeMinute = CurrMinute;
  NewSmsCommand.StartTimeSec = CurrSec;

  
  int pos_new_attr = 0;
  int pos_next_attr = 0;
  String str = cmd;
  String attr = "";
  while (str.indexOf("/")>-1)
   {     
     Serial.println(" ");
      if (pos_new_attr==0) {pos_new_attr = str.indexOf("/");}
      str = str.substring(pos_new_attr+1);
        Serial.println("str: " + str);
      pos_next_attr = str.indexOf("/");
      if (pos_next_attr==-1) {pos_next_attr=1000;}
      attr = str.substring(0, 1);        
 // Serial.println("ATTRIBUTE: " + attr);
  
        //получение значения аттрибута
        String attrVal = "";
        attrVal = str.substring(1, pos_next_attr-1); 
//         Serial.println("ATTRIBUTE_VAL: " + attrVal);     
         
        if (attr=="S")       //время старта
        {                    
          NewSmsCommand.StartTimeHour = GetStartTime(attrVal, 'h');
          NewSmsCommand.StartTimeMinute = GetStartTime(attrVal, 'm');
          NewSmsCommand.StartTimeSec = 0;
          
          //уже прошло время старта + 30 минут, запустим завтра
          if ((CurrHour * 60 + CurrMinute) - (NewSmsCommand.StartTimeHour * 60 + NewSmsCommand.StartTimeMinute) > PossibleMinutesLateStart)
          {
            NewSmsCommand.StartOnNextDay = true;
          }          
        }
        
        else if (attr=="T" && cmdType == 'G')  //температура нагрева
        {
          NewSmsCommand.Temperature = attrVal.toInt();
        }
        
        else if (attr=="K")  //помещение
        {
           int RoomInt = 0;
           float RoomByte = 0;
           float RoomsByte = 0;
           int IntRoomsByte = 0;
           String strRoom = "";
           String strR = attrVal;
           int pos_new_zpt = 0;
           while (pos_new_zpt > -1)
           {
             pos_new_zpt = strR.indexOf(",");
             if (pos_new_zpt > 0)
             {
               strRoom = strR.substring(0, pos_new_zpt);               
               strR = strR.substring(pos_new_zpt+1);
             }
             else // последний номер
             {
               strRoom = strR;
             }
             strRoom.trim();
             RoomInt = strRoom.toInt();
             RoomByte = pow(2, RoomInt-1);
             //Serial.println(RoomByte);
             RoomsByte = RoomsByte + RoomByte;    
           }
           IntRoomsByte = round(RoomsByte);
           NewSmsCommand.Rooms = IntRoomsByte;     
//          Serial.println(NewSmsCommand.Rooms);
        }
        
        else if (attr=="P")  //продолжительность выполнения команды
        {
          //reset default settings   
          NewSmsCommand.DurationSeconds = 0;
          NewSmsCommand.DurationMinutes = 0;
          NewSmsCommand.DurationHours = 0;
          NewSmsCommand.DurationDays = 0;
          
          //set new duration from command
          int i;
          i = attrVal.indexOf("S");
          if (i > -1)
          {
            NewSmsCommand.DurationSeconds = attrVal.substring(0, i).toInt();
          }
          else
          {
            i = attrVal.indexOf("M");
            if (i > -1)
            {
              NewSmsCommand.DurationMinutes = attrVal.substring(0, i).toInt();
            }
            else
            {
              i = attrVal.indexOf("H");
              if (i > -1)
              {
                NewSmsCommand.DurationHours = attrVal.substring(0, i).toInt();
              }
              else
              {
                i = attrVal.indexOf("D");
                if (i > -1)
                {
                  NewSmsCommand.DurationDays = attrVal.substring(0, i).toInt();
                }
              }  
            }  
          }
        } else if (attr=="C")  //подтверждение получения=1 /старта=2 /выполнения=3   команды
        {
          NewSmsCommand.Inform = attrVal.toInt();
          FlSendCmndReceivedInfo = (String(NewSmsCommand.Inform).indexOf("1") >= 0);          
        }
        
        if (cmdType == 'I')
        {
          if (attr=="G")
          {
            NewSmsCommand.Oper = 'I';  //general information
          }
          if (attr=="R")
          {
            NewSmsCommand.Oper = 'R';  //about rooms temperatures
          }
          if (attr=="A")
          {
            NewSmsCommand.Oper = 'A';  //alarm info
          }
        }
      
      pos_new_attr = pos_next_attr;
    }
     
  
  Serial.println("Oper : " + String(NewSmsCommand.Oper));
 // Serial.println("CreateDateYear : " + String(NewSmsCommand.CreateDateYear));
 // Serial.println("CreateDateMonth : " + String(NewSmsCommand.CreateDateMonth));
//  Serial.println("CreateDateDay : " + String(NewSmsCommand.CreateDateDay));
  Serial.println("CreateDateHour : " + String(NewSmsCommand.CreateDateHour));
  Serial.println("CreateDateMinute : " + String(NewSmsCommand.CreateDateMinute));
  Serial.println("CreateDateSec : " + String(NewSmsCommand.CreateDateSec));
//  Serial.println("StartTimeHour : " + String(NewSmsCommand.StartTimeHour));
//  Serial.println("StartTimeMinute : " + String(NewSmsCommand.StartTimeMinute));
  Serial.println("StartTimeSec : " + String(NewSmsCommand.StartTimeSec));
//  Serial.println("DurationDays : " + String(NewSmsCommand.DurationDays));
//  Serial.println("DurationMinutes : " + String(NewSmsCommand.DurationMinutes));
//  Serial.println("DurationSeconds : " + String(NewSmsCommand.DurationSeconds));
//  Serial.println("Rooms : " + String(NewSmsCommand.Rooms));
//  Serial.println("Temperature : " + String(NewSmsCommand.Temperature));  

  SaveSmsCommand(NewSmsCommand, true);     

//  byte operAttr = GetAttributes(cmd);
  
//  CountActiveOpers = CountActiveOpers + 1;
 
//  GetTimeOff(CurrYear, CurrMonth, CurrDay, start_hour, start_minute, 0, 5, 'D');
  
//  EEPROM.write(addrCountActiveOpers, CountActiveOpers);
//  EEPROM.write(addrOper_Code * CountActiveOpers, oper_char);
// // EEPROM.write(addrOper_Attr * CountActiveOpers, byte(attrString.substring(0,1)));  
//  EEPROM.write(addrTime_YearStart * CountActiveOpers, CurrYear);
//  EEPROM.write(addrTime_MonthStart * CountActiveOpers, CurrMonth);
//  EEPROM.write(addrTime_DayStart * CountActiveOpers, CurrDay);
//  EEPROM.write(addrTime_HourStart * CountActiveOpers, start_hour);
//  EEPROM.write(addrTime_MinuteStart * CountActiveOpers, start_minute);
//  EEPROM.write(addrTime_SecStart * CountActiveOpers, 0);  
//  
//  EEPROM.write(addrTime_YearEnd * CountActiveOpers, YearsEnd[CountActiveOpers]);
//  EEPROM.write(addrTime_MonthEnd * CountActiveOpers, MonthsEnd[CountActiveOpers]);
//  EEPROM.write(addrTime_DayEnd * CountActiveOpers, DaysEnd[CountActiveOpers]);
//  EEPROM.write(addrTime_HourEnd * CountActiveOpers, HoursEnd[CountActiveOpers]);
//  EEPROM.write(addrTime_MinuteEnd * CountActiveOpers, MinutesEnd[CountActiveOpers]);
//  EEPROM.write(addrTime_SecEnd * CountActiveOpers, SecsEnd[CountActiveOpers]);

}

byte GetAttributes(String cmd)
{
  int pos_delimiter=0;
  String attribute=0;
  String str_result;
  byte byte_result;
  int i = 0;

  pos_delimiter = cmd.indexOf("/");  
  while (pos_delimiter>=0)
  {
    attribute = cmd.substring(pos_delimiter+1, pos_delimiter+2);
    str_result = str_result + String(attribute);
    cmd = cmd.substring(pos_delimiter+1);
    pos_delimiter = cmd.indexOf("/");
  }
  Serial.println("ATTRIBUTES: " + str_result);
 
  return byte_result;
}

int GetStartTime(String cmd, char element)
{
  int pos_delimiter;
  String str_result;
  int result;
  pos_delimiter = cmd.indexOf(":");
  if (pos_delimiter>0)
  {
      if (element == 'h')
      {
        str_result = cmd.substring(pos_delimiter-2, pos_delimiter);
        str_result.trim();
        Serial.println(str_result);
        result = str_result.toInt();
        if (str_result != "0" && str_result != "00" && result == 0)
        {
          result = -100;
        }
        else if (result<0 || result>24)
        {
          result = -100;        
        }
      }
      else  //minutes
      {
        str_result = cmd.substring(pos_delimiter+1, pos_delimiter+3);
        str_result.trim();
                Serial.println(str_result);
        result = str_result.toInt();
        if (str_result != "0" && str_result != "00" && result == 0)
        {
          result = -100;
        }
        else if (result<0 || result>59)
        {
          result = -100;        
        }        
      }
  }
//  else  //время не указано, стартуем немедленно 
//  {
//      if (element == 'h')
//      {
//        result = CurrHour;
//      }
//      else
//      {
//        result = CurrMinute;
//      }
//  }
  return result;
}

void SaveSmsCommand(SmsCommand Command, boolean isSaveEeprom)
{ 
  for (int i = 9; i>0; i--)
  {
    SmsCommands[i] = SmsCommands[i-1];
  }
  SmsCommands[0] = Command;
  
  if (isSaveEeprom)
  {
    EepromWriteSMSCommands();
  }
  Serial.println("SmsCommands_0 " + String(SmsCommands[0].Oper));
  Serial.println("SmsCommands_1 " + String(SmsCommands[1].Oper));
}

void EepromWriteSMSCommands()
{
  SmsCommand Command;
  for (int i = 9; i>=0; i--)
  {
    Command = SmsCommands[i];
    EEPROM.write(addrOper + CountCommandElements*i, Command.Oper);
    EEPROM.write(addrOper + 1 + CountCommandElements*i, Command.CreateDateYear);
    EEPROM.write(addrOper + 2 + CountCommandElements*i, Command.CreateDateMonth);
    EEPROM.write(addrOper + 3 + CountCommandElements*i, Command.CreateDateDay);
    EEPROM.write(addrOper + 4 + CountCommandElements*i, Command.CreateDateHour);
    EEPROM.write(addrOper + 5 + CountCommandElements*i, Command.CreateDateMinute);
    EEPROM.write(addrOper + 6 + CountCommandElements*i, Command.CreateDateSec);
    EEPROM.write(addrOper + 7 + CountCommandElements*i, Command.StartOnNextDay);
    EEPROM.write(addrOper + 8 + CountCommandElements*i, Command.Repeat);
    EEPROM.write(addrOper + 9 + CountCommandElements*i, Command.Inform);
    EEPROM.write(addrOper + 10 + CountCommandElements*i, Command.StartTimeHour);
    EEPROM.write(addrOper + 11 + CountCommandElements*i, Command.StartTimeMinute);
    EEPROM.write(addrOper + 12 + CountCommandElements*i, Command.StartTimeSec);
    EEPROM.write(addrOper + 13 + CountCommandElements*i, Command.DurationDays);
    EEPROM.write(addrOper + 14 + CountCommandElements*i, Command.DurationHours);
    EEPROM.write(addrOper + 15 + CountCommandElements*i, Command.DurationMinutes);
    EEPROM.write(addrOper + 16 + CountCommandElements*i, Command.DurationSeconds);
    EEPROM.write(addrOper + 17 + CountCommandElements*i, Command.Temperature);
    EEPROM.write(addrOper + 18 + CountCommandElements*i, Command.Rooms);
    EEPROM.write(addrOper + 19 + CountCommandElements*i, Command.Status);
    EEPROM.write(addrOper + 20 + CountCommandElements*i, Command.IncomingPhoneID);
  }
}

void ExecuteCommands()
{
  for (int i = 9; i>=0; i--)
  {    
    if (SmsCommands[i].Status == 1) //ready for execute
    {
      SmsCommands[i].Status = 2;
      Operation(SmsCommands[i]);
    }
  }
}

//вызывается после окончания очередной температурной команды
void RoomsTemperatureReload()
{
  Serial.println("  RoomsTemperatureReload ");
  //reset temperatures set in all rooms
  float CalcRoomsByteF = pow(2, RoomsCount);
  int CalcRoomsByte = round(CalcRoomsByteF);   
  SetRoomsTemperature(CalcRoomsByte-1, EMPTY_TEMP);
  
  for (int i = 9; i>=0; i--)  //from old to new commands
  {
//Serial.println("checkCommand " + String(i) + " oper " + SmsCommands[i].Oper + " status " + SmsCommands[i].Status);
    if (SmsCommands[i].Status == 2 && (SmsCommands[i].Oper == 'G' || SmsCommands[i].Oper == 'Z')) //not done gret command
    {
      SetRoomsTemperature(SmsCommands[i].Rooms, SmsCommands[i].Temperature);
    }
  }
}

void Operation(SmsCommand Command)
{
  Serial.println("Operation: " + String(Command.Oper));
  if (Command.Oper == 'G' || Command.Oper == 'Z')  //нагрев помещений
  {
    SetRoomsTemperature(Command.Rooms, Command.Temperature);
  }
  if (Command.Oper == 'L')  //свет в помещениях
  {
    SetRoomsLight(Command.Rooms, Command.Oper);    // !!!!!!!!!!!!!!!!!
    Command.Status = 3; //done
  }
  
  if (Command.Oper == 'I')  //send sms with different information
  {
    SendSmsInfo(1);
    Command.Status = 3; //done
  }
  if (Command.Oper == 'R')  //send sms with rooms temperatures
  {
    SendSmsInfo(2);
    Command.Status = 3; //done
  }
  if (Command.Oper == 'A')  //send sms with alarm info
  {
    SendSmsInfo(3);
    Command.Status = 3; //done
  }
}

void SetRoomsTemperature(byte RoomsByte, byte Temperature)
{
    NumberRoomsToSend = 0;
     
//  Serial.println("   SetRoomsTemperature=" + String(Temperature));
//   Serial.println("RoomsByte: ");//  Serial.println(RoomsByte);
//   Serial.print("  SetTemperature: ");
//   Serial.print(Temperature);
  for (int i = RoomsCount-1; i>=0; i--)
  {
   // Serial.println(SetRoomsTemperatures[i]);

    float CalcRoomsByteF = pow(2, i);
    int CalcRoomsByte = round(CalcRoomsByteF);
    // Serial.println("");
   //  Serial.println(RoomsByte);
   //  Serial.println(CalcRoomsByte);
    if (RoomsByte / CalcRoomsByte >= 1)
    {
   //   Serial.println("i "+ String(i));
      RoomsByte = RoomsByte % CalcRoomsByte;
//      if ((SetRoomsTemperatures[i] < Temperature || Temperature == EMPTY_TEMP || SetRoomsTemperatures[i] == EMPTY_TEMP) && 
//          (Temperature != EMPTY_TEMP || SetRoomsTemperatures[i] != EMPTY_TEMP)) //оставляем максимальную Т, если было несколько комманд для одной комнаты
//      {
      //оставляем последнюю Т, если было несколько команд для одной комнаты
      SetRoomsTemperatures[i] = Temperature;
      Serial.print("NEW RoomsTemperature: ");
      Serial.print(String(i));
      Serial.print(" : ");
      Serial.println(SetRoomsTemperatures[i]);
        
      if (i > MasterRoomsCount)        
      {          
        NumberRoomsToSend++;         
        RoomsToSend[NumberRoomsToSend-1] = i;              
      }        

//      else
//      {
//        Serial.print("OLD RoomsTemperature: ");
//        Serial.print(String(i));
//        Serial.print(" : ");
//        Serial.println(SetRoomsTemperatures[i]);
//      }
    }
    else
    {
      Serial.print("STAYRoomsTemperature: ");
      Serial.print(String(i));
      Serial.print(" : ");
      Serial.println(SetRoomsTemperatures[i]);
    }
  }
  SendRoomsCommand('T', Temperature);
}

void SetRoomsLight(byte RoomsByte, byte LightStatus)
{
//  Serial.println("   SetRoomsLight=" + String(LightStatus));
  NumberRoomsToSend = 0;
  for (int i = RoomsCount-1; i>=0; i--)
  {
    float CalcRoomsByteF = pow(2, i);
    int CalcRoomsByte = round(CalcRoomsByteF);
    if (RoomsByte / CalcRoomsByte >= 1)
    {
   //   Serial.println("i "+ String(i));
      RoomsByte = RoomsByte % CalcRoomsByte;
      
      //оставляем последний LightStatus, если было несколько комманд Light для одной комнаты
      RoomsLights[i] = LightStatus;
      Serial.print("NEW RoomsLights: ");
      Serial.print(String(i));
      Serial.print(" : ");
      Serial.println(RoomsLights[i]);
      if (i<=MasterRoomsCount-1)
        digitalWrite(MasterRoomsLightPins[i], RoomsLights[i]);
      else
      { 
        NumberRoomsToSend++;         
        RoomsToSend[NumberRoomsToSend-1] = i;                
      }        
    }
  }
  SendRoomsCommand('L', LightStatus);
}

//fill MasterRoomsNagrevStatus array
void TemperaturePreControl()
{
  Serial.println("");
  sensors.requestTemperatures();
//  loopRequestTemp = millis();
 // Serial.println("TemperaturePreControl");
  FlNeedThermoControl = false;
  for (int i = 0; i<MasterRoomsCount; i++)
  {
      Serial.print(String(i) + " T =  ");
      Serial.println(sensors.getTempCByIndex(i)); 
    if (MasterRoomsNagrevStatus[i]==0 && SetRoomsTemperatures[i] != EMPTY_TEMP)
    {
      if (sensors.getTempCByIndex(i) < (SetRoomsTemperatures[i] - DELTA_START_TEMP))
      {
        MasterRoomsNagrevStatus[i] = 3; //включить
        FlNeedThermoControl = true;
      }
    }
    else if (MasterRoomsNagrevStatus[i]==1 && SetRoomsTemperatures[i] != EMPTY_TEMP)
    {
      if (sensors.getTempCByIndex(i) > (SetRoomsTemperatures[i] + DELTA_STOP_TEMP))
       {
        MasterRoomsNagrevStatus[i] = 2; //выключить
        FlNeedThermoControl = true;
      }
    }
    else if (SetRoomsTemperatures[i] == EMPTY_TEMP)
    {
        MasterRoomsNagrevStatus[i] = 2; //выключить
        FlNeedThermoControl = true;
    }
   // Serial.println("RoomsNagrevPreStatus: " + String(i) + " = " + String(MasterRoomsNagrevStatus[i]));
  }
}

//switch on/off Thermo using MasterRoomsNagrevStatus
void TemperatureControl()
{
  //Serial.println("TemperatureControl");
  boolean bIsChanged = false;
  for (int i = 0; i<MasterRoomsCount; i++)
  {
   // Serial.println("MasterRoomsNagrevStatusBefore: " + String(i) + " - " + String(MasterRoomsNagrevStatus[i]));
    
    if (MasterRoomsNagrevStatus[i] == 3)
    {
      digitalWrite(MasterRoomsNagrevPins[i], HIGH);
      MasterRoomsNagrevStatus[i] = 1;
      bIsChanged = true;
    }
    if (MasterRoomsNagrevStatus[i] == 2)
    {
      digitalWrite(MasterRoomsNagrevPins[i], LOW);
      MasterRoomsNagrevStatus[i] = 0;
      bIsChanged = true;
    }
    if (i == MasterRoomsCount-1) //last termo, all rooms has been handled
    {
      FlNeedThermoControl = false;
    }
    if (bIsChanged)
    {
      Serial.print(String(i) + " termo ");
      Serial.print(sensors.getTempCByIndex(i)); 
      Serial.println(" statusAfter " + String(MasterRoomsNagrevStatus[i]));
      break; //only one Thermo is controlled each time
    }
  }
}

String FloatToString(float val)
{
  char temp[10];
  String tempAsString;
  dtostrf(val, 1, 2, temp);
  tempAsString = String(temp);
  return tempAsString;
}

void SendSmsInfo(int typeInfo)  // 1 - general info, 2 - rooms temperatures, 3 - alarm status, 4 - commands log, 5 - command received
{
  String strSMS;
  Serial.println("Sending SMS info " + String(typeInfo));
  if (typeInfo == 1)
  {
    readSensors();
    strSMS = "Temperature: " + FloatToString(Temperature) + "  Humidity: " + FloatToString(Humidity) + "  Svet: " + FloatToString(Svet) + "  Distance: " + FloatToString(DistanceSm);
    SendSMS(strSMS, 99);
  }
  
  if (typeInfo == 2)
  {
    readSensors();
    sensors.requestTemperatures();
    strSMS = "Temperature: " + FloatToString(Temperature);
    
    for (int i = 0; i<MasterRoomsCount; i++)
    {
      strSMS = strSMS + " R_" + String(i+1) + ": " + FloatToString(SetRoomsTemperatures[i]) + " / "  + FloatToString(sensors.getTempCByIndex(i));
      if (MasterRoomsNagrevStatus[i] == 1)
      {
        strSMS = strSMS + " ^";
      }
    }
    SendSMS(strSMS, 99);
  }
  
  if (typeInfo == 5)
  {
    strSMS = "Command received: " + currStr;
    SendSMS(strSMS, 0);
  }
}

String GetPhoneNumber(String str)
{
  String phoneNumber = "";
//  "+CMT: "+79062656420",,"13/04/17,22:04:05+22"    //SMS
//  "+CLIP: "79062656420",145,,,"",0"                //INCOMING CALL
  phoneNumber = str.substring(str.indexOf("\"")+1, 8 + str.substring(8).indexOf("\""));
  if (!phoneNumber.startsWith("+"))
  {
    phoneNumber = "+" + phoneNumber;
  }
  
  return phoneNumber;
}

boolean checkPhone(String phoneNumber)
{
  boolean bResult = false;
  IncomingPhoneID = 0;
  for (int i = 0; i<=5; i++)
  {
    bResult = phoneNumber.endsWith(phones[i]);
    if (bResult)
    {
      IncomingPhoneID = i+1;
      break;
    }
  }
  return bResult;
}

void readSensors()
{
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  Humidity = dht.readHumidity();
  Temperature = dht.readTemperature();
  delay(2000);

  
  // read the analog in value:
  sensorValue = analogRead(SoilAnalogPIN);
  Soil = (Upit - sensorValue * (Upit / 1023.0)) * 50;
//  Serial.println("Humidity earth: ");
//  Serial.println(Soil); 


  sensorValue = analogRead(SVET_ANALOG_PIN);
  Svet = sensorValue * (Upit / 1023.0) * 20;
//  Serial.println("Svet: ");
//  Serial.println(Svet); 
  
   GetDistance();
//  Serial.println("Distance: ");
//  Serial.println(DistanceSm);  
}

void GetDistance()
{
  digitalWrite(DIST_TRIG_PIN, HIGH); // Подаем сигнал на выход микроконтроллера 
  delayMicroseconds(10); // Удерживаем 10 микросекунд 
  digitalWrite(DIST_TRIG_PIN, LOW); // Затем убираем 
  int time_us=pulseIn(DIST_ECHO_PIN, HIGH); // Замеряем длину импульса 
  DistanceSm=time_us/58; // Пересчитываем в сантиметры 
}

void checkDistanceAlarm()
{
  GetDistance();
  if (DistanceSm < (ControlDistanceSm - 10) || DistanceSm > (ControlDistanceSm + 10))
  {
      GetDateTime();
      ShowTime();
      Serial.println("Distance: ");
      Serial.println(DistanceSm); 
      delay(5000);
  }
}

void checkIRAlarm()
{
  if (digitalRead(IR_ALARM_PIN)==1)
  {
//    Serial.println("ALARM");  
    digitalWrite(LED_PIN, HIGH);
    
    if (!AlarmMode)
    {  
      AlarmMode = true;
     
       Serial.println("");
       GetDateTime();
       ShowTime();
       readSensors();
      Serial.print("Temperature: ");
      Serial.println(Temperature); 
      
      Serial.print("Humidity: ");
      Serial.println(Humidity);   
    
      Serial.print("Svet: ");
      Serial.println(Svet); 
      
      Serial.println("Distance: ");
      Serial.println(DistanceSm);  
      
      // cell.print("ATD" + AlarmPnone + ";\r");  //перезвонить
     // SendSmsInfo(1);
    //  cell.print("AT+CEER\r");  //показать последние ошибки
    }
  }
  else
  {
//    Serial.println("NO ALARM");
    digitalWrite(LED_PIN, LOW);
    AlarmMode = false;
  }
}

void SaveTemperatureHistory(byte periodType)  // periodType 0 - Hours, 1 - Days
{
  int addrStart;
  float prevTemperature;
  if (periodType == 0)  //Hours
  {
    addrStart = addrTemperatureHours;
  }
  else  //Days
  {
    addrStart = addrTemperatureDays;
  }
  for (int i = 9; i>0; i--)
  {
    prevTemperature = EEPROM.read(addrStart + i);
    EEPROM.write(addrStart + i, prevTemperature);
  }
  EEPROM.write(addrStart, Temperature);
}

boolean TimeSaveTemperatureHistoryHours()
{
  boolean bResult = false;
  if (currTime-loopTimeSaveTemperatureHistoryHours >= 3600 * 1000)
  {
    Serial.println("TimeSaveTemperatureHistoryHours");
    loopTimeSaveTemperatureHistoryHours = currTime;
    bResult = true;
  }
  return bResult;
}

boolean TimeSaveTemperatureHistoryDays()
{
  boolean bResult = false;
  if (currTime-loopTimeSaveTemperatureHistoryDays >= 24 * 3600 * 1000)
  {
    Serial.println("TimeSaveTemperatureHistoryDays");
    loopTimeSaveTemperatureHistoryDays = currTime;
    bResult = true;
  }
  return bResult;
}

void SendSMS(String strSMS, int phoneNumber)  // phoneNumber: 0 - incoming phone; 1..5 - phones[i]; 99 - alarm phone
{
  String phone;
  if (phoneNumber == 0)
  {
    phone = incomingPhone;
  }
  else if (phoneNumber == 99)
  {
    phone = "+" + String(phones[AlarmPnone-1]);
  }
  else
  {
    phone = "+" + String(phones[phoneNumber-1]);
  }
  Serial.println(strSMS);
   //Serial.println(phone);
      cell.print("AT+CMGS=\"" + phone +"\"\r");
      //cell.print("AT+CMGS=\"+79062656420\"\r");
      delay(1000);
      //cell.print("Test1\r");
      strSMS = strSMS + "\r";
      cell.print(strSMS);     
      delay(1000); 
      cell.write(26);
      //cell.print("AT+CEER\r");  //показать последние ошибки 
}

int read485Data()  //-1-nothing, 0-not me, 1-got answer, 2-got command
{
  int nResult = -1;
  if (ET_Command.receiveData())
  { 
    Serial.print("readData ");
    Serial.println(millis());
    if(Comm.DstID == RS485MyID)
    {
      if (Comm.Command == 'W')
        nResult = 1;
      else
        nResult = 2;      

      digitalWrite(LED_PIN, HIGH);
      Serial.print("   ");
      Serial.println(Comm.SrcID);
      Serial.print("   ");
      Serial.println(Comm.DstID);
      Serial.print("   ");
      Serial.println(Comm.Command);      
      Serial.print("   ");
      Serial.println(Comm.Data3);
    }
    else
    {
        nResult = 0;
          Serial.print("   ");
        Serial.println(Comm.DstID);
    }  
  }
  digitalWrite(LED_PIN, LOW);
  return nResult;
}

void SendRoomsCommand(char Oper, byte Data)
{
  Comm.Command = Oper;
  Comm.Data1 = Data;
  CounterRoomToSend = 0;
  SendRoomCommand(false);
}

void SendRoomCommand(boolean Repeat)
{
  if (Repeat)
  {
    CountResend485++;
  }
  else
  {  
    CountResend485 = 0;
    CounterRoomToSend++;
  }
  
  if (CounterRoomToSend <= NumberRoomsToSend)
  {
    send485Command(RoomsToSend[CounterRoomToSend-1]);
    IsWaitingAnswer485 = true;
    loopTimeSend485Command = millis();
    loopTimeRead485Answer = loopTimeSend485Command;
  }
}

void RS485ExchangeData()
{
  if (IsWaitingAnswer485)
  {
    if (currTime-loopTimeRead485Answer >= periodWait485Answer)
    {
      if (read485Data() == 1)
      {
        IsWaitingAnswer485 = false;
      }
      else
      {
        if (CountResend485 <= MaxCountResend485)
        {
          if (currTime-loopTimeSend485Command >= period485ReSend)
          {            
            SendRoomCommand(true);  //resend            
          }
        }
        else
        {
          SaveLog485Error();
          IsWaitingAnswer485 = false;
        }          
      }
      loopTimeRead485Answer = currTime;
    }
  }
  if (!IsWaitingAnswer485 && NumberRoomsToSend > 0)
  {
    if (CounterRoomToSend == NumberRoomsToSend)
    {
      //reset
      CounterRoomToSend = 0;
      NumberRoomsToSend = 0;
      for (int i = 1; i<8; i++)
      {
        RoomsToSend[i] = 0;
      }
    }
    else
    {
      SendRoomCommand(false);  //send command to next room
    }
  }  
}

void SaveLog485Error()
{
}

void send485Command(char DstID)
{
  Serial.print("sendCommand ");
  Serial.print(DstID);
  Serial.print(" ");
  Serial.println(millis());
    
  digitalWrite(LED_PIN, HIGH);  // Show activity
  digitalWrite(RX485_TX_CONTROL_PIN, RS485Transmit);
  
  Comm.SrcID = RS485MyID;
  Comm.DstID = DstID;
  //Comm.Command = 'O';
  //Comm.Data3 = millis();
  ET_Command.sendData();
 
  digitalWrite(RX485_TX_CONTROL_PIN, RS485Receive);
  digitalWrite(LED_PIN, LOW);  // Show activity
}
