
//
//Почитайте внимательно, AT+CLCC - это не только определение номера, она даёт гораздо больше информации, в частности про состояние исходящего звонка.
//
//Поэкспериментируйте: позвоните себе со своего SIM900, после ATD циклически запускайте AT+CLCC и выводите в монитор её ответы в моменты набора номера, ожидания соединения, после установки соединения и после завершения соединения. В ответе особенно интересен параметр <stat>.
//
//AT+CLCC List Current Calls
//
//
//Command
//
//Possible Responses
//
//
//AT+CLCC     +CLCC: <id1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>[,<priority>]]]
//    +CLCC: <id2>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>[,<priority>]]]
//    ...
//
//
//<id> Integer identifier for the call.
//<dir> Direction of the call: 0 = outgoing MO, 1 = incoming MT.
//<stat> State of the call: 0 active
//1 held
//2 dialing (MO call)
//3 alerting (MO call)
//4 incoming (MT call)
//5 waiting (MT call)
//


//Alarm (из C:\Users\lldmi\YandexDisk\ArduinoWorkingPC\GSM_Alarm)
//Делает звонок на заданный номер при срабатывании сигнализации
//(1 раз при срабатывании штатной сигнализаии или отсутствии сигнала от основного модуля; 2 раза при страбатывании вибро)
//Если после 1 минуты звонка не будет передан сигнал "Занято", то отправится смс с текстом

//Управление предпусковым отопителем по звонку из дома
//при поступлении звонка из дома,
// если нагрев еще не включен, передает "занято". Через пару сек перезванивает и если в течении 3х гудков есть "занято", включает подогрев на nnn минут, если нет отбоя, подогрев не включится (считаем что позвонили случайно)
// если нагрев уже включен, передает "занято" и выключает подогрев
//если не подключено 220 то перезванивает 2 раза
//если в процессе нагрева  пропадет 220 (например выбъет предохранитель) то перезванивает 3 раза
//при включении зажигания выдает звук, если осталось подключено 220
#include <SoftwareSerial.h>
#include <elapsedMillis.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#define SOFT_RX_PIN 2
#define SOFT_TX_PIN 3

#define LED_PIN 13

SoftwareSerial cell(SOFT_RX_PIN, SOFT_TX_PIN);  //Create a 'fake' serial port. Pin 2 is the Rx pin, pin 3 is the Tx pin.    UNO
//SoftwareSerial cell(10, 3); //Create a 'fake' serial port. Pin 2 is the Rx pin, pin 3 is the Tx pin.      MEGA


const char* phones[5] = { "79062656420", "79217512416", "79217472342" };

char incoming_char;      //Will hold the incoming character from the Serial Port.
int IncomingPhoneID;
String smsFromPnone;
String incomingPhone;

boolean lineCondition = false;

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

  //cell.print("ATD + 79062656420;\r");  //позвонить
  delay(300);

  SIMflush();
}

// Flush SIM900 serial port
void SIMflush()
{
  while (cell.available() != 0)
  {
    cell.read();
  }
}


void setup()
{
  pinMode(LED_PIN, OUTPUT);


  //Initialize serial ports for communication.
  Serial.begin(9600);


  cellSetup();
}

String GetPhoneNumber(String str)
{
  String phoneNumber = "";
  //  "+CMT: "+79062656420",,"13/04/17,22:04:05+22"    //SMS
  //  "+CLIP: "79062656420",145,,,"",0"                //INCOMING CALL
  phoneNumber = str.substring(str.indexOf("\"") + 1, 8 + str.substring(8).indexOf("\""));
  if (!phoneNumber.startsWith("+"))
  {
    phoneNumber = "+" + phoneNumber;
  }

  return phoneNumber;
}

void CheckIncomingGSM()
{
  boolean isStringMessage; // Переменная принимает значение True, если текущая строка является сообщением
  String currStr;

  //Serial.println("CheckIncomingGSM");
  //If a character comes in from the cellular module...
  if (cell.available() != 0)
  {
    //    Serial.println("cell.available");
    incoming_char = cell.read();    //Get the character from the cellular serial port.
    Serial.print(incoming_char);  //Print the incoming character to the terminal.

    if ('\r' == incoming_char)
    {

      Serial.println("incoming_char");

      Serial.println("");
      if (isStringMessage)
      {
        isStringMessage = false;
        Serial.println("currStr: " + currStr);
        //если текущая строка - SMS-сообщение,
        //отреагируем на него соответствующим образом
        if (IncomingPhoneID > 0)
        {
          if (currStr.indexOf("SMS") >= 0)
          {
            Serial.println("Return sms command to " + smsFromPnone);
            incomingPhone = smsFromPnone;
            //SendSMS("okk", 0);
          }
          else
          {
            Serial.println("ParseAndSaveCommands");
            // ParseAndSaveCommands();
          }
        }
      }
      else
      {
        if (currStr.startsWith("+CMT"))
        {
          //если текущая строка начинается с "+CMT",
          //то следующая строка является sms сообщением, а из текущей строки выберем тел номер и дату-время
          isStringMessage = true;
          smsFromPnone = GetPhoneNumber(currStr);
          Serial.println("IncomingsmsFromPnone: " + smsFromPnone);
          incomingPhone = smsFromPnone;
          if (checkPhone(incomingPhone))
          {
            Serial.println("smsFromPnone: " + incomingPhone);
          }
          else  //from unknown phone, ignore
          {
            Serial.println("smsFromUnknownPnone");
          }
        }
        else if (currStr.startsWith("+CLIP"))
        {
          //если текущая строка начинается с "+CLIP",
          //то идет входящий вызов, из текущей строки выберем тел номер
          Serial.print("GetPhoneNumber: ");
          Serial.println(currStr);
          incomingPhone = GetPhoneNumber(currStr);
          Serial.println("incomingPhone: " + incomingPhone);
          if (checkPhone(incomingPhone))  //звонящий номер есть в списке доступных, можно действовать
          {
            delay(1000);
            cell.print("ATH\r");  //занято
            //                        cell.print("ATA\r");  //снять трубку
            delay(2000);



            Serial.println("send bussssy");
          }
        }
        else if (currStr.startsWith("BUSY"))
        {
          //HeatControl(1);
          Serial.println("received BUSY!");
        }
        else if (currStr.startsWith("NO CARRIER"))
        {
          //HeatControl(1);
          Serial.println("received NO CARRIER!");
        }
      }
      currStr = "";
    }
    else if ('\n' != incoming_char)
    {
      Serial.print("currStr=");
      Serial.println(currStr);
      currStr += String(incoming_char);

      if (incoming_char == '+')
      {
        Serial.println("CLIP!");
      }
    }
  }
}


boolean checkPhone(String phoneNumber)
{
  Serial.print("checkPhone: ");
  Serial.println(phoneNumber);
  boolean bResult = false;
  IncomingPhoneID = 0;
  for (int i = 0; i < 5; i++)
  {
    bResult = phoneNumber.endsWith(phones[i]);
    if (bResult)
    {
      IncomingPhoneID = i + 1;
      break;
    }
  }
  return bResult;
}

void loop()
{
  CheckIncomingGSM();
}
