#define BLYNK_PRINT Serial
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "time.h"
#include <ArduinoJson.h>

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
#define VP_SHOW_SETTINGS_BTN    V56
#define VP_AUTO_NIGHT_BTN   V57
#define VP_AUTO_NIGHT_START V58
#define VP_AUTO_NIGHT_STOP  V59

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
// Go to the Project Settings (nut icon).
char blynkToken[] = "lxj-qafnfidshbCxIrGHQ4A12NZd2G4G";
// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "WFDV";
char pass[] = "31415926";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 3;
const int  daylightOffset_sec = 3600 * 3;

const char *weatherHost = "api.openweathermap.org";
String APIKEY = "c536de4ac23e5608ec8a62e5e0744ed8";        // Чтобы получить API ключ, перейдите по ссылке http://openweathermap.org/api
String weatherLang = "&lang=ru";
String cityID = "482443"; //Токсово

const byte ROOMS_NUMBER = 7;

byte sets_temp[5] = {2, 15, 20, 23, 25};
enum homeMode_enum {NO_MODE, WAIT_MODE, HOME_MODE, NIGHT_MODE, GUESTS_MODE, STOP_MODE};
byte defaultTemp[5][ROOMS_NUMBER] = { //[дома/ушел/ночь][к1 к2 к3 к4 к5] sets_temp[i]; 0-выключить, 100-не менять
  {1, 0, 1, 0, 0, 0, 0},        //ушел
  {1, 2, 2, 3, 1, 1, 1},        //дома
  {2, 2, 4, 4, 100, 100, 100},  //ночь
  {1, 2, 2, 3, 4, 4, 4},        //гости
  {1, 0, 0, 0, 0, 0, 0},        //стоп
};
homeMode_enum homeMode;
homeMode_enum homeModeBeforeNight;

unsigned count = 0;
BlynkTimer timer;
WidgetTerminal terminal(VP_TERMINAL);
struct tm timeinfo;

bool isWiFiConnected = false;
int numTimerReconnect = 0;

volatile float t_in;
volatile float t_out;

volatile float t_in_room[ROOMS_NUMBER];
volatile byte heat_status_room[ROOMS_NUMBER]; //0-off, 1-on, 2-on in progress; 3-err
volatile byte heat_control_room[ROOMS_NUMBER]; //0-off, 1-on
volatile byte set_temp_room[ROOMS_NUMBER];
volatile byte currentRoom;

long timeStartNightSec;
long timeStopNightSec;
bool allowAuthoNight;
int tMinute;
int tHour;
bool freshTime;

int cntFailedWeather = 0;  // Счетчик отсутстия соединения с сервером погоды
String weatherMain = "";
String weatherDescription = "";
String weatherLocation = "";
String country;
int humidity;
int pressure;
float temp;
float tempMin, tempMax;
int clouds;
float windSpeed;
String date;
String currencyRates;
String weatherString;
int windDeg;
String windDegString;
String cloudsString;
String firstString;

void setup()
{
  // Debug console
  Serial.begin(9600);
  //Blynk.begin(blynkToken, ssid, pass);


  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.begin(ssid, pass);

  Blynk.config(blynkToken);

  if (Blynk.connect())
  {
    Serial.printf("[%8lu] setup: Blynk connected\r\n", millis());
  }
  else
  {
    Serial.printf("[%8lu] setup: Blynk no connected\r\n", millis());
  }
  Serial.printf("[%8lu] Setup: Start timer reconnected\r\n", millis());
  numTimerReconnect = timer.setInterval(60000, ReconnectBlynk);

  // Clear the terminal content
  terminal.clear();

  // Send e-mail when your hardware gets connected to Blynk Server
  // Just put the recepient's "e-mail address", "Subject" and the "message body"
  Blynk.email("LLDmitry@yandex.ru", "Subject", "My Blynk project is online!.");

  // Get the NTP time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  everyHourTimer();
  everyMinuteTimer();

  getWeatherData();

  //String sTime = &timeinfo, "%A, %B %d %Y %H:%M:%S";
  Blynk.notify("Device started ");

  timer.setInterval(600000L, refreshAllTemperatures); //10minutes
  timer.setInterval(3600000L, everyHourTimer);
  timer.setInterval(60000L, everyMinuteTimer);
}

