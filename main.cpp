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
unsigned int syncIndex1 = 0;  // index of the first sync signal
unsigned int syncIndex2 = 0;  // index of the second sync signal


int t2b(unsigned int t0, unsigned int t1);

class CDataWatcher
{
public:
    CDataWatcher()
    {
       m_lLastTick = micros();
    }

    ~CDataWatcher()
    {
    }

    bool OnTick()
    {
       m_alTimings.push_back(micros() - m_lLastTick);
       m_lLastTick = micros();

       if (m_alTimings.size() == 122)
       {
          printf("Finished?\n");


          for (int i = 0; i < m_alTimings.size(); i += 2)
          {
             int bit = t2b(m_alTimings[i], m_alTimings[i + 1]);
             if (bit < 0)
             {
                printf("Errro!!!!!!!!!!!!!1\n");
                return false;
             }
          }
          printf("Success!!!!!!!!!!!!!!\n");
          return false;
       }

       return true;
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
   printf("4 square waves detected\n");
   // check if there is a long sync period prior to the 4 squarewaves
   unsigned long t = timings[(idx + RING_BUFFER_SIZE - i) % RING_BUFFER_SIZE];
   if (t < (SYNC_LENGTH - 400) || t > (SYNC_LENGTH + 400) ||
       digitalRead(DATAPIN) != HIGH) {
      return false;
   }
   printf("sync detected\n");
   return true;
}

void data_handler()
{
   if (data_index < 0)
      return;

   static unsigned long duration2 = 0;
   static unsigned long lastTime2 = 0;

   // calculating timing since last change
   long time = micros();
   duration2 = time - lastTime2;
   lastTime2 = time;

   // store data in ring buffer
   data_timings[data_index] = duration2;

   if (duration2 > 10000 && data_index > 25) {
      printf("count: %i\n", data_index);
      for (int i = 1; i < data_index; i++)
         printf("%i\t", data_timings[i]);
      printf("\n");

      int start_bit = data_index - 101;
      for (int i = start_bit; i < data_index; i++)
         printf("%c", data_timings[i] > 300 ? '1' : '0');

      printf("\n");
      data_index = -1;
   }
   if (data_index >= 0)
      data_index++;
}

/* Interrupt 1 handler */
void handler()
{
   static unsigned long duration = 0;
   static unsigned long lastTime = 0;
   static unsigned int ringIndex = 0;
   static unsigned int syncCount = 0;

   NotifyHandlers();

   // ignore if we haven't processed the previous received signal
   if (received == true) {
      return;
   }
   // calculating timing since last change
   long time = micros();
   duration = time - lastTime;
   lastTime = time;

   // store data in ring buffer
   ringIndex = (ringIndex + 1) % RING_BUFFER_SIZE;
   timings[ringIndex] = duration;

   // detect sync signal
   /*
   if (isSync(ringIndex)) {
     data_index = 0;
     syncCount ++;
     // first time sync is seen, record buffer index
     if (syncCount == 1) {
       syncIndex1 = (ringIndex+1) % RING_BUFFER_SIZE;
     }
     else if (syncCount == 2) {
       // second time sync is seen, start bit conversion
       syncCount = 0;
       syncIndex2 = (ringIndex+1) % RING_BUFFER_SIZE;
       unsigned int changeCount = (syncIndex2 < syncIndex1) ? (syncIndex2+RING_BUFFER_SIZE - syncIndex1) : (syncIndex2 - syncIndex1);
       printf("%i\n", changeCount);
       // changeCount must be 122 -- 60 bits x 2 + 2 for sync
       if (changeCount != 122 && false){
         received = false;
         syncIndex1 = 0;
         syncIndex2 = 0;
    printf("sync detected, incorrect change count\n");
       }
       else {
         received = true;
       }
     }

   }
   */
   if (isSync(ringIndex) && !in_data) {
      data_index = 0;
      syncCount++;
      g_apWatchers.push_back(new CDataWatcher());
      // first time sync is seen, record buffer index
      if (syncCount == 1) {
         syncIndex1 = (ringIndex + 1) % RING_BUFFER_SIZE;
         in_data = true;
      }
   }


   syncIndex2 = (ringIndex + 1) % RING_BUFFER_SIZE;
   unsigned int changeCount = (syncIndex2 < syncIndex1) ? (syncIndex2 + RING_BUFFER_SIZE - syncIndex1) : (syncIndex2 -
                                                                                                          syncIndex1);
   if (syncCount == 1)
      printf("changeCount: %i\n", changeCount);
   if (syncCount == 1 && changeCount == 122) {
      in_data = false;
      // second time sync is seen, start bit conversion
      //syncCount = 0;
      printf("%i\n", changeCount);
      // changeCount must be 122 -- 60 bits x 2 + 2 for sync
      if (changeCount != 122 && false) {
         received = false;
         syncIndex1 = 0;
         syncIndex2 = 0;
         syncCount = 0;
         printf("sync detected, incorrect change count\n");
      } else {
         system("/usr/bin/gpio edge 2 none");
         received = true;
      }
   }

   data_handler();
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
   printf("t0 = %d t1 = %d\n", t0, t1);
   return -1;
}

void loop()
{
   if (received == true) {
      // disable interrupt to avoid new data corrupting the buffer
      system("/usr/bin/gpio edge 2 none");

      // extract humidity value
      unsigned int startIndex, stopIndex;
      unsigned long humidity = 0;
      bool fail = false;
      startIndex = (syncIndex1 + (3 * 8 + 1) * 2) % RING_BUFFER_SIZE;
      stopIndex = (syncIndex1 + (3 * 8 + 8) * 2) % RING_BUFFER_SIZE;

      for (int i = startIndex; i != stopIndex; i = (i + 2) % RING_BUFFER_SIZE) {
         int bit = t2b(timings[i], timings[(i + 1) % RING_BUFFER_SIZE]);
         humidity = (humidity << 1) + bit;
         if (bit < 0) fail = true;
      }
      if (fail) { printf("Decoding error.\n"); }
      else {
         printf("Humidity: %d\% /  ", humidity);
      }

      // extract temperature value
      unsigned long temp = 0;
      fail = false;
      // most significant 4 bits
      startIndex = (syncIndex1 + (4 * 8 + 4) * 2) % RING_BUFFER_SIZE;
      stopIndex = (syncIndex1 + (4 * 8 + 8) * 2) % RING_BUFFER_SIZE;
      for (int i = startIndex; i != stopIndex; i = (i + 2) % RING_BUFFER_SIZE) {
         int bit = t2b(timings[i], timings[(i + 1) % RING_BUFFER_SIZE]);
         temp = (temp << 1) + bit;
         if (bit < 0) fail = true;
      }
      // least significant 7 bits
      startIndex = (syncIndex1 + (5 * 8 + 1) * 2) % RING_BUFFER_SIZE;
      stopIndex = (syncIndex1 + (5 * 8 + 8) * 2) % RING_BUFFER_SIZE;
      for (int i = startIndex; i != stopIndex; i = (i + 2) % RING_BUFFER_SIZE) {
         int bit = t2b(timings[i], timings[(i + 1) % RING_BUFFER_SIZE]);
         temp = (temp << 1) + bit;
         if (bit < 0) fail = true;
      }
      if (fail) { printf("Decoding error.\n"); }
      else {
         printf("Temperature: %d C  %d F\n", (int) ((temp - 1024) / 10 + 1.9 + .5),
                (int) (((temp - 1024) / 10 + 1.9 + 0.5) * 9 / 5 + 32));
      }
/*
	printf("raw data:\n");
	for(int i = syncIndex1; i != syncIndex2; i = (i+1)%RING_BUFFER_SIZE){
		printf("%d ",timings[i]);
	}
	printf("\n");
*/    // delay for 1 second to avoid repetitions
      delay(1000);
      received = false;
      syncIndex1 = 0;
      syncIndex2 = 0;

      // re-enable interrupt
      wiringPiISR(DATAPIN, INT_EDGE_BOTH, &handler);
      //wiringPiISR(DATAPIN,INT_EDGE_BOTH,&data_handler);
   } else
      delay(100);
}

int main(int argc, char *args[])
{
   if (wiringPiSetup() == -1) {
      printf("no wiring pi detected\n");
      return 0;
   }

   wiringPiISR(DATAPIN, INT_EDGE_BOTH, &handler);
   //wiringPiISR(DATAPIN,INT_EDGE_BOTH,&data_handler);
   while (true) {
      loop();
   }
   exit(0);
}
