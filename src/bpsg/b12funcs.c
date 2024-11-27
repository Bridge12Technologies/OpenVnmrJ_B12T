/*
 * Copyright (C) 2015  University of Oregon
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Apache License, as specified in the LICENSE file.
 *
 * For more information, see the LICENSE file.
 */
#include <stdio.h>
#include "acqparms.h"
#include "b12funcs.h"
#include "vnmrsys.h"
#include "abort.h"

extern void dps_off(void);
extern void dps_on(void);
extern int getPhase(int tab, int index);
extern int getRecPhase(int index);
extern void setPhaseInUse(int index);
extern int lIndex;

extern FILE *psgFile;

static char lastElem[512];

#define POWER_REGISTERS 4
int powerVals = 0;
int powerVal[POWER_REGISTERS];

void initPowerVal()
{
   int i;

   powerVals = 0;
   for (i=0; i < POWER_REGISTERS; i++)
   {
      powerVal[i] = -1;
   }
}

static int getPowerRegister(double amp)
{
   int val;
   int i;

   val = (int) ( (amp * 10.0) + 0.1);
   for (i=0; i < powerVals; i++)
   {
      if (val == powerVal[i])
         return(i);
   }
   return(0);
}

void addPowerVal(double amp)
{
   int val;
   int i;
   int found;

   val = (int) ( (amp * 10.0) + 0.1);
   found = 0;
   for (i=0; i < POWER_REGISTERS; i++)
   {
      if (val == powerVal[i])
         found = 1;
   }
   if ( ! found )
   {
      if (powerVals < POWER_REGISTERS)
      {
         powerVal[powerVals] = val;
         powerVals += 1;
      }
      else
      {
         abort_message("only %d different power levels are supported", POWER_REGISTERS);
      }
   }
}

void sendPowerVal()
{
   fprintf(psgFile,"POWERS %d %d %d %d %d\n",powerVals,
		   powerVal[0], powerVal[1], powerVal[2], powerVal[3]);
}

/*----------------------------------------------------------------
|   delay(time)/1
+----------------------------------------------------------------*/
void delay(double time)
{
   if (time <= 0.0)
      return;
   if (psgFile)
   {
      if (lastElem[0] != '\0')
         fprintf(psgFile,"%s\n",lastElem);
      sprintf(lastElem,"DELAY %g",time);
      totaltime += time;
   }
}

void rgpulse(double time, int phase, double rg1, double rg2)
{
   if (time <= 0.0)
      return;
   if (psgFile)
   {
      if (lastElem[0] != '\0')
         fprintf(psgFile,"%s\n",lastElem);
      sprintf(lastElem,"PULSE %g %d %g %g",
              time,getPhase(phase,lIndex),rg1,rg2);
      totaltime += time + rg1 + rg2;
   }
   else
   {
      setPhaseInUse(phase);
   }
}

void pulse(double time, int phase)
{
   if (time <= 0.0)
      return;
   rgpulse(time, phase, rof1, rof2 );
}

void offset(double frequency )
{
   if (psgFile)
   {
      if (lastElem[0] != '\0')
         fprintf(psgFile,"%s\n",lastElem);
      sprintf(lastElem,"OFFSET %g",frequency);
   }
}

void acquire(double datapnts, double dwell )
{
   acqtriggers = 1;
   if (psgFile)
   {
      delay(alfa);
      if (lastElem[0] != '\0')
         fprintf(psgFile,"%s\n",lastElem);
      sprintf(lastElem,"ACQUIRE %d", getRecPhase(lIndex) );
   }
}

void rlpower(double amp, int dev )
{
   if (psgFile)
   {
      int index;
      if (lastElem[0] != '\0')
         fprintf(psgFile,"%s\n",lastElem);
      index = getPowerRegister(amp);
      sprintf(lastElem,"POWER %d",index);
   }
   else
   {
      addPowerVal(amp);
   }
}

void status(int state)
{
   if (psgFile)
   {
      if (lastElem[0] != '\0')
         fprintf(psgFile,"%s\n",lastElem);
      if (state <= xmsize)
         sprintf(lastElem,"STATUS %d",(xm[state]=='y') ? 1 : 0);
      else
         sprintf(lastElem,"STATUS %d",(xm[xmsize-1]=='y') ? 1 : 0);
   }
}

void endElems()
{
   if (psgFile)
   {
      if (lastElem[0] != '\0')
         fprintf(psgFile,"%s\n",lastElem);
   }
   lastElem[0] = '\0';
}

void initElems()
{
   lastElem[0] = '\0';
}

void chkLoop(int maxPh, int totalScans, int *loops,  int *loopcnt, int *rem)
{
   int lps;

   lps = totalScans / maxPh;
   if (lps > 1)
   {
      *loops = lps;
      *loopcnt = maxPh;
      *rem = totalScans - (lps * maxPh);
   }
   else
   {
      *loops = 0;
      *loopcnt = 0;
      *rem = totalScans;
   }
}

void mpsTune(double freq, double width, double power)
{
   dps_off();
   if (psgFile)
   {
      fprintf(psgFile,"MPSTUNE %s/mpsData %g %g %g\n",
		      curexp, freq, width, power);
      totaltime = (double) ((int) (np+0.5)) * 0.057;
   }
   dps_on();
}

void mTune(double freq, double width)
{
   dps_off();
   if (psgFile)
   {
      if (lastElem[0] != '\0')
         fprintf(psgFile,"%s\n",lastElem);
      sprintf(lastElem,"MTUNE %g %g %g\n", freq, width, d1);
      totaltime = (double) ((int) (np+0.5)) * 2.0 * 1e-6 + d1;
   }
   dps_on();
}
