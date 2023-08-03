 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include "inc/spinapi.h"
#include "shrstatinfo.h"

extern void initError( char *infoPath );
extern void dataError( char *infoPath, int ct, int ix );
extern void saveData( int ct, int ix, int arraydim,
                      int *real, int *imag, char *info);
extern void writeConf(const char *confPath);
extern void showConf(const char *confPath);
extern void abortExp( char *infoPath, char *code, int ct, int ix );
extern void sleepMilliSeconds(int msecs);
extern void sleepMicroSeconds(int usecs);
extern void endComm();
extern void requestMpsData(double freq, double width, double power,
	        char *dataPath, char *infoPath );
extern void saveBsData( int *real, int *imag, char *infoPath, int ct);
extern void endMpsData();
extern void sendExpMsg(char *cmd);

#define DEBUG

// User friendly names for the phase registers of the cos and sin channels
#define PHASE000    0
#define PHASE090    1
#define PHASE180    2
#define PHASE270    3

// User friendly names for the control bits
#define TX_ENABLE       1       
#define TX_DISABLE      0
#define PHASE_RESET     1
#define NO_PHASE_RESET  0
#define DO_TRIGGER      1
#define NO_TRIGGER      0
#define NO_DATA         0
#define NO_SHAPE        0
#define AMP0            0
#define MIN_DELAY       70.0  // nanosec
#define MTUNE_DELAY     1000.0 // nanosec

#define MPS_BIT      0
#define RECV_BIT     1
#define AMP_BIT      2
#define AMPBLANK_BIT 3

#define RECV_UNBLANK    (1 << RECV_BIT)
#define AMP_UNBLANK     (1 << AMP_BIT)
#define OUTBLANK        (1 << AMPBLANK_BIT)

struct _PSelem {
                 char inst[64];
                 char vals[256];
                 struct _PSelem *next;
              };
typedef struct _PSelem PSelem;

PSelem *PSstart = NULL;

struct _Globals {
                  int debug;
                  int debugSC;
                  int board_number;
                  int board_open;
                  int board_start;
                  int bypass_fir;
                  int arraydim;
                  int complex_points;   // number of complex pairs
                  int *real;
                  int *imag;
                  int HWTriggerFlag; // do we wait for a HWTrigger?
                  double adc_frequency;
                  double totalTime;
                  char DataDir[512];
                  char InfoFile[512];
                  char CodePath[512];
                };
typedef struct _Globals Globals;

struct _Exps {
                  int nt;
                  int segments;
                  int dec_amount;
                  int loop;
                  int loopCnt;
                  int elem;
                  int phaseReset;
                  int opCode;
                  int opCodeData;
                  int mpsStatus;
        		  int useAmp;
	        	  int useShape;
                  double sw_kHz;
                  double actual_sw;
                  double at;
                  double sfrq;
                  double exp_time;
             };
typedef struct _Exps Exps;


static FILE *debugFile = NULL;
// Constants for receiver phase cycling
static int recReal[4] = {1,1,0,0};
static int recImag[4] = {1,0,0,1};
static int recSwap[4] = {0,1,0,1};
int *re = NULL;
int *im = NULL;

void diagMessage(const char *format, ...)
{
   va_list vargs;
   char    str[2048];

   va_start(vargs, format);
   vsnprintf(str, 2040, format,vargs);
   va_end(vargs);
   if (debugFile == NULL)
   {
      debugFile = fopen("/vnmr/tmp/b12out","w");
   }
   fprintf(debugFile,"%s",str);
   fflush(debugFile);
}

PSelem *newPsElem()
{
   static PSelem *r;

   if ( (r = (PSelem *) malloc(sizeof(PSelem))) == NULL )
   {
      diagMessage("B12proc out of memory\n");
      exit(EXIT_FAILURE);
   }
   r->inst[0] = '\0';
   r->next = NULL;
   return(r);
}

static int readPS(Globals *globals)
{
   PSelem *r;
   FILE *inputFile;
   char inst[64];
   char vals[256];

    /* Open the input file */
   inputFile = fopen(globals->CodePath, "r");
    /* does file exist? */
   if (inputFile == NULL)
   {
      debugFile = fopen("/vnmr/tmp/b12noFile","w");
      diagMessage("B12proc input file %s does not exist\n",globals->CodePath);
      abortExp( & (globals->InfoFile[0]), globals->CodePath, 1, 1);
      PSstart = NULL;
      return(0);
   }
   r =  newPsElem();
   PSstart = r;
   while (fscanf(inputFile,"%s %[^\n]\n",inst,vals) != EOF)
   {
      if (r->inst[0] != '\0')
      {
         r->next =  newPsElem();
         r = r->next;
      }
      strcpy(r->inst,inst);
      strcpy(r->vals,vals);
   }
   fclose(inputFile);

   r = PSstart;
   while ( (r != NULL) && r->inst[0] != '\0')
   {
      diagMessage("%s %s\n",r->inst,r->vals);
      r = r->next;
   }
   return(0);
}

