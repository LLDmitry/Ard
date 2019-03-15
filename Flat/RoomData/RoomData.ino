#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP085.h>
#include "SoftwareSerial.h"
#include "DHT.h"
//#include "TM1637.h"     // Библиотека дисплея
#include <elapsedMillis.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
//#include <ctype.h>


SoftwareSerial mySerial(A0, A1); // A0 - к TX сенсора, A1 - к RX

//#define CLK 12 // К этому пину подключаем CLK дисплея
//#define DIO 11 // К этому пину подключаем DIO дисплея

//#define BTTN_PIN 8
#define DHT_PIN 2
#define SHOW_STATISTIC_PIN 8

//#define LED_RED_PIN 4
//#define LED_GREEN_PIN 5
//#define LED_BLUE_PIN 6

#define DHTTYPE DHT22
#define OLED_RESET 4

Adafruit_BMP085 bmp;
DHT dht(DHT_PIN, DHTTYPE);

//TM1637 tm1637(CLK, DIO);
Adafruit_SSD1306 display(OLED_RESET); //o

const byte ShowModeDelay_s = 3; //5 sec
const uint32_t RefreshSensorInterval_s = 60;  //1 мин
const uint32_t SaveStatisticInterval_s = 1800; //30мин
//const int NormalLevelCO2 = 600;
//const int HighLevelCO2 = 1000;
const byte NUMBER_STATISTICS = 50;

const byte BASE_VAL_T_IN = 200;
const byte BASE_VAL_HM_IN = 10;
const byte BASE_VAL_CO2 = 40;
const byte BASE_VAL_P = 180;
const byte TOP_VAL_T_IN = 250;
const byte TOP_VAL_HM_IN = 80;
const byte TOP_VAL_CO2 = 130;
const byte TOP_VAL_P = 230;

const byte DIFF_VAL_T_IN = 2;
const byte DIFF_VAL_HM_IN = 2;
const byte DIFF_VAL_CO2 = 5;
const byte DIFF_VAL_P = 2;

const int EEPROM_ADR_INDEX_STATISTIC = 1023; //last address in eeprom for store indexStatistic


elapsedMillis showMode_ms;
elapsedMillis refreshSensors_ms = RefreshSensorInterval_s * 1000 + 1;
elapsedMillis saveStatistic_ms = (SaveStatisticInterval_s - RefreshSensorInterval_s * 1000 * 2) * 1000;

char* parameters[] = { "Temper In", "Humidity", "CO2", "Pressure" };

byte h_v = 0;
float t_v = 0.0f;
int p_v = 0;
int ppm_v = 0;

byte Mode = 0;
boolean showStatisticMode = false;

byte values[NUMBER_STATISTICS];
byte indexStatistic = 0;

void setup() {
  //  pinMode(LED_RED_PIN, OUTPUT);
  //  pinMode(LED_GREEN_PIN, OUTPUT);
  //  pinMode(LED_BLUE_PIN, OUTPUT);

  pinMode(SHOW_STATISTIC_PIN, INPUT_PULLUP);

  Serial.begin(9600);

  // Инициация дисплея
  //tm1637.init(D4056A);
  //tm1637.set(0);    // Устанавливаем яркость от 0 до 7

  mySerial.begin(9600);

  dht.begin();

  bmp.begin();

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done

  EEPROM.get(EEPROM_ADR_INDEX_STATISTIC, indexStatistic);
}

void AutoChangeShowMode()
{
  if (showMode_ms > ShowModeDelay_s * 1000)
  {
    //Serial.print("showMode_ms ");
    //Serial.println(millis());
    Mode += 1;
    if (Mode > 4) Mode = 1;

    showMode_ms = 0;
    DisplayData();
  }
}

