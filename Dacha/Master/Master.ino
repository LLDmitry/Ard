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
  unsigned int Data1;
  float Data2;
} CommandStruct;
CommandStruct Cmnd;

typedef struct{
  char SrcID;
  char DstID;
  int Cnt;
} AnswerStruct;
AnswerStruct Answ;

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
  
  ET_Command.begin(details(Cmnd), &RS485Serial);
  ET_Answer.begin(details(Answ), &RS485Serial);
}

void loop()   /****** LOOP: RUNS CONSTANTLY ******/
{ 
  currTime = millis();
  
  if (readData())
  {
 //   for (int i = 1; i <= 10; i++)
  //  {
 //         Answ.Cnt = i;
          SendAnswer(Cmnd.SrcID);
//          delay(10); 
  //  }
  }
  
//  if (!IsWaitAnswer)
//  {
//    delay(5000);
//    currTime = millis();
//    sendCommand();
//    IsWaitAnswer = true;
//    lastSendCommand = currTime;
//    Serial.println("");
//    Serial.println(currTime);
//  }
//  else 
//    if (currTime - lastSendCommand > maxWaitAnswer)
//      {
//      if (readAnswer(Cmnd.DstID))
//      {
//        Serial.print("  ");
//        Serial.println(currTime);
//      }
//      else
//      {
//        Serial.println("NO ANSWER");
//      }
//      IsWaitAnswer = false;
//    }
}

void SendAnswer(char DstID)
{
  delay(3);  //обязательно!  >=2
  digitalWrite(SSerialTxControl, RS485Transmit);
  Answ.SrcID = MyID;
  Answ.DstID =  'C';  // DstID;
  Serial.println("SendAnswer"); 
  ET_Answer.sendData();
  digitalWrite(SSerialTxControl, RS485Receive);
  Serial.println("AnswerSent");
}

void sendCommand()
{
  digitalWrite(Pin13LED, HIGH);  // Show activity
  digitalWrite(SSerialTxControl, RS485Transmit);
  
  Cmnd.SrcID = MyID;
  if (Cmnd.DstID == 'B')
    Cmnd.DstID = 'C';
  else
    Cmnd.DstID = 'B';
  Cmnd.Command = 'K';
  ET_Command.sendData();
  
  //  delay(10);
  digitalWrite(SSerialTxControl, RS485Receive);
  digitalWrite(Pin13LED, LOW);  // Show activity
}

int readData()  //-1-nothing, 0-not me, 1-answer, 2-command
{
  int nResult = -1;
  if (ET_Command.receiveData())
  { 
    Serial.print("readData ");
    Serial.println(millis());
    if(CommStrct.DstID == MyID)
    {
      if (CommStrct.Command == 'W')
        nResult = 1;
      else
        nResult = 2;      

      digitalWrite(Pin13LED, HIGH);
      Serial.print("   ");
      Serial.println(CommStrct.SrcID);
      Serial.print("   ");
      Serial.println(CommStrct.DstID);
      Serial.print("   ");
      Serial.println(CommStrct.Command);      
      Serial.print("   ");
      Serial.println(CommStrct.Data3);
    }
    else
    {
        nResult = 0;
          Serial.print("   ");
        Serial.println(CommStrct.DstID);
    }  
  }
  digitalWrite(Pin13LED, LOW);
  return nResult;
}

//boolean readAnswer(char SrcID)
//{
//  boolean bResult = false;
//  if (ET_Answer.receiveData())
//  { 
//    if(Answ.DstID == MyID && Answ.SrcID == SrcID)
//    {
//      bResult = true;
//      digitalWrite(Pin13LED, HIGH);
//      Serial.println(Answ.SrcID);
//      Serial.println(Answ.DstID);
//      digitalWrite(Pin13LED, LOW);
//    }  
//  }
//  return bResult;
//}
