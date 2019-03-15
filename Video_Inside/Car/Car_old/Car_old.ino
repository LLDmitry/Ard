//CAR

#include <RF24.h>
#include <RF24_config.h>
#include "sav_button.h" // Библиотека работы с кнопками
#include <elapsedMillis.h>
#include <SPI.h>
#include <stdint.h>

#include <OneWire.h>  //18b20
#include <DallasTemperature.h>  //18b20

// GND VCC CE  CSN MOSI  MISO  SCK
//          9  10  11    12    13
//  http://robotclass.ru/tutorials/arduino-radio-nrf24l01/
//

#define CAR_OPEN_DOOR_PIN 6
#define CAR_CLOSE_DOOR_PIN 2
#define CAR_BTTN_PIN 3
#define ALARM_STATUS_PIN 4
#define MONITOR_PIN 5     // Monitor at car
#define SENT_INFORM_PIN 8     // LED or Buzzer


#define ONE_WIRE_PIN 7   // DS18b20

#define PIEZO_SENSOR_PIN A0

//RNF
#define CE_PIN 9
#define CSN_PIN 10

const unsigned long TIME_DOOR_OPEN = 5;  // sec for opening door, and will auto-close door again
const unsigned long TIME_CAMERA1 = 10;  // sec
const unsigned long TIME_CAMERA2 = 8;  // sec
const unsigned long TIME_NEXT_ALARM_CHECK = 10;  // sec
const unsigned long TIME_NO_LISTENING_AFTER_SEND = 300;  // msec pause after sending (за это время Home странслирует в HomeDisplay инф-ю)

const unsigned long RefreshSensorInterval_s = 10;   // DS18b20

const int SENSOR_LEVEL1 = 1000;
const int SENSOR_LEVEL2 = 500;
const int SENSOR_LEVEL3 = 50;

SButton buttonInCar(CAR_BTTN_PIN, 50, 700, 3000, 15000);

// DS18b20
OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress tempDeviceAddress;

elapsedMillis blinkOn_ms;
elapsedMillis camera1On_ms;
elapsedMillis camera2On_ms;
elapsedMillis lastRefreshSensor_ms;  // DS18b20
elapsedMillis lastAlarm_ms;
elapsedMillis lastSend_ms;

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
RF24 radio(CE_PIN, CSN_PIN);
// Single radio pipe address for the 2 nodes to communicate.  Значение "трубы" передатчика и приемника ОБЯЗАНЫ быть одинаковыми.
const uint64_t readingPipe = 0xE8E8F0F0ADLL;
const uint64_t writingPipe = 0xE8E8F0F0AALL;  // or 0xE8E8F0F0ABLL  for B
const uint8_t channelNRF = 0x60;
const char My_Car = 'A';  // or 'B'

enum modeCamera { OFF, CAMERA1, CAMERA2 } modeCamera;
enum ModeDoor { DOOR_OFF, DOOR_CLOSE, DOOR_OPEN } modeDoor;
enum EnSensorLevel { LEVEL1, LEVEL2, LEVEL3 } sensorLevel = LEVEL2;

enum EnOutNRFCommand { OUT_NO, OUT_CAMERA1_ON, OUT_CAMERA2_ON, OUT_CAMERA_OFF, OUT_ALARM } outNRFCommand = OUT_NO;  // 0 - Nothing to do; 1 - Camera1 On; 2 - Camera2 On; 2 - All Camera Off
enum EnInNRFCommand { IN_C_NO, IN_OPEN_DOOR, IN_CLOSE_DOOR, IN_DECREASE_SENSOR_LEVEL, IN_INCREASE_SENSOR_LEVEL, REQUEST_CAR_DATA };

bool alarmSystemOn = LOW;
int sensorValue = 0;  // variable to store the value coming from the sensor
float tempCar;
int numberAlarms = 0;

typedef struct {
  EnInNRFCommand Command;
  byte Car;
} InCommand;
InCommand inNRFCommand;

typedef struct {
  EnInNRFCommand Command;
  byte SensorLevel;
  byte NumberAlarms;
  int TempCarX10;
  bool AlarmSystemOn;
} CarInfo;

CarInfo carInfo;

