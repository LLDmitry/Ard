//GSM_Car_Simple
//gsm atwin datashit: at139 http://s3.amazonaws.com/linksprite/Shields/ATWIN/AT139+Hardware+Design+Manual_V1.3.pdf

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

//  ATDdialstring[i][; ] CONNECT[<rate>]: Data call connect at rate.
//  OK : Voice call connects.
//ON CARRIER : Establish connection.
//BUSY : Call busy party.
//      ERROR : Command connect.
//                    AT+CHUP, Hangup Call  AT+CHUP command is alias for ATH.
/*AT + CSTA command select type of number dial with ATD command.
  Implementation supports 129.
  If dial string start with + , then 145 implicit select.No other dial
  number type support.
  Command Possible Response
  AT + CSTA = [<type>] OK, +CME ERROR : 3
  AT + CSTA ? +CSTA : 129
  AT + CSTA = ? +CSTA : (129)
  AT + VTD Tone Duration
  AT + VTD command use to define length of tone emit result of AT + VTS
  command.
  Command Possible Response
  AT + VTD = <n> +CME ERROR : 3
  AT + VTD ? +VTD : <n>
  AT + VTD = ? +VTD : (0 - 255)
  Tone duration queried, never set.Implement return zero to indicate
  "manufacturer specific".
  AT + VTS DTMF and Tone Generation
  AT + VTS command use to generate DTMF tone during voice call.
  Command Possible Response
  AT + VTS = <tones> OK
  AT + VTS = ? +VTS : (0 - 9, *, #, A, B, C, D)
  <tones> parameter string contain digits sent as DTMF tones.
  Dual tone frequencies tone duration parameters.*/

#include <SoftwareSerial.h>
#include <elapsedMillis.h>

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#define WAKE_UP_PIN 2    //для просыпания от прерывания по входящему звонку/по сигнализации/по включению зажигания - объединить через диоды (обязательно = 2 т.к. attachInterrupt(0,...)
#define SOFT_RX_PIN 3
#define SOFT_TX_PIN 4
#define U_220_PIN 6  // оптрон с конденсатором на выходе, чтобы 1 пропадала не сразу (исключить кратковременные помехи)

#define ONE_WIRE_PIN 9   // DS18b20
#define HEAT_PIN 13
#define CONTROLED_LINE_PIN 10
#define BZZ_PIN 12
#define IGNITION_PIN 7

SoftwareSerial mySerialGSM(SOFT_RX_PIN, SOFT_TX_PIN);  //Create a 'fake' serial port

const float T_COLD = -10; // -10 C
const unsigned long HEAT_TIME1_M = 1; // > T_COLD C
const unsigned long HEAT_TIME2_M = 2; // < T_COLD C
const unsigned long REFRESH_SENSOR_INTERVAL_S = 100;

const unsigned long IN_DISCONNECT_ON_PERIOD_S = 5;
const unsigned long IN_DISCONNECT_OFF_PERIOD_S = 15;
const unsigned long OUT_TONE_DISCONNECT_PERIOD_S = 10;
const unsigned long OUT_NO_TONE_DISCONNECT_PERIOD_S = 15;
const unsigned long OUT_INFORM_PERIOD_1_S = 2;
const unsigned long OUT_INFORM_PERIOD_2_S = 3;
const unsigned long RECALL_PERIOD_S = 7;
const unsigned long AFTER_WAKE_UP_PERIOD_S = 15; //на принятие входящего звонка и определение номера
const char* phones[5] = { "79062656420", "79217512416", "79990424298" };

int incomingPhoneID;
String incomingPhone;
int ringNumber;

boolean AlarmMode = false;
const int AlarmPnone = 1;  //number in phones[5]
boolean lineCondition = false;
volatile boolean isActiveWork = false;  //true когда работаем с GSM или идет нагрев. Переменные, изменяемые в функции обработки прерывания, должным быть объявлены как volatile.
boolean bOutgoingCallToneStarted = false;
String sSoftSerialData = "";

