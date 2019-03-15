/* ** Энкодер ** Для управлением яркостью LED используется энкодер Sparkfun */  
//int brightness = 120;
// яркость LED, начинаем с половины
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include "Arduino.h"
#include <DS1307.h> // written by  mattt on the Arduino forum and modified by D. Sjunnesson

unsigned long currentTime; 
unsigned long loopTimeClock=0;
unsigned long loopTimeEncoder=0;
const int pin_A = 12;       // pin 12 const 
int pin_B = 11;       // pin 11 
int pin_C = 10;       // pin 10
unsigned char encoder_A; 
unsigned char encoder_B; 
unsigned char encoder_A_prev=0; 
unsigned char encoder_Btn;
unsigned char encoder_Btn_prev = 0;
int counter_encoder_Btn = 0;
int iAction0;
int iAction1;
int iRow=0;
String Actions[] = {"act1", "act2", "act3", "act4", "act5"};
String SettingMenu[] = {"Current Time", "Start Time", "Stop Time"};
char currSettingsMenu=0;
char iSettingPosition=0;
unsigned char currHr;
unsigned char currMin;
unsigned char currSec;
unsigned char currDay;
unsigned char currMonth;
unsigned char currYear;

unsigned char currMode=0;  //0-Show; 1-Settings menu; 2-Modifying
unsigned char currParamSet=0;  //0-CurrDateTime; 1-StartTime; 2-StopTime
unsigned char currSetPosition=0;  //0-Hours; 1-Minutes; 2-Days; 3-Month; 4-Year
int changeValue=0;

LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

void setup()  {   // declare pin 9 to be an output:  
  Serial.begin(9600);
  lcd.init();                      // initialize the lcd 
 
  // Print a message to the LCD.
  lcd.backlight();
  
//  RTC.stop();
//  RTC.set(DS1307_SEC,1);        //set the seconds
//  RTC.set(DS1307_MIN,46);     //set the minutes
//  RTC.set(DS1307_HR,9);       //set the hours
//  RTC.set(DS1307_DOW,4);       //set the day of the week
//  RTC.set(DS1307_DATE,17);       //set the date
//  RTC.set(DS1307_MTH,3);        //set the month
//  RTC.set(DS1307_YR,13);         //set the year
//  RTC.start();
  
  pinMode(pin_A, INPUT_PULLUP);  
  pinMode(pin_B, INPUT_PULLUP);  
  pinMode(pin_C, INPUT_PULLUP);  
}    

void loop()  
{  
  int iBtn;
    
  currentTime = millis();
  
  if(currentTime >= (loopTimeClock +1000))  // проверяем каждые 1с
  {
//    Serial.print(RTC.get(DS1307_HR,true)); //read the hour and also update all the values by pushing in true
//    Serial.print(":");
//    Serial.print(RTC.get(DS1307_MIN,false));//read minutes without update (false)
//    Serial.print(":");
//    Serial.print(RTC.get(DS1307_SEC,false));//read seconds
//    Serial.print("      ");                 // some space for a more happy life
//    Serial.print(RTC.get(DS1307_DATE,false));//read date
//    Serial.print("/");
//    Serial.print(RTC.get(DS1307_MTH,false));//read month
//    Serial.print("/");
//    Serial.print(RTC.get(DS1307_YR,false)); //read year
//    Serial.println();
    
    if (currMode==0)
    {
      currHr = RTC.get(DS1307_HR,true);
      currMin = RTC.get(DS1307_MIN,false);
      currSec = RTC.get(DS1307_SEC,false);
      
      currDay = RTC.get(DS1307_DATE,false);
      currMonth = RTC.get(DS1307_MTH,false);
      currYear = RTC.get(DS1307_YR,false);      
      
      PrintDateTime(true);
    }   
    
    loopTimeClock = currentTime;
  }
  
  if(currentTime >= (loopTimeEncoder + 5))  // проверяем каждые 5мс (200 Гц)
  {         
    //unsigned char iBtn;
    iBtn = GetButton();  
    if (iBtn>0)
    {
      ActionBtn(iBtn); 
    }
    loopTimeEncoder = currentTime;
  }    
} 


