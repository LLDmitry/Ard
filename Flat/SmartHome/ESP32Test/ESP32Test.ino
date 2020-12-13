#define BLYNK_PRINT Serial
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "time.h"
/* Set this to a bigger number, to enable sending longer messages */
//#define BLYNK_MAX_SENDBYTES 128


#define VP_MODE             V1 //Дома/Ушел/Гости
#define VP_ALARM_STATUS     V2
#define VP_ALARM_SOURCE     V3
#define VP_ALARM_DETAILS    V4
#define VP_ALARM_BTN        V5
#define VP_TMP_OUT          V6
#define VP_TMP_IN           V7
#define VP_TMP_BTN_REFRESH  V8

#define VP_ROOM_SELECT      V20
#define VP_ROOM_TMP         V21
#define VP_ROOM_TMP_SET     V22
#define VP_ROOM_HEAT_BTN    V23
#define VP_ROOM_HEAT_STATUS V24
#define VP_LCD_ROW1         V25
#define VP_LCD_ROW2         V26
#define VP_ROOM_TEMP_SET2   V28

#define VP_DEVICE1_BTN      V30
#define VP_DEVICE2_BTN      V31
#define VP_DEVICE3_BTN      V32
#define VP_DEVICE1_TIMER    V33
#define VP_DEVICE2_TIMER    V34
#define VP_DEVICE3_TIMER    V35

#define VP_SETTINGS_TXT     V50
#define VP_TERMINAL         V51
#define VP_AUTO_NIGHT_BTN   V55
#define VP_SHOW_SETTINGS_BTN    V56

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
// Go to the Project Settings (nut icon).
char auth[] = "lxj-qafnfidshbCxIrGHQ4A12NZd2G4G";

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "WFDV";
char pass[] = "31415926";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

byte sets_temp[5] = {2, 15, 20, 23, 25};
enum homeMode_enum {WAIT_MODE, STOP_MODE, HOME_MODE, GUESTS_MODE, NIGHT_MODE};
byte defaultTemp[5][6] = { //[дома/ушел/ночь][к1 к2 к3 к4 к5]
  {12, 15, 20, 23, 0},  //дома
  {2, 0, 2, 2, 0},  //ушел
  {15, 15, 20, 20, 0},  //ночь
  {15, 15, 20, 23, 25},  //гости
};
homeMode_enum homeMode;
homeMode_enum homeModeSwitch;

unsigned count = 0;
BlynkTimer timer;
WidgetTerminal terminal(VP_TERMINAL);

volatile float t_in;
volatile float t_out;

volatile float t_in_room[6];
volatile byte heat_status_room[6]; //0-off, 1-on, 2-on in progress; 3-err
volatile byte heat_control_room[6]; //0-off, 1-on
volatile byte set_temp_room[6]; //degrees C
volatile byte roomNmbr;



void setup()
{
  // Debug console
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  // Clear the terminal content
  terminal.clear();

  // Send e-mail when your hardware gets connected to Blynk Server
  // Just put the recepient's "e-mail address", "Subject" and the "message body"
  Blynk.email("LLDmitry@yandex.ru", "Subject", "My Blynk project is online!.");

  refreshLocalTime();

  //String sTime = &timeinfo, "%A, %B %d %Y %H:%M:%S";
  Blynk.notify("Device started at ");

  timer.setInterval(60000L, refreshAllTemperatures);
}

BLYNK_WRITE(VP_MODE)
{
  homeModeSwitch = (homeMode_enum)param.asInt();
  changeHomeMode();
}

void changeHomeMode()
{
  homeMode = homeModeSwitch;
  for (int i = 0; i <= 5; i++) {
    set_temp_room[i] = defaultTemp[homeMode][i];
    if (set_temp_room[i] == 0) //reset all heater where T=0
    {
      heat_control_room[i] = 0;
      Blynk.virtualWrite(VP_ROOM_HEAT_BTN, LOW);
    }
    else
      heat_control_room[i] = 1;
    setRoomHeatStatus(true);
  }
}

BLYNK_WRITE(VP_TMP_BTN_REFRESH)
{
  int btnVal = param.asInt();
  if (btnVal == HIGH)
  {
    refreshTemperatures(true);
  }
}

void refreshAllTemperatures() {
  refreshTemperatures(true);
}

