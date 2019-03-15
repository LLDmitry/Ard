// Simple Proximity Sensor using Infrared
// Description: Measure the distance to an obstacle using infrared light emitted by IR LED and
//   read the value with a IR photodiode. The accuracy is not perfect, but works great
//   with minor projects.
// Author: Ricardo Ouvina
// Date: 01/10/2012
// Version: 1.0

#include <avr/io.h>

byte ambientIR;                // variable to store the IR coming from the ambient
byte obstacleIR;               // variable to store the IR coming from the object
int irSignalLevel;                 // variable that will tell if there is an obstacle or not
boolean lampMode = false;
boolean allowSwitch = true;
boolean prevIRLevel = false;
boolean longWork;
boolean waitCheckLongWork;

#define IR_RECEIVER_PIN 2  //IR photodiode on analog pin       PB4
#define IR PB3
#define LAMP PB2
#define IR_ON() PORTB |= (1 << IR)
#define IR_OFF() PORTB &= ~(1 << IR)
#define LAMP_ON() PORTB |= (1 << LAMP)
#define LAMP_OFF() PORTB &= ~(1 << LAMP)

unsigned long lastIrSwitch_ms = 0;
unsigned long lastLampOn_ms = 0;

const unsigned long LONG_WORK_SWITCH_MS = 500;
const unsigned long PAUSE_BETWEEN_SWITCHES_MS = 200;
const unsigned long MAX_PERIOD_LAMP_ON_S = 1200;  //15 min

void setup() {
  pinMode(IR, OUTPUT); // IR emitter LED on digital pin 2
  //digitalWrite(IR_EMITTER_PIN, LOW); // setup IR LED as off
  IR_OFF();
  pinMode(LAMP, OUTPUT);
  analogReference(0); //0=DEFAULT: the default analog reference of 5 volts
}

void loop() {
  if (millis() - lastIrSwitch_ms > PAUSE_BETWEEN_SWITCHES_MS)
  {
    readIR();       // calling the function that will read the distance and passing the "accuracy" to it
    LampControl();
  }
}

void readIR() {
  irSignalLevel = 0;
  for (int x = 0; x < 5; x++) {
    IR_OFF();   //turning the IR LEDs off to read the IR coming from the ambient
    delay(1);                        // minimum delay necessary to read values
    ambientIR = analogRead(IR_RECEIVER_PIN);   // storing IR coming from the ambient
    IR_ON();  //turning the IR LEDs on to read the IR coming from the obstacle
    delay(1);                        // minimum delay necessary to read values
    obstacleIR = analogRead(IR_RECEIVER_PIN);  // storing IR coming from the obstacle
    irSignalLevel += ambientIR - obstacleIR; // calculating changes in IR values and storing it for future average
  }
  IR_OFF();   //turning the IR LEDs off to read the IR coming from the ambient

  irSignalLevel = irSignalLevel / 5;         // average value
}

void LampControl()
{
  if (irSignalLevel > 90) // switch lamp if the obstacle is too close    //150
  {
    prevIRLevel = true;
    if (allowSwitch)
    {
      lampMode = !lampMode;
      if (lampMode)
      {
        lastLampOn_ms = millis();
        waitCheckLongWork = true;
        longWork = false;
      }
      lastIrSwitch_ms = millis();
      allowSwitch = false;
    }

    if (lampMode)
    {
      if (waitCheckLongWork && (millis() - lastIrSwitch_ms > LONG_WORK_SWITCH_MS))
      {
        longWork = true;
        waitCheckLongWork = false;
      }
    }
  }
  else
  {
    if (prevIRLevel)
    {
      lastIrSwitch_ms = millis();
      allowSwitch = true;
    }
    waitCheckLongWork = false;
    prevIRLevel = false;
  }

  if (lampMode)
  {
    if (millis() - lastLampOn_ms > MAX_PERIOD_LAMP_ON_S * 1000 * (longWork ? 3 : 1))
    {
      lampMode = false;
      lastIrSwitch_ms = millis();
      allowSwitch = true;
    }
  }
  lampMode ? LAMP_ON() : LAMP_OFF();
}