void setup()
{
  delay(2000);

  Serial.begin(9600);

  pinMode(ALARM_STATUS_PIN, INPUT_PULLUP);
  pinMode(CAR_OPEN_DOOR_PIN, OUTPUT);
  pinMode(CAR_CLOSE_DOOR_PIN, OUTPUT);
  pinMode(MONITOR_PIN, OUTPUT);
  pinMode(SENT_INFORM_PIN, OUTPUT);

  // Инициация кнопки
  buttonInCar.begin();

  //RF24
  radio.begin();                          // Включение модуля;
  delay(2);
  // radio.enableAckPayload(); //+
  radio.setPayloadSize(8);
  radio.setChannel(channelNRF);            // Установка канала вещания;
  radio.setRetries(10, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(writingPipe);     // Активация данных для отправки
  radio.openReadingPipe(1, readingPipe);   // Активация данных для чтения
  radio.startListening();

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, 10);

  lastRefreshSensor_ms = RefreshSensorInterval_s - 3 * 1000;
}

void RefreshSensorData()
{
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  if (lastRefreshSensor_ms > RefreshSensorInterval_s * 1000)
  {
    sensors.requestTemperatures();
    //float realTemper = sensors.getTempCByIndex(0);
    tempCar = sensors.getTempC(tempDeviceAddress);
    lastRefreshSensor_ms = 0;

    Serial.print("T= ");
    Serial.println(tempCar);
  }
}

void ButtonClick()
{
  Serial.print("modeCamera: ");
  Serial.println(modeCamera);

  if (modeCamera == CAMERA1)
  {
    modeCamera = CAMERA2;
  }
  else
  {
    modeCamera = CAMERA1;
  }

  MonitorControl(HIGH);
}

void ButtonLongClick() // выкл камеры и монитор
{
  modeCamera = OFF;
  MonitorControl(HIGH);
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
      lastSend_ms = 0;
      if (outNRFCommand != OUT_ALARM) {
        SentInform(HIGH);
      }
    }
    else
    {
      Serial.println("Failed Send");
      if (outNRFCommand != OUT_ALARM) {
        SentInform(LOW);
      }
    }
  }
  //delay(100);
  outNRFCommand = OUT_NO;
}

void ReadCommandNRF()
{
  inNRFCommand.Command = IN_C_NO;

  if (lastSend_ms > TIME_NO_LISTENING_AFTER_SEND)
  {
    radio.startListening();
  }

  if (radio.available())
  {
    Serial.println(millis());
    Serial.println("radio.available check");
    while (radio.available())                                    // While there is data ready
    {
      radio.read(&inNRFCommand, sizeof(inNRFCommand)); // по адресу переменной inNRFCommand функция записывает принятые данные
      delay(20);
      Serial.print("inNRFCommand: ");
      Serial.println(inNRFCommand.Car);
      Serial.println(inNRFCommand.Command);
    }
    if (inNRFCommand.Command != IN_C_NO && inNRFCommand.Car == My_Car)
    {
      radio.stopListening();  // First, stop listening so we can talk
      carInfo.Command = inNRFCommand.Command;
      carInfo.SensorLevel = sensorLevel;
      carInfo.NumberAlarms = numberAlarms;
      carInfo.TempCarX10 = tempCar * 10;
      carInfo.AlarmSystemOn = alarmSystemOn;

      Serial.print("send back carInfo.TempCar: ");
      Serial.println(carInfo.TempCarX10);

      if (radio.write(&carInfo, sizeof(carInfo)))           // Send the response
        Serial.println("send back writen 1");
      else
        Serial.println("send back writen 0");
    }
    radio.startListening();                                // Now, resume listening so we catch the next packets.
  }
}

void ParseAndHandleInputCommand()
{
  switch (inNRFCommand.Command) {
  case IN_DECREASE_SENSOR_LEVEL:
    AlarmLevelControl(-1);
    break;
  case IN_INCREASE_SENSOR_LEVEL:
    AlarmLevelControl(1);
    break;
  case IN_OPEN_DOOR:
    DoorControl(HIGH);
    break;
  case IN_CLOSE_DOOR:
    DoorControl(LOW);
    break;
  }
}

