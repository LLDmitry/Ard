/*
 * IRremote: IRrecvDemo - demonstrates receiving IR codes with IRrecv
 * An IR detector/demodulator must be connected to the input RECV_PIN.
 * Version 0.1 July, 2009
 * Copyright 2009 Ken Shirriff
 * http://arcfn.com
 */

#include <IRremote.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

const int ONE_WIRE_PIN = 9;
const int RECV_PIN = 8;   //11
const int LED_PIN =  13;      // the number of the LED pin
const int TERMO_PIN =  7;     //10 
const int BZ_PIN =  6;      // signal //12

const int addrModeTermo = 1;

const String btn1Code = "38863bd2";  //On/Of
const String btn2Code = "38863bc2";  //T Down
const String btn3Code = "38863bca";  //T Up
const String btn4Code = "38863bf4";  //AV/TV

//const unsigned long TermoPeriodOff = 36000;  //10 ч
const unsigned long periodTermoControl = 180;
const unsigned long periodShowMode = 30;
const int Termo1StartA = 16;
const int Termo1StopA = 17;
const int Termo2StartA = 18;
const int Termo2StopA = 19;
const int Termo3StartA = 20;
const int Termo3StopA = 21;
const int Termo1StartB = 17;
const int Termo1StopB = 19;
const int Termo2StartB = 19;
const int Termo2StopB = 21;
const int Termo3StartB = 21;
const int Termo3StopB = 23;

float TermoA;  //температура датчика A
float TermoB;  //температура датчика B

boolean OnA = false;
boolean OnB = false;

// Variables will change:
//int mode = 0;
unsigned long loopTimeTermo = 0;
unsigned long loopTimeShowMode = 0;
unsigned long currTime = 0;
int modeTermo = 0;  // 0-выкл  1-Termo1  2 Termo2  3 Termo3

OneWire  ds(ONE_WIRE_PIN);
DeviceAddress tempDeviceAddress;
DallasTemperature sensors(&ds);

IRrecv irrecv(RECV_PIN);

decode_results results;


void setup()
{
  Serial.begin(9600);
  irrecv.enableIRIn(); // Start the receiver
  pinMode(TERMO_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BZ_PIN, OUTPUT);
  digitalWrite(TERMO_PIN, LOW);   
  digitalWrite(LED_PIN, LOW);
  
  sensors.begin(); 
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, 10);
//  sensors.getAddress(tempDeviceAddress, 1);
//  sensors.setResolution(tempDeviceAddress, 12);  

  modeTermo = EEPROM.read(addrModeTermo); //при подаче питания восстановим modeTermo к-й был до выключения
  delay(1000); //для уменьшения скачка напряжения при включении питания
  Termo();
  if (modeTermo < 0 || modeTermo > 2)
  {
    modeTermo = 1;
  }
}

void loop() {
  if (irrecv.decode(&results)) {
    String res = String(results.value,HEX);
    Serial.println(res);
    if (res == btn4Code)
    {
     if (modeTermo > 0)
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
        if (modeTermo==0)
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
        
   irrecv.resume(); // Receive the next value
  }
 
   unsigned long diffTime = 0;
    
   currTime = millis();
   if (currTime >= loopTimeTermo)
   {
    diffTime = currTime - loopTimeTermo;
   }
   else
   {
    diffTime = currTime - loopTimeTermo + 4294967295;
   }
   
   if (diffTime >= periodTermoControl * 1000)
   {
     Termo();
     loopTimeTermo = currTime;
   }
 
   
   if (currTime >= loopTimeShowMode)
   {
    diffTime = currTime - loopTimeShowMode;
   }
   else
   {
    diffTime = currTime - loopTimeShowMode + 4294967295;
   }
   
   if (diffTime >= periodShowMode * 1000)
   {
     if (modeTermo==0)
     {
       LCDPulse(-1);
     }
     else
     {
       LCDPulse(modeTermo);
     }
     loopTimeShowMode = currTime;
   }
}

void Termo()
{
  sensors.requestTemperatures();
  Serial.print("T1= ");
//  TermoA = sensors.getTempCByIndx(0);
//  Serial.println(String(TermoA);
  Serial.println(sensors.getTempCByIndex(0));
//  Serial.print("T2= ");
  //sensors.getTempCByIndex(1) = sensors.getTempCByIndex(0);
//  Serial.println(String(sensors.getTempCByIndex(1));
//  Serial.println(sensors.getTempCByIndex(1));
//  delay(600);
  
  switch (modeTermo)
  {
    case 0:
      OnA = false;      
      break;      
    case 1:
      OnA = (sensors.getTempCByIndex(0) < Termo1StartA || OnA && sensors.getTempCByIndex(0) < Termo1StopA);
      break;
    case 2:
      OnA = (sensors.getTempCByIndex(0) < Termo2StartA || OnA && sensors.getTempCByIndex(0) < Termo2StopA);
      break;      
    case 3:
      OnA = (sensors.getTempCByIndex(0) < Termo3StartA || OnA && sensors.getTempCByIndex(0) < Termo3StopA);
      break;      
  } 
  digitalWrite(TERMO_PIN, OnA);   
  digitalWrite(LED_PIN, (modeTermo>0));
//Serial.println(String(modeTermo));
//Serial.println(String(OnA));
}

void BuzzerPulse(int bzMode)
{
  boolean longBz = (bzMode<0);
  
  for (int i = 0; i < abs(bzMode); i++) 
  {
      digitalWrite(BZ_PIN, HIGH);
      if (longBz)
      {
        delay(600);
      }
      else
      {
        delay(200);
      }
      digitalWrite(BZ_PIN, LOW);
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
