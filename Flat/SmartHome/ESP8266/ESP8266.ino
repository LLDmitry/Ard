//AT+CIOBAUD=9600
HardwareSerial & ESPport = Serial;

const int ledPin =  13;
const int ledPin12 =  12;
const int ledPin9 =  9;
int ledState = HIGH;
int ledState12 = HIGH;
int ledState9 = HIGH;
#define BUFFER_SIZE 128
char buffer[BUFFER_SIZE];
String vklotkl;
String vklotkl12;
String vklotkl9;
int temp = 22; // переменная, которой будет присваиваться значения например с датчика температуры

void setup()
{
  pinMode(ledPin, OUTPUT);
  pinMode(ledPin12, OUTPUT);
  pinMode(ledPin9, OUTPUT);
  ESPport.begin(115200); // ESP8266
  clearSerialBuffer();
  GetResponse("AT+RST", 3400); // перезагрузка ESP
  GetResponse("AT+CWMODE=1", 300); // режим клиента
  GetResponse("AT+CSYSWDTENABLE", 300); // сторож
  connectWiFi("myrouter", "parolparol"); // подключаемся к домашнему роутеру (имя точки, пароль)
  GetResponse("AT+CIPMODE=0", 300); // сквозной режим передачи данных.
  GetResponse("AT+CIPMUX=1", 300); // multiple connection.
  GetResponse("AT+CIPSERVER=1,88", 300); // запускаем ТСР-сервер на 88-ом порту
  GetResponse("AT+CIPSTO=3", 300); // таймаут сервера 3 сек
  GetResponse("AT+CIFSR", 300); // узнаём адрес
  digitalWrite(ledPin, ledState);
  digitalWrite(ledPin12, ledState12);
  digitalWrite(ledPin9, ledState9);
}
///////////////////основной цикл, принимает запрос от клиента///////////////////
void loop()
{
  int ch_id, packet_len;
  char *pb;
  ESPport.readBytesUntil('\n', buffer, BUFFER_SIZE);

  if (strncmp(buffer, "+IPD,", 5) == 0)
  {
    sscanf(buffer + 5, "%d,%d", &ch_id, &packet_len);
    if (packet_len > 0)
    {
      pb = buffer + 5;
      while (*pb != ':') pb++;
      pb++;

      if (strncmp(pb, "GET / ", 6) == 0)
      {
        clearSerialBuffer();
        otvet_klienty(ch_id);
      }

      //D13
      if (strncmp(pb, "GET /a", 6) == 0)
      {
        clearSerialBuffer();

        if (ledState == LOW)
        {
          ledState = HIGH;
          vklotkl = "VKL";
        }

        else
        {
          ledState = LOW;
          vklotkl = "OTKL";
        }

        digitalWrite(ledPin, ledState);
        otvet_klienty(ch_id);
      }

      //D12
      if (strncmp(pb, "GET /b", 6) == 0)
      {
        clearSerialBuffer();

        if (ledState12 == LOW)
        {
          ledState12 = HIGH;
          vklotkl12 = "VKL";
        }

        else
        {
          ledState12 = LOW;
          vklotkl12 = "OTKL";
        }

        digitalWrite(ledPin12, ledState12);
        otvet_klienty(ch_id);
      }

      //D9
      if (strncmp(pb, "GET /c", 6) == 0)
      {
        clearSerialBuffer();

        if (ledState9 == LOW)
        {
          ledState9 = HIGH;
          vklotkl9 = "VKL";
        }

        else
        {
          ledState9 = LOW;
          vklotkl9 = "OTKL";
        }

        digitalWrite(ledPin9, ledState9);
        otvet_klienty(ch_id);
      }

    }
  }
  clearBuffer();
}
//////////////////////формирование ответа клиенту////////////////////
void otvet_klienty(int ch_id)
{
  String Header;

  Header =  "HTTP/1.1 200 OK\r\n";
  Header += "Content-Type: text/html\r\n";
  Header += "Connection: close\r\n";

  String Content;

  Content = "<html><body>";
  Content += "<form action='a' method='GET'>D13 <input type='submit' value='VKL/OTKL'> " + vklotkl + "</form>";
  Content += "<form action='b' method='GET'>D12 <input type='submit' value='VKL/OTKL'> " + vklotkl12 + "</form>";
  Content += "<form action='c' method='GET'>D9   <input type='submit' value='VKL/OTKL'> " + vklotkl9 + "</form>";
  Content += "<br />Temp: " + String(temp) + " C";
  Content += "</body></html>";

  Header += "Content-Length: ";
  Header += (int)(Content.length());
  Header += "\r\n\r\n";

  ESPport.print("AT+CIPSEND="); // ответ клиенту
  ESPport.print(ch_id);
  ESPport.print(",");
  ESPport.println(Header.length() + Content.length());
  delay(20);

  if (ESPport.find(">"))
  {
    ESPport.print(Header);
    ESPport.print(Content);
    delay(200);
  }
}
/////////////////////отправка АТ-команд/////////////////////
String GetResponse(String AT_Command, int wait)
{
  String tmpData;

  ESPport.println(AT_Command);
  delay(wait);
  while (ESPport.available() > 0 )
  {
    char c = ESPport.read();
    tmpData += c;

    if ( tmpData.indexOf(AT_Command) > -1 )
      tmpData = "";
    else
      tmpData.trim();

  }
  return tmpData;
}
//////////////////////очистка ESPport////////////////////
void clearSerialBuffer(void)
{
  while ( ESPport.available() > 0 )
  {
    ESPport.read();
  }
}
////////////////////очистка буфера////////////////////////
void clearBuffer(void) {
  for (int i = 0; i < BUFFER_SIZE; i++ )
  {
    buffer[i] = 0;
  }
}
////////////////////подключение к wifi/////////////////////
boolean connectWiFi(String NetworkSSID, String NetworkPASS)
{
  String cmd = "AT+CWJAP=\"";
  cmd += NetworkSSID;
  cmd += "\",\"";
  cmd += NetworkPASS;
  cmd += "\"";

  ESPport.println(cmd);
  delay(6500);
}