void setGlobalsDefault(Globals *globals, const char *path)
{
   globals->debug = 0;
   globals->debugSC = 0;
   globals->board_number = 0;
   globals->board_open = 0;
   globals->board_start = 0;
   globals->bypass_fir = 1;
   globals->adc_frequency = 75.0;
   globals->DataDir[0] = '\0';;
   globals->arraydim = 1;
   globals->complex_points = 0;
   globals->HWTriggerFlag = 0;
   globals->totalTime = 0.0;
   globals->real = NULL;
   globals->imag = NULL;
   strcpy(globals->InfoFile, path);
   strcpy(globals->CodePath, path);
   strcat(globals->CodePath,".Code");

}

void printGlobals(Globals *globals)
{
   diagMessage("Globals\n\n");
   diagMessage("  debug:          %d\n",globals->debug);
   diagMessage("  debugSC:        %d\n",globals->debugSC);
   diagMessage("  board_number:   %d\n",globals->board_number);
   diagMessage("  board_open:     %d\n",globals->board_open);
   diagMessage("  board_start:    %d\n",globals->board_start);
   diagMessage("  bypass_fir:     %d\n",globals->bypass_fir);
   diagMessage("  complex_points: %d\n",globals->complex_points);
   diagMessage("  arraydim:       %d\n",globals->arraydim);
   diagMessage("  adc_frequency:  %g\n",globals->adc_frequency);
   diagMessage("  DataDir:        %s\n",globals->DataDir);
   diagMessage("  InfoFile:       %s\n",globals->InfoFile);
   diagMessage("  HWTriggerFlag:  %d\n",globals->HWTriggerFlag);
}

void printExps(Exps *exps)
{
   diagMessage("Exps\n\n");
   diagMessage("  nt:            %d\n",exps->nt);
   diagMessage("  segments:      %d\n",exps->segments);
   diagMessage("  dec_amount:    %d\n",exps->dec_amount);
   diagMessage("  sw_kHz:        %g\n",exps->sw_kHz);
   diagMessage("  actual_sw:     %g\n",exps->actual_sw);
   diagMessage("  at:            %g\n",exps->at);
   diagMessage("  sfrq:          %g\n",exps->sfrq);
}

int initSystem(Globals *globals)
{
   if (globals->debug)
      diagMessage("call pb_count_boards = ");
   int numBoards = pb_count_boards();
   if (globals->debug)
      diagMessage("%d\n", numBoards);

   if (numBoards <= 0)
   {
      diagMessage("No RadioProcessor boards were detected in your system.\n");
      return -1;
   }
   if (globals->board_number > numBoards )
   {
      diagMessage("Invalid board number. Use (0-%d).\n", numBoards - 1);
      return -1;
   }
   if (globals->debug)
      diagMessage("call pb_select_board(%d)\n", globals->board_number);
   pb_select_board(globals->board_number);
   if (globals->debug)
      diagMessage("call pb_init\n");
   if (pb_init())
   {
      diagMessage("Error initializing board: %s\n", pb_get_error ());
      return -1;
   }
   globals->board_open = 1;
   return(0); 
}

int configSystem(Globals *globals)
{
   int i;

   if (globals->debug)
      diagMessage("call pb_set_defaults\n");
   pb_set_defaults ();
   if (globals->debug)
      diagMessage("call pb_core_Clock(%g)\n", globals->adc_frequency);
   pb_core_clock (globals->adc_frequency);
	
   // Load the shape parameters
   float shape_data[1024];

   for (i = 0; i < 1024; i++)  // rectangular shape
      shape_data[i] = 1.0;
   if (globals->debug)
      diagMessage("call pb_dds_load(%d)\n", DEVICE_SHAPE);
   pb_dds_load (shape_data, DEVICE_SHAPE);
   return(0);
}
	
