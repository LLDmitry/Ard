//Fridge
#include "TM1637.h"          // Библиотека дисплея 
#include <elapsedMillis.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "sav_button.h"

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>

#define BTTN_PIN 2          // кнопка, attachInterrupt(0)
#define BAT_DIGIT_PIN 3     // дублируется BAT_PIN, attachInterrupt(1)

#define ONE_WIRE_PIN 11   // DS18b20
#define CLK 5 // К этому пину подключаем CLK дисплея
#define DIO 6 // К этому пину подключаем DIO дисплея
#define FRIDGE_PIN 13           // холодильник мотор
#define BZ_PIN 12               // сигнал   
#define BAT_PIN A0              // от прикуривателя


TM1637 tm1637(CLK, DIO);

OneWire ds(ONE_WIRE_PIN);
DallasTemperature sensors(&ds);
DeviceAddress innerTempDeviceAddress;

elapsedMillis lastVentOn_ms;
elapsedMillis lastVentOff_ms;
elapsedMillis lastChangeShowMode_ms = 999999;
elapsedMillis lastRefreshSensor_ms = 999999;
elapsedMillis lastUpdStatistic_ms = 0;
elapsedMillis checkSourceVoltage_ms = 999999;
elapsedMillis timeAfterLastStartEngine_ms = 0;
elapsedMillis timeAfterLastStopEngine_ms = 0;

int prevSetFridgeMode = 0; // для работы с PERIOD_SHORT_TRIP_S
int fridgeMode = 0;  //0 - Off
//1 - T1 C Автоматически ставим при работающем двигателе  и при первом включении при неработающем двигателе
//2 - T2 C
//3 - T3 C
int ShowMode = 1;   // 1-показать внутр T и режим 1-2-3
// 2-показать общее число часов охлаждения с момента отключения двигателя


boolean isBttnInterrupt = false;
boolean isBatteryInterrupt = false;
volatile float t_in;    //температура внутри
volatile unsigned long totalStatisticCool = 0;  //активной работы в минутах при неработающем двигателе

boolean isFridgeOn = false;
boolean isEngineWork = false;
boolean isConnected = false;
boolean isLowVoltage = false;
boolean isJustStartedEngine = false;  //выставляется при старте и сбрасывается после обработки

const float LOW_VOLT_ALARM = 11.7;      //минимально допустимое напряжение для работы холодильника
const float ENGINE_WORK_VOLT = 13.0;    //минимальное напряжение работающего двигателя
const unsigned long SHOW_MODE_DELAY_S = 2; //sec
const unsigned long REFRESH_SENSOR_INTERVAL_S = 300;   //5 мин
const unsigned long PERIOD_CHECK_SOURCE_VOLTAGE_S = 10;
const unsigned long PERIOD_SHORT_TRIP_S = 600; //10 мин Если была короткая поездка, то восстановим prevSetFridgeMode после выключения двигателя. А если длинная - fridgeMode не меняем

const int T1 = 0;       //заданная температура для режима 1
const int T2 = 7;       //заданная температура для режима 2
const int T3 = 15;       //заданная температура для режима 3
// резисторы делителя напряжения
const float R1 = 15238;         // 150K
const float R2 = 9870;          // 10K
const float VCC = 1.12345;        //  внутреннее опорное напряжение, необходимо откалибровать индивидуально  (м.б. 1.0 -- 1.2)

SButton button1(BTTN_PIN, 50, 1000, 10000, 1000);

void setup()
{
    pinMode(BAT_DIGIT_PIN, INPUT);
    pinMode(BTTN_PIN, INPUT_PULLUP);
    pinMode(FRIDGE_PIN, OUTPUT);
    pinMode(BZ_PIN, OUTPUT);

    Serial.begin(9600);

    // Инициация дисплея
    tm1637.set(0);    // Устанавливаем яркость от 0 до 7
    tm1637.init(D4056A);
    tm1637.point(false);

    // Инициация кнопки
    button1.begin();

    sensors.begin();
    sensors.getAddress(innerTempDeviceAddress, 0);
    sensors.setResolution(innerTempDeviceAddress, 10);  //9 bits 0.5°C ; 10 bits 0.25°C ; 11 bits 0.125°C ; 12 bits  0.0625°C
   
    attachInterrupt(0, BttnInterrupt, FALLING);
    attachInterrupt(1, BatteryInterrupt, CHANGE);
}

