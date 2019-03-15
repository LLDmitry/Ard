#include <OneWire.h>
#include <DallasTemperature.h>

OneWire  ds(10);  // on pin 10
DeviceAddress tempDeviceAddress;
DallasTemperature sensors(&ds);

const int NAGREV_PIN = 0;
const int NASOS_PIN = 13;
const int ContLow = A0;
const int ContTop = A1;

boolean VContLow = false;
boolean VContTop = false;

int OnSec = 3000;
int OffSec = 2000;
double TNorma = 50;
double TDelta = 10;
double T1;
double T2;
double T1Prev;

unsigned long currTime = 0;
unsigned long TempReadTime = 0;
unsigned long TempSetTime = 0;

void setup(void) {
  Serial.begin(9600);
  sensors.begin();

  sensors.getAddress(tempDeviceAddress, 0);
//    sensors.setResolution(tempDeviceAddress, 9);
  sensors.setResolution(tempDeviceAddress, 12);
//  sensors.getAddress(tempDeviceAddress, 1);
//  sensors.setResolution(tempDeviceAddress, 12);  

    pinMode(ContLow, INPUT_PULLUP); 
    pinMode(ContTop, INPUT_PULLUP); 
}

void loop(void) {
  currTime = millis();
  
  if ((currTime-TempReadTime)>10000)
  {
    sensors.requestTemperatures();
    Serial.print("T1= ");
    T1 = sensors.getTempCByIndex(0);
  //T2 = sensors.getTempCByIndex(1);
  //  Serial.println(sensors.getTempCByIndex(0));
  //  Serial.println(T1);
  //  Serial.print("T2= ");
  //  Serial.println(T2);
   // Serial.println(sensors.getTempCByIndex(1));
  
    HeatControl();
  
    T1Prev = T1;
    
    TempReadTime = currTime;
  }

  if ((currTime-TempSetTime)>1000)
  {
    digitalWrite(NAGREV_PIN, true); 
    TempSetTime = currTime;
  }
  
  VContLow = !digitalRead(ContLow);
  VContTop = !digitalRead(ContTop);
  if (VContTop)
  {
    digitalWrite(NASOS_PIN, false);  
  }
  else if (VContLow)
  {
    digitalWrite(NASOS_PIN, true); 
  }
  
  delay(1000);
}

void HeatControl()
{
  double TDiff;
  double TDiffPrev;
  
  TDiff = T1 - TNorma;
  //TDiffPrev = T1 - T1Prev;
  
  if (abs(TDiff)>TDelta)
  {
    if (TDiff<0) //надо греть
    {
      if (abs(TDiff)>20) //греть максимально
      {
        OffSec = 0;
      }
      else // греть с учетом разницы
      {
        OffSec = 1000;
      }
    }
    else // охлаждать
    {
      if (abs(TDiff)>15) //охлаждать максимально
      {
        OffSec = 5000;
      }
      else // охлаждать с учетом разницы
      {
        OffSec = 3000;
      }      
    }
  }
}
