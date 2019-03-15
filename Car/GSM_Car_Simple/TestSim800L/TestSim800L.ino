#include <SoftwareSerial.h>
SoftwareSerial mySerial(2, 3); // RX, TX
unsigned long c = 0;
void setup() {
  Serial.begin(9600);  //Скорость порта для связи Arduino с компьютером
  Serial.println("Goodnight moon!");
  mySerial.begin(9600);  //Скорость порта для связи Arduino с GSM модулем
  mySerial.println("AT");
  mySerial.println("AT+DDET=1,1000,0,0");
  delay(1000);
  mySerial.println("AT+CLIP=1");
  delay(1000);
  mySerial.println("AT+CSCLK=2");
  delay(1000);

  //Ответы
  //+DTMF: 0
  //+DTMF: 8
  //+DTMF: *
  //+DTMF: #

}

void loop() {
  if (mySerial.available())
    Serial.write(mySerial.read());
  if (Serial.available())
    mySerial.write(Serial.read());

  //  if (millis() - c > (1000.0 * 10))
  //  {
  //    Serial.println("----------------");
  //    c = millis();
  //    mySerial.println("AT");
  //    delay(1000);
  //    mySerial.println("AT+CBC");
  //    delay(1000);
  //    mySerial.println("AT+CSQ");
  //    delay(1000);
  //
  ////    mySerial.println("AT+CSCLK=2");
  //  //  delay(1000);
  //    //    mySerial.println("AT+CCLK?");
  //    //    delay(1000);
  //
  //
  //  }
}
