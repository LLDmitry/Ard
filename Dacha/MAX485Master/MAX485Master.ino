#include <SoftEasyTransfer.h>

/* YourDuino SoftwareSerialExample1
   - Connect to another Arduino running "YD_SoftwareSerialExampleRS485_1Remote"
   - Connect this unit Pins 10, 11, Gnd
   - Pin 3 used for RS485 direction control
   - To other unit Pins 11,10, Gnd  (Cross over)
   - Open Serial Monitor, type in top window. 
   - Should see same characters echoed back from remote Arduino

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

#define MyID             'A'

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

boolean IsWaitAnswer = false;
unsigned long currTime;
unsigned long lastSendCommand;
const unsigned long maxWaitAnswer = 40; //msec    >= 30

void setup()   /****** SETUP: RUNS ONCE ******/
{

  // Start the built-in serial port, probably to Serial Monitor
  Serial.begin(9600);
  Serial.println("SerialMaster");
  
  pinMode(Pin13LED, OUTPUT);   
  pinMode(SSerialTxControl, OUTPUT);    
  
  digitalWrite(SSerialTxControl, RS485Receive);  // Init Transceiver   
  
  // Start the software serial port, to another device
  RS485Serial.begin(4800);   // set the data rate 
  
  ET_Command.begin(details(CommStrct), &RS485Serial);
  ET_Answer.begin(details(AnswStrct), &RS485Serial);
}

void sendAnswer(char DstID)
{
  digitalWrite(SSerialTxControl, RS485Transmit);
  AnswStrct.SrcID = MyID;
  AnswStrct.DstID = DstID;
  ET_Answer.sendData();
  //  delay(100);
  digitalWrite(SSerialTxControl, RS485Receive);
}

void sendCommand()
{
  digitalWrite(Pin13LED, HIGH);  // Show activity
  digitalWrite(SSerialTxControl, RS485Transmit);
  
  CommStrct.SrcID = MyID;
  if (CommStrct.DstID == 'B')
    CommStrct.DstID = 'C';
  else
    CommStrct.DstID = 'B';
  CommStrct.Command = 'K';
  ET_Command.sendData();
  
  //  delay(10);
  digitalWrite(SSerialTxControl, RS485Receive);
  digitalWrite(Pin13LED, LOW);  // Show activity
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
    }  
  }
  digitalWrite(Pin13LED, LOW);
  return bResult;
}

boolean readAnswer(char SrcID)
{
  boolean bResult = false;
  if (ET_Answer.receiveData())
  { 
    if(AnswStrct.DstID == MyID && AnswStrct.SrcID == SrcID)
    {
      bResult = true;
      digitalWrite(Pin13LED, HIGH);
      Serial.println(AnswStrct.SrcID);
      Serial.println(AnswStrct.DstID);
      digitalWrite(Pin13LED, LOW);
    }  
  }
  return bResult;
}

void loop()   /****** LOOP: RUNS CONSTANTLY ******/
{ 
  currTime = millis();
  if (!IsWaitAnswer)
  {
    delay(5000);
    currTime = millis();
    sendCommand();
    IsWaitAnswer = true;
    lastSendCommand = currTime;
    Serial.println("");
    Serial.println(currTime);
  }
  else 
    if (currTime - lastSendCommand > maxWaitAnswer)
      {
      if (readAnswer(CommStrct.DstID))
      {
        Serial.print("ANSWER ");
        Serial.println(currTime);
      }
      else
      {
        Serial.println("NO ANSWER");
      }
      IsWaitAnswer = false;
    }

}
