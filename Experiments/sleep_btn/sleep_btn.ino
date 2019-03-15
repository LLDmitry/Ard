//sleep_btn
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>

#define BUTTON_PIN    2
#define LED_PIN       13

void setup()
{
  Serial.begin(9600);
  Serial.println("Setup start");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(13, HIGH);
  _delay_ms(100); //delay
  digitalWrite(LED_PIN, LOW);
  _delay_ms(100);
  digitalWrite(13, HIGH);
  _delay_ms(100);
  digitalWrite(LED_PIN, LOW);
  _delay_ms(100);
  digitalWrite(13, HIGH);
  _delay_ms(100);
  digitalWrite(LED_PIN, LOW);
  _delay_ms(100);
  digitalWrite(13, HIGH);
  _delay_ms(100);
  digitalWrite(LED_PIN, LOW);
  _delay_ms(100);

  Serial.println("Setup done");
}

void loop()
{
  // светим светодиодом 5 секунд
  digitalWrite(13, HIGH);
  _delay_ms(5000);
  digitalWrite(LED_PIN, LOW);
  // и засыпаем
  PrepareSleep();

  Serial.println("Sleep_start");
  _delay_ms(20);

  GoSleep();

  // отсюда выполнения программы продолжится при пробуждении МК
  _delay_ms(100);
  Serial.println("Sleep_stop");
  _delay_ms(20);

  wdt_enable(WDTO_4S);
  wdt_reset();
}

void PrepareSleep()
{
  // все пины на выход и в низкий уровень
  for (byte i = 0; i <= A7; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  // установливаем на пине с кнопкой подтяжку к VCC
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // устанавливаем обработчик прерывания INT0
  attachInterrupt(0, WakeUp, FALLING);

  wdt_disable();
}

void GoSleep() {
  // отключаем АЦП
  ADCSRA = 0;
  // отключаем всю периферию
  power_all_disable();
  // устанавливаем режим сна - самый глубокий, здоровый сон :)
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  // разрешаем спящий режим
  sleep_enable();
  // разрешаем прерывания
  sei();
  // собственно засыпаем
  sleep_cpu();
}

void WakeUp()
{
  // запрещаем режим сна
  sleep_disable();                       // отключаем спящий режим
  power_all_enable();                    // включаем все внутренние блоки ЦП
  // power_adc_enable();    // ADC converter
  // power_spi_enable();    // SPI
  // power_usart0_enable(); // Serial (USART)
  // power_timer0_enable(); // Timer 0
  // power_timer1_enable(); // Timer 1
  // power_timer2_enable(); // Timer 2
  // power_twi_enable();    // TWI (I2C)  

  // запрещаем прерывания
  cli();
}
