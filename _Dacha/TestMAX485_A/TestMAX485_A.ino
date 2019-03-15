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
  unsigned long Data3;
} CommandStruct;
CommandStruct CommStrct;

typedef struct{
  char SrcID;
  char DstID;
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
  RS485Serial.begin(9600);   // set the data rate 
  
  ET_Command.begin(details(CommStrct), &RS485Serial);
  ET_Answer.begin(details(Answ), &RS485Serial);
  delay(3000);
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

boolean readAnswer(char SrcID)
{
  boolean bResult = false;
  if (ET_Answer.receiveData())
  { 
    Serial.println(Answ.SrcID);
    Serial.println(Answ.DstID);
    if(Answ.DstID == MyID)
    {
      bResult = true;
      digitalWrite(Pin13LED, HIGH);
      Serial.println("Got Answer");
      delay(100);
      digitalWrite(Pin13LED, LOW);
    }  
  }
  return bResult;
}

void sendCommand(char DstID)
{
  Serial.print("sendCommand ");
  Serial.print(DstID);
  Serial.print(" ");
  Serial.println(millis());
    
  digitalWrite(Pin13LED, HIGH);  // Show activity
  digitalWrite(SSerialTxControl, RS485Transmit);
  
  CommStrct.SrcID = MyID;
  CommStrct.DstID = DstID;
  CommStrct.Command = 'O';
  CommStrct.Data3 = millis();
  ET_Command.sendData();
  
//  delay(10);
//  
//  CommStrct.Command = 'Y';
//  ET_Command.sendData();
//  
//  CommStrct.Command = 'Z';
//  ET_Command.sendData();
  

  digitalWrite(SSerialTxControl, RS485Receive);
  digitalWrite(Pin13LED, LOW);  // Show activity
}

void sendAnswer(char DstID)
{
  Serial.print("sendAnswer ");
  Serial.print(DstID);
  Serial.print(" ");
  Serial.println(millis());
  
  digitalWrite(Pin13LED, HIGH);  // Show activity
  digitalWrite(SSerialTxControl, RS485Transmit);
  
  CommStrct.SrcID = MyID;
  CommStrct.DstID = DstID;
  CommStrct.Command = 'W';
  CommStrct.Data3 = millis();
  ET_Command.sendData(); 

  digitalWrite(SSerialTxControl, RS485Receive);
  digitalWrite(Pin13LED, LOW);  // Show activity
}

void loop()   /****** LOOP: RUNS CONSTANTLY ******/
{ 
  currTime = millis();

    sendCommand('B');
    delay(30);
    readData();
    
    sendCommand('C');
    delay(30);
    readData();

    sendCommand('B');
    delay(100);

 int n=1;
 while (n>0)
  {
    n=readData();
    if (n==2)
    {
       sendAnswer(CommStrct.SrcID);
    }
  }
    
   // readAnswer('B');
  //  readData();

Serial.println("---------------------------------------");
  delay(100000);
  
   // delay(3000);
//  if (readAnswer('C'))
//  {
//    Serial.print("ANSWER ");
//    Serial.println(currTime);
//  }
}
