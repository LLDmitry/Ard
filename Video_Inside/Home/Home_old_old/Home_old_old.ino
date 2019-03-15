// HOME
#include <RF24.h>
#include <RF24_config.h>
#include "sav_button.h" // Библиотека работы с кнопками
#include <elapsedMillis.h>
#include <SPI.h>
#include <stdint.h>

#include <Adafruit_SSD1306.h>

// GND VCC CE  CSN MOSI  MISO  SCK
//          9  10  11    12    13
//  http://robotclass.ru/tutorials/arduino-radio-nrf24l01/
//

#define HOME_BTTN_OPEN_DOOR_PIN 2
#define HOME_BTTN_INCREASE_SENSOR_LEVEL_PIN 4
#define HOME_BTTN_STOP_ALARM_PIN 5

#define CAMERA1_PIN 6     // Camera1 at home
#define CAMERA2_PIN 7     // Camera2 at home
#define ALERT_PIN 8     // LED ot Buzzer

#define OLED_RESET 3

//RNF
#define CE_PIN 9
#define CSN_PIN 10

int sensorValue = 0;  // variable to store the value coming from the sensor

const unsigned long TIME_CAMERA1 = 4;  // sec
const unsigned long TIME_CAMERA2 = 6;  // sec

const unsigned long TIME_ALARM = 30;  // sec
const unsigned long TIME_REQUEST_CAR_DATA = 30;  // sec
const unsigned long TIME_CHECK_LAST_CONNECT_CAR = 120; //sec

const int SENSOR_LEVEL1 = 100;
const int SENSOR_LEVEL2 = 50;
const int SENSOR_LEVEL3 = 10;

SButton btnOpenDoor(HOME_BTTN_OPEN_DOOR_PIN, 50, 700, 3000, 15000);
SButton btnIncreaseSensorLevel(HOME_BTTN_INCREASE_SENSOR_LEVEL_PIN, 50, 700, 3000, 15000);
SButton btnStopAlarm(HOME_BTTN_STOP_ALARM_PIN, 50, 700, 3000, 15000);

//TM1637 tm1637(CLK, DIO);
Adafruit_SSD1306 display(OLED_RESET);

elapsedMillis blinkOn_ms;
elapsedMillis camera1On_ms;
elapsedMillis camera2On_ms;
elapsedMillis alarmOn_ms;
elapsedMillis lastRequestCarData_ms;
elapsedMillis lastConnectCar_ms;
elapsedMillis lastAlarm_ms;


// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(CE_PIN, CSN_PIN);
// Single radio pipe address for the 2 nodes to communicate.  Значение "трубы" передатчика и приемника ОБЯЗАНЫ быть одинаковыми.
const uint64_t readingPipe = 0xE8E8F0F0AALL;
const uint64_t writingPipe = 0xE8E8F0F0ABLL;
const uint8_t channelNRF = 0x60;

enum ModeCamera { CAMERA_OFF, CAMERA1_ON, CAMERA2_ON, CAMERA1_NEW, CAMERA2_NEW } modeCamera;
enum ModeAlarm { ALARM_NEW, ALARM_ON, ALARM_OFF } modeAlarm;
enum ENoutNRFCommand { OUT_NO, OUT_OPEN_DOOR, OUT_CLOSE_DOOR, OUT_DECREASE_SENSOR_LEVEL, OUT_INCREASE_SENSOR_LEVEL, REQUEST_CAR_DATA } outNRFCommand = OUT_NO;  // 0 - Nothing to do; 1 - Camera1 On; 2 - Camera2 On; 2 - All Camera Off
enum EnINNRFCommand { IN_H_NO, CAMERA1_H_ON, CAMERA2_H_ON, CAMERA_H_OFF, SENSOR_ALARM } inNRFCommand = IN_H_NO;
enum EnAlarmLevel { ALARM_LEVEL1, ALARM_LEVEL2, ALARM_LEVEL3 } alarmLevel;

unsigned long lastChangeSettings_ms = 0;

bool alarmSystemOn = LOW;
bool bDoRefreshData = LOW;
int miNoConnectM = 0;
byte mMaxAlarmLevel = 0;

typedef struct {
  ENoutNRFCommand NRFCommand;
  byte SensorLevel;
  byte NumberAlarms;
  int TempCarX10;
  bool AlarmSystemOn;
  byte LastAlarmLevel;
} CarInfo;

