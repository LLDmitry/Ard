#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include <elapsedMillis.h>
#include "sav_button.h"

#define RNF_CE_PIN    10
#define RNF_CSN_PIN   9
#define RNF_MOSI      11
#define RNF_MISO      12
#define RNF_SCK       13

#define BTN1_PIN      2
#define BTN2_PIN      3
#define LED1_PIN      4
#define LED2_PIN      5
#define BZZ_PIN      6

RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);

const byte CH_NRF = 15;
const uint64_t central_read_pipe = 0xE8E8F0F0E1LL;
const uint64_t central_write_pipe = 0xFFFFFFFF00ULL;
const unsigned long ALARM_SENSOR_INTERVAL_S = 5;

int inCommand = 0;
int outCommand = 0;

bool alarm_sensor1 = false;
bool alarm_sensor2 = false;

SButton button1(BTN1_PIN, 100, 1000, 10000, 1000);
SButton button2(BTN2_PIN, 100, 1000, 10000, 1000);

elapsedMillis alarm_Sensor1_ms;
elapsedMillis alarm_Sensor2_ms;

void setup(void)
{

  // RF24
  radio.begin();                          // Включение модуля;
  delay(2);
  radio.setPayloadSize(8); //18
  radio.setChannel(CH_NRF);            // Установка канала вещания;
  radio.setRetries(10, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(central_write_pipe);     // Активация данных для отправки
  radio.openReadingPipe(1, central_read_pipe);   // Активация данных для чтения
  radio.startListening();

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(BZZ_PIN, OUTPUT);

  // Инициация кнопок
  button1.begin();
  button2.begin();
}

void ReadCommandNRF()
{
  if (radio.available())
  {
    Serial.println("radio.available!!");
    radio.read(&inCommand, sizeof(inCommand));
    delay(20);
    Serial.println("inCommand=" + inCommand);
    HandleCommand();
  }
}

void HandleCommand()
{
  if (lowByte(inCommand) > 0)

    switch highByte(inCommand)
    {
      case 1:
        alarm_sensor1 = true;
        digitalWrite(LED1_PIN, alarm_sensor1);
        alarm_Sensor1_ms = 0;
        break;
      case 2:
        alarm_sensor2 = true;
        digitalWrite(LED2_PIN, alarm_sensor2);
        alarm_Sensor2_ms = 0;
        break;
    }
  digitalWrite(BZZ_PIN, (alarm_sensor1 || alarm_sensor2));
}

void WriteCommandNRF()
{
  radio.stopListening();
  if (radio.write(&outCommand, sizeof(outCommand)))
  {
    Serial.println("Success Send");
  }
  radio.startListening();
}

void ActionBtn(byte btn, byte state) //btn: 1 or 2; state: 1-short, 0-long
{
  outCommand = (btn << 8) | state;
  WriteCommandNRF();
}

void AlarmControl()
{
  if (alarm_sensor1 && alarm_Sensor1_ms > ALARM_SENSOR_INTERVAL_S * 1000)
  {
    alarm_sensor1 = false;
    digitalWrite(LED1_PIN, alarm_sensor1);
    digitalWrite(BZZ_PIN, (alarm_sensor1 || alarm_sensor2));
  }

  if (alarm_sensor2 && alarm_Sensor2_ms > ALARM_SENSOR_INTERVAL_S * 1000)
  {
    alarm_sensor2 = false;
    digitalWrite(LED2_PIN, alarm_sensor2);
    digitalWrite(BZZ_PIN, (alarm_sensor1 || alarm_sensor2));
  }
}

void loop(void)
{
  ReadCommandNRF();
  AlarmControl();

  switch (button1.Loop()) {
    case SB_CLICK:
      //Serial.println("Press button 1");
      ActionBtn(1, 1);
      break;
    case SB_LONG_CLICK:
      //Serial.println("Long press button 1");
      ActionBtn(1, 0);
      break;
  }
  switch (button2.Loop()) {
    case SB_CLICK:
      //Serial.println("Press button 1");
      ActionBtn(2, 1);
      break;
    case SB_LONG_CLICK:
      //Serial.println("Long press button 1");
      ActionBtn(2, 0);
      break;
  }
}
