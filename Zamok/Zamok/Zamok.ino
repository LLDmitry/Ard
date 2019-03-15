/*
* Закрывает и открывает замок капота
*  Закрытие - через паузу по одиночному срабатыванию поворотника при выключенном зажигании, при этом не д.б. ServiceMode. 
* Либо по спец комбинации включений-выключений зажигания.
* Открытие - после поднесения RF метки при выкл зажигании а затем включении зажигания. 
* Либо по спец комбинации включений-выключений зажигания.
* ServiceMode включается по спец комбинации включений-выключений зажигания. При этом при постановке на охрану замок не будет закрываться. Действует до спец комбинации или до метки.
*
* подключение считывателя RF меток:
* MOSI: Pin 11 / ICSP-4
* MISO: Pin 12 / ICSP-1
* SCK: Pin 13 / ISCP-3
* SS: Pin 10
* RST: Pin 9
*/

#include <SPI.h>
#include <RFID.h>
#include <EEPROM.h>

#define SS_PIN 10
#define RST_PIN 9
#define MOSI_PIN 11
#define MISO_PIN 12
#define SCK_PIN 13

#define OPEN_PIN 6
#define CLOSE_PIN 7
#define ALARM_OUT_PIN 1
#define ZAZIG_PIN 2
#define SIGN_ON_PIN 3
#define BZZ_PIN 5
//#define KAPOT_PIN 5

RFID rfid(SS_PIN, RST_PIN);

const byte ZazigCodeZamokClose[3] = {1,0,1};
const byte ZazigCodeZamokOpen[3] = {1,0,0};
const byte ZazigCodeZamokOpenService[3] = {1,1,0};
int CounterZazigCode;

byte ZazigCodeReal[3] = {0,0,0};

unsigned char reading_card[5]; 
//for reading card
unsigned char master[5];// = {157,90,21,46,252};
unsigned char metka[5];
// allowed card
unsigned char i;
const int addressMaster = 1;
const int addressMetka = 10;
const int addressServiceMode = 20;

volatile boolean zazigState = false;
boolean zazigStatePrev = false;
//boolean kapotState = false;
boolean bRfAccepted = false;
boolean bServiceMode = false;
volatile boolean bIsWaitSign = false;

volatile unsigned long timeZazigOn;
unsigned long timeZazigOff;
unsigned long timeBreakPausePedal;
unsigned long periodZazigOn;
unsigned long periodZazigOff;
unsigned long timeRF;
volatile unsigned long timeSign;
boolean mbIsReadyForCheckOpening = true;

void allow();
void denied();

const unsigned long PauseBeforeRfUnlockCheck = 3000;
const unsigned long MinZazigOff = 150;
const unsigned long MaxZazigOff = 2000;
const unsigned long MinZazigOn = 150;
const unsigned long MaxZazigOn = 3000;
const unsigned long ShortZazigOn = 1000;
const int ImpulseToZamok = 500;
const unsigned long MaxPauseBetweenRfReadAndZazigOn = 5000;
const unsigned long PauseBetweenSignAndZamokClose = 10000;
const unsigned long PauseBetweenSign = 1500;


void setup()
{   
  Serial.begin(9600);  
  SPI.begin();   
  rfid.init();  
  pinMode(OPEN_PIN, OUTPUT);  
  pinMode(CLOSE_PIN, OUTPUT);
  pinMode(ALARM_OUT_PIN, OUTPUT);
  pinMode(BZZ_PIN, OUTPUT);
  
  pinMode(ZAZIG_PIN, INPUT_PULLUP);
  pinMode(SIGN_ON_PIN, INPUT);
 // pinMode(KAPOT_PIN, INPUT);
//  pinMode(BREAK_PEDAL_PIN, INPUT);
  
  digitalWrite(OPEN_PIN, LOW);
  digitalWrite(CLOSE_PIN, LOW);
  
  for (i = 0; i < 5; i++) 
  {
    master[i] = EEPROM.read(addressMaster + i);
    metka[i] = EEPROM.read(addressMetka + i);
  }
  bServiceMode = EEPROM.read(addressServiceMode);
  CounterZazigCode = 0;
  bIsWaitSign = false;
  
//  attachInterrupt(0, changeZazig, CHANGE); // привязываем 1-е прерывание к функции changeZazig(). 0 - номер прерывания (pin 2 ZAZIG_PIN) 
 // attachInterrupt(1, lightPovorotnik, RISING); // привязываем 2-е прерывание к функции lightPovorotnik(). 1 - номер прерывания (pin 3 SIGN_ON_PIN)
}