CarInfo carInfo;

void setup()
{
  delay(2000);

  Serial.begin(9600);

  Serial.println("setup_1");

  pinMode(CAMERA1_PIN, OUTPUT);
  pinMode(CAMERA2_PIN, OUTPUT);
  //pinMode(HOME_ALARM_PIN, OUTPUT);
  pinMode(ALERT_PIN, OUTPUT);

  // Инициация кнопок
  btnOpenDoor.begin();
  btnIncreaseSensorLevel.begin();
  btnStopAlarm.begin();


  //display
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done

  //RF24
  //стоило переключить CS с 9 ножки в + и все бодренько заработало

  radio.begin();                          // Включение модуля;
  delay(2);
  //radio.enableAckPayload();       //+
  radio.setPayloadSize(8);
  radio.setChannel(channelNRF);            // Установка канала вещания;
  radio.setRetries(10, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(writingPipe);     // Активация данных для отправки
  radio.openReadingPipe(1, readingPipe);   // Активация данных для чтения
  radio.startListening();

  radio.printDetails();
}

void SendDoorControl(byte command)
{
  outNRFCommand = command == 1 ? OUT_OPEN_DOOR : OUT_CLOSE_DOOR;

  SendCommandNRF();
}

void ChangeSensorLevel(byte command)
{
  outNRFCommand = command == 1 ? OUT_INCREASE_SENSOR_LEVEL : OUT_DECREASE_SENSOR_LEVEL;

  SendCommandNRF();
}

void SendCommandNRF()
{
  if (outNRFCommand != OUT_NO)
  {
    Serial.print("SendCommandNRF: ");
    Serial.println(outNRFCommand);
    radio.startListening();
    radio.stopListening();

    if (radio.write(&outNRFCommand, sizeof(outNRFCommand)))
    {
      Serial.println("Success Send");
      if (outNRFCommand != REQUEST_CAR_DATA) {
        SentInform(HIGH);
      }
      lastConnectCar_ms = 0;

      // Now, continue listening
      radio.startListening();

      //Get response with car info
      // Wait here until we get a response, or timeout (250ms)
      unsigned long started_waiting_at = millis();
      bool timeout = false;
      while (!radio.available() && !timeout)
        if (millis() - started_waiting_at > 200)
          timeout = true;

      // Describe the results
      if (timeout)
        Serial.println("Failed, response timed out.");
      else
      {
        // Grab the response, compare, and send to debugging spew
        while (radio.available()) // While there is data ready
        {
          radio.read(&carInfo, sizeof(carInfo));
          delay(20);
        }
        bDoRefreshData = HIGH;
        if (carInfo.NRFCommand == outNRFCommand)
        {
          Serial.println("Got correct response");
          Serial.print("Car T: ");
          Serial.println(carInfo.TempCarX10);
        }
        else
          Serial.println("Wrong response");
      }
    }
    else
    {
      Serial.println("Failed Send");
      if (outNRFCommand != REQUEST_CAR_DATA) {
        SentInform(LOW);
      }
      bDoRefreshData = HIGH;
    }

    delay(100);
    radio.startListening();
  }
  outNRFCommand = OUT_NO;
}

void ReadCommandNRF()
{
  //Serial.println("ReadCommandNRF");
  inNRFCommand = IN_H_NO;
  if (radio.available())
  {
    Serial.println("radio.available!!");
    while (radio.available()) // While there is data ready
    {
      radio.read(&inNRFCommand, sizeof(inNRFCommand)); // по адресу переменной inNRFCommand функция записывает принятые данные
      delay(20);
      Serial.print("radio.available: ");
      Serial.println(inNRFCommand);
      lastConnectCar_ms = 0;
    }
    radio.startListening();                                // Now, resume listening so we catch the next packets.
  }
}

void ParseAndHandleInputCommand()
{
  switch (inNRFCommand) {
    case CAMERA1_H_ON:
      modeCamera = CAMERA1_NEW;
      break;
    case CAMERA2_H_ON:
      modeCamera = CAMERA2_NEW;
      break;
    case CAMERA_H_OFF:
      modeCamera = CAMERA_OFF;
      break;
    case SENSOR_ALARM:
      lastAlarm_ms = 0;
      modeAlarm = ALARM_NEW;
      break;
  }
}

void CameraControl()
{
  switch (modeCamera) {
    case CAMERA1_NEW:
      camera1On_ms = 0;
      modeCamera = CAMERA1_ON;
      break;
    case CAMERA2_NEW:
      camera2On_ms = 0;
      modeCamera = CAMERA2_ON;
      break;
    case CAMERA1_ON:
      if (camera1On_ms > (TIME_CAMERA1 * 1000))
      {
        modeCamera = CAMERA_OFF;
      }
      break;
    case CAMERA2_ON:
      if (camera2On_ms > (TIME_CAMERA2 * 1000))
      {
        modeCamera = CAMERA_OFF;
      }
      break;
  }

  digitalWrite(CAMERA1_PIN, modeCamera == CAMERA1_ON);
  digitalWrite(CAMERA2_PIN, modeCamera == CAMERA2_ON);
}


void AlarmControl()
{
  if (modeAlarm == ALARM_NEW)
  {
    alarmOn_ms = 0;
    modeAlarm = ALARM_ON;
    lastRequestCarData_ms = TIME_REQUEST_CAR_DATA * 1000; //request car data now
  }

  if (modeAlarm == ALARM_ON)
  {
    if (alarmOn_ms > (TIME_ALARM * 1000))
    {
      modeAlarm = ALARM_OFF;
    }
    else
      CarAlarm();
  }

}

void loop()
{
  switch (btnOpenDoor.Loop()) {
    case SB_CLICK:
      Serial.println("btnOpenDoor");
      SendDoorControl(1);
      break;
    case SB_LONG_CLICK:
      SendDoorControl(0);
      break;
  }
  switch (btnIncreaseSensorLevel.Loop()) {
    case SB_CLICK:
      ChangeSensorLevel(1);
      break;
    case SB_LONG_CLICK:
      ChangeSensorLevel(1);
      break;
  }
  switch (btnStopAlarm.Loop()) {
    case SB_CLICK:
      modeAlarm = ALARM_OFF;
      break;
  }

  //outNRFCommand = 2; //
  //SendCommandNRF(); //

  ReadCommandNRF();

  ParseAndHandleInputCommand();

  CameraControl();
  AlarmControl();
  RequestCarData();
  DisplayData();
  //delay(200);
}

void RequestCarData()
{
  if (lastRequestCarData_ms > (TIME_REQUEST_CAR_DATA * 1000))
  {
    Serial.println("RequestCarData");
    outNRFCommand = REQUEST_CAR_DATA;
    SendCommandNRF();
    lastRequestCarData_ms = 0;
  }
}

void DisplayData()
{
  if (bDoRefreshData)
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    //display.setTextColor(BLACK, WHITE); // 'inverted' text

    display.print("SL:  ");
    display.println((int)carInfo.SensorLevel);

    display.print("Alarms:  ");
    display.print((int)carInfo.NumberAlarms);
    if (carInfo.NumberAlarms > 0)
    {
      if (carInfo.LastAlarmLevel > mMaxAlarmLevel)
        mMaxAlarmLevel = carInfo.LastAlarmLevel;
      display.print(" : ");
      display.print((float)lastAlarm_ms / 1000.0 / 60.0, 1);
      display.print(" ");
      display.print((int)carInfo.LastAlarmLevel);
      display.print("/");
      display.println(mMaxAlarmLevel);

    }
    else
    {
      mMaxAlarmLevel = 0;
      display.println();
    }


    display.print("T:   ");
    display.print((float)carInfo.TempCarX10 / 10.0, 1);
    display.println(" C");

    if (carInfo.AlarmSystemOn && lastConnectCar_ms > TIME_CHECK_LAST_CONNECT_CAR * 1000)
    {
      display.print("Last: ");
      display.println((float)lastConnectCar_ms / 1000.0 / 60.0, 1);
    }
    else
      display.println("---");

    display.display();
    bDoRefreshData = false;
  }
}

void SentInform(bool success)
{
  digitalWrite(ALERT_PIN, HIGH);
  if (success)
    delay(200);
  else
    delay(1000);
  digitalWrite(ALERT_PIN, LOW);
}

void CarAlarm()
{
  digitalWrite(ALERT_PIN, HIGH);
  delay(1000);
  digitalWrite(ALERT_PIN, LOW);
  delay(500);
}