enum EnCallInform { NO_220, BREAK_220, REQUEST_HEAT_START_CONFIRMATION, ALARM };
enum EnHeatMode { HEAT_OFF, CHECK_HEAT_POSSIBILITY, WAIT_HEAT_START_CONFIRMATION, HEAT_START_CONFIRMED, HEAT_TIME1_ON, HEAT_TIME2_ON, REQUEST_HEAT_OFF } heatMode = HEAT_OFF;

enum EnGSMSubMode {
  WAIT_GSM_SUB,
  INCOMING_UNKNOWN_CALL,
  CONFIRM_CALL,
  START_INFO_CALL,
  FINISH_INFO_CALL,
} gsmSubMode = WAIT_GSM_SUB;

enum EnGSMMode {
  WAIT_GSM, INCOMING_CALL_START, INCOMING_CALL_PROGRESS, INCOMING_CALL_DISCONNECTED, INCOMING_CALL_HANGUP, INCOMING_CALL_ANSWERED,
  TODO_CALL, OUTGOING_CALL_PROGRESS, OUTGOING_CALL_HANGUP, OUTGOING_CALL_NOANSWER, OUTGOING_CALL_ANSWER, OUTGOING_CALL_BUSY
} gsmMode = WAIT_GSM;

float t_in;    //температура двигателя
float t_out;  //температура воздуха

elapsedMillis inDisconnect_ms;
elapsedMillis outToneStartedDisconnect_ms;
elapsedMillis outCallStarted_ms;
elapsedMillis recallPeriod_ms;
elapsedMillis heatTime_ms;
elapsedMillis lastRefreshSensor_ms;
elapsedMillis afterWakeUp_ms;

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress innerTempDeviceAddress;
DeviceAddress outerTempDeviceAddress;

void setup()
{
  // immediately disable watchdog timer so set will not get interrupted
  //wdt_disable();

  //pinMode(WAKE_UP_PIN, INPUT_PULLUP);
  pinMode(CONTROLED_LINE_PIN, INPUT_PULLUP);
  pinMode(IGNITION_PIN, INPUT);
  pinMode(U_220_PIN, INPUT);
  pinMode(HEAT_PIN, OUTPUT);
  pinMode(BZZ_PIN, OUTPUT);

  digitalWrite(13, HIGH);
  _delay_ms(300);
  digitalWrite(13, LOW);
  _delay_ms(300);
  digitalWrite(13, HIGH);
  _delay_ms(300);
  digitalWrite(13, LOW);
  _delay_ms(300);
  digitalWrite(13, HIGH);
  _delay_ms(300);
  digitalWrite(13, LOW);

  //Initialize serial ports for communication.
  Serial.begin(9600);
  Serial.println("Setup start");

  //attachInterrupt(0, WakeUp, FALLING);
  //attachInterrupt(1, ControlLineCnanged, CHANGE); // привязываем 1-е прерывание к функции ControlLineCnanged(). 1 - номер прерывания (pin 3)

  cellSetup();

  sensors.begin();
  sensors.getAddress(innerTempDeviceAddress, 0);
  sensors.getAddress(outerTempDeviceAddress, 1);
  sensors.setResolution(innerTempDeviceAddress, 10);
  sensors.setResolution(outerTempDeviceAddress, 10);

  // enable the watchdog timer. There are a finite number of timeouts allowed (see wdt.h).
  // Notes I have seen say it is unwise to go below 250ms as you may get the WDT stuck in a
  // loop rebooting.
  // The timeouts I'm most likely to use are:
  // WDTO_1S
  // WDTO_2S
  // WDTO_4S
  // WDTO_8S
  //wdt_enable(WDTO_8S);

  Serial.println("Setup done");

  digitalWrite(BZZ_PIN, HIGH);
  _delay_ms(300);
  digitalWrite(BZZ_PIN, LOW);
}