BLYNK_CONNECTED()
{
  Blynk.syncAll();
  Blynk.virtualWrite(VP_ALARM_DETAILS, "Connected");
  Serial.println("BLYNK_CONNECTED");
}

BLYNK_WRITE(VP_MODE)
{
  changeHomeMode((homeMode_enum)param.asInt(), false);
}

void changeHomeMode(homeMode_enum newHomeMode, bool authoChange)
{
  Serial.print("changeHomeMode = ");
  homeMode = newHomeMode;
  Serial.println(homeMode);
  for (int i = 0; i <= ROOMS_NUMBER - 1; i++) {
    if (defaultTemp[homeMode - 1][i] != 100 && set_temp_room[i] != defaultTemp[homeMode - 1][i])
    {
      set_temp_room[i] = defaultTemp[homeMode - 1][i];
      Serial.print("room ");
      Serial.println(i);
      Serial.print("set_temp_room=");
      Serial.println(set_temp_room[i]);
      if (set_temp_room[i] == 0) //0 -switch off eater
      {
        Serial.println("stop heat");
        heat_control_room[i] = 0;
      }
      else
        heat_control_room[i] = 1;
      setRoomHeatStatus(true);
      displayCurrentRoomTemperatureSet();
      displayCurrentRoomHeatBtn();
    }
  }
  if (authoChange)
  {
    Blynk.virtualWrite(VP_MODE, homeMode);
  }
}

void displayCurrentRoomTemperatureSet()
{
  Blynk.virtualWrite(VP_ROOM_TMP_SET, set_temp_room[currentRoom - 1]);
  //  for (int i = 0; i <= 5; i++) {
  //    if (set_temp_room[currentRoom] <= sets_temp[i])
  //    {
  //      Blynk.virtualWrite(VP_ROOM_TMP_SET, i);
  //      break;
  //    }
}

void displayCurrentRoomHeatBtn()
{
  Blynk.virtualWrite(VP_ROOM_HEAT_BTN, heat_control_room[currentRoom - 1]);
}


BLYNK_WRITE(VP_TMP_BTN_REFRESH)
{
  int btnVal = param.asInt();
  if (btnVal == HIGH)
  {
    refreshTemperature(true);
  }
}

void refreshAllTemperatures() {
  refreshTemperature(true);
}

void everyMinuteTimer() {
  if (!freshTime)
  {
    Serial.println("             everyMinuteTimer");
    tMinute += 1;
    if (tMinute == 60)
    {
      tHour += 1;
      tMinute = 0;
    }
    if (tHour == 24)
    {
      tHour = 0;
    }
    Serial.println(tHour);
    Serial.println(tMinute);
  }

  checkTimer();
  freshTime = false;
}

void checkTimer() {
  Serial.println("checkTimer");
  long nowSec = 3600 * tHour + 60 * tMinute;
  Serial.println(nowSec);
  if (allowAuthoNight && (homeMode == HOME_MODE || homeMode == GUESTS_MODE) && nowSec == timeStartNightSec)
  {
    Serial.println("START NIGHT");
    homeModeBeforeNight = homeMode;
    changeHomeMode(NIGHT_MODE, true);
  }

  if (homeMode == NIGHT_MODE && nowSec == timeStopNightSec)
  {
    Serial.println("STOP NIGHT");
    changeHomeMode(homeModeBeforeNight, true);
  }
}

void everyHourTimer() {
  Serial.println("             everyHourTimer");
  refreshLocalTime();
  freshTime = true;
}


void refreshTemperature(bool allRooms) {
  Serial.println("refreshTemperatures");
  t_out = random(-20, 35);
  t_in = random(-5, 35);
  Blynk.virtualWrite(VP_TMP_OUT, t_out);
  Blynk.virtualWrite(VP_TMP_IN, t_in);
  Blynk.virtualWrite(VP_TMP_BTN_REFRESH, LOW);

  if (allRooms)
  {
    for (int i = 0; i <= ROOMS_NUMBER - 1; i++) {
      t_in_room[i] = random(-5, 35);
    }
  }
  else
  {
    t_in_room[currentRoom - 1] = random(-5, 35);
  }

  setRoomHeatStatus(allRooms);
  displayCurrentRoomInfo();
}