void ActionBtn(int iBtn) //1-short;2-long;3-left;4-right
{
  switch (iBtn) 
  { 
    case 1:
      if (currMode==0)
      {
        lcd.setCursor(0,iRow);
        lcd.noBlink();
        iRow = !iRow;
        lcd.setCursor(0,iRow);
        lcd.blink();
      }
      if (currMode==1)
      {
        currMode=2;
//        Serial.println(currSettingsMenu);
          switch(currSettingsMenu)
          {
            case 0:
              PrintDateTime(true);
              break;
            case 1:
              PrintDateTime(false);  
              break;              
            case 2:
              PrintDateTime(false);
              break;              
          }
      }
      
      if (currMode==2)
      {
        ChangingPosition();
//        iSettingPosition = iSettingPosition +3;
//        SetSettingPosition();
      }
      break;    
    case 2:
      if (currMode==0)
      {
        currMode = 1;
      }
      else
      {
        currMode = 0;
      }
      break;    
    case 3:
      switch (currMode) 
      { 
        case 0:      
          if (iRow==0)
          {
            iAction0=iAction0-1;
            if (iAction0<0) {iAction0=4;}        
          }
          else
          {
            iAction1=iAction1-1;
            if (iAction1<0) {iAction1=4;}    
          }
          
        case 1:      
          currSettingsMenu = currSettingsMenu -1;
          if (currSettingsMenu < 0) {currSettingsMenu=2;}
          break;
          
        case 2:  
          ChangingValue(iBtn);
          break;    
      }
      break;  
    case 4:
      switch (currMode)
      {
        case 0:      
          if (iRow==0)
          {
            iAction0 = iAction0 + 1;
            if (iAction0>4) {iAction0=0;}
          }
          else
          {
            iAction1 = iAction1 + 1;
            if (iAction1>4) {iAction1=0;}
          }
        
        case 1:      
          currSettingsMenu = currSettingsMenu +1;
          if (currSettingsMenu > 2) {currSettingsMenu=0;}
          break;
          
        case 2:  
          ChangingValue(iBtn);
          break;    
      }  
      break;     
  }
  
  if (currMode==0)
  {
    lcd.setCursor(0,iRow);
    if (iRow==0)
    {
      lcd.print(Actions[iAction0]);
    }
    else
    {
      lcd.print(Actions[iAction1]);
    }
  }
  
  if (currMode==1)
  {
    lcd.clear();
    lcd.print(SettingMenu[currSettingsMenu]);
  }
}


int GetButton()  // 0-nothing; 1-short button; 2-long; 3-left; 4-right
{
  int iResult=0;
    encoder_A = digitalRead(pin_A);     // считываем состояние выхода А энкодера      
    encoder_B = digitalRead(pin_B);     // считываем состояние выхода B энкодера
    encoder_Btn = !digitalRead(pin_C);     // считываем состояние выхода Btn энкодера и инвертируем т.к. 0-нажата 1-отпущена    
    if((!encoder_A) && (encoder_A_prev)){    // если состояние изменилось с положительного к нулю       
      if(encoder_B){         // выход В в полож. сост., значит вращение по часовой стрелке
         iResult = 3; 
         //lcd.scrollDisplayLeft();
      }          
      else {         // выход В в 0 сост., значит вращение против часовой стрелки        
         iResult = 4;
        // lcd.scrollDisplayLeft(); 
      }          
    }        
    encoder_A_prev = encoder_A;     // сохраняем значение А для следующего цикла
    
    if(encoder_Btn)         // нажата кнопка энкодера
    {
      //encoder_Btn_result = 0;
      counter_encoder_Btn = counter_encoder_Btn + 1;
      if (counter_encoder_Btn == 200)
      {
        iResult = 2;
      }
    }
    else
    {
      if(encoder_Btn_prev) // если кнопку отпустили
      {
        if (counter_encoder_Btn < 200)
        {
         iResult = 1; 
        }   
      }  
      counter_encoder_Btn = 0;
    }
    encoder_Btn_prev = encoder_Btn;
    
    return iResult;
}


