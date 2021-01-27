/*
 общие переменные и структуры для домашних модулей ESP+Atmega
*/
#ifndef NrfCommands_h
#define NrfCommands_h
 
#include "Arduino.h"

//const uint64_t CentralReadingPipe = 0xE8E8F0F0ABLL;  //Mega  д.б. один для всех rooms, = readingPipe CentralControl 
//const uint64_t RoomReadingPipe = 0xE8E8F0F0AALL;  //Mega  д.б. один для всех rooms
const uint64_t CentralReadingPipe = 0xF0F0F0F0A1LL;  //ESP32 д.б. один для всех rooms, = readingPipe CentralControl 
const uint64_t RoomReadingPipe =    0xF0F0F0F0A2LL;  //ESP32 д.б. один для всех rooms

const uint8_t ArRoomsChannelsNRF[] = {50, 51, 52, 53, 54, 55, 56, 57, 58, 59};

//const uint64_t ArRoomsReadingPipes[] = {0xE8E8F0F0AALL, 0xE8E8F0F0A1LL, 0xE8E8F0F0A2LL, 0xE8E8F0F0A3LL, 0xE8E8F0F0A4LL, 0xE8E8F0F0A5LL, 0xE8E8F0F0A6LL, 0xE8E8F0F0A7LL};
//const uint8_t ChannelNRF = 0x60;

enum enRequestCommand { RQ_NO, RQ_T_INFO, RQ_CLOSE_VODA_1, RQ_CLOSE_VODA_2, RQ_ALARM_SHORT, RQ_ALARM_LONG, RQ_VENT };
enum enResponseCommand { RSP_NO, RSP_INFO, RSP_COMMAND };
enum enAlarmType { ALR_NO, ALR_VODA, ALR_MOTION, ALR_PEREGREV };
enum enRooms { ROOM_GOST, ROOM_BED, ROOM_KITCHEN, ROOM_HALL, ROOM_GUEST1, ROOM_GUEST2, ROOM_VANNA1, ROOM_VANNA2, ROOM_VENT, ROOM_SENSOR};

//!!! все property д.б. byte, иначе будет разная длина в Atmega и ESP32
//Request - от Central к Room
typedef struct {
  byte Command;               //тип команды
  byte roomNumber; 	          //команда для комнаты №
  byte tOutSign;              //внешняя темп  '+' '-'
  byte tOut;                  //внешняя темп
  byte tOutDec;               //внешняя темп десятые
  byte p;                     //атм давление, целая часть - 600
  byte pDec;                  //атм давление, десятые
  byte hours;                 //текущее время
  byte minutes;               //текущее время
  byte tInnSet = 100;         //установить температуру в комнате  set_temp[k], k = 0 - выкл, 1..5, 100 - оставить без изменений. 
  byte tInnSetVal1 = 100;       //установка температуры в комнате  1е значение 0..30, 100 - оставить без изменений
  byte tInnSetVal2 = 100;       //установка температуры в комнате  2е значение 0..30, 100 - оставить без изменений
  byte tInnSetVal3 = 100;       //установка температуры в комнате  3е значение 0..30, 100 - оставить без изменений
  byte tInnSetVal4 = 100;       //установка температуры в комнате  4е значение 0..30, 100 - оставить без изменений
  byte tInnSetVal5 = 100;       //установка температуры в комнате  5е значение 0..30, 100 - оставить без изменений
  byte ventSpeed;             //установить скорость вентилятора (для комнат где есть вентилятор)
  byte alarmMaxStatus;        //инф-я о max из статусов тревоги для отображения
  byte alarmMaxStatusRoom;  	//№ комнаты с alarmMaxStatus
} NRFRequest;         	//18b

//Response - от Room к Central
typedef struct {
  byte Command = RSP_NO;                                                         
  byte roomNumber;                                                                            //1,2,3,4                                                    
  byte tInnSign;                                                                              //внутр темп  '+' '-'
  byte tInn;                                                                                  //целая часть
  byte tInnDec;                                                                               //десятые
  byte tOutSign;                                                                              //внешняя темп  '+' '-'
  byte tOut;                                                                                  //внешняя темп
  byte tOutDec;                                                                               //внешняя темп десятые
  byte tInnSet = 100;	                                                                        //set_temp[k], k =0 - выкл, 1..5 , 100 - оставить без изменений
  byte nagrevStatus;                                                                          //1
  byte addParam1;      	                                                                      // 0..255
  byte addParam2;      	                                                                      // 0..255
  byte co2;                                                                                   //co2/10
  byte h;                                                                                     //влажность, целая часть
  byte alarmType = ALR_NO;                                                                    //voda/motion alarm/...
} NRFResponse;  //15b
 
void EmptyF();
#endif