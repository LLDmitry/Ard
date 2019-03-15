#include <SoftEasyTransfer.h>
#include <IRremote.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <avr/wdt.h>

/*-----( Declare Constants and Pin Numbers )-----*/
#define SSerialRX        10  //Serial Receive pin
#define SSerialTX        11  //Serial Transmit pin

#define SSerialTxControl 3   //RS485 Direction control
#define RS485Transmit    HIGH
#define RS485Receive     LOW

const int IR_RECV_PIN = 7; //1 - out 2 - gnd 3 - vcc
const int DIST_TRIG_PIN = 5;
const int DIST_ECHO_PIN = 6;
const int IR_ALARM_PIN = 4;
const int SoilAnalogPIN = 0;
const int SVET_ANALOG_PIN = 1;
const int RX485_SOFT_RX_PIN = 15;
const int RX485_SOFT_TX_PIN = 16;
const int RX485_TX_CONTROL_PIN = 17;  //RS485 Direction control
const int DHT_PIN = 8;
const int ONE_WIRE_PIN = 12;
const int BZZ_PIN = 11;
const int LED_PIN = 13;
const int TERMO_PIN = 14;
const int LIGHT_PIN = 2;

const char RS485MyID = 'C';
const char RS485MasterID = 'A';
const int MaxCountResend485 = 3;
const int Resolution18b20 = 11;

const String btn1Code = "807ff807";  //On/Of
const String btn2Code = "807fe01f";  //T Up
const String btn3Code = "807f20df";  //T Down
const String btn4Code = "807f50af";  //AV/TV

const int addrModeTermo = 1;
const int addrSetTemp = 2;
const unsigned int PERIOD_GET_TEMP_SEC = 30;
const unsigned long periodTermoControl = 180;
const unsigned long periodShowMode = 30;

const float Mode1Temp = 2.0;
const float Mode2Temp = 18.0;
const float Mode3Temp = 20.0;
const float DeltaTemp = 0.5;

const unsigned long periodWait485Answer = 40; // millisec
const unsigned long period485ReSend = 50; // millisec

unsigned long currTime;
unsigned long loopTimeTermo;
unsigned long loopTimeShowMode;
unsigned long loopTimeGetTemper;
unsigned long lastSendCommand;
unsigned long loopTimeSend485Command;
unsigned long loopTimeRead485Answer;

float CurrTemp;
float SetTemp;
int modeTermo = 0;  // -1-задана внешняя SetTemp 0-выкл  1-Termo1  2 Termo2  3 Termo3
boolean OnA = false;
int CountResend485 = 0;
unsigned int lastAlarm;  //minutes ago
boolean needCheckAlarm = false;
boolean AlarmMode = false;
boolean IsWaitingAnswer485;

/*-----( Declare objects )-----*/
SoftwareSerial RS485Serial(SSerialRX, SSerialTX); // RX, TX

SoftEasyTransfer ET_Command;
SoftEasyTransfer ET_Answer;

OneWire  ds(ONE_WIRE_PIN);
DeviceAddress tempDeviceAddress;
DallasTemperature sensors(&ds);

IRrecv irrecv(IR_RECV_PIN);

decode_results results;

typedef struct{
  char SrcID;
  char DstID;
  char Command;
  char Attribute;
  unsigned int Data1;
  float Data2;
} CommandStruct;
CommandStruct Comm;