void PrintDateTime(boolean isPrintDate)
{
  lcd.setCursor(0,0);    
  lcd.clear();
  if (currHr<10) {lcd.print("0");}
  lcd.print(currHr);
  lcd.print(":");
  if (currMin<10) {lcd.print("0");}
  lcd.print(currMin);    
 // lcd.print(":");
 // if (currSec<10) {lcd.print("0");}
 // lcd.print(currSec);

//  if (isPrintDate==false)
//  {
//    iSettingPosition=0;
//    SetSettingPosition();
//  } 
//    
  if (isPrintDate==true)
  {
    lcd.print("  ");
    if (currDay<10) {lcd.print("0");}  
    lcd.print(currDay);
    lcd.print("-");
    if (currMonth<10) {lcd.print("0");}
    lcd.print(currMonth);    
    lcd.print("-");
    lcd.print(currYear);
  }
}

void SetSettingPosition(char startCol, char endCol)
{
   lcd.setCursor(0,1);
   lcd.print("                ");
   lcd.setCursor(startCol, 1);
   for (char i=startCol; i <= endCol; i++)
   {
     lcd.print("-");
   }
   lcd.setCursor(startCol,0);
}

void ChangingPosition()
{
  int maxCurrSetPosition;
  currSetPosition = currSetPosition+1;
  switch(currParamSet) //0-CurrDateTime; 1-StartTime; 2-StopTime
  {  
    case 0:
      maxCurrSetPosition = 4;
      break;
    case 1:
      maxCurrSetPosition = 1;
      break;
    case 2:
      maxCurrSetPosition = 1;
      break;       
  }
  if (currSetPosition > maxCurrSetPosition)
  {
    currSetPosition = 0;
  }
  
  if (currParamSet>=0 && currParamSet<=2)
  {
    switch (currSetPosition)
    {
      case 0:
        SetSettingPosition(0, 1);  //h
        break;               
      case 1:
        SetSettingPosition(3, 4);  //m
        break;
      case 2:
        SetSettingPosition(6, 7);  //D
        break;
      case 3:
        SetSettingPosition(9, 10);  //M
        break;                               
      case 4:
        SetSettingPosition(12, 15);  //Y
        break;                               
     }
  }
}

void ChangingValue(char iButton) //iButton 3-; 4+
{
int minValue = 0;
int maxValue=0; 
int stepValue = 1; 
 
  if (currParamSet>=0 && currParamSet<=2)
  {
    switch (currSetPosition)
    {
      case 0:
        maxValue=24;  //h
        break;               
      case 1:
        maxValue=59;  //m
        break;
      case 2:
        minValue=1;
        maxValue=31;  //D
        break;
      case 3:
        minValue=1;
        maxValue=12;  //M
        break;                               
      case 4:
        minValue=13;
        maxValue=50;  //Y
        break;                               
     }
     
    if (iButton==4)
    {
      changeValue = changeValue+stepValue;
      if (changeValue>maxValue) {changeValue=minValue;}
    }
    else
    {
      changeValue = changeValue-stepValue;
      if (changeValue<minValue) {changeValue=maxValue;}
    }
  }
//  switch(currParamSet) //0-CurrDateTime; 1-StartTime; 2-StopTime
//  {
//    break;  
//    case 4:
//
//    switch(currSetPosition) //0-Hours; 1-Minutes; 2-Days; 3-Month; 4-Year
//    {
//    }
//  unsigned char currParamSet=0;  //0-CurrDateTime; 1-StartTime; 2-StopTime
///  , int minValue, int maxValue, int stepValue
}