//void changeZazig()
//{
//        Serial.println("changeZazig");  
//    // через 3 сек после включения зажигания проверим zazigCode и RF
//    zazigState = digitalRead(ZAZIG_PIN);
//    if (zazigState)
//    {
//      timeZazigOn = millis();
//      bIsWaitSign = false;
//    }
//    else
//    {
//      timeZazigOff = millis();
//    }
//    ReadZazigSwitches();    
//  //  prevZazigState = zazigState;    
//}
//
//void lightPovorotnik()
//{
//          Serial.println("lightPovorotnik");  
//  //проверим включение сигнализации (это одиночное срабатывание поворотника при выкл зажигании)
//  //если не ServiceMode то закроем замок через 3с (если за это время не будет повторного включения сигнализации)
//  if (((millis() - timeZazigOff) > PauseBetweenSign) && ((millis() - timeSign) > PauseBetweenSign) && !bServiceMode)
//  {
//    bIsWaitSign = true;          
//  }
//  else
//  {
//    bIsWaitSign = false;
//  }
//  timeSign = millis();        
//}  

void loop()
{
 // kapotState = digitalRead(KAPOT_PIN);
  zazigState = digitalRead(ZAZIG_PIN);
  if (!zazigState)
  {
    checkRF();
  }
  digitalWrite(13, zazigState);  
  if (zazigState != zazigStatePrev)
  {
    if (zazigState)
    {
      timeZazigOn = millis();
      bIsWaitSign = false;
      mbIsReadyForCheckOpening = true;
    }
    else
    {
      timeZazigOff = millis();
    }
    ReadZazigSwitches();
    zazigStatePrev = zazigState;
  }
 
  if (zazigState) // зажигание включено
  {
    if (mbIsReadyForCheckOpening && (millis() - timeZazigOn) >= PauseBeforeRfUnlockCheck)  // через 2 сек после включения зажигания проверим ZazigCodes; если код не готов - проверим RF
    {
      if (CounterZazigCode == 3)
      {
        Serial.println("CheckZazigCodes");
        int resCheck = CheckZazigCodeResult();
        switch (resCheck) 
        {
          case 1:    
            ZamokClose();
            Serial.println("Z1");
            bServiceMode = false;
            EEPROM.write(addressServiceMode, bServiceMode);
            break;
          case 2:    
            ZamokOpen();
            bServiceMode = false;
            EEPROM.write(addressServiceMode, bServiceMode);            
            break;
           case 3:    
            ZamokOpen();
            bServiceMode = true;
            EEPROM.write(addressServiceMode, bServiceMode);
            break;  
        }
      }
      else  //ZazigCode не готово, проверим RF code
      {
//        checkRF();
        Serial.println(CounterZazigCode);                
        Serial.println("check RF card result");
        if (bRfAccepted)
        {
          ZamokOpen();
          bServiceMode = false;
          EEPROM.write(addressServiceMode, bServiceMode);         
        }
        else
        {
           Serial.println("RF card not acepted");
        }
      }
      CounterZazigCode = 0;
      bRfAccepted = false;
      mbIsReadyForCheckOpening = false;
    }
  }
  else  // зажигание выключено
  {
    if (bRfAccepted && ((millis() - timeRF) > PauseBetweenSignAndZamokClose))  // через 5 сек после RF read, если не включили зажигание, сбросим RF
    {
          ZamokClose(); //временно, пока не подключен сигнал с поворотника
      bRfAccepted = false;        
    }
      
    if (bIsWaitSign && !bServiceMode && ((millis() - timeSign) > PauseBetweenSignAndZamokClose) && ((millis() - timeSign) > PauseBetweenSign))
    {
      ZamokClose();
      bIsWaitSign = false;
    }
  }
  


  delay(50);
}


//читаем коды переключений зажигания
void ReadZazigSwitches()
{ 
  if (zazigState)  //включили
  {
    periodZazigOff = timeZazigOn - timeZazigOff;
    if (periodZazigOff > MaxZazigOff || periodZazigOff < MinZazigOff)  //too long off or too short, reset ZazigCode
    {
      CounterZazigCode = 0;
    }
  }  
  else //выключили
  {    
    periodZazigOn = timeZazigOff - timeZazigOn;
    
    if (periodZazigOn > MaxZazigOn || periodZazigOn < MinZazigOn)  //too long off or too short, reset ZazigCode
    {
      CounterZazigCode = 0;
                    Serial.println("Code reset");
    }
    else
    {
      if (periodZazigOn > ShortZazigOn)  //was long zazig on
      {        
        ZazigCodeReal[CounterZazigCode] = 1;
                      Serial.println("Code 1");
      }
      else //was short zazig on
      {    
        ZazigCodeReal[CounterZazigCode] = 0;
              Serial.println("Code 0");
      }
      CounterZazigCode = CounterZazigCode + 1;
    }    
  }
}