void cellSetup()
{
  mySerialGSM.begin(9600);

  _delay_ms(1000);

  mySerialGSM.println("AT+IPR?\r");
  _delay_ms(300);

  mySerialGSM.println("AT+CMGF=1\r");  //    mySerialGSM.print("AT+CCMGF=1\r");
  _delay_ms(300);

  mySerialGSM.println("AT+IFC=1, 1\r");
  _delay_ms(300);

  mySerialGSM.println("AT+CPBS=\"SM\"\r");
  _delay_ms(300);

  mySerialGSM.println("AT+CNMI=1,2,2,1,0\r");
  _delay_ms(500);

  mySerialGSM.println("AT+CLIP=1\r");  //разрешает показ инфо о входящем вызове номер
  _delay_ms(300);

  mySerialGSM.println("AT+CSCLK=2");
  _delay_ms(300);

  //mySerialGSM.print("AT+CEER\r");  //показать последние ошибки
  //mySerialGSM.print("AT+CSQ\r");  // +CSQ: 8,99    1st shows signal quality 0-31
  //_delay_ms(1000);
  ////mySerialGSM.print("AT+CCID\r");  // +ccid:       display SIM card: ESN разнный почему-то
  ////mySerialGSM.print("AT+CMGR=1\r");
  //mySerialGSM.print("AT+CMGL=\"SENT\"\r");
  //_delay_ms(1000);

  SIMflush();
}

void SIMflush()
{
  while (mySerialGSM.available() != 0)
  {
    mySerialGSM.read();
  }
}

void ControlLineCnanged()
{
  lineCondition = digitalRead(CONTROLED_LINE_PIN);  //store new condition
}


void DoCall()
{
  SIMflush();
  Serial.println("DoCall");
  mySerialGSM.println("AT");
  _delay_ms(200);
  mySerialGSM.println("ATD + " + incomingPhone + ";\r"); //позвонить
  _delay_ms(500);

  outCallStarted_ms = 0;
  bOutgoingCallToneStarted = false;
  gsmMode = OUTGOING_CALL_PROGRESS;
  /*ATD + 790XXXXXXXX; Позвонить на номер + 790XXXXXXXX;
    NO DIALTONE Нет сигнала
    BUSY  Если вызов отклонён
    NO CARRIER Повесили трубку
    NO ANSWER Нет ответа*/
}

//for incoming and outgoing calls
void BreakCall()
{
  mySerialGSM.println("AT");
  _delay_ms(200);

  mySerialGSM.println("ATH");
  _delay_ms(500);
}


void InformCall(EnCallInform typeCallInform) //NO_220, BREAK_220, REQUEST_HEAT_START
{
  switch (typeCallInform)
  {
    case REQUEST_HEAT_START_CONFIRMATION:
      DoCall();
      break;
    case BREAK_220:
      DoCall(); //3 раза
      DoCall();
      DoCall();
      break;
    case NO_220:
      DoCall(); //2 раза
      DoCall();
      break;
  }
}

void RefreshSensorData()
{
  if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
  {
    sensors.requestTemperatures();
    //float realTemper = sensors.getTempCByIndex(0);
    t_in = sensors.getTempC(innerTempDeviceAddress);
    t_out = sensors.getTempC(outerTempDeviceAddress);

    lastRefreshSensor_ms = 0;
  }
}

//если в процессе нагрева  пропадет 220 (например выбъет предохранитель)
void CheckHeatAndU220()
{
  if ((heatMode == HEAT_TIME1_ON || heatMode == HEAT_TIME2_ON) && !digitalRead(U_220_PIN))
  {
    heatMode = HEAT_OFF;
    digitalWrite(HEAT_PIN, LOW);
    InformCall(BREAK_220);
    Serial.println("CheckHeatAndU220");
  }
}

//при включении зажигания выдает звук, если осталось подключено 220
void CheckIgnitionAndU220()
{
  if (digitalRead(U_220_PIN) && digitalRead(IGNITION_PIN))
  {
    digitalWrite(BZZ_PIN, HIGH);
    _delay_ms(500);
    digitalWrite(BZZ_PIN, LOW);
    _delay_ms(100);
  }
}

