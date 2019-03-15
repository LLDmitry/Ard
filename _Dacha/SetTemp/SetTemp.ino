boolean RoomsStatusGret[];
int RoomsPinsNagrev[];
float RoomsSetTemperature[];
float RoomsTemperature[];
float DeltaTemperature = 0.5;
int DeltaControlTempMills = 1000; //èíòåðâàë äëÿ ïðåäîòâðàùåíèÿ ñêà÷êîâ
int DeltaCheckTempSec = 120; //èíòåðâàë äëÿ êîíòðîëÿ òåìïåðàòóðû
unsigned long CurrMills;
unsigned long LastCheckTempMills;
unsigned long LastChangeNagrevMills;
unsigned long LastSetNagrevSec[];
unsigned long TotalNagrevSec[];
unsigned long CurrTotalSec = 0;
unsigned long LastTotalSec = 0;
int DeltaLogSec = 10;
int TotalRoomsNumber = 8;

CurrMills = Millis();

if ((CurrMills - LastCheckTempMills) > (DeltaCheckTempSec * 1000))
{
      for (int i=1; i <= TotalRoomsNumber; i++)
      {
            GetTemperature(i);
            CheckTemperature(i);
      }
      LastCheckTempMills = CurrMills;
}

CurrTotalSec = GetCurrTotalSec();
if ((CurrMills - LastChangeNagrevMills) > DeltaControlTempMills)
{
      for (int i=1; i <= TotalRoomsNumber; i++)
      {
            if (Digital.Read(RoomsPinsNagrev[i]) != RoomsStatusGret[i])
            {           
                  Digital.Write(RoomsPinsNagrev[i]) = RoomsStatusGret[i];                 
                  LastChangeNagrevMills = CurrMills;
                  if (RoomsStatusGret[i])
                  {
                        LastSetNagrevSec[i] = GetCurrTotalSec();
                  }
                  break;
            }     
      }     
}

if (CurrTotalSec < LastTotalSec)
{
	LastTotalSec = LastTotalSec - 24 * 3600;
}

if ((CurrTotalSec - LastTotalSec) >= DeltaLogSec)
{
      for (int i=1; i <= TotalRoomsNumber; i++)
      {
            if (RoomsStatusGret[i])
            {
                  TotalNagrevSec[i] = TotalNagrevSec[i] + DeltaLogSec;
            }
      )
      LastTotalSec = CurrTotalSec;
}


void CheckTemperature(int Room)
{
      if (RoomsSetTemperature[Room] - RoomsTemperature[Room] > DeltaTemperature)
      {
            RoomsStatusGret[Room] = True;
      }
      else if (RoomsTemperature[Room] - RoomsSetTemperature[Room] < DeltaTemperature)
      {
            RoomsStatusGret[Room] = False;
      }     
}

// get total time in seconds from 00:00 time
unsigned long GetCurrTotalSec()
{
      unsigned long Result;      
      Result = currHours * 3600 + currMinutes * 60 + currSec     
	  return Result;
}