BLYNK_WRITE(VP_ROOM_SELECT)
{
  currentRoom = param.asInt();
  displayCurrentRoomTemperatureSet();
  displayCurrentRoomHeatBtn();
  Serial.println("Room=" + currentRoom);
}

BLYNK_WRITE(VP_ROOM_HEAT_BTN)
{
  if (param.asInt() == 1 && set_temp_room[currentRoom - 1] == 0) //try to push but T was not selected before -> unpush
  {
    Blynk.virtualWrite(VP_ROOM_HEAT_BTN, LOW);
    return;
  }

  heat_control_room[currentRoom - 1] = param.asInt();
  //  Serial.print("currentRoom=");
  //  Serial.println(currentRoom);
  //  Serial.println("heat_control_room = ");
  //  Serial.println(heat_control_room[currentRoom - 1]);
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
  allowAuthoNight = param.asInt();
  if (allowAuthoNight)
    terminal.println("VP_AUTO_NIGHT_BTN On");
  else
    terminal.println("VP_AUTO_NIGHT_BTN Off");
  terminal.flush();

  Blynk.setProperty(VP_AUTO_NIGHT_START, "color", allowAuthoNight ? "#6b8e23" : "#877994");
  Blynk.setProperty(VP_AUTO_NIGHT_STOP, "color", allowAuthoNight ? "#6b8e23" : "#877994");
}

BLYNK_WRITE(VP_AUTO_NIGHT_START)
{
  timeStartNightSec = param[0].asLong();
  Serial.println(timeStartNightSec);
  // terminal.println(sec);
  //terminal.flush();
}

BLYNK_WRITE(VP_AUTO_NIGHT_STOP)
{
  timeStopNightSec = param[0].asLong();
  Serial.println(timeStopNightSec);
  //terminal.println(sec);
  //terminal.flush();
}

void setRoomHeatStatus(bool allRooms)
{
  Serial.println("setRoomHeatStatus");
  if (allRooms)
  {
    for (int i = 0; i <= ROOMS_NUMBER - 1; i++) {
      heat_status_room[i] = heat_control_room[i];
      if (checkHeatSwitch(i + 1))
      {
        heat_status_room[i] = 2;
      }
    }
  }
  else
  {
    heat_status_room[currentRoom - 1] = heat_control_room[currentRoom - 1];
    if (checkHeatSwitch(currentRoom))
    {
      heat_status_room[currentRoom - 1] = 2;
    }
  }
  displayCurrentRoomInfo();
}

bool checkHeatSwitch(byte room)
{
  if (room == 1)
  {
    Serial.println("checkHeatSwitch");
    Serial.print("t_in_room ");
    Serial.println(t_in_room[room - 1]);
    Serial.print("set_temp_room ");
    Serial.println(set_temp_room[room - 1]);
  }
  return (heat_status_room[room - 1] == 1 && t_in_room[room - 1] < sets_temp[set_temp_room[room - 1] - 1]); //включить нагрев
}

BLYNK_WRITE(VP_ROOM_TMP_SET)
{
  set_temp_room[currentRoom - 1] = param.asInt();
  heat_control_room[currentRoom - 1] = 1;
  setRoomHeatStatus(false);
  displayCurrentRoomHeatBtn();
}

void displayCurrentRoomInfo()
{
  Serial.print("displayCurrentRoomInfo ");
  Serial.println(heat_status_room[currentRoom - 1]);
  Blynk.virtualWrite(VP_ROOM_TMP, t_in_room[currentRoom - 1]);
  switch (heat_status_room[currentRoom - 1])
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
  Blynk.virtualWrite(VP_ROOM_TMP, t_in_room[currentRoom - 1]);
}

BLYNK_READ(VP_TMP_BTN_REFRESH)
{
  Blynk.virtualWrite(V4, millis() / 1000);
}