void GetSoftSerialData()
{
  sSoftSerialData = "";
  if (mySerialGSM.available()) //если модуль что-то послал
  {
    char ch = ' ';
    unsigned long start_timeout = millis();            // Start the timer
    const unsigned int time_out_length = 3000; //ms
    while (mySerialGSM.available() && ((millis() - start_timeout) < time_out_length))
    {
      start_timeout = millis();
      ch = mySerialGSM.read();
      if ((int)ch != 17 && (int)ch != 19)
      {
        sSoftSerialData.concat(ch);
      }
      _delay_ms(2);
    }

    sSoftSerialData.trim();
    if (sSoftSerialData != "")
    {
      Serial.print("GSM talk> ");
      Serial.println(sSoftSerialData);
    }
    if ((millis() - start_timeout) < time_out_length)
    {
      Serial.println("timeout");
    }
  }
}

void CheckGSMModule()
{
  //if (mySerialGSM.available()) //если модуль что-то послал
  //{
  //  char ch = ' ';
  //  String sSoftSerialData = "";
  //  while (mySerialGSM.available())
  //  {
  //    ch = mySerialGSM.read();
  //    if ((int)ch != 17 && (int)ch != 19)
  //    {
  //      sSoftSerialData += char(ch); //собираем принятые символы в строку
  //    }
  //    _delay_ms(2);
  //  }
  //  sSoftSerialData.trim();
  //  if (sSoftSerialData != "")
  //  {
  //    Serial.print("GSM module talk> ");
  //    Serial.println(sSoftSerialData);

  //    if (sSoftSerialData.indexOf("RING") > -1)
  //    {
  //      ringNumber += 1;
  //    }
  //  }

  if (gsmMode == WAIT_GSM && sSoftSerialData.indexOf("+CLIP") > -1) //если пришел входящий вызов
  {
    Serial.println("INCOMING RING!");
    gsmMode = INCOMING_CALL_START;
    if (CheckPhone(sSoftSerialData))  // из текущей строки выберем тел номер. если звонящий номер есть в списке доступных, можно действовать
    {
      Serial.println("Call from Phone: " + incomingPhone + " " + incomingPhoneID);
    }
    else
    {
      gsmSubMode = INCOMING_UNKNOWN_CALL;
      Serial.println("Call from Phone: UNKNOWN");
    }
  }
  else if ((gsmMode == INCOMING_CALL_PROGRESS || gsmMode == INCOMING_CALL_ANSWERED) && sSoftSerialData.indexOf("NO CARRIER") > -1) //если входящий вызов сбросили не дождавшись ответа блока или повесили трубку не дождавшись выполнения команды
  {
    Serial.println("Incoming call is hang up");
    gsmMode = INCOMING_CALL_HANGUP;
  }
  //sim800 не выдает "NO CARRIER", а выдает "BUSY"
  //    else if (gsmMode == OUTGOING_CALL_PROGRESS && sSoftSerialData.indexOf("NO CARRIER") > -1) //если исходящий вызов сбросили
  else if (gsmMode == OUTGOING_CALL_PROGRESS && sSoftSerialData.indexOf("BUSY") > -1)
  {
    Serial.println("Outgoing call is hang up");
    gsmMode = OUTGOING_CALL_HANGUP;
  }
  //    else if (gsmMode == OUTGOING_CALL_PROGRESS && sSoftSerialData.indexOf("BUSY") > -1) //если на исходящий вызов линия занята
  //    {
  //      Serial.println("Outgoing call - line is busy");
  //      gsmMode = OUTGOING_CALL_BUSY;
  //    }
  else if (gsmMode == OUTGOING_CALL_PROGRESS && sSoftSerialData.indexOf("NO ANSWER") > -1) //если на исходящий вызов линия  нет ответа
  {
    Serial.println("Outgoing call - line no answer");
    gsmMode = OUTGOING_CALL_NOANSWER;
  }
  else if (gsmMode == OUTGOING_CALL_PROGRESS && sSoftSerialData.indexOf("NO DIALTONE") > -1) //если нет сигнала
  {
    Serial.println("Outgoing call - line no dialstone");
    gsmMode = OUTGOING_CALL_NOANSWER;
  }
  else if (gsmMode == OUTGOING_CALL_PROGRESS && !bOutgoingCallToneStarted)
  {
    Serial.println("GetOutgoingCallStatus");
    if (GetOutgoingCallStatus(sSoftSerialData) == "3") //если пошел исходящмй гудок
    {
      Serial.println("Tone");
      bOutgoingCallToneStarted = true;
      outToneStartedDisconnect_ms = 0;
    }
  }
  //}
}