int configExp(Globals *globals, Exps *exps)
{
   int cmd = 0;

   if (globals->bypass_fir)
      cmd = BYPASS_FIR;
   if (globals->debug)
      diagMessage("call pb_setup_filters(%g,%d,%d) = ", exps->sw_kHz/1000.0, exps->nt, cmd);
   exps->dec_amount = pb_setup_filters (exps->sw_kHz/1000.0, exps->nt, cmd);
   if (globals->debug)
      diagMessage("%d\n", exps->dec_amount);
   if (exps->dec_amount <= 0)
      exps->dec_amount = 1;
   exps->actual_sw = (globals->adc_frequency*1000.0) /(double) exps->dec_amount;
   exps->at = (double) globals->complex_points / (exps->actual_sw);
   diagMessage("actual_sw %g adc=%g decimate=%d at=%g\n",
                  exps->actual_sw, globals->adc_frequency,
                  exps->dec_amount, exps->at);

   if (globals->debug)
      diagMessage("call pb_overflow(1, 0)\n");
   pb_overflow (1, 0);		//Reset the overflow counters
   if (globals->debug)
      diagMessage("call pb_scan_count(1)\n");
   pb_scan_count (1);		//Reset scan counter

   if (globals->debug)
      diagMessage("call pb_set_num_points(%d)\n", globals->complex_points);
   pb_set_num_points(globals->complex_points);
   if (globals->debug)
      diagMessage("call pb_set_scan_segments(1)\n");
   pb_set_scan_segments(1);

   if (globals->debug)
      diagMessage("call pb_start_programming(%d)\n", FREQ_REGS);
   pb_start_programming(FREQ_REGS);
   if (globals->debug)
      diagMessage("call pb_set_freq(%g)\n", exps->sfrq);
   pb_set_freq(exps->sfrq);
   if (globals->debug)
      diagMessage("call pb_stop_programming\n");
   pb_stop_programming();

   //Real Phase
   if (globals->debug)
   {
      diagMessage("call pb_start_programming(%d)\n", COS_PHASE_REGS);
      diagMessage("call pb_set_phase 0, 90, 180, 270\n");
   }
   pb_start_programming (COS_PHASE_REGS);
   pb_set_phase (0.0);
   pb_set_phase (90.0);
   pb_set_phase (180.0);
   pb_set_phase (270.0);
   if (globals->debug)
      diagMessage("call pb_stop_programming\n");
   pb_stop_programming ();
 
   //Imaginary Phase
   if (globals->debug)
   {
      diagMessage("call pb_start_programming(%d)\n", SIN_PHASE_REGS);
      diagMessage("call pb_set_phase 0, 90, 180, 270\n");
   }
   pb_start_programming (SIN_PHASE_REGS);
   pb_set_phase (0.0);
   pb_set_phase (90.0);
   pb_set_phase (180.0);
   pb_set_phase (270.0);
   if (globals->debug)
      diagMessage("call pb_stop_programming\n");
   pb_stop_programming ();
 
   //Transmitted Phase
   if (globals->debug)
   {
      diagMessage("call pb_start_programming(%d)\n", TX_PHASE_REGS);
      diagMessage("call pb_set_phase 0, 90, 180, 270\n");
   }
   pb_start_programming (TX_PHASE_REGS);
   pb_set_phase (0.0);
   pb_set_phase (90.0);
   pb_set_phase (180.0);
   pb_set_phase (270.0);
   if (globals->debug)
      diagMessage("call pb_stop_programming\n");
   pb_stop_programming ();

   exps->exp_time = 0.0;
   return(0);
}

int getData(Globals *globals, Exps *exps)
{
   int ct = 0;
   double msecPerCT;

   msecPerCT = 1000.0 * exps->exp_time / exps->nt;
   diagMessage("wait %d ms for exp to finish\n",(int) (exps->exp_time*1000.0));
   globals->totalTime += exps->exp_time;
   while (ct < exps->nt)
   {
       double waitTime;

       waitTime = msecPerCT;
       ct++;
       setStatCT(ct);
       while ( (int) waitTime / 1000 > 1)
       {
          if ( access(globals->CodePath, F_OK) )
          {
             abortExp( & (globals->InfoFile[0]), globals->CodePath, ct, exps->elem);
             return(-1);
          }
          sleepMilliSeconds(1000);
          waitTime -= 1000.0;
       }
       if ( access(globals->CodePath, F_OK) )
       {
          abortExp( & (globals->InfoFile[0]), globals->CodePath, ct, exps->elem);
          return(-1);
       }
       sleepMilliSeconds((int) (waitTime) );
   }
   if (globals->debug)
      diagMessage("call pb_read_status\n");
   while (!(pb_read_status() & STATUS_STOPPED))
   {
       if ( access(globals->CodePath, F_OK) )
       {
          abortExp( & (globals->InfoFile[0]), globals->CodePath, ct, exps->elem);
          return(-1);
       }

      pb_sleep_ms (1);
   }

   if (globals->debug)
      diagMessage("call pb_get_data(%d, real, imag)\n", globals->complex_points);
   if( pb_get_data (globals->complex_points, re, im) < 0 )
   {
      diagMessage("Failed to retrieve data\n");
      dataError( & (globals->InfoFile[0]), ct, exps->elem);
      return -1;
   }
   globals->real = re;
   globals->imag = im;
   if (globals->debug)
   {
      FILE *fd;
      int i;
      fd = fopen("/vnmr/tmp/b12data","w");
      for (i=0; i< globals->complex_points; i++)
      {
         fprintf(fd,"%d %d\n", re[i], im[i]);
      }
      fclose(fd);
   }
   diagMessage("save data to %s\n",globals->DataDir);
   return(0);
}

void resetExp(int pbRes, Exps *exps)
{
   if (exps->opCode == LOOP)
   {
      diagMessage("set loop index to %d\n",pbRes);
      exps->loop = pbRes;
   }
   if (exps->opCode == END_LOOP)
   {
// diagMessage("end_loop exptime= %g loop count= %d\n",exps->exp_time, exps->loopCnt);
      exps->exp_time *= exps->loopCnt;
// diagMessage("end_loop new exptime= %g\n",exps->exp_time);
   }

   exps->phaseReset = NO_PHASE_RESET;
   exps->opCode = CONTINUE;
   exps->opCodeData = NO_DATA;
}

