#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include <elapsedMillis.h>

#define RNF_CE_PIN    10
#define RNF_CSN_PIN   9
#define RNF_MOSI      11
#define RNF_MISO      12
#define RNF_SCK       13

#define BTN_PIN       2
#define SENSOR_PIN    3
#define LED_PIN       5

RF24 radio(RNF_CE_PIN, RNF_CSN_PIN);

const byte SENSOR_NUMBER = 1;
const byte CH_NRF = 15;
const uint64_t central_read_pipe = 0xE8E8F0F0E1LL;
const uint64_t central_write_pipe = 0xFFFFFFFF00ULL;
const unsigned long REFRESH_SENSOR_INTERVAL_S = 5;

int inCommand = 0;
int outCommand = 0;

elapsedMillis lastRefreshSensor_ms = REFRESH_SENSOR_INTERVAL_S * 1000;

void setup(void)
{

  // RF24
  radio.begin();                          // Включение модуля;
  delay(2);
  radio.setPayloadSize(8);
  radio.setChannel(CH_NRF);            // Установка канала вещания;
  radio.setRetries(10, 10);                // Установка интервала и количества попыток "дозвона" до приемника;
  radio.setDataRate(RF24_1MBPS);        // Установка скорости(RF24_250KBPS, RF24_1MBPS или RF24_2MBPS), RF24_250KBPS на nRF24L01 (без +) неработает.
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;
  //radio.setAutoAck(0);                    // Установка режима подтверждения приема;
  radio.openWritingPipe(central_read_pipe);     // Активация данных для отправки
  radio.openReadingPipe(1, central_write_pipe);   // Активация данных для чтения
  radio.startListening();

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
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
  if (highByte(inCommand) == SENSOR_NUMBER)

    switch lowByte(inCommand)
    {
      case 0:
        digitalWrite(LED_PIN, LOW);
        break;
      case 1:
        digitalWrite(LED_PIN, HIGH);
        break;
    }
}

void RefreshSensorData()
{
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    Serial.println("RefreshSensorData");

    byte sensor = digitalRead(SENSOR_PIN);
    outCommand = (SENSOR_NUMBER << 8) | sensor;
    WriteCommandNRF();
    lastRefreshSensor_ms = 0;
  }
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

void loop(void)
{
  ReadCommandNRF();
  RefreshSensorData();
}