void RefreshSensorData()
{
  byte cmd[9] = { 0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79 };
  char response[9];

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  if (refreshSensors_ms > RefreshSensorInterval_s * 1000)
  {
    //Serial.print("startrefreshSensors_ms ");
    //Serial.println(millis());
    h_v = dht.readHumidity();
    //Serial.println("readHumidity ");
    t_v = dht.readTemperature();
    //Serial.println("readTemperature ");

    p_v = 0.0075 * bmp.readPressure();
    //Serial.println("readPressure ");

    //delay(300);
    mySerial.write(cmd, 9);
    memset(response, 0, 9);
    mySerial.readBytes(response, 9);
    int responseHigh;
    int responseLow;
    responseHigh = (int)response[2];
    responseLow = (int)response[3];
    ppm_v = (256 * (responseHigh + (responseLow < 0 ? 1 : 0)) + responseLow);


    refreshSensors_ms = 0;

    //Serial.print("endRefreshSensors_ms ");
    //Serial.println(millis());
    //delay(1000);
    if (saveStatistic_ms > SaveStatisticInterval_s * 1000)
    {
      //Serial.println("SaveStatistic");
      SaveStatistic();
      saveStatistic_ms = 0;
    }
  }
}

void DisplayData()
{
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  //display.setTextColor(BLACK, WHITE); // 'inverted' text

  switch (Mode)
  {
    case 1: //T
      display.print(t_v);
      display.setTextSize(2);
      display.println(" c");
      break;
    case 2: //Влажн
      display.print((int)h_v);
      display.setTextSize(2);
      display.println(" %");
      break;
    case 3: //CO2
      //Serial.println((int)ppm_v);
      display.print((int)ppm_v);
      display.setTextSize(2);
      display.println(" co2");
      break;
    case 4: //P
      display.print((int)p_v);
      display.setTextSize(2);
      display.println(" mm");
      break;
  }

  //  display.setTextSize(1);
  //  display.setCursor(110, 18);
  //  display.println(indexStatistic);

  display.display();

  ShowStatistic();
}

void SaveStatistic()
{
  //temperatures[indexStatistic] = t_v * 10;  //197 вместо 19.7, чтобы хранить integer
  //humidities[indexStatistic] = h_v;
  //co2s[indexStatistic] = ppm_v / 10;
  //pressures[indexStatistic] = p_v - 550;

  //Serial.println("");
  //  Serial.println(NUMBER_STATISTICS * 2 + indexStatistic);
  //  Serial.println((int)(t_v * 10));
  //  Serial.println(h_v);
  //  Serial.println(ppm_v/10);
  //  Serial.println(p_v-550);

  EEPROM.put(indexStatistic, ConvertToByte(1, t_v));
  //Serial.println("EEPROM.put(indexStatistic, ConvertToByte(1, t_v));");
  //Serial.println(t_v);
  //Serial.println(ConvertToByte(1, t_v));

  EEPROM.put(NUMBER_STATISTICS + indexStatistic, ConvertToByte(2, h_v));
  EEPROM.put(NUMBER_STATISTICS * 2 + indexStatistic, ConvertToByte(3, ppm_v));
  EEPROM.put(NUMBER_STATISTICS * 3 + indexStatistic, ConvertToByte(4, p_v));
  //////Serial.println(millis());
  indexStatistic = (indexStatistic < NUMBER_STATISTICS - 1 ? indexStatistic + 1 : 0);  //доходим до NUMBER_STATISTICS и затем снова начинаем с 0
  EEPROM.put(EEPROM_ADR_INDEX_STATISTIC, indexStatistic);
}

byte ConvertToByte(byte mode, float val)
{
  switch (mode)
  {
    case 1: //T
      return ((byte)(val * 10));
      break;
    case 2: //Влажн
      return ((byte)val);
      break;
    case 3: //CO2
      return ((byte)(val / 10));
      break;
    case 4: //P
      return ((byte)(val - 550));
      break;
  }
}

