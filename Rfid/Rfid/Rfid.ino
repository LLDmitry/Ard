/*
www.lab409.ru
Danil Borchevkin
15/12/2013
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

RFID rfid(SS_PIN, RST_PIN);

const char breakCodeZamokClose[5] = {1,0,1,0,1};
const char breakCodeZamokOpen[5] = {1,0,0,1,1};
const char breakCodeZamokOpenService[5] = {1,1,0,1,1};
int CounterBreakCode;

const int addressMaster = 1;
const int addressMetka = 10;


char breakCodeReal[5] = {0,0,0,0,0};

unsigned char reading_card[5]; 
//for reading card
unsigned char master[5];// = {157,90,21,46,252};
unsigned char metka[5];
// allowed card
unsigned char i;

boolean prevSignState = false;
boolean prevZazigState = false;
boolean zazigState = false;
boolean signState = false;
boolean kapotState = false;
boolean breakPedalState = false;
boolean prevBreakPedalState = false;
boolean bRfAccepted = false;
boolean bServiceMode = false;

unsigned long timeStartBreakOnPedal;
unsigned long timeBreakOffPedal;
unsigned long timeBreakPausePedal;
unsigned long longBreakOnPedal;
unsigned long longBreakOffPedal;

void allow();
void denied();

const unsigned long MinBreakPausePedal = 200;
const unsigned long MaxBreakPausePedal = 2000;
const unsigned long MinBreakOnPedal = 200;
const unsigned long MaxBreakOnPedal = 1500;
const unsigned long ShortBreakOnPedal = 700;
const int ImpulseZamok = 500;
const unsigned long MaxPauseBeforeZazig = 3000;


void setup()
{   
  Serial.begin(9600);  
  SPI.begin();   
  rfid.init();  
  pinMode(OPEN_PIN, OUTPUT);  
  pinMode(CLOSE_PIN, OUTPUT);
  
  digitalWrite(OPEN_PIN, LOW);
  digitalWrite(CLOSE_PIN, LOW);
  
//  for (i = 1; i < 5; i++) 
//  {
//    master[i] = EEPROM.read(addressMaster + i);
//    metka[i] = EEPROM.read(addressMetka + i);
//  }
  CounterBreakCode = 0;   
  
  EEPROM.write(addressMetka, 157);
  EEPROM.write(addressMetka+1, 90);
  EEPROM.write(addressMetka+2, 21);
  EEPROM.write(addressMetka+3, 46);
  EEPROM.write(addressMetka+4, 252);

  for (i = 0; i < 5; i++) 
  {
    master[i] = EEPROM.read(addressMaster + i);
    metka[i] = EEPROM.read(addressMetka + i);
  }
}


void loop()
{   
    checkMetka();
       if (bRfAccepted)
       {
         ZamokOpen();
         bServiceMode = false;
       }
     bRfAccepted = false;   
  
  delay(50);
}

void checkMetka()
{
  if (rfid.isCard())     
  {        
    if (rfid.readCardSerial())
    {                
      /* Reading card */                
      Serial.println(" ");                
      Serial.println("Card found");                
      Serial.println("Cardnumber:");                 
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
      if (i == 5)                
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
  
    delay(ImpulseZamok);    
    digitalWrite(CLOSE_PIN, LOW);
    
}

void ZamokOpen()
{
    Serial.println("ZamokOpen");
    digitalWrite(OPEN_PIN, HIGH);
    
    delay(ImpulseZamok);    
    digitalWrite(OPEN_PIN, LOW);

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


