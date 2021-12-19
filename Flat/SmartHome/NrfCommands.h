/*
 общие переменные и структуры для домашних модулей
*/
#ifndef NrfCommands_h
#define NrfCommands_h
 
#include "Arduino.h"

const uint64_t CentralReadingPipe = 0xE8E8F0F0ABLL;  // д.б. один для всех rooms, = readingPipe CentralControl 
const uint64_t RoomReadingPipe = 0xE8E8F0F0AALL;  // д.б. один для всех rooms
const uint8_t ArRoomsChannelsNRF[] = {50, 51, 52, 53, 54, 55, 56, 57};

//const uint64_t ArRoomsReadingPipes[] = {0xE8E8F0F0AALL, 0xE8E8F0F0A1LL, 0xE8E8F0F0A2LL, 0xE8E8F0F0A3LL, 0xE8E8F0F0A4LL, 0xE8E8F0F0A5LL, 0xE8E8F0F0A6LL, 0xE8E8F0F0A7LL};
//const uint8_t ChannelNRF = 0x60;

enum enRequestCommand { RQ_NO, RQ_T_INFO, RQ_CLOSE_VODA_1, RQ_CLOSE_VODA_2, RQ_ALARM_SHORT, RQ_ALARM_LONG, RQ_VENT };
enum enResponseCommand { RSP_NO, RSP_INFO, RSP_COMMAND };
enum enAlarmType { ALR_NO, ALR_VODA, ALR_DOOR };
enum enRooms { ROOM_GOST, ROOM_BED, ROOM_VANNA1, ROOM_VANNA2, ROOM_HALL, ROOM_VENT, ROOM_SENSOR, ROOM_DET};

enum EnServoPosition { SERVO_0, SERVO_1, SERVO_2, SERVO_3 }; //SERVO_0 - полностью закрыт, SERVO_3 - полностью открыт

typedef struct {
  enRequestCommand Command;  //1b
  byte roomNumber; 	// 1b
  float tOut;           //4b
  int p_v;              //2b
  boolean nagrevStatus; //1b
  byte ventSpeed;       //1b
  byte hours;           //1b
  byte minutes;         //1b
  byte alarmMaxStatus;  //1b
  byte alarmRooms;  	//1b
  byte servoDet;       //1b
  byte servoBed;       //1b
  byte servoGost;       //1b
} NRFRequest;         	//11b

typedef struct {
  enResponseCommand Command = RSP_NO;                                               //1
  byte roomNumber;  //1,2,3,4                                                                 //2
  float tInn;                                                                                   //4
  float tOut;                                                                                   //4
  byte t1;	                                                                              //4
  byte t2;	                                                                              //4
  int co2;                                                                                    //2
  byte h;                                                                                     //1
  enAlarmType alarmType = ALR_NO;  //voda/door alarm/...                                      //1
  byte ventSpeed;   //0-not supported, 1-1st speed, 2-2nd speed, 10 - off, 100 - auto         //1
  float t_set = -100;    //желаемая температура (-100 если не задано)                         //4
  byte scenarioVent = 99;    //для команды из центрального упр-я 0/1/2/3/99  99 - nothing do  //1
  byte scenarioNagrev = 99;  //для команды из центрального упр-я 0/1/2/3/99  99 - nothing do  //1
} NRFResponse;  //18b
 
void EmptyF();
#endif