void BttnInterrupt ()
{
    isBttnInterrupt = true;
    WakeUp();
}

void BatteryInterrupt()
{
    isConnected = (digitalRead(BAT_DIGIT_PIN) == 1);
    if (!isConnected) // отключить дисплей, чтобы не ел
    {
        tm1637.clearDisplay(); //off display
    }
    else
        WakeUp();
    isBatteryInterrupt = true;
}


void ActionBtn(char state) //state: 'S'-short, 'L'-long
{
    switch (state)
    {
        case 'S': // fridgeMode/ShowMode
            if (fridgeMode > 0) //если включен, то меняем fridgeMode
            {
                digitalWrite(BZ_PIN, true);
                _delay_ms(200);
                digitalWrite(BZ_PIN, false);

                fridgeMode += 1;
                if (fridgeMode > 3)
                    fridgeMode = 1;
            }
            else //если выключен то ShowMode
            {
                lastRefreshSensor_ms = 999999;
                RefreshSensorData();
                ShowMode = 0;
                for (int i = 0; i < 2; i++) //покажем оба параметра по 1 сек и выключимся
                {
                    AutoChangeShowMode(true);
                    _delay_ms(1000);
                }
                tm1637.clearDisplay(); //off display
            }
            break;
        case 'L': // on/off
            digitalWrite(BZ_PIN, true);
            _delay_ms(500);
            digitalWrite(BZ_PIN, false);
            if (fridgeMode == 0)
                if (isConnected && !isLowVoltage) // можно включать
                    fridgeMode = 1;
                else //can't work
                    LowVoltageAlarm();
            else
            {
                fridgeMode = 0;
                tm1637.clearDisplay(); //off display
            }
            break;
    }
}

void CheckSourceVoltage()
{
    boolean isPrevEngineWork = isEngineWork;

    if (!isConnected)
    {
        isEngineWork = false;
        if (isPrevEngineWork) //отошел контакт во время движения
        {
            LowVoltageAlarm();
        }                  
                PrepareSleep();
                Serial.println("DoSleep");
                _delay_ms(20);
                DoSleep();
                //Serial.flush();
                _delay_ms(100);
                Serial.println("ExitSleep");
                wdt_enable(WDTO_8S);        
    }    
    else if (isBttnInterrupt || checkSourceVoltage_ms > PERIOD_CHECK_SOURCE_VOLTAGE_S * 1000) //если подали питание или пришло время проверки
    {
        boolean isPrevEngineWork = isEngineWork;

        float curAnalogData = 0.0;
        float v_bat = 0.0;
        for (int i = 0; i < 3; i++)
        {
            curAnalogData = curAnalogData + analogRead(BAT_PIN);
            Serial.println(curAnalogData);
            _delay_ms(10);
        }
        curAnalogData = curAnalogData / 3;
        v_bat = (curAnalogData * VCC) / 1024.0 / (R2 / (R1 + R2));
        Serial.print("bat=");
        Serial.println(v_bat);

        isEngineWork = (v_bat > ENGINE_WORK_VOLT);
        isJustStartedEngine = (isEngineWork && !isPrevEngineWork);
        isLowVoltage = (v_bat < LOW_VOLT_ALARM);
      
            if (isLowVoltage)
            {
                if (fridgeMode > 0)
                {
                    fridgeMode = 0;
                    LowVoltageAlarm();
                }
            }
            else if (!isEngineWork)
            {
                if (isPrevEngineWork) //just stopped
                {
                    timeAfterLastStopEngine_ms = 0;
                    if (timeAfterLastStartEngine_ms < PERIOD_SHORT_TRIP_S * 1000) //это была короткая поездка. Восстановим fridgeMode в значение до поездки
                    {
                        fridgeMode = prevSetFridgeMode;
                    }
                    else //это была долгая поездка. Сбросим статистику
                        totalStatisticCool = 0;
                }
            }
            else //isEngineWork
            {
                if (isJustStartedEngine)
                {
                    prevSetFridgeMode = fridgeMode;
                    fridgeMode = 1;
                    timeAfterLastStartEngine_ms = 0;
                }
            }     
        checkSourceVoltage_ms = 0;
    }
}

