#include <SoftEasyTransfer.h>

/* http://arduino-info.wikispaces.com/SoftwareSerialRS485Example 
  YourDuino SoftwareSerialExample1Remote
   - Used with YD_SoftwareSerialExampleRS485_1 on another Arduino
   - Remote: Receive data, loop it back...
   - Connect this unit Pins 10, 11, Gnd
   - To other unit Pins 11,10, Gnd  (Cross over)
   - Pin 3 used for RS485 direction control   
   - Pin 13 LED blinks when data is received  
   
   Questions: terry@yourduino.com 
*/

/*-----( Import needed libraries )-----*/
#include <SoftwareSerial.h>
/*-----( Declare Constants and Pin Numbers )-----*/
#define SSerialRX        10  //Serial Receive pin
#define SSerialTX        11  //Serial Transmit pin

#define SSerialTxControl 3   //RS485 Direction control
#define RS485Transmit    HIGH
#define RS485Receive     LOW

#define Pin13LED         13
#define MyID             'C'

/*-----( Declare objects )-----*/
SoftwareSerial RS485Serial(SSerialRX, SSerialTX); // RX, TX

SoftEasyTransfer ET_Command;
SoftEasyTransfer ET_Answer;

typedef struct{
  char SrcID;
  char DstID;
  char Command;
  char Attribute;
  int Data1;
  float Data2;
} CommandStruct;
CommandStruct CommStrct;

typedef struct{
  char SrcID;
  char DstID;
} AnswerStruct;
AnswerStruct AnswStrct;

void setup()   /****** SETUP: RUNS ONCE ******/
{
  // Start the built-in serial port, probably to Serial Monitor
  Serial.begin(9600);
  Serial.println("SerialSlave");  // Can be ignored
  
  pinMode(Pin13LED, OUTPUT);   
  pinMode(SSerialTxControl, OUTPUT);
   
  digitalWrite(SSerialTxControl, RS485Receive);  // Init Transceiver
  
  // Start the software serial port, to another device
  RS485Serial.begin(4800);   // set the data rate 
  
  ET_Command.begin(details(CommStrct), &RS485Serial);
  ET_Answer.begin(details(AnswStrct), &RS485Serial);
}//--(end setup )---

void sendAnswer(char DstID)
{
  delay(3);  //обязательно!  >=2
  digitalWrite(SSerialTxControl, RS485Transmit);
  AnswStrct.SrcID = MyID;
  AnswStrct.DstID = DstID;
  ET_Answer.sendData();
  //delay(3);
 Serial.println("sendAnswer");
  digitalWrite(SSerialTxControl, RS485Receive);
}

boolean readData()
{
  boolean bResult = false;
  if(ET_Command.receiveData())
  { 
    if(CommStrct.DstID == MyID)
    {
      bResult = true;
      digitalWrite(Pin13LED, HIGH);
      Serial.println(CommStrct.SrcID);
      Serial.println(CommStrct.DstID);
      Serial.println(CommStrct.Command);
      digitalWrite(Pin13LED, LOW);
    }  
  }
  
  return bResult;
}

void loop()
{
  if (readData())
  {    
    sendAnswer(CommStrct.SrcID);
  }
   
//  delay(3000);    
}

