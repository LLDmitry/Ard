//StopVoda+NRF
#include <NrfCommands.h>  // C:\Program Files (x86)\Arduino\libraries\NrfCommands
#include "sav_button.h" // Библиотека работы с кнопками
#include <elapsedMillis.h>
//#include "DHT.h"
#include <Arduino.h>
#include <avr/wdt.h>
#include <RF24.h>
#include <RF24_config.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//#define DHTTYPE DHT22


#define DHT_PIN       6
#define BTN_PIN       3
#define SOLENOID_PIN  8
#define BUZZ_PIN      4
#define VODA_PIN      2     //300Kom +5v
#define ONE_WIRE_PIN  5    // DS18b20

//RNF  SPI bus plus pins 9 & 10  9,10 для Уно или 9, 53 для Меги
#define RNF_CE_PIN    10
#define RNF_CSN_PIN   9
#define RNF_MOSI      11
#define RNF_MISO      12
#define RNF_SCK       13

const byte ROOM_NUMBER = ROOM_VANNA2; //ROOM_VANNA1
const boolean SOLENOID_NORMAL_OPENED = false; //true

const unsigned long MANUAL_OPEN_DURATION_SEC = 10800; //3 hours
const unsigned long MANUAL_CLOSE_DURATION_SEC = 10; //10s
const unsigned long CHECK_VODA_PERIOD_SEC = 3;
const unsigned long ALARM_INTERVAL_SEC = 2;
const uint32_t REFRESH_SENSOR_INTERVAL_S = 60;  //1 мин
const uint32_t READ_COMMAND_NRF_INTERVAL_S = 1;

RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);

SButton btn(BTN_PIN, 50, 1000, 5000, 15000);

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress InnTempDeviceAddress;
DeviceAddress ColdVodaTempDeviceAddress;
DeviceAddress HotVodaTempDeviceAddress;

elapsedMillis CheckVoda_ms;
elapsedMillis manualOpenTime_ms;
elapsedMillis manualCloseTime_ms;
elapsedMillis alarmInterval_ms;
elapsedMillis lastRefreshSensor_ms = REFRESH_SENSOR_INTERVAL_S * 1000 + 1;
elapsedMillis readCommandNRF_ms = 0;

//byte h_v = 0;
float t_inn = 0.0f;
float t_cold = 0.0f;
float t_hot = 0.0f;

volatile float AvgVoda = 0;
volatile int Voda = 0;
volatile int Voda1 = 0;
volatile int Voda2 = 0;
volatile boolean closeVoda = false;
volatile int vodaMode = 0;  //0-open, 1-closed, 2-open temporary, 3-close temporary, for test
volatile boolean isAlarm = false;

NRFResponse nrfResponse;
NRFRequest nrfRequest;

//DHT dht(DHT_PIN, DHTTYPE);