int getMpsData(int del, char *path,  Globals *globals, Exps *exps)
{
   int msec;
   int count;

   // del is in msec
   msec = (del * globals->complex_points) + 100;
   count = msec / 100;
   diagMessage("start getMPS wait %d ms\n", count * 100);
   // wait for tuning to start
   while (count > 0)
   {
      count--;
      sleepMilliSeconds(100);
      if ( access(globals->CodePath, F_OK) )
      {
	 unlink(path);
         diagMessage("getMpsData Exp aborted while waiting\n");
         abortExp( & (globals->InfoFile[0]), globals->CodePath, 1, exps->elem);
//	 if (count)
//            sleepMilliSeconds(100*count);
         return(-1);
      }
   }
   // now wait for data
   count = 100;
   while (count > 0)
   {
      count--;
      if ( ! access(path, F_OK) )
      {
         FILE *fd;
	 int *ptr;
	 int index;
	 int val;

         diagMessage("Tune data present count= %d\n", 100 - count);
         fd = fopen(path,"r");
	 ptr = globals->real;
	 for (index=0; index<globals->complex_points; index++)
         {
            if (fscanf(fd,"%d\n", &val))
               *ptr++ = val;
	 }
	 fclose(fd);
	 unlink(path);
         return(0);
      }
      sleepMilliSeconds(100);
      if ( access(globals->CodePath, F_OK) )
      {
	 unlink(path);
         abortExp( & (globals->InfoFile[0]), globals->CodePath, 1, exps->elem);
         diagMessage("getMpsData Exp aborted after waiting\n");
         return(-1);
      }
   }
   abortExp( & (globals->InfoFile[0]), globals->CodePath, 1, exps->elem);
   diagMessage("getMpsData failed to appear\n");
   return(-1);
}