void ShowStatistic()
{
  byte val = 0;
  byte minVal = 255;
  byte maxVal = 0;
  byte i;
  byte baseVal;
  byte topVal;

  ReadFromEEPROM();

  switch (Mode)
  {
    case 1: //T
      baseVal = BASE_VAL_T_IN;
      topVal = TOP_VAL_T_IN;
      break;
    case 2: //Влажн
      baseVal = BASE_VAL_HM_IN;
      topVal = TOP_VAL_HM_IN;
      break;
    case 3: //CO2
      baseVal = BASE_VAL_CO2;
      topVal = TOP_VAL_CO2;
      break;
    case 4: //P
      baseVal = BASE_VAL_P;
      topVal = TOP_VAL_P;
      break;
  }

  // Serial.println("");
  // Serial.println(parameters[mode - 1]);
  for (byte i = 0; i < NUMBER_STATISTICS; i++)
  {
    //val = GetVal(mode, i);
    val = values[i];


    if (val < minVal)
    {
      minVal = val;
    }
    if (val > maxVal)
    {
      maxVal = val;
    }
  }

  //  display.clearDisplay();
  //  display.setTextSize(1);
  //  display.setTextColor(WHITE);
  //  display.setCursor(0, 0);
  //  display.println(parameters[Mode - 1]);
  //  display.print("Min = ");
  //  display.println(Mode == 1 ? (float)minVal / 10 : (Mode == 3 ? (int)minVal * 10 : (Mode == 4 ? (int)minVal + 550 : minVal)));
  //  display.print("Max = ");
  //  display.println(Mode == 1 ? (float)maxVal / 10 : (Mode == 3 ? (int)maxVal * 10 : (Mode == 4 ? (int)maxVal + 550 : maxVal)));

  byte height = display.height() - 30;  //display.width()
  float k = (float)height / (float)(topVal - baseVal);
  byte bottom = display.height();
  //Serial.print("Mode= ");
  //Serial.println(Mode);
  //Serial.print("k= ");
  //Serial.println(k);
  // Serial.print("minVal= ");
  // Serial.println(minVal);
  // Serial.print("maxVal= ");
  // Serial.println(maxVal);
  //Serial.println("");

  byte val_prev;
  byte val_last;

  for (byte x = 0; x < NUMBER_STATISTICS; x++)
  {
    i = indexStatistic + x;
    if (i >= NUMBER_STATISTICS)
    {
      i = i - NUMBER_STATISTICS;
    }

    if (x == NUMBER_STATISTICS - 2)
    {
      val_prev = values[i];
    }
    if (x == NUMBER_STATISTICS - 1)
    {
      val_last = values[i];
    }
    //Serial.println(values[i]);
    //display.drawLine(x * 5, bottom, x * 5, bottom - k * (float)(values[i] - baseVal), WHITE);
    display.drawLine(x * 2, bottom, x * 2, bottom - k * (float)(values[i] - baseVal), WHITE);
    display.display();
    // Serial.print("Line= ");
    // Serial.println(bottom - ((float)GetVal(mode, i) - (float)minVal)/ k  - 3);
  }
  byte compareVal1 = saveStatistic_ms > SaveStatisticInterval_s / 2 ? val_last : val_prev;
  byte compareVal2;

  byte diffVal;
  switch (Mode)
  {
    case 1: //T
      diffVal = DIFF_VAL_T_IN;
      compareVal2 = ConvertToByte(Mode, t_v);
      break;
    case 2: //Влажн
      diffVal = DIFF_VAL_HM_IN;
      compareVal2 = ConvertToByte(Mode, h_v);
      break;
    case 3: //CO2
      diffVal = DIFF_VAL_CO2;
      compareVal2 = ConvertToByte(Mode, ppm_v);
      break;
    case 4: //P
      diffVal = DIFF_VAL_P;
      compareVal2 = ConvertToByte(Mode, p_v);
      break;
  }
  display.setTextSize(1);
  display.setCursor(110, 18);


  if (compareVal2 > compareVal1 && compareVal2 - compareVal1 > diffVal)
  {
    display.println("/");
    display.display();
  }
  if (compareVal2 < compareVal1 && compareVal1 - compareVal2 > diffVal)
  {
    display.println("\\");
    display.display();
  }

  delay(7000);
}

void ReadFromEEPROM()
{
  //  Serial.println("");
  //  Serial.print("ReadFromEEPROM. Mode =");
  //  Serial.println(Mode);
  for (byte i = 0; i < NUMBER_STATISTICS; i++)
  {
    EEPROM.get(NUMBER_STATISTICS * (Mode - 1) + i, values[i]);
    //  Serial.println(values[i]);
  }
}

void loop()
{
  RefreshSensorData();
  AutoChangeShowMode();
  //  if (!digitalRead(SHOW_STATISTIC_PIN))
  //  {
  //    showStatisticMode = true;
  //
  //    ReadFromEEPROM();
  //  }
  //  else
  //  {
  //    showStatisticMode = false;
  //  }
}

