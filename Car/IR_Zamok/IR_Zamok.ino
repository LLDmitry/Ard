#include <IRremote.h>

const int OPEN_PIN = 6;
const int CLOSE_PIN = 7;
const int BZZ_PIN = 5;
const int RECV_PIN = 9;
const int ledPin =  13;      // the number of the LED pin

const String btnOpenCode = "38863bc2";
const String btnCloseCode = "38863bca";

const unsigned long delayPeriod = 3000;
const int ImpulseToZamok = 500;

IRrecv irrecv(RECV_PIN);

decode_results results;


void setup()
{
  Serial.begin(9600);
  irrecv.enableIRIn(); // Start the receiver
  pinMode(ledPin, OUTPUT);
  pinMode(BZZ_PIN, OUTPUT);
  pinMode(OPEN_PIN, OUTPUT);
  pinMode(CLOSE_PIN, OUTPUT);
  
  delay(1000);
  ZamokClose();
}

void loop() {
  if (irrecv.decode(&results)) {
   String res = String(results.value,HEX);
   Serial.println(res);
   if (res == btnOpenCode)
   {
      delay (delayPeriod);
      ZamokOpen();
   }
   else if (res == btnCloseCode)
   {
      delay (delayPeriod);
      ZamokClose();
   }
   else if (res != "0") //wrong code, do pause
   {
    Serial.println("WRONG");
     delay (10000);
   }
   irrecv.resume(); // Receive the next value
  }
}



void ZamokClose()
{
    Serial.println("ZamokClose");
    digitalWrite(CLOSE_PIN, HIGH);
  
    delay(ImpulseToZamok);    
    digitalWrite(CLOSE_PIN, LOW);
    
    digitalWrite(BZZ_PIN, HIGH);
    delay(600);    
    digitalWrite(BZZ_PIN, LOW);
}

void ZamokOpen()
{
    Serial.println("ZamokOpen");
    digitalWrite(OPEN_PIN, HIGH);
    
    delay(ImpulseToZamok);    
    digitalWrite(OPEN_PIN, LOW);

    digitalWrite(BZZ_PIN, HIGH);
    delay(300);    
    digitalWrite(BZZ_PIN, LOW);
    delay(300);
    digitalWrite(BZZ_PIN, HIGH);
    delay(300);    
    digitalWrite(BZZ_PIN, LOW);
}