void SentInform(bool success)
{
  digitalWrite(SENT_INFORM_PIN, HIGH);
  if (success)
    delay(200);
  else
    delay(1000);
  digitalWrite(SENT_INFORM_PIN, LOW);
}

void MonitorControl(bool manualControl)
{
  if (manualControl)
  {
    switch (modeCamera) {
    case CAMERA1:
      camera1On_ms = 0;
      outNRFCommand = OUT_CAMERA1_ON;
      break;
    case CAMERA2:
      camera2On_ms = 0;
      outNRFCommand = OUT_CAMERA2_ON;
      break;
    case OFF:
      outNRFCommand = OUT_CAMERA_OFF;
      break;
    }
    SendCommandNRF();
  }
  else
  {
    if (modeCamera != OFF)
    {
      if (modeCamera == CAMERA1)
      {
        if (camera1On_ms > (TIME_CAMERA1 * 1000))
        {
          modeCamera = OFF;
          outNRFCommand = OUT_CAMERA_OFF;
          SendCommandNRF();
        }
      }
      else  //CAMERA2
      {
        if (camera2On_ms > (TIME_CAMERA2 * 1000))
        {
          modeCamera = OFF;
          outNRFCommand = OUT_CAMERA_OFF;
          SendCommandNRF();
        }
      }
    }
  }
  digitalWrite(MONITOR_PIN, modeCamera != OFF);
}

void DoorControl(bool modeDoor)
{
  if (modeDoor)
  {
    digitalWrite(CAR_OPEN_DOOR_PIN, HIGH);
    delay(100);
    digitalWrite(CAR_OPEN_DOOR_PIN, LOW);

    delay(TIME_DOOR_OPEN * 1000);

    digitalWrite(CAR_CLOSE_DOOR_PIN, HIGH);
    delay(100);
    digitalWrite(CAR_CLOSE_DOOR_PIN, LOW);
  }
  else
  {
    digitalWrite(CAR_CLOSE_DOOR_PIN, HIGH);
    delay(100);
    digitalWrite(CAR_CLOSE_DOOR_PIN, LOW);
  }
}

bool AlarmLevelControl(byte command)  //true если поменяли sensorLevel
{
  bool bResult = false;
  if (alarmSystemOn)
  {
    sensorLevel = sensorLevel + (command == 1 ? 1 : -1);
    if (sensorLevel > LEVEL3)
      sensorLevel = LEVEL3;
    else
      bResult = true;

    if (sensorLevel < LEVEL1)
      sensorLevel = LEVEL1;
    else
      bResult = true;
  }
  return bResult;
}

void CheckAlarmStatus()
{
  alarmSystemOn = !digitalRead(ALARM_STATUS_PIN);
  alarmSystemOn = HIGH; //a
  if (!alarmSystemOn)  {
    numberAlarms = 0;
  }
}

void SensorCheck()
{
  if (lastAlarm_ms > TIME_NEXT_ALARM_CHECK * 1000)
  {
    int compareLevel = 0;
    // read the value from the sensor:
    sensorValue = analogRead(PIEZO_SENSOR_PIN);

    if (sensorValue > SENSOR_LEVEL3)
    {
      Serial.print("sensorValue: ");
      Serial.println(sensorValue);

      switch (sensorLevel) {
      case LEVEL1:
        compareLevel = SENSOR_LEVEL1;
        break;
      case LEVEL2:
        compareLevel = SENSOR_LEVEL2;
        break;
      case LEVEL3:
        compareLevel = SENSOR_LEVEL3;
        break;
      }
      if (sensorValue > compareLevel)
      {
        lastAlarm_ms = 0;
        Serial.println("OUT_ALARM");
        outNRFCommand = OUT_ALARM;
        numberAlarms += 1;
        SendCommandNRF();
      }
    }
  }
}



void loop()
{
  switch (buttonInCar.Loop()) {
  case SB_CLICK:
    Serial.println("ButtonClick");
    ButtonClick();
    break;
  case SB_LONG_CLICK:
    ButtonLongClick();
    break;
  }

  ReadCommandNRF();
  ParseAndHandleInputCommand();

  MonitorControl(LOW);
  RefreshSensorData();
  CheckAlarmStatus();
  if (alarmSystemOn)
  {
    SensorCheck();
  }
  delay(200);
}
