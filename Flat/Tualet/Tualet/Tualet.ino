/*
 Для запоминания в еепром начального расстояния, прикрыть вплотную излучатель и выйти. 
 Через 6 сек устройство считает расстояние и примет его за начальное
 
 */
#include <avr/wdt.h>
#include <EEPROM.h>

const int LED_PIN = 13;      // select the pin for the LED
const int DIST_TRIG_PIN = 11;
const int DIST_ECHO_PIN = 12;
const int MaxCountBeforeOn= 2;
const int MaxCountBeforeOff= 10;
const int MaxCountBeforeCalibr= 2;

byte ControlDistanceSm = 60;

float DistanceSm;
int countBeforeOn= 0;
int countBeforeOff= 0;
int countBeforeCalibr = 0;
boolean switchedOn = false;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(DIST_TRIG_PIN, OUTPUT); 
  pinMode(DIST_ECHO_PIN, INPUT);
  
  // initialize serial communications at 9600 bps:
  Serial.begin(9600); 
  
  Serial.println("START");
  
  ControlDistanceSm = EEPROM.read(0);
  
  wdt_enable(WDTO_8S);  //если в loop не делать wdt_reset, то каждые 8сек будет перезагрузка
  wdt_reset();
}

void loop() {
  // read the analog in value:
  int checkDistanceStatus = checkDistanceAlarm();
  if (checkDistanceStatus == 1)
  {
    countBeforeOff = 0;
    countBeforeCalibr = 0;
    if (countBeforeOn < MaxCountBeforeOn)
    {
      countBeforeOn ++;
    }
    else
    {
      countBeforeOn = 0;
      switchedOn = true;
      swithRelay();
    }
  }
  else if (checkDistanceStatus == 0)
  {
    countBeforeOn = 0;
    countBeforeCalibr = 0;
    if (countBeforeOff < MaxCountBeforeOff)
    {
      countBeforeOff ++;
    }
    else
    {
      countBeforeOff = 0;
      switchedOn = false;
      swithRelay();
    }
  }
  else  //checkDistanceStatus == 2
  {
    countBeforeOn = 0;
    countBeforeOff = 0;
    if (countBeforeCalibr < MaxCountBeforeCalibr)
    {
      countBeforeCalibr ++;
    }
    else
    {
      countBeforeCalibr = 0;
      Serial.println("eeprom");
      
      delay(6000);
      GetDistance();
      ControlDistanceSm = DistanceSm;
      Serial.println(ControlDistanceSm);
      saveEeprom();
    }
  }
  
  delay(300);

  cli(); //запрещает прерывания
  wdt_reset(); //сброс собаки, если не сбросить - вызовется перезагрузка процессора
  sei(); //разрешает прерывания  
}

// return - 0 если никого нет; 1 - если есть; 2 - если сенсор закрыли перед калибровкой
int checkDistanceAlarm()
{
  boolean nResult = 0;
  GetDistance();
//      Serial.print("DistanceZ: ");
//      Serial.println(ControlDistanceSm);
        Serial.print("Distance: ");
      Serial.println(DistanceSm);
      
  if (DistanceSm < 0)
  {
    Serial.println("калибровка");
    nResult = 2;
  }
  else if (DistanceSm < (ControlDistanceSm - 10) || DistanceSm > (ControlDistanceSm + 10))
  {
      nResult = 1;
  }
  else
  {
      nResult = 0;
  }
  return nResult;
}

void GetDistance()
{
  digitalWrite(DIST_TRIG_PIN, HIGH); // Подаем сигнал на выход микроконтроллера 
  delayMicroseconds(10); // Удерживаем 10 микросекунд 
  digitalWrite(DIST_TRIG_PIN, LOW); // Затем убираем 
  int time_us=pulseIn(DIST_ECHO_PIN, HIGH); // Замеряем длину импульса 
  DistanceSm=time_us/58; // Пересчитываем в сантиметры 
}

void swithRelay()
{
  if (switchedOn)
  {
    digitalWrite(LED_PIN, HIGH);
  }
  else
  {
    digitalWrite(LED_PIN, LOW);
  } 
}

void saveEeprom()
{
    EEPROM.write(0, ControlDistanceSm);
}