void setup()   /****** SETUP: RUNS ONCE ******/
{
  // Start the built-in serial port, probably to Serial Monitor
  Serial.begin(9600);
  
  irrecv.enableIRIn(); // Start the receiver
  pinMode(TERMO_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BZZ_PIN, OUTPUT); 
  pinMode(DIST_TRIG_PIN, OUTPUT); 
  pinMode(DIST_ECHO_PIN, INPUT);
  pinMode(IR_ALARM_PIN, INPUT);
 
  digitalWrite(TERMO_PIN, LOW);   
  digitalWrite(LED_PIN, LOW);
  
  sensors.begin(); 
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, Resolution18b20);
  sensors.setWaitForConversion(false);  // makes it async  
  sensors.requestTemperatures();  // for preparing Temp for 1st read 
  
  digitalWrite(SSerialTxControl, RS485Receive);  // Init Transceiver
  
  RS485Serial.begin(4800);   // set the data rate 
  
  pinMode(RX485_TX_CONTROL_PIN, OUTPUT);    
  digitalWrite(RX485_TX_CONTROL_PIN, RS485Receive);  // Init Transceiver   
  RS485Serial.begin(9600);   // set the data rate 
  ET_Command.begin(details(Comm), &RS485Serial);
  
  modeTermo = EEPROM.read(addrModeTermo); //при подаче питания восстановим modeTermo к-й был до выключения
  SetTemp = EEPROM.read(addrSetTemp); //при подаче питания восстановим SetTemp к-й был до выключения 
    
  delay(1000); //для уменьшения скачка напряжения при включении питания
  Termo();
  
  wdt_enable(WDTO_8S);  //если в loop не делать wdt_reset, то каждые 8сек будет перезагрузка
  wdt_reset();
}//--(end setup )---


void loop()
{
  currTime = millis();
  
    //checkDistanceAlarm();
  checkIRAlarm();
  
  readTemperature();
  
  TermoControl();
  
  if (!IsWaitingAnswer485)
  {
    if (read485Data() == 2)
    {
      ExecReceived485Command();
    }
  }
  
  String resIR = readIRData();
  if (resIR.length() > 0)
  {
    ExecIRCommand(resIR);    
    //SendCommandFromIR(resIR);
  }
 
//   if (currTime - loopTimeShowMode >= periodShowMode * 1000)
//   {
//     if (modeTermo==0)
//     {
//       LCDPulse(-1);
//     }
//     else
//     {
//       LCDPulse(modeTermo);
//     }
//     loopTimeShowMode = currTime;
//   }   
     
}

void readTemperature()
{
  if (currTime - loopTimeGetTemper > PERIOD_GET_TEMP_SEC * 1000)
  {
    loopTimeGetTemper = currTime;
    sensors.requestTemperatures();  // 754 ms
    CurrTemp = sensors.getTempCByIndex(0);  // 35 ms
    Serial.println(CurrTemp);
  }
}

void TermoControl()
{
  if (currTime - loopTimeTermo >= periodTermoControl * 1000)
  {
    Termo();
    loopTimeTermo = currTime;
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
            SendCommand(true);  //resend            
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
//  if (!IsWaitingAnswer485)
//  {   
//      SendCommand(false);  //send command to next room   
//  }  
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
      Serial.println(Comm.Data1);
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

void SaveLog485Error()
{
}

void send485Command(char DstID, byte IsAnswer)
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

String readIRData()
{
  String res = "";
  if (irrecv.decode(&results)) {
    res = String(results.value,HEX);
    Serial.println(res);
    irrecv.resume(); // Receive the next value    
  }
  return res;
}

void ExecReceived485Command()
{
   if (Comm.Command == 'Q') //запрос
   {
      if (Comm.Attribute == 'T')  //запрос текущей температуры
      {
        Comm.Data2 = CurrTemp;
      }
      if (Comm.Attribute == 'G')  // нагрев
      {
        Comm.Data1 = digitalRead(TERMO_PIN);
        if (modeTermo !=0)
        {
          Comm.Data2 = SetTemp;
        }
        else
        {
          Comm.Data2 = -40;
        }
      }
      if (Comm.Attribute == 'L')  // свет
      {
        Comm.Data1 = digitalRead(LIGHT_PIN);
      }
      if (Comm.Attribute == 'A')  // последнее присутствие, минут назад
      {
        Comm.Data1 = lastAlarm;
      }
      Comm.Command = 'A'; // answer      
      SendCommand(false);
   }
   else if (Comm.Command == 'S') //установка
   {
     //send answer
     send485Command(Comm.SrcID, true);
     
     if (Comm.Attribute == 'G')  //вкл-выкл нагрев
     {
       digitalWrite(TERMO_PIN, Comm.Data1);
     }
     if (Comm.Attribute == 'L')  //вкл-выкл свет
     {
       digitalWrite(LIGHT_PIN, Comm.Data1);
     }
     if (Comm.Attribute == 'T') //установка температуры
     {
       SetTemp = Comm.Data2;
       modeTermo = -1;
       EEPROM.write(addrSetTemp, SetTemp);
       EEPROM.write(addrModeTermo, modeTermo);
       Termo();
     }
     if (Comm.Attribute == 'A')  //вкл-выкл сигнализацию движения
     {
       needCheckAlarm = Comm.Data1;
     }     
   }
}

void SendCommand(boolean Repeat)
{
  if (Repeat)
    CountResend485++;
  else
    CountResend485 = 0;
  
  send485Command(RS485MasterID, false);
  IsWaitingAnswer485 = true;
  loopTimeSend485Command = millis();
  loopTimeRead485Answer = loopTimeSend485Command; 
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
     
      Comm.Command = 'A';
      Comm.Data1 = 1;          
      SendCommand(false);
    }
  }
  else
  {
//    Serial.println("NO ALARM");
    digitalWrite(LED_PIN, LOW);
    AlarmMode = false;
  }
}

