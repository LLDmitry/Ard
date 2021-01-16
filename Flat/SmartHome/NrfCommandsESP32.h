/*
 общие переменные и структуры для домашних модулей
*/
#ifndef NrfCommands_h
#define NrfCommands_h
 
#include "Arduino.h"

//const uint64_t CentralReadingPipe = 0xE8E8F0F0ABLL;  // д.б. один для всех rooms, = readingPipe CentralControl 
//const uint64_t RoomReadingPipe =    0xE8E8F0F0AALL;  // д.б. один для всех rooms
const uint64_t CentralReadingPipe = 0xF0F0F0F0A1LL;  // д.б. один для всех rooms, = readingPipe CentralControl 
const uint64_t RoomReadingPipe =    0xF0F0F0F0A2LL;  // д.б. один для всех rooms
const uint8_t ArRoomsChannelsNRF[] = {50, 51, 52, 53, 54, 55, 56, 57, 58, 59};

//const uint64_t ArRoomsReadingPipes[] = {0xE8E8F0F0AALL, 0xE8E8F0F0A1LL, 0xE8E8F0F0A2LL, 0xE8E8F0F0A3LL, 0xE8E8F0F0A4LL, 0xE8E8F0F0A5LL, 0xE8E8F0F0A6LL, 0xE8E8F0F0A7LL};
//const uint8_t ChannelNRF = 0x60;

enum enRequestCommand { RQ_NO, RQ_T_INFO, RQ_CLOSE_VODA_1, RQ_CLOSE_VODA_2, RQ_ALARM_SHORT, RQ_ALARM_LONG, RQ_VENT };
enum enResponseCommand { RSP_NO, RSP_INFO, RSP_COMMAND };
enum enAlarmType { ALR_NO, ALR_VODA, ALR_MOTION, ALR_PEREGREV };
enum enRooms { ROOM_GOST, ROOM_BED, ROOM_KITCHEN, ROOM_HALL, ROOM_GUEST1, ROOM_GUEST2, ROOM_VANNA1, ROOM_VANNA2, ROOM_VENT, ROOM_SENSOR};

typedef struct {
  enRequestCommand Command;  //1b  тип команды
  byte roomNumber; 	    //1b команда для комнаты №
  float tOut;           //4b внешняя темп
  int p_v;              //2b атм давление
  byte hours;           //1b текущее время
  byte minutes;         //1b
  byte tInnSet;         //1b установить температуру в комнате   0-off, 1...
  byte ventSpeed;       //1b установить скорость вентилятора (для комнат где есть вентилятор)
  byte alarmMaxStatus;  //1b инф-я о max из статусов тревоги для отображения
  byte alarmMaxStatusRoom;  	  //1b № комнаты с alarmMaxStatus
} NRFRequest;         	//14b

typedef struct {
  enResponseCommand Command = RSP_NO;                                                         //1
  byte roomNumber;  //1,2,3,4                                                                 //1
  float tInn;                                                                                 //4
  float tOut = -100;      //-100 или T если в комнате есть сбор t_Out                         //4
  byte tInnSet;	                                                                              //1   0 - выкл, 1..
  bool nagrevStatus;                                                                          //1
  byte t1;      	                                                                            //1
  byte t2;	                                                                                  //1
  int co2;                                                                                    //2
  byte h;                                                                                     //1
  enAlarmType alarmType = ALR_NO;  //voda/motion alarm/...                                    //1    
} NRFResponse;  //18b
 
void EmptyF();
#endif