void refreshLocalTime()
{
  for (int i = 1; i <= 3; i++) {
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      if (i == 3)
        return;
    }
    else
      break;
  }

  time_t now;
  time(&now);
  long g = now;
  Serial.println(now);

  Serial.println(timeinfo.tm_hour);
  Serial.println(timeinfo.tm_min);
  Serial.println(timeinfo.tm_sec);
  tHour = timeinfo.tm_hour;
  tMinute = timeinfo.tm_min;
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

void getWeatherData() //client function to send/receive GET request data.
{
  String result = "";
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(weatherHost, httpPort)) {
    Serial.println("connection to openweather failed");
    cntFailedWeather++;
    return;
  }
  else {
    Serial.println("connection to openweather ok");
    cntFailedWeather = 0;
  }
  // We now create a URI for the request
  String url = "/data/2.5/weather?id=" + cityID + "&units=metric&cnt=1&APPID=" + APIKEY + weatherLang;

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + weatherHost + "\r\n" +
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      client.stop();
      return;
    }
  }

  // Read all the lines of the reply from server
  while (client.available()) {
    result = client.readStringUntil('\r');
  }

  result.replace('[', ' ');
  result.replace(']', ' ');

  char jsonArray [result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';

  StaticJsonBuffer<1024> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if (!root.success())
  {
    Serial.println("parseObject() failed");
  }

  weatherMain = root["weather"]["main"].as<String>();
  weatherDescription = root["weather"]["description"].as<String>();
  weatherDescription.toLowerCase();
  weatherLocation = root["name"].as<String>();
  country = root["sys"]["country"].as<String>();
  temp = root["main"]["temp"];
  humidity = root["main"]["humidity"];
  pressure = root["main"]["pressure"];

  windSpeed = root["wind"]["speed"];

  clouds = root["main"]["clouds"]["all"];
  String deg = String(char('~' + 25));
  weatherString =  "  Температура " + String(temp, 1) + "'C ";

  weatherString += " Влажность " + String(humidity) + "% ";
  weatherString += " Давление " + String(int(pressure / 1.3332239)) + "ммРтСт ";



  weatherString += " Ветер " + String(windSpeed, 1) + "м/с   ";

  if (clouds <= 10) cloudsString = "   Ясно";
  if (clouds > 10 && clouds <= 30) cloudsString = "   Малооблачно";
  if (clouds > 30 && clouds <= 70) cloudsString = "   Средняя облачность";
  if (clouds > 70 && clouds <= 95) cloudsString = "   Большая облачность";
  if (clouds > 95) cloudsString = "   Пасмурно";

  weatherString += cloudsString;

  Serial.println(weatherString);

  terminal.print("Weather: ");
  terminal.println(weatherString);

}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  isWiFiConnected = true;
  Serial.printf("[%8lu] Interrupt: Connected to AP, IP: ", millis());
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  isWiFiConnected = false;
  Serial.printf("[%8lu] Interrupt: Disconnected to AP!\r\n", millis());
}

void ReconnectBlynk(void)
{
  if (!Blynk.connected())
  {
    if (Blynk.connect())
    {
      Serial.printf("[%8lu] ReconnectBlynk: Blynk reconnected\r\n", millis());
    }
    else
    {
      Serial.printf("[%8lu] ReconnectBlynk: Blynk not reconnected\r\n", millis());
    }
  }
  else
  {
    Serial.printf("[%8lu] ReconnectBlynk: Blynk connected\r\n", millis());
  }
}

void BlynkRun(void)
{
  if (isWiFiConnected)
  {
    if (Blynk.connected())
    {
      if (timer.isEnabled(numTimerReconnect))
      {
        timer.disable(numTimerReconnect);
        Serial.printf("[%8lu] BlynkRun: Stop timer reconnected\r\n", millis());
      }

      Blynk.run();
    }
    else
    {
      if (!timer.isEnabled(numTimerReconnect))
      {
        timer.enable(numTimerReconnect);
        Serial.printf("[%8lu] BlynkRun: Start timer reconnected\r\n", millis());
      }
    }
  }
}

void loop()
{
  BlynkRun();
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