void ExecIRCommand(String res)
{
  if (res == btn4Code)
  {
     if (modeTermo != 0)
     {
       modeTermo = 0;
     }
     else
     {
       modeTermo = 2;
     }
  }
  else if (res == btn2Code && modeTermo < 3)
  {
     modeTermo = modeTermo + 1;
  }
  else if (res == btn3Code)
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
   
   if (res == btn4Code || res == btn2Code || res == btn3Code)
   {
      EEPROM.write(addrModeTermo, modeTermo);
      Serial.println(String(modeTermo));
      if (modeTermo == 0)
      {
        BuzzerPulse(-1);
      }
      else
      {
        BuzzerPulse(modeTermo);
      }
        
      Termo();  // применим новую установку температуры сразу по нажатию пульта
      loopTimeShowMode = currTime;
   }
}



//void SendCommandFromIR(String res)
//{
//   if (res == btn4Code || res == btn2Code || res == btn3Code)
//   {
//      Comm.Command = 'G';
//      Comm.Data1 = modeTermo;
//      
//      switch (modeTermo)
//      {       
//      case 0:
//        Comm.Data2 = 0; 
//        break;      
//      case 1:
//        Comm.Data2 = Mode1Temp; 
//        break;  
//      case 2:
//        Comm.Data2 = Mode2Temp; 
//        break;      
//      case 3:
//        Comm.Data2 = Mode3Temp; 
//        break;       
//      }    
//      sendCommand(true);
//   }
//}

void Termo()
{
//  sensors.requestTemperatures();
  Serial.print("T1= ");
//  TermoA = sensors.getTempCByIndx(0);
//  Serial.println(String(TermoA);
  Serial.println(CurrTemp);
//  Serial.print("T2= ");
//  Serial.println(String(sensors.getTempCByIndex(1));
//  Serial.println(sensors.getTempCByIndex(1));
//  delay(600);
  
  switch (modeTermo)
  {
    case -1:
      break;
    case 0:
      OnA = false;      
      break;      
    case 1:
      SetTemp = Mode1Temp;
      break;
    case 2:
      SetTemp = Mode2Temp;
      break;      
    case 3:
      SetTemp = Mode3Temp;
      break;      
  }
  if (modeTermo != 0)
  {
    OnA = (CurrTemp < (SetTemp - DeltaTemp) || OnA && CurrTemp < (SetTemp + DeltaTemp));
  }
  digitalWrite(TERMO_PIN, OnA);   
  digitalWrite(LED_PIN, (modeTermo>0));
  Serial.print("modeTermo= ");
  Serial.println(String(modeTermo));
//Serial.println(String(OnA));
}

void BuzzerPulse(int bzMode)
{
  boolean longBz = (bzMode<0);
  
  for (int i = 0; i < abs(bzMode); i++) 
  {
      digitalWrite(BZZ_PIN, HIGH);
      if (longBz)
      {
        delay(600);
      }
      else
      {
        delay(200);
      }
      digitalWrite(BZZ_PIN, LOW);
      delay(400);
  }
}

void LCDPulse(int Mode)
{
  boolean longPulse = (Mode<0);
  boolean reverse = (modeTermo>0);  
  
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