void setup()
{
  // immediately disable watchdog timer so set will not get interrupted
  wdt_disable();

  //pinMode(VODA_PIN, INPUT_PULLUP);
  pinMode(VODA_PIN, INPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  Serial.begin(9600);   // Debugging only
  Serial.println("setup");

  // Инициация кнопок
  btn.begin();

  // RF24
  radio.begin();                          // Включение модуля;
  delay(2);
  radio.enableAckPayload();                     // Allow optional ack payloads
  //radio.enableDynamicPayloads();                // Ack payloads are dynamic payloads

  radio.setPayloadSize(32); //18
  radio.setChannel(ArRoomsChannelsNRF[ROOM_NUMBER]);            // Установка канала вещания;
  radio.setRetries(0, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(CentralReadingPipe);     // Активация данных для отправки
  radio.openReadingPipe(1, RoomReadingPipe);   // Активация данных для чтения
  radio.startListening();

  radio.printDetails();

  sensors.begin();

  sensors.getAddress(HotVodaTempDeviceAddress, 0);
  sensors.getAddress(ColdVodaTempDeviceAddress, 1);
  sensors.getAddress(InnTempDeviceAddress, 2);

  sensors.setResolution(InnTempDeviceAddress, 12);
  sensors.setResolution(ColdVodaTempDeviceAddress, 10);
  sensors.setResolution(HotVodaTempDeviceAddress, 10);

  switchSolenoid(); // для открытия NormalOpen клапана

  // enable the watchdog timer. There are a finite number of timeouts allowed (see wdt.h).
  // Notes I have seen say it is unwise to go below 250ms as you may get the WDT stuck in a
  // loop rebooting.
  // The timeouts I'm most likely to use are:
  // WDTO_1S
  // WDTO_2S
  // WDTO_4S
  // WDTO_8S
  wdt_enable(WDTO_8S);
}

void PrepareCommandNRF()
{
  nrfResponse.Command = RSP_INFO;
  nrfResponse.roomNumber = ROOM_NUMBER;
  nrfResponse.alarmType = (vodaMode == 1 ? ALR_VODA : ALR_NO);
  nrfResponse.tInn = t_inn;
  nrfResponse.t1 = t_cold;
  nrfResponse.t2 = t_hot;
  //nrfResponse.h = h_v;

  radio.flush_tx();
  radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
}

void RefreshSensorData()
{
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    Serial.println("RefreshSensorData");
    //    h_v = dht.readHumidity();
    //    t_inn = dht.readTemperature();
    //t_inn = millis() / 1000;

    sensors.requestTemperatures();
    t_inn = sensors.getTempC(InnTempDeviceAddress);
    t_cold = sensors.getTempC(ColdVodaTempDeviceAddress);
    t_hot = sensors.getTempC(HotVodaTempDeviceAddress);

    Serial.println(t_hot);

    PrepareCommandNRF();
    lastRefreshSensor_ms = 0;
  }
}

void CheckVoda()
{
  if (CheckVoda_ms > CHECK_VODA_PERIOD_SEC * 1000)
  {
    calcAvgVoda();
    if (vodaMode == 0 && AvgVoda == 1)
    {
      closeVoda = true;
      alarmInterval_ms = 0;
      vodaMode = 1;
      switchSolenoid();
    }
    CheckVoda_ms = 0;
  }
}

void VodaControl(int typeControl) //0 - auto check, 1-short click, 2-long click
{
  if (typeControl == 0)
  {
    if ((vodaMode == 2 && manualOpenTime_ms > MANUAL_OPEN_DURATION_SEC * 1000) || (vodaMode == 3 && manualCloseTime_ms > MANUAL_CLOSE_DURATION_SEC * 1000))
    {
      Serial.println(manualOpenTime_ms); //Reset
      vodaMode = 0;
      closeVoda = false;
      CheckVoda();
      switchSolenoid();
    }
  }
  else //manual
  {
    if (typeControl == 1)
    {
      if (vodaMode == 0)
      {
        vodaMode = 3; //temporary close voda (testing)
        manualCloseTime_ms = 0;
        closeVoda = true;
        alarmInterval_ms = 0;
        switchSolenoid();
      }
      else if (vodaMode == 1)
      {
        vodaMode = 2; //temporary open voda
        manualOpenTime_ms = 0;
        closeVoda = false;
        switchSolenoid();
      }
    }
    else // 2-long click, reset
    {
      vodaMode = 0;
      closeVoda = false;
      Voda1 = 0;
      Voda2 = 0;
      AvgVoda = 0;
      switchSolenoid();
    }
  }
}

void switchSolenoid()
{
  digitalWrite(SOLENOID_PIN, SOLENOID_NORMAL_OPENED ? closeVoda : !closeVoda);
  if (closeVoda)
  {
    PrepareCommandNRF();
  }
}

//average voda for 3 last measures
void calcAvgVoda()
{
  Voda = !digitalRead(VODA_PIN);
  //  if (Voda == 1 && AvgVoda == 0)
  //  {
  //    digitalWrite(BUZZ_PIN, HIGH);
  //    delay(100);
  //    digitalWrite(BUZZ_PIN, LOW);
  //  }
  AvgVoda = (Voda2 + Voda1 + Voda) / 3;
  Voda2 = Voda1;
  Voda1 = Voda;
  Serial.print("Voda: ");
  Serial.println(Voda);
  Serial.print("VodaMode: ");
  Serial.println(vodaMode);
}

void AlarmControl()
{
  if (closeVoda)
  {
    if (alarmInterval_ms > ALARM_INTERVAL_SEC * 1000)
    {
      isAlarm = !isAlarm;
      alarmInterval_ms = 0;
    }
  }
  else
  {
    isAlarm = false;
  }
  digitalWrite(BUZZ_PIN, isAlarm);
}

//Get T out, Pressure and Command
//void ReadCommandNRF()
//{
//  if (radio.available())
//  {
//    Serial.println("radio.available!!");
//    //radio.writeAckPayload(1, &nrfResponse, sizeof(nrfResponse));          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
//    while (radio.available()) // While there is data ready
//    {
//      radio.read(&nrfRequest, sizeof(nrfRequest)); // по адресу переменной nrfRequest функция записывает принятые данные
//      delay(20);
//      Serial.println("radio.available: ");
//      Serial.println(nrfRequest.tOut);
//    }
//    radio.startListening();   // Now, resume listening so we catch the next packets.
//    nrfResponse.Command == RSP_NO;
//    nrfResponse.ventSpeed = 0;
//  }
//}

//Get Command
void ReadCommandNRF()
{
  if (readCommandNRF_ms > READ_COMMAND_NRF_INTERVAL_S * 1000)
  {
    bool done = false;
    if (radio.available())
    {
      int cntAvl = 0;
      Serial.println("radio.available!!");
      while (!done) {
        done = radio.read(&nrfRequest, sizeof(nrfRequest));
        delay(20);
        Serial.println("radio.available: ");
        Serial.println(nrfRequest.tOut);

        cntAvl++;
        if (cntAvl > 10)
        {
          Serial.println("powerDown");
          _delay_ms(20);
          radio.powerDown();
          radio.powerUp();
        }
//        if (nrfRequest.Command != RQ_NO) {
//          HandleInputNrfCommand(); //closeVoda?
//        };

        nrfResponse.Command == RSP_NO;
        nrfResponse.tOut = 99.9;
      }
    }
    readCommandNRF_ms = 0;
  }
}

void loop()
{
  switch (btn.Loop()) {
    case SB_CLICK:
      Serial.println("btnShort"); //temporary open voda if was closed, or close if was opened for testing
      VodaControl(1);
      break;
    case SB_LONG_CLICK:
      Serial.println("btnLong"); //Reset
      VodaControl(2);
      break;
  }
  RefreshSensorData();
  VodaControl(0);
  CheckVoda();
  AlarmControl();
  //PrepareCommandNRF();
  ReadCommandNRF();
  wdt_reset();
}