void WorkflowGSM()
{
  switch (gsmMode)
  {
    case WAIT_GSM:
      break;
    case INCOMING_CALL_START:
      if (gsmSubMode == INCOMING_UNKNOWN_CALL) //do nothing (ignore)
        gsmMode = WAIT_GSM;
      else
      {
        inDisconnect_ms = 0;
        gsmMode = INCOMING_CALL_PROGRESS;
      }
      break;
    case INCOMING_CALL_PROGRESS:
      if (inDisconnect_ms > (heatMode != HEAT_OFF ? IN_DISCONNECT_OFF_PERIOD_S : IN_DISCONNECT_ON_PERIOD_S) * 1000)
      {
        Serial.println("IN Disconnection");
        BreakCall();
        mySerialGSM.println("AT+CBC"); //текущее U
        _delay_ms(500);
        if (heatMode != HEAT_OFF) //todo heat off
        {
          heatMode = REQUEST_HEAT_OFF;
        }
        else
        {
          recallPeriod_ms = 0;
          gsmMode = INCOMING_CALL_DISCONNECTED;
          heatMode = CHECK_HEAT_POSSIBILITY;
        }
      }
      break;
    case INCOMING_CALL_HANGUP:
      gsmMode = WAIT_GSM;
      break;
    case INCOMING_CALL_DISCONNECTED:
      if (heatMode == WAIT_HEAT_START_CONFIRMATION)
      {
        if (recallPeriod_ms > RECALL_PERIOD_S * 1000)
        {
          Serial.println("Time for ReCall");
          gsmMode = TODO_CALL;
          gsmSubMode = CONFIRM_CALL;
        }
      }
      break;
    case TODO_CALL:
      switch (gsmSubMode)
      {
        case CONFIRM_CALL:
          Serial.println("CONFIRM_CALL");
          DoCall();
          break;
        case START_INFO_CALL:
          Serial.println("START_INFO_CALL");
          DoCall();
          break;
        case FINISH_INFO_CALL:
          Serial.println("FINISH_INFO_CALL");
          DoCall();
          break;
      }
      break;

    case OUTGOING_CALL_PROGRESS:
      if (!bOutgoingCallToneStarted && outCallStarted_ms > OUT_NO_TONE_DISCONNECT_PERIOD_S * 1000)  //если вызов не пошел (сбой сети или вне зоны)
      {
        Serial.println("OUT without tone signal. Disconnection");
        BreakCall();
        gsmSubMode = WAIT_GSM_SUB;
        gsmMode = WAIT_GSM;
        bOutgoingCallToneStarted = false;
      }
      else
        switch (gsmSubMode)
        {
          case START_INFO_CALL:
          case FINISH_INFO_CALL:
            if (bOutgoingCallToneStarted && outToneStartedDisconnect_ms > (gsmSubMode == START_INFO_CALL ? OUT_INFORM_PERIOD_1_S : OUT_INFORM_PERIOD_2_S) * 1000)
            {
              Serial.println("Disconnect after 1/2 ring");
              BreakCall();
              gsmSubMode = WAIT_GSM_SUB;
              gsmMode = WAIT_GSM;
              bOutgoingCallToneStarted = false;
            }
            break;
          default:
            if (bOutgoingCallToneStarted && outToneStartedDisconnect_ms > OUT_TONE_DISCONNECT_PERIOD_S * 1000)
            {
              Serial.println("OUT without answer. Disconnection");
              BreakCall();
              //gsmSubMode = WAIT_GSM_SUB;
              gsmMode = OUTGOING_CALL_NOANSWER;
              bOutgoingCallToneStarted = false;
            }
            break;
        }

      if (gsmMode == OUTGOING_CALL_PROGRESS && !bOutgoingCallToneStarted)   //пока не ответили и не истекло время ожидания ответа, вызываем запрос состояния "AT+CLCC" чтобы поймать момент ответа
      {
        mySerialGSM.println("AT");
        _delay_ms(200);
        Serial.println("AT+CLCC");
        mySerialGSM.println("AT+CLCC");
        _delay_ms(500);
      }
      break;
    case OUTGOING_CALL_HANGUP:
      Serial.println("gsmMode = OUTGOING_CALL_HANGUP");
      if (gsmSubMode == CONFIRM_CALL)
      {
        heatMode = HEAT_START_CONFIRMED;
      }
      gsmSubMode = WAIT_GSM_SUB;
      gsmMode = WAIT_GSM;
      break;
    case OUTGOING_CALL_NOANSWER:
      if (gsmSubMode == CONFIRM_CALL)
      {
        heatMode = HEAT_OFF;
      }
      gsmSubMode = WAIT_GSM_SUB;
      gsmMode = WAIT_GSM;
      break;
      //    case OUTGOING_CALL_BUSY:
      //      if (gsmSubMode == CONFIRM_CALL)
      //      {
      //        recallPeriod_ms = 0; //перезвонить еще раз через recallPeriod
      //        Serial.println("gsmMode = TODO_CONFIRM_CALL");
      //      }
      //      gsmMode = TODO_CALL;
      //      break;
  }
}

