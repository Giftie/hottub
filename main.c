/*---------------------------------------------------------------------------
  main.c
  By Ted B. Hale
	Hot Tub controller on the Raspberry Pi	
	Version 2
    
	This file implements the main for a daemon process

  2013-12-22  initial edits
  2014-01-03  add PulseThread
  2015-01-22  remove pulsethread
  2015-08-19  start v2
  
---------------------------------------------------------------------------*/

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sys/timeb.h>
#include <dirent.h>
#include <pthread.h>

#include <wiringPi.h>

// this defines the pre-processor variable EXTERN to be nothing
// it results in the variables in RPiHouse.h being defined only here 
#define EXTERN
#include "hottub.h"

//************************************************************************
void readDesiredTemp()
{
	char temp[80];
	FILE *f;

	f = fopen(DESIREDTEMPFILE,"r");
	if ((f!=NULL) && (read_line(f,temp,sizeof(temp))>0))
	{	
		desiredTemp = atof(temp);
	}
	else
	{
		desiredTemp = 104.0;
	}
	if (f!=NULL) fclose(f);
	Log("desired temp is %6.1f",desiredTemp);
}

//**************************************************************************
// look for a DS18B40 device
void findTempSensor(char *id)
{
	DIR *dp;
	struct dirent *ep;

	dp = opendir ("/sys/bus/w1/devices");
	if (dp != NULL)
	{
		while ((ep = readdir (dp)))
		{
			if (strncmp(ep->d_name,"28-",3)==0)
			{
				strcpy(id,ep->d_name);
				Log("findTempSensor> found sensor: %s",id);
				break;
			}
		}
		closedir (dp);
	}
	else
	{
		Log("findTempSensor> Couldn't open the 1-wire devices directory");
	}
}

//************************************************************************
void readConfig(char *fname)
{
	char temp[80];
	FILE *f;
	
	ReadConfigString("debug","0",temp,sizeof(temp),fname);
	debug = atoi(temp);
	ReadConfigString("ThingSpeakAPIkey","",ThingSpeakAPIkey,sizeof(ThingSpeakAPIkey),fname);
	ReadConfigString("NoticeToAddress","",NoticeToAddress,sizeof(NoticeToAddress),fname);
	ReadConfigString("NoticeFromAddress","",NoticeFromAddress,sizeof(NoticeFromAddress),fname);
	ReadConfigString("MTA","",MTA,sizeof(MTA),fname);
	ReadConfigString("waterSensorID","",waterSensorID,sizeof(waterSensorID),fname);
	ReadConfigString("heaterSensorID","",heaterSensorID,sizeof(heaterSensorID),fname);
	ReadConfigString("outdoorSensorID","",outdoorSensorID,sizeof(outdoorSensorID),fname);
	ReadConfigString("equipSensorID","",equipSensorID,sizeof(equipSensorID),fname);
	ReadConfigString("maxHeaterTemp","106.5",temp,sizeof(temp),fname);
	maxHeaterTemp = atof(temp);
	ReadConfigString("maxDesired","109.5",temp,sizeof(temp),fname);
	maxDesired = atof(temp);
	ReadConfigString("minDesired","99.5",temp,sizeof(temp),fname);
	minDesired = atof(temp);
	ReadConfigString("slopTemp","0.5",temp,sizeof(temp),fname);
	slopTemp = atof(temp);
	ReadConfigString("pumpOnFor","15",temp,sizeof(temp),fname);
	pumpOnDuration = atoi(temp) * 60;
	ReadConfigString("pumpOffFor","30",temp,sizeof(temp),fname);
	pumpOffDuration = atoi(temp) * 60;
	ReadConfigString("filterOnFor","30",temp,sizeof(temp),fname);
	filterOnDuration = atoi(temp) * 60;
	ReadConfigString("filterOffFor","720",temp,sizeof(temp),fname);
	filterOffDuration = atoi(temp) * 60;
	ReadConfigString("textThrottle","3600",temp,sizeof(temp),fname);
	textThrottle = atoi(temp) * 60;

	readDesiredTemp();
	
	temp[0]=0;
	findTempSensor(temp);
	if (temp[0]==0)
	{
		Log("********** no temp sensors found **************");
	} 
	else 
	{
		if (strlen(waterSensorID)<=0) strcpy(waterSensorID,temp);
		if (strlen(heaterSensorID)<=0) strcpy(heaterSensorID,temp);
		if (strlen(outdoorSensorID)<=0) strcpy(outdoorSensorID,temp);
		if (strlen(equipSensorID)<=0) strcpy(equipSensorID,temp);
	}
}

//************************************************************************
// handles signales to restart or shutdown
void sig_handler(int signo)
{
    switch (signo) {
      case SIGPWR:
        break;

      case SIGHUP:
	    // do a restart
	    Log("SIG restart\n");
		kicked = 1;
        break;

      case SIGINT:
      case SIGTERM:
		// do a clean exit
	    Log("SIG exit\n");
	    kicked = 2;
        break;
    }
}
//************************************************************************
// and finally, the main program
// a cmd line parameter of "f" will cause it to run in the foreground 
// instead of as a daemon 
int main(int argc, char *argv[])
{
    pid_t		pid;
	FILE		*f;
	pthread_t	tid1,tid2,tid3,tid4;		// thread IDs
	struct tm	*today;
	int x;

	// check cmd line param
	if ((argc==1) || strncmp(argv[1],"f",1))
	{
		printf("running in daemon mode\n");
		// Spawn off a child, then kill the parent.
		// child will then have no controlling terminals, 
		// and will become adopted by the init proccess.
		if ((pid = fork()) < 0) {
			printf("Error forking process\n");
			perror("Error forking process ");
			exit (-1);
		}
		else if (pid != 0) {
			printf("Exiting\n");
			exit (0);  // parent process goes bye bye
		}

		// The child process continues from here
		setsid();  // Become session leader;
	}
    printf("trapping some signals\n");
	// trap some signals 
	signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGPWR, sig_handler);
    signal(SIGHUP, sig_handler);
    printf("saving pid in a file\n");
    // save the pid in a file
	pid = getpid();
	f = fopen(PIDFILE,"w");
	if (f) {
		fprintf(f,"%d",pid);
		fclose(f);
	}
    printf("opening debug log\n");
	// open the debug log
	LogOpen("/opt/projects/logs/hottub");

	// read config values
	readConfig(CONFIGFILE);
	
	// start the main loop
	do
	{
		setupGPIO();
		
		// start the various threads
		Log("Main> start threads");
		tid1 = tid2 = tid3 = tid4 = 0;
		pthread_create(&tid1, NULL, UiThread, NULL);
		Sleep(100);
		pthread_create(&tid2, NULL, TempThread, NULL);
		Sleep(1500);
		pthread_create(&tid3, NULL, HotTubLogic, NULL);
		Sleep(100);
		pthread_create(&tid4, NULL, ListenerThread, NULL);
				
		// wait for signal to restart or exit
		do
		{
			sleep(1);
		} while (!kicked);
	
		// wait for running threads to stop
		if (tid1!=0) pthread_join(tid1, NULL);
		if (tid2!=0) pthread_join(tid2, NULL);
		if (tid3!=0) pthread_join(tid3, NULL);
		if (tid4!=0) pthread_join(tid4, NULL);

		// exit?
		if (kicked==2) break;
		// else restart, set flag back to 0
		kicked = 0;

	} while (1); // forever

	// delete the PID file
    unlink(PIDFILE);

	Log("Program Exit *****");
	Log(" ");

	return 0;
}
