//Klop 2017 говорилка чисел https://mysku.ru/blog/aliexpress/50345.html
#include <SoftwareSerial.h>
#include <mp3TF.h>
#define BusyState 9 // пин BUSY плеера
#define c19 19
#define c100 29
#define c1000 38
#define odna 76
#define dve 77
bool fl;
mp3TF mp3tf = mp3TF ();
char ccc[3];
byte troyka [3];
SoftwareSerial mySerial(2, 3); // RX, TX
//------------------------------------------------------------
void setup()
{

  Serial.begin(9600);
  mySerial.begin (9600);
  mp3tf.init (&mySerial);
  delay(200);
  pinMode(BusyState, INPUT);
  mp3tf.volumeSet(22);
  delay(100);
  Serial.println("sd");
}
//------------------------------------------------------------
void ozv(int myfile)
{ mp3tf.playFolder2(1, myfile);
  delay(200);
  while (!digitalRead(BusyState));
}
//------------------------------------------------------------
void voicedig(char cc[])
{ int a, b, c, d, jj, sme, dp;
  a = strlen(cc);
  for (byte i = 0; i < 3; i++) ccc[i] = 0;
  b = a % 3; c = a / 3; jj = 0;
  for (byte i = 0; i < c + 1; i++)
  { strncpy(ccc, cc + jj, b);
    d = atoi(ccc); a = d;
    for (byte i = 0; i < 3; i++)
    { troyka[2 - i] = a % 10;
      a = a / 10;
    }
    if (d > 0)
    { dp = troyka[2];
      if (c - i == 1)
        if (troyka[2] == 1) dp = odna;
        else if (troyka[2] == 2) dp = dve;
      if (troyka[0] > 0) ozv(c100 + troyka[0] - 1);
      if (troyka[1] > 1) ozv(c19 + troyka[1]);
      else if (troyka[1] == 1)
      { ozv(troyka[1] * 10 + troyka[2] + 1);
        goto m1;
      }
      if (troyka[2] > 0) ozv(dp + 1);
m1: a = d % 100;
      if (a > 19) a = d % 10;
      if (a == 1) sme = 0; else if (a > 1 && a < 5) sme = 1; else sme = 2;
      if (c - i > 0) ozv(c1000 + (c - i - 1) * 3 + sme);
    }
    jj = jj + b; b = 3;
    delay(100);
  }
}
//------------------------------------------------------------

void loop()
{
  // voicedig("91352412028529003471097014534460011762920");
  //Serial.println("1");
  delay(1000);
  ozv(2);
  //Serial.println("2");
  ozv(1);
  ozv(3);
//  if (mySerial.available())
//    Serial.write(mySerial.read());
//  if (Serial.available())
//    mySerial.write(Serial.read());

}
