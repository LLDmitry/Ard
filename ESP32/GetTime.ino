/*
 *  This sketch sends a message to a TCP server
 *
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiMulti.h>
#include "TM1637.h"

#define TM1637_CLK  22
#define TM1637_DIO  23


IPAddress timeServerIP; 
const char* ntpServerName = "time.nist.gov";
int TIMEZONE=3;
const int NTP_PACKET_SIZE = 48; 
byte packetBuffer[ NTP_PACKET_SIZE]; 
WiFiUDP udp;
TM1637 tm1637(TM1637_CLK,TM1637_DIO);
WiFiMulti WiFiMulti;

unsigned int  localPort = 2390;      // local port to listen for UDP packets
unsigned long ntp_time = 0;
long  t_correct        = 0;
unsigned long cur_ms   = 0;
unsigned long ms1      = 0;
unsigned long ms2      = 10000000UL;
unsigned long t_cur    = 0;
bool          points   = true;
unsigned int err_count = 0;
uint16_t     vdd       = 0;




void setup()
{
    Serial.begin(115200);
    delay(10);

// Инициализация дисплея
   tm1637.init();
// Установка яркости дисплея  
   tm1637.set(2);
   tm1637.point(false);
   tm1637.display(0,'-');
   tm1637.display(1,'-');
   tm1637.display(2,'-');
   tm1637.display(3,'-');
   delay(3000);

    // We start by connecting to a WiFi network
    WiFiMulti.addAP("WFDV", "31415926");

    Serial.println();
    Serial.println();
    Serial.print("Wait for WiFi... ");

    while(WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    delay(500);
}


void loop()
{
   cur_ms       = millis();
   t_cur        = cur_ms/1000;
// Каждые 60 секунд считываем время в интернете 
   if( cur_ms < ms2 || (cur_ms - ms2) > 60000 ){
       err_count++;
// Делаем три  попытки синхронизации с интернетом
       if( GetNTP() ){
          ms2       = cur_ms;
          err_count = 0;
          t_correct = ntp_time - t_cur;
       }
   }
   
// Каждые 0.5 секунды выдаем время
   if( cur_ms < ms1 || (cur_ms - ms1) > 500 ){
       ms1 = cur_ms;
       ntp_time    = t_cur + t_correct;
       DisplayTime();
       points = !points;
   }

// Если нет соединения с интернетом, перезагружаемся
   if( err_count > 10 ){
       Serial.println("NTP connect false");
       Serial.println("Reset ESP32 ...");
//       ESP.reset();
    
   }
   delay(100);
}


/**
 * Выдача текущего времени на индикатор
 */
void DisplayTime(void){
   uint16_t m = ( ntp_time/60 )%60;
   uint16_t h = ( ntp_time/3600 )%24;
   Serial.print("Time: ");
   Serial.print(h);
   Serial.print(":");
   Serial.println(m);
   tm1637.point(points);
   tm1637.display(0,h/10);
   tm1637.display(1,h%10);
   tm1637.display(2,m/10);
   tm1637.display(3,m%10);
    
}


/**
 * Посылаем и парсим запрос к NTP серверу
 */
bool GetNTP(void) {
  WiFi.hostByName(ntpServerName, timeServerIP); 
  sendNTPpacket(timeServerIP); 
  delay(1000);
  
  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("No packet yet");
    return false;
  }
  else {
    Serial.print("packet received, length=");
    Serial.println(cb);
// Читаем пакет в буфер    
    udp.read(packetBuffer, NTP_PACKET_SIZE); 
// 4 байта начиная с 40-го сождержат таймстамп времени - число секунд 
// от 01.01.1900   
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
// Конвертируем два слова в переменную long
    unsigned long secsSince1900 = highWord << 16 | lowWord;
// Конвертируем в UNIX-таймстамп (число секунд от 01.01.1970
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;
// Делаем поправку на местную тайм-зону
    ntp_time = epoch + TIMEZONE*3600;    
    Serial.print("Unix time = ");
    Serial.println(ntp_time);
  }
  return true;
}

/**
 * Посылаем запрос NTP серверу на заданный адрес
 */
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
// Очистка буфера в 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
// Формируем строку зыпроса NTP сервера
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
// Посылаем запрос на NTP сервер (123 порт)
  udp.beginPacket(address, 123); 
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}