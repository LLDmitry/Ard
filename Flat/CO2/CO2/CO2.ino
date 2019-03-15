#include <SoftwareSerial.h>;
#include "DHT.h"

SoftwareSerial mySerial(A0, A1); // A0 - к TX сенсора, A1 - к RX
byte cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79}; 
char response[9];

#include "TM1637.h" // Подключаем библиотеку
#define CLK 12 // К этому пину подключаем CLK
#define DIO 11 // К этому пину подключаем DIO

#define DHTTYPE DHT11
#define DHTPIN 2

TM1637 tm1637(CLK,DIO);

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);

  // Устанавливаем яркость от 0 до 7
  tm1637.init(D4056A);
  tm1637.set(1);

  mySerial.begin(9600);
  
  dht.begin();
  
}

void loop() 
{
  mySerial.write(cmd, 9);
  memset(response, 0, 9);
  mySerial.readBytes(response, 9);
    //  int i;
    //  byte crc = 0;
    //  for (i = 1; i < 8; i++) crc+=response[i];
    //  crc = 255 - crc;
    //  crc++;

    //  if ( !(response[0] == 0xFF && response[1] == 0x86 && response[8] == crc) ) {
     //   Serial.println("CRC error: " + String(crc) + " / "+ String(response[8]));
    //  } else {
    int responseHigh = (int) response[2];
    int responseLow = (int) response[3];
    int ppm = 256*(responseHigh + (responseLow<0?1:0)) + responseLow;
    
    Serial.println(responseHigh);
    Serial.println(responseLow);
//  Serial.print("CO2: "); 
    Serial.print(ppm);
//    Serial.println(" ppm\t");

    tm1637.display(ppm/10); // Выводим значение
   tm1637.display(0,10);
   delay (5000);
    Serial.println("");
    
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    int h = dht.readHumidity();
    int t = dht.readTemperature();
//  Serial.print("Humidity: "); 
//    Serial.print(h);
//    Serial.print(" %\t");
//    Serial.print("Temperature: "); 
//    Serial.print(t);
//    Serial.println(" *C");
 
 tm1637.display(t);
   tm1637.display(0,11);
delay(5000);

 tm1637.display(h);
   tm1637.display(0,12);
delay(5000); 
// Serial.println("");
   // }
}