void refreshTemperatures(bool allRooms) {
  t_out = random(-20, 35);
  t_in = random(-5, 35);
  Blynk.virtualWrite(VP_TMP_OUT, t_out);
  Blynk.virtualWrite(VP_TMP_IN, t_in);
  Blynk.virtualWrite(VP_TMP_BTN_REFRESH, LOW);

  if (allRooms)
  {
    for (int i = 0; i <= 5; i++) {
      t_in_room[i] = random(-5, 35);
    }
  }
  else
  {
    t_in_room[roomNmbr] = random(-5, 35);
  }

  displayRoomInfo();

  Serial.println("refreshTemperatures");
}

BLYNK_WRITE(VP_ROOM_SELECT)
{
  roomNmbr = param.asInt();
  refreshTemperatures(false);
  Serial.println("Room=" + roomNmbr);
}

BLYNK_WRITE(VP_ROOM_HEAT_BTN)
{
  heat_control_room[roomNmbr] = param.asInt();
  Serial.println("roomNmbr=" + roomNmbr);
  Serial.println("heat_control_room = " + heat_control_room[roomNmbr]);
  setRoomHeatStatus(false);
}

BLYNK_WRITE(VP_TERMINAL)
{
  String command = param.asStr();

  terminal.print("You said:");
  terminal.write(param.getBuffer(), param.getLength());
  terminal.println();

  execTerminalCommands(command);

  // Ensure everything is sent
  terminal.flush();
}

BLYNK_WRITE(VP_SHOW_SETTINGS_BTN)
{
  if (param.asInt())
  {
    terminal.println("Next settings");
    terminal.flush();
  }
}

BLYNK_WRITE(VP_AUTO_NIGHT_BTN)
{
  if (param.asInt())
    terminal.println("VP_AUTO_NIGHT_BTN On");
  else
    terminal.println("VP_AUTO_NIGHT_BTN Off");
  terminal.flush();
}

void setRoomHeatStatus(bool allRooms)
{
  if (allRooms)
  {
    for (int i = 0; i <= 5; i++) {
      heat_status_room[i] = heat_control_room[i];
    }
  }
  else
  {
    heat_status_room[roomNmbr] = heat_control_room[roomNmbr];
  }
  displayRoomInfo();
}

BLYNK_WRITE(VP_ROOM_TMP_SET)
{
  set_temp_room[roomNmbr] = sets_temp[param.asInt()];
  setRoomHeatStatus(false);
}

void displayRoomInfo()
{
  Serial.print("displayRoomInfo ");
  Serial.println(heat_status_room[roomNmbr]);
  Blynk.virtualWrite(VP_ROOM_TMP, t_in_room[roomNmbr]);
  switch (heat_status_room[roomNmbr])
  {
    case 0:
      Blynk.setProperty(VP_ROOM_TMP, "color", "#0047AB");
      Blynk.virtualWrite(VP_ROOM_HEAT_BTN, LOW);
      break;
    case 1:
      Blynk.setProperty(VP_ROOM_TMP, "color", "#FFB317");
      break;
    case 2:
      Blynk.setProperty(VP_ROOM_TMP, "color", "#D3435C");
      break;
    case 3:
      Blynk.setProperty(VP_ROOM_TMP, "color", "#FF0000");
      Blynk.virtualWrite(VP_ROOM_HEAT_BTN, LOW);
      break;
  }
  Blynk.virtualWrite(VP_ROOM_TMP, t_in_room[roomNmbr]);
}

BLYNK_CONNECTED()
{
  Blynk.virtualWrite(VP_ALARM_DETAILS, "Connected");
  Serial.println("BLYNK_CONNECTED");
}

BLYNK_READ(VP_TMP_BTN_REFRESH)
{
  Blynk.virtualWrite(V4, millis() / 1000);
}

void refreshLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

//уст
//  к1=22
//  ночь к1т=19,к2т=2,к5т=21
//  ночь к1=19,к2=2,к5=21
//  дома к1т=22,к2т=21,к5т=21
//  гости к1=19,к2=2,k4=22,к5=21
// уст ночь к1т9 к2т2 к5т21
// уст дома к1т19 к2т2 к3т22 к5т21

//инф
//  ком
//=> к1 гостинная
//   к2 кухня
// ....
//  к1
//> тем=8 нагрев=22

const String setWord = "уст";
const String infWord = "инф";