String GetOutgoingCallStatus(String sRespond) //2 - набираем номер, 3 - идет исходящий вызов, ждем ответа, 0 - сняли трубку, идет разговор
{
  String result = "9";

  sRespond.trim();
  int i = sRespond.indexOf("+CLCC:");
  if (i > -1)
  {
    result = sRespond.substring(i + 11, i + 12);
  }
  Serial.print("Status=");
  Serial.println(result);
  return result;
}

void WorkflowMain(byte mode) //0-auto(from loop), 1-manual
{
  switch (heatMode)
  {
    case CHECK_HEAT_POSSIBILITY:
      digitalWrite(U_220_PIN, HIGH); //!!!
      if (digitalRead(U_220_PIN))
      {
        heatMode = WAIT_HEAT_START_CONFIRMATION;
        Serial.println("heatMode = WAIT_HEAT_START_CONFIRMATION");
      }
      else // нет 220
      {
        InformCall(NO_220);
        heatMode = WAIT_HEAT_START_CONFIRMATION; // HEAT_OFF;
        Serial.println("heatMode = HEAT_OFF");
      }
      break;
    case HEAT_START_CONFIRMED:
      heatTime_ms = 0;
      heatMode = (t_in > T_COLD ? HEAT_TIME1_ON : HEAT_TIME2_ON);
      digitalWrite(HEAT_PIN, HIGH);
      gsmMode = TODO_CALL;
      gsmSubMode = START_INFO_CALL;
      Serial.println("HEAT ON");
      break;

    case HEAT_TIME1_ON: //включен на HEAT_TIME1_M
    case HEAT_TIME2_ON: //включен на HEAT_TIME2_M
      if (heatTime_ms > (heatMode == HEAT_TIME1_ON ? HEAT_TIME1_M : HEAT_TIME2_M) * 1000 * 60)
      {
        heatMode = HEAT_OFF;
        digitalWrite(HEAT_PIN, LOW);
        gsmMode = TODO_CALL;
        gsmSubMode = FINISH_INFO_CALL;
        Serial.println("HEAT OFF");
      }
      break;
    case REQUEST_HEAT_OFF:
      heatMode = HEAT_OFF;
      digitalWrite(HEAT_PIN, LOW);
      gsmMode = TODO_CALL;
      gsmSubMode = FINISH_INFO_CALL;
      Serial.println("HEAT OFF DONE");
      break;
  }
}