int CheckZazigCodeResult() // return 0 - code not recognised; 1 - ZamokClose; 2 - ZamokOpen; 3 - ZamokOpenServiceMode
{
  boolean bZamokClose = false;
  boolean bZamokOpen = false;
  boolean bZamokOpenService = false;
  int iResult = 0;
  
  if (CounterZazigCode == 3)
  {
    for (int i = 0; i < 3; i++)
    {
                                      Serial.print(i);
                                      Serial.print(" : ");
              Serial.println(ZazigCodeReal[i]);                
              
      if (ZazigCodeReal[i] == ZazigCodeZamokClose[i])
      {
        Serial.println(" close");
        if (i == 0 || bZamokClose)
        {
          bZamokClose = true;
        }
      }
      else
      {
        bZamokClose = false;
        //break;        
      }   
      
      if (ZazigCodeReal[i] == ZazigCodeZamokOpenService[i])
      {
                Serial.println(" op s");
        if (i == 0 || bZamokOpenService)
        {
          bZamokOpenService = true;
        }
      }
      else
      {
        bZamokOpenService = false;
        //break;
      }      
      
      if (ZazigCodeReal[i] == ZazigCodeZamokOpen[i])
      {

                        Serial.println(" open");
        if (i == 0 || bZamokOpen)
        {
          bZamokOpen = true;
        }
      }
      else
      {
        bZamokOpen = false;
        //break;
      }                  
    }
  }
  
  if (bZamokClose)
  {
    iResult = 1;
  }
  else if (bZamokOpen)
  {
    iResult = 2;
  }
  else if (bZamokOpenService)
  {
    iResult = 3;
  }
  
  return iResult;
}

void checkRF()
{
//        Serial.println("checkRF");                
  if (rfid.isCard())     
  {        
    if (rfid.readCardSerial())
    {                
      /* Reading card */                
      Serial.println(" ");                
      Serial.println("Card found");                
      Serial.println("Cardnumber:");
      timeRF = millis();
      for (i = 0; i < 5; i++)                
      {                       
        Serial.print(rfid.serNum[i]);                  
        Serial.print(" ");                  
        reading_card[i] = rfid.serNum[i];                
      }                
      Serial.println();
      
      //verification
      boolean checkingMaster = true;
      boolean checkingMetka = true;      
      for (i = 0; i < 5; i++)                
      {                  
        if (checkingMaster)
        {
          if (reading_card[i] != master[i])
          {
            checkingMaster = false;
          }
        }
        if (checkingMetka)
        {
          if (reading_card[i] != metka[i])
          {
            checkingMetka = false;
          }
        }
        
        if (!checkingMaster && !checkingMetka)
        {                    
          break;                  
        }                
      }                
      if (i == 5) //совпали все 5 цифр         
      {                  
        allow();                
      }                
      else                
      {                  
        denied();                
      }         
    }
  }
  rfid.halt();
}


void ZamokClose()
{
    Serial.println("ZamokClose");
    digitalWrite(CLOSE_PIN, HIGH);
  
    delay(ImpulseToZamok);    
    digitalWrite(CLOSE_PIN, LOW);
    
    digitalWrite(BZZ_PIN, HIGH);
    delay(600);    
    digitalWrite(BZZ_PIN, LOW);
}

void ZamokOpen()
{
    Serial.println("ZamokOpen");
    digitalWrite(OPEN_PIN, HIGH);
    
    delay(ImpulseToZamok);    
    digitalWrite(OPEN_PIN, LOW);

    digitalWrite(BZZ_PIN, HIGH);
    delay(300);    
    digitalWrite(BZZ_PIN, LOW);
    delay(300);
    digitalWrite(BZZ_PIN, HIGH);
    delay(300);    
    digitalWrite(BZZ_PIN, LOW);
}

void allow()
{ 
  bRfAccepted = true; 
  Serial.println("Access accepted!");  
}

void denied()
{  
  bRfAccepted = false;
  Serial.println("Access denied!");
}