const String waitWord = "ушел";
const String stopWord = "стоп";
const String homeWord = "дома";
const String guestsWord = "гости";
const String nightWord = "ночь";

void execTerminalCommands(String command)
{
  int str_len = command.length() + 1;
  char char_array[str_len];
  char *str;

  String typeComnand;
  String keyWord;
  homeMode_enum keyCommand;

  command.toCharArray(char_array, str_len);
  //char sz[] = "wills,this,works,40,43";
  char *p = char_array;
  int i = 0;
  while ((str = strtok_r(p, " ", &p)) != NULL)
  {
    if (str == " ") {
      continue;
    }
    i++;
    Serial.println(str);
    if (i == 1)
      typeComnand = str;
    else
    {
      if (i == 2)
      {
        if (typeComnand == setWord)
        {
          keyWord = str;
          keyCommand = getKeyCommand(keyWord);
        }
      }
      else
      {
        defaultTemp[keyCommand][getRoom(str)] = getValue(str);
        //VP_ROOM_TEMP_SET2
      }
    }
  }

  if (command.startsWith(setWord))
  {
    if (command.startsWith(nightWord))
    {

    }
    else if (command.startsWith(homeWord))
    {

    }
  }
  else if (command.startsWith(infWord))
  {

  }
}

homeMode_enum getKeyCommand(String key)
{
  if (key == waitWord) return WAIT_MODE;
  if (key == stopWord) return STOP_MODE;
  if (key == homeWord) return HOME_MODE;
  if (key == guestsWord) return GUESTS_MODE;
  if (key == nightWord) return NIGHT_MODE;
}

byte getRoom(String key)
{
  return key.substring(2, 3).toInt();
}

byte getValue(String key)
{
  return key.substring(5).toInt();
}


void testsChar()
{
  char *str;
  char sz[] = "wills,this,works,40,43";
  char *p = sz;
  while ((str = strtok_r(p, ",", &p)) != NULL)
  {
    Serial.println(str);
  }
}

void loop()
{
  Blynk.run();
  timer.run();
}

//void receiveJsonDataBySerial()
//{
//  if (Serial.available() > 0)
//  {
//    StaticJsonBuffer<650> jsonBuffer;
//    JsonObject& root = jsonBuffer.parseObject(Serial);
//    temperatureInside = root["tempIn"];
//    temperatureOutside = root["tempOut"];
//    temperatureWater = root["tempW"];
//    const char* _currentDate = root["date"];
//    const char* _currentTime = root["time"];
//    currentDate = String(_currentDate);
//    currentTime = String(_currentTime);
//    manualModeSetPoint = root["manSetPoint"];
//    heaterStatus = root["heatSt"];
//    modeNumber = root["modeNumber"];
//    daySetPoint = root["daySetPoint"];
//    nightSetPoint = root["nightSetPoint"];
//    outsideLampMode = root["outLampMode"];
//    waterSetPoint = root["waterSetPoint"];
//    panicMode = root["panicMode"];
//  }
//}

//WiFi.begin(ssid, pass);
//delay(2000);
//if (WiFi.status() == WL_CONNECTED)
//{
//    Blynk.config(auth, IPAddress(89,31,107,158));
//    Blynk.connect();
//
//  Serial.print("\nIP address: ");
//    Serial.println(WiFi.localIP());
//
//    ESP.deepSleep(60000000*10); // Сон на 10 Минут
//}
//else
//{
//    Serial.print("HET WIFI, COH HA 1M");
//    ESP.deepSleep(60000000*10); // Сон на 10 Минут
//}

// void emailOnButtonPress()
// {
//   // *** WARNING: You are limited to send ONLY ONE E-MAIL PER 5 SECONDS! ***

//   // Let's send an e-mail when you press the button
//   // connected to digital pin 2 on your Arduino

//   int isButtonPressed = !digitalRead(17); // Invert state, since button is "Active LOW"

//   if (isButtonPressed) // You can write any condition to trigger e-mail sending
//   {
//     Serial.println("Button is pressed."); // This can be seen in the Serial Monitor

//     count++;

//     String body = String("You pushed the button ") + count + " times.";

//     Blynk.email("LLDmitry@yandex.ru", "Subject: Button Logger2", body);

//     // Or, if you want to use the email specified in the App (like for App Export):
//     //Blynk.email("Subject: Button Logger", "You just pushed the button...");
//   }
// }
