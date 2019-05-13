/*
 * Press and hold Mute while led will light
 */

#include <IRremote.h>

const int RECV_PIN = 9;  //11;
const int LED_PIN = 13;
const int BZ_PIN = 5;      // signal

const String Part1Code = "0";  
const String Part2Code = "38863bda"; 
// "38863bca" - btnCloseCode
const String Part3Code = "0"; 

IRrecv irrecv(RECV_PIN);

decode_results results;

String resPart1 = "";
String resPart2 = "";

void setup()
{
  Serial.begin(9600);
  irrecv.enableIRIn(); // Start the receiver
  pinMode(BZ_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  delay(1000); //для уменьшения скачка напряжения при включении питания
}

void loop() {
  if (irrecv.decode(&results)) {
    String res = String(results.value,HEX);
    Serial.println(res);
    
    //if (res == Part3Code && resPart1 == Part1Code && resPart2 == Part2Code)
    if (res == "38863bda") // && resPart1 == "" && resPart2 == "")
    {
          Serial.println("YES!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
          Serial.println(resPart1);
          Serial.println(resPart2);
      BuzzerPulse(1);
      resPart1 = "";
      resPart2 = "";

    }
    else
    {
      resPart1 = resPart2;
      resPart2 = res;
    }    
        
   irrecv.resume(); // Receive the next value
  }
 
}

void BuzzerPulse(int bzMode)
{
  boolean longBz = (bzMode<0);
  
  for (int i = 0; i < abs(bzMode); i++) 
  {
      digitalWrite(BZ_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);      
      if (longBz)
      {
        delay(600);
      }
      else
      {
        delay(200);
      }
      digitalWrite(BZ_PIN, LOW);
      digitalWrite(LED_PIN, LOW);      
      delay(400);
  }
}


