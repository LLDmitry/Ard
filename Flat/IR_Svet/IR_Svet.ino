// Simple Proximity Sensor using Infrared
// Description: Measure the distance to an obstacle using infrared light emitted by IR LED and
//   read the value with a IR photodiode. The accuracy is not perfect, but works great
//   with minor projects.
// Author: Ricardo Ouvina
// Date: 01/10/2012
// Version: 1.0

#include <elapsedMillis.h>

//int IR_RECEIVER_PIN = A0;               // IR photodiode on analog pin A0
//int IR_EMITTER_PIN = 2;            // IR emitter LED on digital pin 2
int ambientIR;                // variable to store the IR coming from the ambient
int obstacleIR;               // variable to store the IR coming from the object
int value[10];                // variable to store the IR values
int distance;                 // variable that will tell if there is an obstacle or not
boolean lampMode = false;
boolean readyToSwitch = true;

#define IR_RECEIVER_PIN A0  //IR photodiode on analog pin
#define IR_EMITTER_PIN 2    //IR emitter LED on digital pin
#define LAMP_PIN 13

elapsedMillis lastLampSwitch_ms;
elapsedMillis lastLampOn_ms;

const unsigned long PAUSE_BETWEEN_SWITCHES_MS = 500;
const unsigned long PERIOD_LAMP_ON_S = 1800;  //30 min

void setup() {
  Serial.begin(9600);         // initializing Serial monitor
  pinMode(IR_EMITTER_PIN, OUTPUT); // IR emitter LED on digital pin 2
  digitalWrite(IR_EMITTER_PIN, LOW); // setup IR LED as off
  pinMode(11, OUTPUT);        // buzzer in digital pin 11
  pinMode(LAMP_PIN, OUTPUT);
}

void loop() {
  if (lastLampSwitch_ms > PAUSE_BETWEEN_SWITCHES_MS)
  {
    distance = readIR(5);       // calling the function that will read the distance and passing the "accuracy" to it
  }
  //Serial.println(distance);   // writing the read value on Serial monitor
  // buzzer();                // uncomment to activate the buzzer function
  LampControl();
}

int readIR(int times) {
  for (int x = 0; x < times; x++) {
    digitalWrite(IR_EMITTER_PIN, LOW);    //turning the IR LEDs off to read the IR coming from the ambient
    delay(1);                        // minimum delay necessary to read values
    ambientIR = analogRead(IR_RECEIVER_PIN);   // storing IR coming from the ambient
    digitalWrite(IR_EMITTER_PIN, HIGH);   //turning the IR LEDs on to read the IR coming from the obstacle
    delay(1);                        // minimum delay necessary to read values
    obstacleIR = analogRead(IR_RECEIVER_PIN);  // storing IR coming from the obstacle
    value[x] = ambientIR - obstacleIR; // calculating changes in IR values and storing it for future average
    //    Serial.println(obstacleIR);
    //    Serial.println("");
  }

  for (int x = 0; x < times; x++) {  // calculating the average based on the "accuracy"
    distance += value[x];
  }
  return (distance / times);         // return the final value
}

void LampControl()
{
  if (distance > 50) // switch lamp if the obstacle is too close
  {
    if (readyToSwitch)
    {
      lampMode = !lampMode;
      digitalWrite(LAMP_PIN, lampMode);
      if (lampMode)
      {
        lastLampOn_ms = 0;
      }
      lastLampSwitch_ms = 0;
      readyToSwitch = false;
    }
  }
  else
  {
    readyToSwitch = true;
  }

  if (lampMode)
  {
    if (lastLampOn_ms > PERIOD_LAMP_ON_S * 1000)
    {
      lampMode = false;
      digitalWrite(LAMP_PIN, lampMode);
    }
  }

}

////-- Function to sound a buzzer for audible measurements --//
//void buzzer() {
//  if (distance > 1) {
//    if (distance > 100) { // continuous sound if the obstacle is too close
//      digitalWrite(11, HIGH);
//    }
//    else { // bips faster when an obstacle approaches
//      digitalWrite(11, HIGH);
//      delay(150 - distance); // adjust this value for your convenience
//      digitalWrite(11, LOW);
//      delay(150 - distance); // adjust this value for your convenience
//    }
//  }
//  else { // off if there is no obstacle
//    digitalWrite(11, LOW);
//  }
//}

