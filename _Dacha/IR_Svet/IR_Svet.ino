/*
 * IRremote: IRrecvDemo - demonstrates receiving IR codes with IRrecv
 * An IR detector/demodulator must be connected to the input RECV_PIN.
 * Version 0.1 July, 2009
 * Copyright 2009 Ken Shirriff
 * http://arcfn.com
 */

#include <IRremote.h>

int RECV_PIN = 5;

const int ledPin =  13;      // the number of the LED pin

const String btn1Code = "38863bd2";
const String btn2Code = "12121212121";  // "38863bf2";
const String btn3Code = "38863bda";

const unsigned long shortPeriod = 500;  //8 минут
//const unsigned long longPeriod = 5;

// Variables will change:
int mode = 0;
unsigned long onTime = 0;

IRrecv irrecv(RECV_PIN);

decode_results results;


void setup()
{
  Serial.begin(9600);
  irrecv.enableIRIn(); // Start the receiver
  pinMode(ledPin, OUTPUT);      
}

void loop() {
  if (irrecv.decode(&results)) {
    String res = String(results.value,HEX);
Serial.println(res);
   if (res == btn1Code)
   {
     if (mode == 1)
     {
       mode = 0;
     }
     else
     {
       mode = 1;
     }
      lamp();
      delay (500);
   }
//   else if (res == btn2Code)
//   {
//      mode = 2;
//      lamp();
//   }
//   else if (res == btn3Code)
//   {
//      mode = 0;
//      lamp();
//   }
   else if (res != "0")
   {
      mode = 0;
      lamp();
    //  delay(10000);
   }
        
    irrecv.resume(); // Receive the next value
  }
  
  if (mode != 0)
  {
      OffLamp();
  }
}

void lamp()
{
  switch (mode){
    case 0:
       digitalWrite(ledPin, LOW);
     //        Serial.println("000");
       break;             
    case 1:
       onTime = millis();
       digitalWrite(ledPin, HIGH);       
   //                 Serial.println("111");
       break;
//    case 2:
//       onTime = millis();
//       digitalWrite(ledPin, HIGH);
// //                   Serial.println("222");
//       break;
  }

}

void OffLamp()
{
  unsigned long currTime = 0;
  unsigned long diffTime = 0;
    
  currTime = millis();
  if (currTime >= onTime)
  {
    diffTime = currTime - onTime;
  }
  else
  {
    diffTime = currTime - onTime + 4294967295;
  }
 //  Serial.println(String(currTime));    
 //Serial.println(String(onTime));    
   //                 Serial.println(String(diffTime));    

//  if (mode == 1 && diffTime >= shortPeriod * 1000 || mode == 2 && diffTime >= longPeriod * 1000)    
  if (mode == 1 && diffTime >= shortPeriod * 1000)
  {
    mode = 0;
    lamp();
  }
}