int main (int argc, char *argv[])
{
   PSelem *r;
   Globals globals;
   Exps exps;
   int pbRes;
	
   // Special cases to handle configuration file
   if (argc == 3)
   {
      if ( ! strcmp(argv[1],"Conf") )
         writeConf(argv[2]);
      if ( ! strcmp(argv[1],"show") )
         showConf(argv[2]);
      if (debugFile != NULL)
         fclose(debugFile);
      exit(EXIT_SUCCESS);
   }

   if (argc == 1)
   {
      diagMessage("%s must be called with Codepath\n", argv[0]);
      if (debugFile != NULL)
         fclose(debugFile);
      exit(EXIT_SUCCESS);
   }

   //
   //Process Arguments
   //
		
   setGlobalsDefault( &globals, argv[1] );
   initExpStatus(0);
   readPS(&globals);
   r = PSstart;
   while ( (r != NULL) && r->inst[0] != '\0')
   {
//      diagMessage("%s %s\n",r->inst,r->vals);
      if ( ! strcmp(r->inst,"DEBUG") )
      {
         globals.debug = atoi(r->vals);
      }
      else if ( ! strcmp(r->inst,"BOARD_NUMBER") )
      {
         globals.board_number = atoi(r->vals);
      }
      else if ( ! strcmp(r->inst,"BYPASS_FIR") )
      {
         globals.bypass_fir = atoi(r->vals);
      }
      else if ( ! strcmp(r->inst,"NUMBER_POINTS") )
      {
         globals.complex_points = atoi(r->vals) / 2;
         if (re == NULL)
         {
            diagMessage("malloc %d ints\n",globals.complex_points );
            re = (int *)malloc(sizeof(int) * globals.complex_points);
            im = (int *)malloc(sizeof(int) * globals.complex_points);
         }
      }
      else if ( ! strcmp(r->inst,"ADC_FREQUENCY") )
      {
         globals.adc_frequency = atof(r->vals);
      }
      else if ( ! strcmp(r->inst,"FILE") )
      {
         strcpy(globals.DataDir,r->vals);
      }
      else if ( ! strcmp(r->inst,"ARRAYDIM") )
      {
         globals.arraydim = atoi(r->vals);
      }
      else if ( ! strcmp(r->inst,"PULSEPROG_START") )
      {
         // scan values
         int HWTriggerFlag=0;
         double pulseprogVal;
         sscanf(r->vals,"%lg %d", &pulseprogVal, &HWTriggerFlag);

         exps.elem= pulseprogVal;
         exps.opCode = CONTINUE;
         exps.mpsStatus = 0;
	      exps.useAmp = AMP0;
	      exps.useShape = NO_SHAPE;
         resetExp(0, &exps);
         if (exps.elem == 1)
         {
            if (globals.debug)
               printGlobals( &globals);
            if (initSystem( &globals))
            {
               initError( & (globals.InfoFile[0]) );
               break;
            }
            if (globals.debugSC)
            {
               if (globals.debug)
                  diagMessage("call pb_set_debug(1)\n");
               pb_set_debug(1);
            }
            if (configSystem( &globals))
               break;
         }
         else
         {
            setStatElem(exps.elem);
            setStatCT(1);
         }
         if ( access(globals.CodePath, F_OK) )
         {
               abortExp( & (globals.InfoFile[0]), globals.CodePath, 1, exps.elem);
               break;
         }
         // set HWTriggerFlag
         if (HWTriggerFlag != 0)
         {
            globals.HWTriggerFlag=HWTriggerFlag;
         }

      }
      else if ( ! strcmp(r->inst,"RATTN") )
      {
	 char cmd[512];
	 int ret __attribute__((unused));

	 sprintf(cmd,"/vnmr/bin/B12ATTNControl %s\n",r->vals);
         diagMessage("set atten with %s\n", cmd);
         ret = system(cmd);
      }
      else if ( ! strcmp(r->inst,"MPS") )
      {
	 char cmd[512];

	 sprintf(cmd,"mpsCntrl mpsMode %s\n",r->vals);
	 sendExpMsg(cmd);
      }
      else if ( ! strcmp(r->inst,"MPSPOWER") )
      {
	 char cmd[512];

	 sprintf(cmd,"mpsCntrl mpsPower %s\n",r->vals);
	 sendExpMsg(cmd);
      }
      else if ( ! strcmp(r->inst,"POWERS") )
      {
         int i;
         int num;
	 int val[4];

         sscanf(r->vals,"%d %d %d %d %d", &num, &(val[0]), &(val[1]),
                                          &(val[2]), &(val[3]));
	 for (i=0; i<num; i++)
	 {
            if (globals.debug)
               diagMessage("call pb_set_amp(%g, %d)\n",
                           (double) val[i]/1000.0, i);
            pb_set_amp((double) val[i]/1000.0, i);
	 }
	 exps.useAmp = AMP0;
	 exps.useShape = NO_SHAPE;
      }
      else if ( ! strcmp(r->inst,"SPECTROMETER_FREQUENCY") )
      {
         exps.sfrq= atof(r->vals);
      }
      else if ( ! strcmp(r->inst,"NUMBER_OF_SCANS") )
      {
         exps.nt= atoi(r->vals);
         exps.segments = 1;
      }
      else if ( ! strcmp(r->inst,"SPECTRAL_WIDTH") )
      {
         exps.sw_kHz= atof(r->vals)/1000.0;
      }
      else if ( ! strcmp(r->inst,"POWER") )
      {
	 int num = atoi(r->vals);
	 exps.useAmp = num;
	 exps.useShape = (num > 0) ? 1 : NO_SHAPE;
      }
      else if ( ! strcmp(r->inst,"PULSE_ELEMENTS") )
      {
         configExp( &globals, &exps);
         if (globals.debug)
            printExps( &exps);
         if (globals.debug)
            diagMessage("call pb_start_programming(%d)\n", PULSE_PROGRAM);
         pb_start_programming (PULSE_PROGRAM);
         if (globals.HWTriggerFlag != 0)
         {
            if (globals.debug)
            {
               diagMessage("HWTriggerFlag is set to 1 - add 70ns WAIT cmd\n");
               diagMessage("call pb_inst_radio_shape(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%g) = ",
                                         0, PHASE090, PHASE000, 0,
               TX_DISABLE, 0,
               NO_TRIGGER, NO_SHAPE, AMP0, 0,
               0, NO_DATA, MIN_DELAY);
            }
            pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                           TX_DISABLE, NO_PHASE_RESET,
                           NO_TRIGGER, NO_SHAPE, AMP0,
                           0,
                           0, NO_DATA, MIN_DELAY); //spincore.WAIT seems to be not working -> replaced with 0
         }
      }
      else if ( ! strcmp(r->inst,"PHASE_RESET") )
      {
         exps.phaseReset = PHASE_RESET;
      }
      else if ( ! strcmp(r->inst,"NSC_LOOP") )
      {
         exps.opCode = LOOP;
         exps.loopCnt = exps.opCodeData = atoi(r->vals);
/*
         pbRes = pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
               TX_DISABLE, exps.phaseReset,
               NO_TRIGGER, NO_SHAPE, AMP0, exps.mpsStatus,
               exps.opCode, exps.opCodeData, MIN_DELAY);
         resetExp(pbRes, &exps);
 */
      }
      else if ( ! strcmp(r->inst,"DELAY") )
      {
         double duration = atof(r->vals);
         if (duration > 0.0)
         {
            if (globals.debug)
            {
               diagMessage("call pb_inst_radio_shape(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%g) = ",
                                         0, PHASE090, PHASE000, 0,
               TX_DISABLE, exps.phaseReset,
               NO_TRIGGER, NO_SHAPE, AMP0, exps.mpsStatus,
               exps.opCode, exps.opCodeData, duration * 1e9);
            }
            pbRes = pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
               TX_DISABLE, exps.phaseReset,
               NO_TRIGGER, NO_SHAPE, AMP0, exps.mpsStatus,
               exps.opCode, exps.opCodeData, duration * 1e9);
            if (globals.debug)
               diagMessage("%d (for delay)\n", pbRes);
            exps.exp_time += duration;
            resetExp(pbRes, &exps);
         }
            
      }
      else if ( ! strcmp(r->inst,"PULSE") )
      {
         int iphase;
         double duration;
         double phase;
         double blank_delay;
         double ringdown_delay;
         sscanf(r->vals,"%lg %lg %lg %lg",
                &duration, &phase, &blank_delay, &ringdown_delay);
         iphase = (int) (phase+0.001);
         if (duration > 0.0)
         {
            if (blank_delay > 0.0)
            {
               if (globals.debug)
               {
                  diagMessage("call pb_inst_radio_shape(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%g) = ",
                                            0, PHASE090, PHASE000, 0,
                  TX_DISABLE, exps.phaseReset,
                  NO_TRIGGER, NO_SHAPE, AMP0,
                  AMP_UNBLANK + exps.mpsStatus,
                  CONTINUE, NO_DATA, blank_delay * 1e9);
               }
               pbRes = pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                  TX_DISABLE, exps.phaseReset,
                  NO_TRIGGER, NO_SHAPE, AMP0,
                  AMP_UNBLANK + exps.mpsStatus,
                  CONTINUE, NO_DATA, blank_delay * 1e9);
               if (globals.debug)
                  diagMessage("%d (for blank_delay > 0)\n", pbRes);
               exps.exp_time += blank_delay;
            }
            if (ringdown_delay > 0.0)
            {
               if (globals.debug)
               {
                  diagMessage("call pb_inst_radio_shape(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%g) = ",
                                            0, PHASE090, PHASE000, iphase,
                  TX_ENABLE, exps.phaseReset,
                  NO_TRIGGER, exps.useShape, exps.useAmp,
		            AMP_UNBLANK + exps.mpsStatus,
                  CONTINUE, NO_DATA, duration * 1e9);
               }
               pbRes = pb_inst_radio_shape (0, PHASE090, PHASE000, iphase,
                  TX_ENABLE, exps.phaseReset,
                  NO_TRIGGER, exps.useShape, exps.useAmp,
		            AMP_UNBLANK + exps.mpsStatus,
                  CONTINUE, NO_DATA, duration * 1e9);
               if (globals.debug)
               {
                  diagMessage("%d (for ringdown_delay > 0)\n", pbRes);
                  diagMessage("call pb_inst_radio_shape(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%g) = ",
                                            0, PHASE090, PHASE000, 0,
                  TX_DISABLE, exps.phaseReset,
                  NO_TRIGGER, NO_SHAPE, AMP0,
		            exps.mpsStatus,
                  exps.opCode, exps.opCodeData, ringdown_delay * 1e9);
               }
               pbRes = pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                  TX_DISABLE, exps.phaseReset,
                  NO_TRIGGER, NO_SHAPE, AMP0,
		            exps.mpsStatus,
                  exps.opCode, exps.opCodeData, ringdown_delay * 1e9);
               if (globals.debug)
               {
                  diagMessage("%d (for ringdown_delay > 0)\n", pbRes);
               }
            }
            else
            {
               if (globals.debug)
               {
                  diagMessage("call pb_inst_radio_shape(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%g) = ",
                                            0, PHASE090, PHASE000, iphase,
                  TX_ENABLE, exps.phaseReset,
                  NO_TRIGGER, exps.useShape, exps.useAmp,
		            AMP_UNBLANK + exps.mpsStatus,
                  exps.opCode, exps.opCodeData, duration * 1e9);
               }
               pbRes = pb_inst_radio_shape (0, PHASE090, PHASE000, iphase,
                  TX_ENABLE, exps.phaseReset,
                  NO_TRIGGER, exps.useShape, exps.useAmp,
		            AMP_UNBLANK + exps.mpsStatus,
                  exps.opCode, exps.opCodeData, duration * 1e9);
               if (globals.debug)
               {
                  diagMessage("%d (for ringdown_delay <= 0)\n", pbRes);
               }
            }
            exps.exp_time += duration + ringdown_delay;
            resetExp(pbRes, &exps);
         }
      }
      else if ( ! strcmp(r->inst,"ACQUIRE") )
      {
         int recIndex = atoi(r->vals);
         if (globals.debug)
         {
            diagMessage("call pb_inst_radio_shape(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%g) = ",
                                              0, PHASE090, PHASE000, 0,
               TX_DISABLE, exps.phaseReset,
               DO_TRIGGER, NO_SHAPE, AMP0,
               recReal[recIndex], recImag[recIndex], recSwap[recIndex],
               RECV_UNBLANK + exps.mpsStatus + OUTBLANK,
               exps.opCode, exps.opCodeData, exps.at * 1e6);
         }
         pbRes = pb_inst_radio_shape_cyclops (0, PHASE090, PHASE000, 0,
               TX_DISABLE, exps.phaseReset,
               DO_TRIGGER, NO_SHAPE, AMP0,
               recReal[recIndex], recImag[recIndex], recSwap[recIndex],
               RECV_UNBLANK + exps.mpsStatus + OUTBLANK,
               exps.opCode, exps.opCodeData, exps.at * 1e6);
         if (globals.debug)
         {
            diagMessage("%d (for acquire)\n", pbRes);
         exps.exp_time += exps.at * 1e-3;
         resetExp(pbRes, &exps);
         }
      }
      else if ( ! strcmp(r->inst,"STATUS") )
      {
         int state;

         state = atoi(r->vals);
         exps.mpsStatus = state << MPS_BIT;
      }     
      else if ( ! strcmp(r->inst,"NSC_ENDLOOP") )
      {
         exps.opCode = END_LOOP;
         exps.opCodeData = exps.loop;
      }
      else if ( ! strcmp(r->inst,"PULSEPROG_DONE") )
      {
/*
         pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
               TX_DISABLE, exps.phaseReset,
               NO_TRIGGER, NO_SHAPE, AMP0, exps.mpsStatus,
               END_LOOP, exps.loop, MIN_DELAY);
         exps.exp_time += MIN_DELAY * 1e-9;
 */

          if (globals.debug)
          {
               diagMessage("call pb_inst_radio_shape(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%g) (done)\n",
                              0, PHASE090, PHASE000, 0,
               TX_DISABLE, NO_PHASE_RESET,
               NO_TRIGGER, NO_SHAPE, AMP0,
	           0,
               STOP, NO_DATA, MIN_DELAY);
          }
         pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
               TX_DISABLE, NO_PHASE_RESET,
               NO_TRIGGER, NO_SHAPE, AMP0,
	           0,
               STOP, NO_DATA, MIN_DELAY);
         exps.exp_time += MIN_DELAY * 1e-9;

          if (globals.debug)
          {
             diagMessage("call pb_stop_programming\n");
          }
         pb_stop_programming();