boolean CheckPhone(String str)
{
  incomingPhoneID = 0;
  incomingPhone = "";
  if (str.indexOf("+CLIP:") > -1 || str.indexOf("+CMT:") > -1)
  {
    for (int i = 0; i < 5; i++)
    {
      if (str.indexOf(phones[i]) > -1)
      {
        incomingPhoneID = i + 1;
        incomingPhone = phones[i];
        break;
      }
    }
  }
  return (incomingPhoneID > 0);
}

void PrepareSleep()
{
  // все пины на выход и в низкий уровень (закоментарил чтобы после просыпания работал SoftSerial)
  //  for (byte i = 0; i <= A7; i++) {
  //    pinMode(i, OUTPUT);
  //    digitalWrite(i, LOW);
  //  }
  // установливаем на пине с кнопкой подтяжку к VCC
  // устанавливаем обработчик прерывания INT0
  pinMode(WAKE_UP_PIN, INPUT_PULLUP);
  attachInterrupt(0, WakeUp, FALLING);

  //  mySerialGSM.println("AT");
  //  _delay_ms(100);
  //  mySerialGSM.println("AT+CSCLK=2");
  //  _delay_ms(1000);

  wdt_disable();
}

void DoSleep()
{
  // отключаем АЦП
  ADCSRA = 0;
  // отключаем всю периферию
  power_all_disable();
  // устанавливаем режим сна - самый глубокий, здоровый сон :)
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  //set_sleep_mode(SLEEP_MODE_STANDBY);
  // разрешаем спящий режим
  sleep_enable();
  // разрешаем прерывания
  sei();
  // собственно засыпаем
  sleep_cpu();
}

void WakeUp()
{
  // запрещаем режим сна
  sleep_disable();
  // включаем все внутренние блоки ЦП
  power_all_enable();
  // power_adc_enable();    // ADC converter
  // power_spi_enable();    // SPI
  //power_usart0_enable(); // Serial (USART)
  // power_timer0_enable(); // Timer 0
  // power_timer1_enable(); // Timer 1
  // power_timer2_enable(); // Timer 2
  // power_twi_enable();    // TWI (I2C)

  // запрещаем прерывания
  cli();

  isActiveWork = true;
  afterWakeUp_ms = 0;
}


void loop()
{
  GetSoftSerialData();
  CheckGSMModule();
  WorkflowGSM();
  CheckIgnitionAndU220();
  // CheckHeatAndU220();
  RefreshSensorData();
  WorkflowMain(0);
  if (gsmMode == WAIT_GSM && heatMode == HEAT_OFF && afterWakeUp_ms > AFTER_WAKE_UP_PERIOD_S * 1000)
    isActiveWork = false; //reset flag in the end of work

  if (!isActiveWork)
  {
    PrepareSleep();
    Serial.println("DoSleep");
    _delay_ms(20);
    DoSleep();
    //Serial.flush();
    _delay_ms(100);
    Serial.println("ExitSleep");
    digitalWrite(13, HIGH);
    _delay_ms(100);
    digitalWrite(13, LOW);
    _delay_ms(100);
    digitalWrite(13, HIGH);
    _delay_ms(100);
    digitalWrite(13, LOW);
    _delay_ms(100);

    mySerialGSM.println("AT");
    _delay_ms(100);
    mySerialGSM.println("AT");
    _delay_ms(100);

    wdt_enable(WDTO_8S);
  }

  //  if (mySerialGSM.available())
  //    Serial.write(mySerialGSM.read());
  //  if (Serial.available())
  //    mySerialGSM.write(Serial.read());

  // _delay_ms(1000);
  //Serial.println("wdt_reset");
  wdt_reset();
}
