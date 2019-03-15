unsigned long Event = 0;//Счетчик количества событий
int TimeOut = 1000;//Таймаут повторения события
unsigned long Correct = 0;//Счетчик количества событий c
//с момента прошлой корректировки
unsigned long isCorrect = 5;//константа определяющая 
//индивидуальную погрешность конкретного кварцевого
//резонатора и означающая, что раз в это количество циклов
//необходимо прибавить (или отнять если значение отрицательное)
 //1 миллисекунду к функции delay для приведения системы 
 //к истенному показателю точности времени
int Delta = 0;//Переменная для корректировки
void setup()
{
  Serial.begin(9600);
  //Инициализация последовательного порта
}

void loop()
{
  Serial.print("Number of events: ");
  Serial.print(Event);
  Serial.print("\t Time from start: ");
  Serial.println(millis());
  //Вывод результата в консоль
  Event++;//Итеррация счетчика
  Correct++;
  if (Correct == isCorrect) 
  {
    Correct = 0;//Обнуление счетчика
    Delta = 1;//Корректировка
  }
  delay(TimeOut - millis()%TimeOut+Delta);//Таймаут
  if (Correct == 0) Delta = 0;
}