void LowVoltageAlarm()
{
    for (int i = 0; i < 5; i++)
    {
        digitalWrite(BZ_PIN, true);
        _delay_ms(1000);        
        digitalWrite(BZ_PIN, false);
        _delay_ms(300);
        if (isBttnInterrupt || (isBatteryInterrupt && isConnected)) //нажали кнопку или восстановили питание
            break;
    }
}

void FridgeControl()
{
    switch (fridgeMode)
    {
        case 0:
            isFridgeOn = false;
            break;
        case 1:
            isFridgeOn = t_in > T1 || isEngineWork;
            break;
        case 2:
            isFridgeOn = t_in > T2;
            break;
        case 3:
            isFridgeOn = t_in > T3;
            break;
    }
    digitalWrite(FRIDGE_PIN, isFridgeOn);
}

void GetStatistic()
{
    if (isFridgeOn == 1 && isEngineWork && lastUpdStatistic_ms > 60000) //update every minute
    {
        totalStatisticCool += 1;
        lastUpdStatistic_ms = 0;
    }
    else if (isFridgeOn == 0)
        lastUpdStatistic_ms = 0;
}

void AutoChangeShowMode(bool needRefresh)
{
    if (needRefresh || lastChangeShowMode_ms > SHOW_MODE_DELAY_S * (ShowMode == 1 ? 3 : 1) * 1000) //для T  - в 3 р дольше чем для статистики
    {
        ShowMode += 1;
        if (ShowMode > 2) ShowMode = 1;
        lastChangeShowMode_ms = 0;

        tm1637.clearDisplay();
        switch (ShowMode)
        {
            case 1:     //режим работы в 1м разряде + округленная внутр T
                tm1637.set(7);    // яркость от 0 до 7
                tm1637.display(0, fridgeMode);
                if (t_in < 0)
                    tm1637.display(1, 16);
                tm1637.display(2, int(t_in));
                break;
            case 2:     //время активной работы в ч (округляем до целых) при неработающем двигателе
                tm1637.set(1);    // яркость от 0 до 7
                tm1637.display(int(totalStatisticCool / 60));
                break;
                //case 2:     //внеш T
                //    tm1637.set(1);    // яркость от 0 до 7
                //    tm1637.display(FormatDisplay(t_out));
                //    if (abs((int)(t_out * 10.0)) <= 9)  //<=0.9c
                //        tm1637.display(2, 0);
                //    tm1637.display(0, 11);
                //    if (t_out < 0)
                //        tm1637.display(1, 16);
                //    if (abs((int)(t_out * 10.0)) <= 99) //<=9.9c
                //    {
                //        tm1637.display(3, 99); //мигнем последним разрядом - признак что это десятые градуса
                //        delay(200);
                //        tm1637.display(FormatDisplay(t_out));
                //        if (abs((int)(t_out * 10.0)) <= 9)  //<=0.9c
                //            tm1637.display(2, 0);
                //        tm1637.display(0, 11);
                //        if (t_out < 0)
                //            tm1637.display(1, 16);
                //    }
                //    break;
        }
    }
}

void RefreshSensorData()
{
    if (lastRefreshSensor_ms > REFRESH_SENSOR_INTERVAL_S * 1000)
    {
        sensors.requestTemperatures();
        float realTemper = sensors.getTempCByIndex(0);
        t_in = sensors.getTempC(innerTempDeviceAddress);
        //    Serial.println(lastRefreshSensor_ms);
        //    Serial.println("T=");
        //    Serial.println(t_in);
        //    Serial.println(t_out);
        lastRefreshSensor_ms = 0;
    }
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
    pinMode(BTTN_PIN, INPUT_PULLUP);
    pinMode(BAT_DIGIT_PIN, INPUT);
    attachInterrupt(0, BttnInterrupt, FALLING);
    attachInterrupt(1, BatteryInterrupt, CHANGE);

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
    // запрещаем прерывания
    cli();
}

void loop()
{
    switch (button1.Loop())
    {
        case SB_CLICK:
            Serial.println("Short press button");
            ActionBtn('S');
            break;
        case SB_LONG_CLICK:
            Serial.println("Long press button");
            ActionBtn('L');
            break;
    }

    CheckSourceVoltage();
    if (fridgeMode > 0)
    {        
        RefreshSensorData();
        FridgeControl();
        GetStatistic();
        AutoChangeShowMode(false);
    }
    isBttnInterrupt = false;
    isBatteryInterrupt = false;
}
