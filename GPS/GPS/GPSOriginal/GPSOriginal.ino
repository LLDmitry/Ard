/*
 
 Начинает выдавать инф-ю только когда поймал спутники
 TXD - 2pin; RXD - 3pin for UNO, mini;  (51,50) for Mega TXD-51, RXD-50
 GND, VCC
 */

#include <TinyGPS.h> 
#include <SoftwareSerial.h>

TinyGPS gps; 
// Дискретные пины, к которым подключен GPS SKM53 
SoftwareSerial skm53(2, 3);  //TXD - 2pin; RXD - 3pin for UNO, mini;  (51,50) for Mega TXD-51, RXD-50

void setup() 
{  
  // Скорость работы с GPS модулем (для SKM53 9600) 
  skm53.begin(9600); 
    Serial.begin(9600); 
 //  Serial.println("Search sattelites. Wait.");

}

void loop() 
{ 
   //Serial.println("loo");
   
   bool newData = false;

  // Каждую секунду парсим GPS данные 
  for (unsigned long start = millis(); millis() - start < 1000;)
  { 
    while (skm53.available()) 
    { 
      char c = skm53.read(); 
      if (gps.encode(c)) newData = true; 
    } 
  }

  if (newData) 
  { 
    float lat, lon; 
    unsigned long age;

    int year; 
    byte month, day, hour, minutes, second, hundredths;

    // Получаем координаты 
    gps.f_get_position(&lat, &lon, &age); 
    // Получаем дату и время 
    gps.crack_datetime(&year, &month, &day, &hour, &minutes, &second, &hundredths, &age);

   
   Serial.print("Lat: ");
   Serial.println(lat,6);
   
   Serial.print("Lon: ");
   Serial.println(FloatToString(lon));

   Serial.print("Date/Time: ");
   Serial.print(year); 
   Serial.print("-"); 
   Serial.print(month); 
   Serial.print("-"); 
   Serial.print(day);
   Serial.print("  ");
   Serial.print(hour);
   Serial.print(":");
   Serial.print(minutes);
   Serial.print(":");   
   Serial.println(second);
   Serial.print("Number Sat: "); 
   Serial.println(String(gps.satellites())); 
  //      lcd.print(" Alt:"); 
  //      lcd.print(gps.f_altitude()); 
  //      lcd.print("Speed: "); 
  //      lcd.print(gps.f_speed_kmph()); 
   Serial.println("");
  } 
}

String FloatToString(float val)
{
  char temp[10];
  String tempAsString;
  dtostrf(val, 2, 6, temp);
  tempAsString = String(temp);
  return tempAsString;
}