//         exps.exp_time *= exps.nt;
          if (globals.debug)
          {
               diagMessage("call pb_reset\n");
          }
         pb_reset();

         if (globals.debug)
         {
            diagMessage("call pb_start\n");
         }
         pb_start();
         globals.board_start = 1;
         if (getData( &globals, &exps))
         {
            break;
         }
         saveData( exps.nt, atof(r->vals), globals.arraydim,
                  globals.real, globals.imag,
                  &(globals.InfoFile[0]));

      }
      else if ( ! strcmp(r->inst,"MTUNE") )
      {
         double freq;
         double width;
         double delay;
         double mtuneStart;
         double mtuneIncr;
	     int ct = 1;

         setStatAcqState(ACQ_TUNING);
         sscanf(r->vals,"%lg %lg %lg", &freq, &width, &delay);
         diagMessage("Start mtune\n");
	     width *= 1e-6; // convert to MHz
         mtuneStart = freq - (width/2.0);
         mtuneIncr = width / (double) (globals.complex_points - 1);
         exps.exp_time = 2.0 * MTUNE_DELAY * 1e-9 * globals.complex_points + delay;
         exps.nt = 1;
         if (globals.debug)
         {
               diagMessage("call pb_stop_programming MTUNE case\n");
         }
         pb_stop_programming();
         while ( 1 == 1)
         {
            int aborted;
            int incr;
	        int loops;
            int resetTTL;

            //Reset scan counter to avoid time averaging
            pb_scan_count (1);
            pb_set_num_points(1);
            pb_set_scan_segments(globals.complex_points);
            pb_start_programming(FREQ_REGS);
            pb_set_freq(mtuneStart);
            pb_stop_programming();
            pb_start_programming(PULSE_PROGRAM);
            // Initial delay to set RF routing
            if (ct == 1)
            {
               pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                  TX_DISABLE, NO_PHASE_RESET,
                  NO_TRIGGER, NO_SHAPE, AMP0,
                  0,
                  CONTINUE, NO_DATA, delay * 1e9);
            }
            // Loop np times, each time acquire one data point
            loops = pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                  TX_DISABLE, PHASE_RESET,
                  NO_TRIGGER, exps.useShape, exps.useAmp,
		          AMP_UNBLANK + exps.mpsStatus,
                  LOOP, globals.complex_points, MIN_DELAY);
            // Wait for new frequency to be set
            pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                  TX_DISABLE, NO_PHASE_RESET,
                  NO_TRIGGER, exps.useShape, exps.useAmp,
		          AMP_UNBLANK + exps.mpsStatus,
                  WAIT, NO_DATA, MIN_DELAY);
            // Delay for frequency to settle
            pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                  TX_ENABLE, NO_PHASE_RESET,
                  NO_TRIGGER, exps.useShape, exps.useAmp,
		          AMP_UNBLANK + exps.mpsStatus,
                  CONTINUE, NO_DATA, MTUNE_DELAY);
            // Acquire one data point and loop to top
            pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                  TX_ENABLE, NO_PHASE_RESET,
                  DO_TRIGGER, exps.useShape, exps.useAmp,
		          RECV_UNBLANK + AMP_UNBLANK + exps.mpsStatus,
                  CONTINUE, NO_DATA, MTUNE_DELAY);
            // Add some small delay with nothing
            pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                  TX_DISABLE, NO_PHASE_RESET,
                  NO_TRIGGER, exps.useShape, exps.useAmp,
		          AMP_UNBLANK + exps.mpsStatus,
                  END_LOOP, loops, MTUNE_DELAY);
            pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                  TX_DISABLE, NO_PHASE_RESET,
                  NO_TRIGGER, NO_SHAPE, AMP0,
		          0,
                  STOP, NO_DATA, MIN_DELAY);
            pb_stop_programming();
            pb_reset();
            pb_start();
            globals.board_start = 1;
            incr = 0;
            aborted = 0;
            if (globals.debug)
            {
               diagMessage("Starting to call while globals.complex_points loop\n");
            }
            while (incr < globals.complex_points )
            {
               //this will take miliseconds!
               while (!(pb_read_status() & STATUS_WAITING))
               {
                  sleepMicroSeconds(1000);
               }
               if ( access(globals.CodePath, F_OK) )
               {
                  diagMessage("mtune aborted\n");
                  aborted = 1;
                  break;
               }
               pb_start_programming(FREQ_REGS);
               pb_set_freq(mtuneStart + mtuneIncr*incr);
               pb_stop_programming();
               pb_start();
               incr++;
               if (globals.debug)
               {
                     diagMessage("finished loop %d\n",incr);
               }
            }

            resetTTL = 0;
            if (aborted)
            {
               abortExp( & (globals.InfoFile[0]), globals.CodePath, 1, 1);
               resetTTL = 1;
            }
            else if (getData( &globals, &exps))
            {
               resetTTL = 1;
            }
            if (resetTTL)
            {
               pb_start_programming (PULSE_PROGRAM);
               pb_inst_radio_shape (0, PHASE090, PHASE000, 0,
                  TX_DISABLE, NO_PHASE_RESET,
                  NO_TRIGGER, NO_SHAPE, AMP0,
		          0,
                  STOP, NO_DATA, MIN_DELAY);
               pb_stop_programming();
               pb_reset();
               pb_start();
               break;
            }
            diagMessage("save MTUNE data\n");
	        saveBsData( globals.real, globals.imag,
                      &(globals.InfoFile[0]), ct);
            sleepMilliSeconds(200);
	        ct = 2;
         }
         diagMessage("End mtune\n");
      }
      else if ( ! strcmp(r->inst,"MPSTUNE") )
      {
         double freq;
         double width;
         double power;
         int del;
         char dataPath[512];
	     int doEndCall = 1;
	     int ct = 1;

         sscanf(r->vals,"%s %lg %lg %lg", dataPath,
			 &freq, &width, &power);
         del = atoi(argv[2]);
         diagMessage("start MPSTUNE\n");
	     memset ((void *) globals.imag, 0,
			  sizeof(int) * globals.complex_points );
	     unlink(dataPath);
	     while ( 1 == 1)
         {
            requestMpsData( freq, width, power, dataPath,
                            &(globals.InfoFile[0]));
	        diagMessage("call getMPS\n");
            if (getMpsData( del, dataPath, &globals, &exps ) )
	        {
	           doEndCall = 0;
               break;
	        }
            diagMessage("save MPSTUNE data\n");
	        saveBsData( globals.real, globals.imag,
                      &(globals.InfoFile[0]), ct);
	        ct = 2;
	     }
	     if (doEndCall)
	        endMpsData();
         diagMessage("end MPSTUNE\n");
      }     
      r = r->next;
   }
   if (globals.board_start)
   {
      if (globals.debug)
         diagMessage("call pb_stop\n");
      pb_stop();
   }
   if (globals.board_open)
   {
      if (globals.debug)
         diagMessage("call pb_close\n");
      pb_close();
   }
   diagMessage("call endComm\n");
   endComm();
   if (re)
      free(re);
   if (im)
      free(im);
   r = PSstart;
   while ( (r != NULL))
   {
      PSelem *n;
      
      n = r->next;
      free(r);
      r = n;
   }
   if (globals.totalTime > 60.0)
      diagMessage("Total experiment time %d:%d sec\n",
         (int) (globals.totalTime/60.0),
         (int) globals.totalTime - (int) (globals.totalTime/60.0) * 60 );
   else
      diagMessage("Total experiment time %g sec\n",globals.totalTime );
   if (debugFile != NULL)
      fclose(debugFile);
   exit(EXIT_SUCCESS);
}
