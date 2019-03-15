
//Делает звонок на заданный номер при срабатывании сигнализации
//(1 раз при срабатывании штатной сигнализаии или отсутствии сигнала от основного модуля; 2 раза при страбатывании вибро)
//Если после 1 минуты звонка не будет передан сигнал "Занято", то отправится смс с текстом
#include <SoftwareSerial.h>

// SoftwareSerial cell(2,3);  //Create a 'fake' serial port. Pin 2 is the Rx pin, pin 3 is the Tx pin.    UNO
SoftwareSerial cell(10,3);  //Create a 'fake' serial port. Pin 2 is the Rx pin, pin 3 is the Tx pin.      MEGA

const int CONTROLED_LINE_PIN = 3;
const int BZZ_PIN = 12;

char* phones[5]={"79062656420", "79217512416", "79217472342"};
boolean AlarmMode = false;
const int AlarmPnone = 1;  //number in phones[5]
boolean lineCondition = false;

void setup()
{
  pinMode(CONTROLED_LINE_PIN, INPUT_PULLUP); 
      
  //Initialize serial ports for communication.
  Serial.begin(9600);
  
  attachInterrupt(1, ControlLineCnanged, CHANGE); // привязываем 1-е прерывание к функции ControlLineCnanged(). 1 - номер прерывания (pin 3)

  cellSetup();
}

void cellSetup()
{
  delay(30000);
  cell.begin(9600);
 
  delay(1000);
  
  cell.print("AT+IPR?\r");  
  delay(300);

  cell.print("AT+CMGF=1\r");  //    cell.print("AT+CCMGF=1\r");  
  delay(300);  

  cell.print("AT+IFC=1, 1\r");  
  delay(300);  

  cell.print("AT+CPBS=\"SM\"\r");  
  delay(300);  

  cell.print("AT+CNMI=1,2,2,1,0\r");  
  delay(500); 


  cell.print("AT+CLIP=1\r");  //разрешает показ инфо о входящем вызове номер  
  delay(300);  

  Serial.println(" ATWIN ");

  digitalWrite(BZZ_PIN, HIGH);
  delay(100);
  digitalWrite(BZZ_PIN, LOW);
  
}

void ControlLineCnanged()
{
  lineCondition = digitalRead(CONTROLED_LINE_PIN);  //store new condition
}

void SendSMS(String strSMS, int phoneNumber)  // phoneNumber: 0 - incoming phone; 1..5 - phones[i]; 99 - alarm phone
{
  String phone;
  if (phoneNumber == 0)
  {
  //  phone = incomingPhone;
  }
  else if (phoneNumber == 99)
  {
    phone = "+" + String(phones[AlarmPnone-1]);
  }
  else
  {
    phone = "+" + String(phones[phoneNumber-1]);
  }
  Serial.println(strSMS);
   //Serial.println(phone);
      cell.print("AT+CMGS=\"" + phone +"\"\r");
      //cell.print("AT+CMGS=\"+79062656420\"\r");
      delay(1000);
      //cell.print("Test1\r");
      strSMS = strSMS + "\r";
      cell.print(strSMS);     
      delay(1000); 
      cell.write(26);
      //cell.print("AT+CEER\r");  //показать последние ошибки 
}

void DoCall()
{
  String phone;
  
  //  if (incomingPnone == "+79062656420")
  phone = "+" + String(phones[AlarmPnone-1]);
  Serial.println("call");                        
  cell.print("ATD" + phone + ";\r");  //позвонить             
}

void loop()
{
  
}
