//CheckBattery
#include "DHT.h"
#include "sav_button.h" // Библиотека работы с кнопками
#include <Adafruit_SSD1306.h>
#include <elapsedMillis.h>

#define U_PIN 1 // аналоговый вход измерения напряжения аккумулятора
#define A_PIN 2 // аналоговый вход измерения тока разряда/заряда

#define CHARGE_PIN 13     // управление зарядом
#define BTTN_PIN 4
#define OLED_RESET 4

char* parameters[] = { "High Volt", "Low Volt", "Time,m" };

enum BatteryType { INIT, AA1_2, AA1_5, LI3_7, CAR12 };
typedef enum BatteryType BatteryType_t;
BatteryType_t battery;
float arChargeVoltRange[] = { 1.5, 1.5, 4.2, 14.3, 99 };
float arDischargeVoltRange[] = { 1.1, 1.2, 3.2, 11.3, 0 };

float valuesU[50];
float valuesI[50];

SButton button1(BTTN_PIN, 50, 1000, 10000, 500);

elapsedMillis measure_ms;

// резисторы делителя напряжения
const float R1 = 15238;          // 150K
const float R2 = 9870;          // 10K
const float VCC = 1.12345;        //  внутреннее опорное напряжение, необходимо откалибровать индивидуально  (м.б. 1.0 -- 1.2)

const float PERIOD_MEASURE_MS = 10000;  // период измерений I и U, 10s
const float Vcc = 1.12345;  //  эту константу необходимо откалибровать индивидуально    1.0 -- 1.2

const int X_MAX = 256;  //размерность экрана по горизонтали
const int MAX_NUMBER_STATISTICS = 50; // максимальное кол-во показываемых статистик


int totalNumberMeasurement = 0;
byte ModeBattery = 0;     // 0 - init, 1 - discharge, 2 - charge
byte ModeControl = 0;     // 0 - init, 1-сводная информация, 2 - график U, 3 - график I

int DischargeOn = -1;  // -1 - initial, 0/1
int ChargeOn = -1;  // -1 - initial, 0/1


const int ShowModeDelay_ms = 5000; //msec

float VReal = 0;
float IReal = 0;

unsigned long lastChangeShowMode_ms = 0;
unsigned long lastDischargeOn_ms = 0;
unsigned long last5ModeOn_ms = 0;
unsigned long currTime_ms = 0;
unsigned long prevTimeCheck_ms = 0;


float TotalTok_mkACh = 0;
float dischargeVoltRange = 0;
float chargeVoltRange = 0;
unsigned int totalProcessTime_min = 0;

unsigned int baseValU;
unsigned int topValU;
unsigned int baseValI;
unsigned int topValI;

// Инициация дисплея
Adafruit_SSD1306 display(OLED_RESET); //o



void setup() {

  pinMode(CHARGE_PIN, OUTPUT);

  Serial.begin(9600);

  // Инициация кнопки
  button1.begin();

  // mySerial.begin(9600);

  analogReference(INTERNAL);  // DEFAULT: стандартное опорное напряжение 5 В (на платформах с напряжением питания 5 В) или 3.3 В (на платформах с напряжением питания 3.3 В) 
  //INTERNAL: встроенное опорное напряжение около 1.1 В на микроконтроллерах ATmega168 и ATmega328, и 2.56 В на ATmega8.
  //EXTERNAL : внешний источник опорного напряжения, подключенный к выводу AREF 

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done

  battery = INIT;

  //typedef enum BatteryType BatteryType_t ;
  //BatteryType_t battery
}

void AutoDetectBatteryType() // напряжение без нагрузки
{
  ReadVoltage();
  if (VReal < 1.45)
  {
    battery = AA1_2;
    dischargeVoltRange = arDischargeVoltRange[0];
    chargeVoltRange = arChargeVoltRange[0];
    //baseValU = 1.0;
    //topValU = 1.5;
  }
  else if (VReal < 2)
  {
    battery = AA1_5;
    dischargeVoltRange = arDischargeVoltRange[1];
    chargeVoltRange = arChargeVoltRange[1];
    //baseValU = 1.2;
    //topValU = 1.6;
  }
  else if (VReal < 5)
  {
    battery = LI3_7;
    dischargeVoltRange = arDischargeVoltRange[2];
    chargeVoltRange = arChargeVoltRange[2];
    /*  baseValU = 3.1;
    topValU = 4.5;*/
  }
  else
  {
    battery = CAR12;
    dischargeVoltRange = arDischargeVoltRange[3];
    chargeVoltRange = arChargeVoltRange[3];
    /*baseValU = 10.0;
    topValU = 14.5;*/
  }
}

void ButtonClick()
{
  if (battery == INIT)
  {
    AutoDetectBatteryType();
  }

  ModeControl = ModeControl + 1;
  if (ModeControl > 2)
  {
    ModeControl = 1;
  }

  DisplayData();
}

//void ButtonLongClick()
//{
//    if (ModeControl == 5)
//    {
//      tm1637.set(0);
//      ModeControl = 4;
//      AutoChangeShowMode(true);
//    }
//    else
//    {
//      if (TermoLevel < 999)
//      {
//        ModeControl = 5;
//        last5ModeOn_ms = millis();
//      }
//    }
//}


void ReadData()
{
  ReadVoltage();
  ReadCurrent();
  currTime_ms = millis();
  CalcTotalTok();
  prevTimeCheck_ms = currTime_ms;

  totalNumberMeasurement += 1;
  if (totalNumberMeasurement < MAX_NUMBER_STATISTICS)
  {
    valuesU[totalNumberMeasurement] = VReal;
    valuesI[totalNumberMeasurement] = IReal;
  }
  else  //delete every 2nd
  {
    for (int i = 0; MAX_NUMBER_STATISTICS - 1; i + 2)
    {
      valuesU[i / 2] = valuesU[i + 1];
      valuesI[i / 2] = valuesI[i + 1];
    }
    totalNumberMeasurement = MAX_NUMBER_STATISTICS / 2;
  }
}

