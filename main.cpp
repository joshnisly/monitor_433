/* Convert RF signal into bits (humidity/temperature sensor version) 
 * Written by : Ray Wang (Rayshobby LLC)
 * http://rayshobby.net/?p=8998
 * Update: adapted to RPi using WiringPi
 */

// ring buffer size has to be large enough to fit
// data between two successive sync signals

#include <wiringPi.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>

using namespace std;


#define RING_BUFFER_SIZE  256

#define SYNC_LENGTH 2200

#define SYNC_HIGH  600
#define SYNC_LOW   600

#define DATAPIN  2  // wiringPi GPIO 2 (P1.13)

unsigned long timings[RING_BUFFER_SIZE];


int t2b(unsigned int t0, unsigned int t1);

class CDataWatcher
{
public:
    CDataWatcher()
    {
       m_lLastTick = micros();
    }

    bool OnTick()
    {
       m_alTimings.push_back(micros() - m_lLastTick);
       m_lLastTick = micros();

       if (m_alTimings.size() == 122)
       {
          int iSignature = 0;
          if (!InterpretValue(2 * 8, 8, iSignature) || iSignature != 68)
             return false;

          int iABC = 0;
          if (!InterpretValue(0, 2, iABC))
             return false;

          int iChannel = 0;
          if (!InterpretValue(0 * 8, 16, iChannel))
             return false;

          int iHumidity = 0;
          if (!InterpretValue(3 * 8 + 1, 7, iHumidity))
             return false;

          int iTemp = 0;
          if (!InterpretValue(4 * 8 + 4, 4, iTemp) || !InterpretValue(5 * 8 + 1, 7, iTemp))
             return false;

          // Check CRC
          int iTotal = 0;
          for (int i = 0; i < 6; i++)
          {
             int iByte = 0;
             if (!InterpretValue(i * 8, 8, iByte))
             {
                printf("Bad CRC?\n");
                return false;
             }
             iTotal += iByte;
          }
          int iCRC = 0;
          if (!InterpretValue(6 * 8, 8, iCRC))
          {
             printf("Invalid CRC\n");
             return false;
          }
          bool bValidCRC = (iTotal & 0xFF) == iCRC;

          printf("%c\t%i\t%i\t%i\t%0.2f C\t%0.2f F\n",
                 bValidCRC ? 'S' : 'F', iABC, iChannel, iHumidity, GetCelTempVal(iTemp), GetCelTempVal(iTemp) * 9 / 5 + 32);

          return false;
       }

       return true;
    }

protected:
    bool InterpretValue(int iStart, int iLength, int& riValue)
    {
       for (int i = iStart; i < iStart + iLength; i++)
       {
          int iBit = t2b(m_alTimings[i*2], m_alTimings[i*2 + 1]);
          if (iBit < 0)
             return false;

          riValue = (riValue << 1) + iBit;
       }

       return true;
    }

    float GetCelTempVal(int iRawTemp)
    {
       return (float)iRawTemp / 10.0 - 100.0;
    }

protected:
    unsigned long m_lLastTick = 0;
    vector<unsigned long> m_alTimings;
};

vector<CDataWatcher *> g_apWatchers;

void NotifyHandlers()
{
   for (int i = g_apWatchers.size() - 1; i >= 0; i--) {
      if (!g_apWatchers[i]->OnTick())
         g_apWatchers.erase(g_apWatchers.begin() + i);
   }
};

// detect if a sync signal is present
bool isSync(unsigned int idx)
{
   // check if we've received 4 squarewaves of matching timing
   int i;
   for (i = 0; i < 8; i += 2) {
      unsigned long t1 = timings[(idx + RING_BUFFER_SIZE - i) % RING_BUFFER_SIZE];
      unsigned long t0 = timings[(idx + RING_BUFFER_SIZE - i - 1) % RING_BUFFER_SIZE];
      if (t0 < (SYNC_HIGH - 100) || t0 > (SYNC_HIGH + 100) ||
          t1 < (SYNC_LOW - 100) || t1 > (SYNC_LOW + 100)) {
         return false;
      }
   }
   // check if there is a long sync period prior to the 4 squarewaves
   unsigned long t = timings[(idx + RING_BUFFER_SIZE - i) % RING_BUFFER_SIZE];
   if (t < (SYNC_LENGTH - 400) || t > (SYNC_LENGTH + 400) ||
       digitalRead(DATAPIN) != HIGH) {
      return false;
   }
   return true;
}

/* Interrupt 1 handler */
void handler()
{
   static unsigned long duration = 0;
   static unsigned long lastTime = 0;
   static unsigned int ringIndex = 0;
   static unsigned int syncCount = 0;

   NotifyHandlers();

   // calculating timing since last change
   long time = micros();
   duration = time - lastTime;
   lastTime = time;

   // store data in ring buffer
   ringIndex = (ringIndex + 1) % RING_BUFFER_SIZE;
   timings[ringIndex] = duration;

   if (isSync(ringIndex))
      g_apWatchers.push_back(new CDataWatcher());
}


int t2b(unsigned int t0, unsigned int t1)
{
   if (t0 > 50 && t0 < 650 && t1 > 50 && t1 < 650) {
      if (t0 > t1) {
         return 1;
      } else if (t1 > t0) {
         return 0;
      }
   }
   return -1;
}

int main(int argc, char *args[])
{
   if (wiringPiSetup() == -1)
   {
      printf("no wiring pi detected\n");
      return 0;
   }

   wiringPiISR(DATAPIN, INT_EDGE_BOTH, &handler);
   while (true)
      delay(100);
}
