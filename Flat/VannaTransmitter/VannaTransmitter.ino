// transmitter.pde
//
// Simple example of how to use VirtualWire to transmit messages
// Implements a simplex (one-way) transmitter with an TX-C1 module
//
// See VirtualWire.h for detailed API docs
// Author: Mike McCauley (mikem@open.com.au)
// Copyright (C) 2008 Mike McCauley
// $Id: transmitter.pde,v 1.3 2009/03/30 00:07:24 mikem Exp $

#include <VirtualWire.h>

const int RF_TX_PIN = 2;
const int BTN_PIN = 3;
const int VENT_PIN = 4;
const int BUZZ_PIN = 5;
const int analogInPin = A0;  // Analog input pin that the potentiometer is attached to

const int OnDensity = 20;
const int OffDensity = 1;
const long ManualShortOnDurationSec = 300; //5 minute
const long ManualLongOnDurationSec = 1800; //30 minute
const int PressSwitchLongOnMiliSec = 1000; //1 sec
const int DrebezgMiliSec = 20; 

volatile boolean buttonPressed = false;
volatile int Density = 0;
volatile int AvgDensity = 0;
volatile int Density2 = 0;
volatile int Density1 = 0;
volatile unsigned long lastChangeTime = 0;
volatile unsigned long currTime = 0;
volatile unsigned long VentOnTime = 0;
volatile boolean ventOn = false;
volatile int ventMode = 0;  //0-off, 1-densityOn, 2-manualShortOn, 3-manualLongOn

void setup()
{
    pinMode(BTN_PIN, INPUT_PULLUP); 
    pinMode(VENT_PIN, OUTPUT);
    pinMode(BUZZ_PIN, OUTPUT);
    Serial.begin(9600);	  // Debugging only
    Serial.println("setup");

    // Initialise the IO and ISR
    //vw_set_ptt_inverted(true); // Required for DR3100
    vw_set_tx_pin(RF_TX_PIN); // Setup transmit pin
    vw_setup(2000);	 // Bits per sec

    delay(1000);
    attachInterrupt(1, changeBtn, CHANGE); // привязываем 1-е прерывание к функции changeBtn(). 1 - номер прерывания (pin 3)
}

void loop()
{
    Density = analogRead(analogInPin);
    calcAvgDensity();

    char msg[8];
    dtostrf(AvgDensity,5,2,msg);
   // Serial.println(AvgDensity);
    
    digitalWrite(13, true); // Flash a light to show transmitting
    //vw_send((uint8_t *)msg, strlen(msg));
    //vw_wait_tx(); // Wait until the whole message is gone
    delay(100);
    digitalWrite(13, false);

  if (ventMode==0 && AvgDensity > OnDensity)
  {
      ventOn = true;
      ventMode=1;
      switchVent();
  }
  else if (ventMode==1 && AvgDensity < OffDensity)
  {
      ventOn = false;
      ventMode=0;
      switchVent();
  };

  delay(500);
  
  currTime = millis();
  if (ventMode==2 || ventMode==3)
  {
    if (VentOnTime>currTime)
    {
      VentOnTime=0;
    }

    if (buttonPressed && (currTime-lastChangeTime > PressSwitchLongOnMiliSec)) //долго удерживали
    {
      digitalWrite(BUZZ_PIN, true);
      delay(500);
      digitalWrite(BUZZ_PIN, false);
    }
    
    if ((ventMode==2 && (currTime - VentOnTime > ManualShortOnDurationSec*1000)) || (ventMode==3 && (currTime - VentOnTime > ManualLongOnDurationSec*1000)))
    {
      ventOn = false;
      ventMode = 0;
      switchVent();      
    }
  }
}

void changeBtn()
{ 
  buttonPressed = false;
  if (ventMode!=1)  //не включен по влажности
  {
     currTime = millis();
     if (currTime<lastChangeTime) //millis was reseted
     {
       lastChangeTime=0;
     }
     if (currTime - lastChangeTime > DrebezgMiliSec) //new change (ne drebezg)
     {
         buttonPressed = !digitalRead(BTN_PIN);  //store new condition
         if (buttonPressed)
         {
           if (ventMode==0) 
           {
             ventMode=2;
             VentOnTime=currTime;
             ventOn = true;
           }
           else
           {
             ventMode=0;
             ventOn = false;
           }
           switchVent();
         }
         else //отпустили кнопку
         {
           if (ventMode==2 && (currTime-lastChangeTime > PressSwitchLongOnMiliSec)) //долго удерживали
           {
             ventMode = 3;
           }      
         }
     }
     lastChangeTime = currTime;
  }
}

void switchVent()
{
  digitalWrite(VENT_PIN, ventOn);
}

//average density for 3 last measures
void calcAvgDensity()
{
  AvgDensity = (Density2 + Density1 + Density)/3;
  Density2 = Density1;
  Density1 = Density;
}