void ReadVoltage()
{
  float curAnalogData = 0.0;
  for (int i = 0; i < 3; i++) {
    curAnalogData = curAnalogData + analogRead(U_PIN);
    Serial.println(curAnalogData);
    delay(10);
  }
  curAnalogData = curAnalogData / 3;
  VReal = (curAnalogData * VCC) / 1024.0 / (R2 / (R1 + R2));

  Serial.print("bat=");
  Serial.println(v_bat);

  if (VReal < baseValU)
    baseValU = VReal;
  if (IReal > topValU)
    topValU = VReal;
}

void ReadCurrent()
{
  float  curVal = 0.0;
  curVal = analogRead(A_PIN); // читаем значение I на аналоговом входе
  delay(10);
  curVal = curVal + analogRead(A_PIN); // читаем значение на аналоговом входе
  curVal = curVal / 2;
  IReal = 0.0264 * (curVal - 512.0)
  //IReal = (curVal - 512) * 1000 / 20;

  if (IReal < baseValI)
    baseValI = IReal;
  if (IReal > topValI)
    topValI = IReal;
}

bool CheckDischargeStop()
{
  bool bResult = false;
  bResult = (VReal < dischargeVoltRange);
  return bResult;
}


void CalcTotalTok()
{
  TotalTok_mkACh = TotalTok_mkACh + (IReal * (currTime_ms - prevTimeCheck_ms)) / 3.6;
}

void DischargeControl()
{
  if (DischargeOn == -1) //включить при старте, без доп проверок
  {
    DischargeOn = 1;
    lastDischargeOn_ms = currTime_ms;
  }
  else if (DischargeOn == 1) // проверить текущее напряжение и выключить
  {
    totalProcessTime_min = (currTime_ms - lastDischargeOn_ms) / 1000 / 60;
    if (CheckDischargeStop())
    {
      DischargeOn = 0;
    }
  }
  digitalWrite(CHARGE_PIN, DischargeOn);
}

void DisplayData()
{
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  //display.setTextColor(BLACK, WHITE); // 'inverted' text

  switch (ModeControl)
  {
  case 0: // INIT
    display.setTextSize(3);
    display.println("Press to start");
  case 1: // сводная инф-я
    display.setTextSize(3);
    display.print(VReal);
    if (DischargeOn == 1)
      display.println(" \\");
    else
      display.println(" = ");

    display.setTextSize(2);
    display.println(" V");

    display.setTextSize(2);
    display.print(topValU);
    display.print(" / ");
    display.println(baseValU);

    display.setTextSize(2);
    display.print(IReal);
    display.setTextSize(2);
    display.println(" A");

    display.setTextSize(2);
    display.print(totalProcessTime_min);
    display.setTextSize(2);
    display.println(" m");

    display.setTextSize(2);
    display.print(TotalTok_mkACh / 1000);
    display.setTextSize(2);
    display.println(" mAH");

    break;

  case 2://график напряжения
    //case 3://график тока
    ShowStatistic();
    break;
  }

  display.display();
}

void ShowStatistic()
{
  byte val = 0;
  byte minVal = 255;
  byte maxVal = 0;
  byte i;
  byte baseVal;
  byte topVal;

  switch (ModeControl)
  {
  case 2: //Напряжение
    baseVal = baseValU;
    topVal = topValU;
    break;
    //case 3: //Ток
    //  baseVal = baseValI;
    //  topVal = topValI;
    //  break;
  }

  byte height = display.height() - 30;  //display.width()
  float k = (float)height / (float)(topVal - baseVal);
  byte bottom = display.height();


  for (byte x = 0; x < totalNumberMeasurement; x++)
  {
    //    i = indexStatistic + x;
    //    if (i >= NUMBER_STATISTICS)
    //    {
    //      i = i - NUMBER_STATISTICS;
    //    }
    //
    //    if (x == NUMBER_STATISTICS - 2)
    //    {
    //      val_prev = valuesU[i];
    //    }
    //    if (x == NUMBER_STATISTICS - 1)
    //    {
    //      val_last = valuesU[i];
    //    }
    if (ModeControl == 2)
    {
      //Serial.println(valuesU[i]);
      display.drawLine(x * 5, bottom, x * 5, bottom - k * (float)(valuesU[i] - baseVal), WHITE);
      display.drawLine(x * 2, bottom, x * 2, bottom - k * (float)(valuesU[i] - baseVal), WHITE);
    }
    display.display();
    //Serial.print("Line = ");
    //if (ModeControl == 2)
    //  Serial.println(bottom - ((float)valuesU[i] - (float)minVal) / k - 3);
    //if (ModeControl == 3)
    //  Serial.println(bottom - ((float)valuesI[i] - (float)minVal)/ k  - 3);
  }
  display.setTextSize(1);
  display.setCursor(110, 18);

}

void loop()
{
  switch (button1.Loop()) {
  case SB_CLICK:
    ButtonClick();
    break;
  case SB_LONG_CLICK:
    //ButtonLongClick();
    break;
  case SB_AUTO_CLICK:
    ButtonClick();
    break;
  }

  if (measure_ms > PERIOD_MEASURE_MS)
  {
    ReadData();
    DischargeControl();
    DisplayData();
    measure_ms = 0;
  }
}
