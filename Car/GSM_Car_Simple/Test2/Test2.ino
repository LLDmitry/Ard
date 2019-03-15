//GSM_Test
//gsm atwin datashit: at139 http://s3.amazonaws.com/linksprite/Shields/ATWIN/AT139+Hardware+Design+Manual_V1.3.pdf

#include <SoftwareSerial.h>
#include <elapsedMillis.h>

SoftwareSerial mySerial(2, 3);  //10,11
int led1 = 0;//вкл или выкл светодиод

byte led = 13;
byte mode = 0; //0 - waiting, 1-got incoming call, 2 - wait disconnect incom call, 3 - disconnected incom call, 4 - outgoing call

elapsedMillis inDisconnect_ms;
elapsedMillis outDisconnect_ms;
elapsedMillis recallPeriod_ms;

const unsigned long IN_DISCONNECT_PERIOD_S = 7;
const unsigned long OUT_DISCONNECT_PERIOD_S = 15;
const unsigned long RECALL_PERIOD_S = 5;

void setup()
{

  delay(2000);
  pinMode(led, OUTPUT);

  Serial.begin(9600);
  mySerial.begin(9600);

  mySerial.print("AT+IPR?\r");
  delay(300);

  mySerial.print("AT+CMGF=1\r");  //    mySerial.print("AT+CCMGF=1\r");
  delay(300);

  mySerial.print("AT+IFC=1, 1\r");
  delay(300);

  mySerial.print("AT+CPBS=\"SM\"\r");
  delay(300);

  mySerial.print("AT+CNMI=1,2,2,1,0\r");
  delay(500);

  mySerial.print("AT+CLIP=1\r");  //разрешает показ инфо о входящем вызове номер
  delay(300);

  SIMflush();

  Serial.println(" ATWIN ");




  //  Serial.println("SETUP GSM NEOWAY");
  //  Serial.println();
  //  Serial.println("Turn on AOH:");
  //  mySerial.println("AT+CLIP=1");  // включить АОН
  //  delay(100);
  //  Serial.println("Text format sms:");
  //  mySerial.println("AT+CMGF=1"); // текстовый формат SMS
  //  delay(100);
  //  Serial.println("Mode GSM:");
  //  mySerial.println("AT+CSCS=\"GSM\"");  // кодировка текста - GSM
  //  delay(100);
  // ПРИ СТАРТЕ, НА КАЖДУЮ КОМАНДУ ДОЛЖНО БЫТЬ ПОДТВЕРЖДЕНИЕ - ОК
}

void SIMflush()
{
  while (mySerial.available() != 0)
  {
    mySerial.read();
  }
}

void loop()
{
  if (mySerial.available()) //если модуль что-то послал
  {
    char ch = ' ';
    String val = "";

    while (mySerial.available())
    {
      ch = mySerial.read();
      val += char(ch); //собираем принятые символы в строку
      delay(5);
    }

    Serial.print("Neo send> ");
    Serial.println(val);

    //if (val.indexOf("RING") > -1) //если есть входящий вызов, то проверяем номер
    if (mode == 0 && val.indexOf("+CLIP") > -1) //если есть входящий вызов, то проверяем номер
    {
      Serial.println("INCOMING RING!");

      Serial.print("val.indexOf= ");
      Serial.println(val.indexOf("79062656420"));
      
      if (val.indexOf("79062656420") > -1)
      {
        Serial.println("Call from my phone 1");
        mode = 1;
      }
    }
    else if (mode == 4 && val.indexOf("NO CARRIER") > -1) //если исходящий вызов сбросили
    {
      Serial.println("Outgoing call is hang up");
      mode = 0;
    }
    else if (mode == 4 && val.indexOf("BUSY") > -1) //если на исходящий вызов линия занята
    {
      Serial.println("Outgoing call - line is busy");
      recallPeriod_ms = 0;
      mode = 3; //перезвонить еще раз
    }
    //else if (mode == 4 && val.indexOf("BUSY") > -1) //если на исходящий вызов ответили
    //{
    //  Serial.println("Outgoing call - answer");
    //  mode = 0;
    //}
  }

  DoWork();

  //В какой то момент перестали проходить команды в порт. Решил эту проблему добавлением в void loop() кода:
  if (Serial.available()) {//если в мониторе порта что-то ввели
    mySerial.write(Serial.read());
  }
}

void DoWork()
{
  switch (mode)
  {
  case 0:
    break;
  case 1:
    inDisconnect_ms = 0;
    mode = 2;
    break;
  case 2:
    if (inDisconnect_ms > IN_DISCONNECT_PERIOD_S * 1000)
    {
      Serial.println("IN Disconnection");
      mySerial.println("ATH");  //разрываем связь
      delay(100);
      recallPeriod_ms = 0;
      mode = 3;
    }
    break;
  case 3:
    if (recallPeriod_ms > RECALL_PERIOD_S * 1000)
    {
      Serial.println("ReCall");
      mySerial.print("ATD + 79062656420;\r");  //перезвонить
      delay(100);
      outDisconnect_ms = 0;
      mode = 4;
    }
    break;
  case 4:
    if (outDisconnect_ms > OUT_DISCONNECT_PERIOD_S * 1000)
    {
      Serial.println("OUT without answer. Disconnection");
      mySerial.println("ATH");  //разрываем связь через 20с
      delay(100);
      mode = 0;
      //SIMflush();
    }
    break;
  }
}

