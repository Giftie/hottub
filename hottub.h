/*---------------------------------------------------------------------------
   hottub.h - include file for the Raspberry Pi Hot Tub project
         
   2013-12-22  initial edits
   2014-01-03  add PulseThread and maxHeaterTemp
   2015-02-05  add failMessage
   2015-03-03  add jet and filter control
   2015-Oct-04  mods for V2
---------------------------------------------------------------------------*/

#define PIDFILE "/var/run/hottub.pid"
#define CONFIGFILE "./config"
#define DESIREDTEMPFILE "./desiredtemp"
#define MYPORT 17200
#define MAXCONNECTIONS 10

#define BYTE unsigned char

// causes Global variables to be defined in the main
// and referenced as extern in all the other source files
#ifndef EXTERN
#define EXTERN extern
#endif
 
#define pumpPin 0
#define heaterPin 2
#define jet1LoPin 3
#define jet1HiPin 12
#define jet2LoPin 13
#define jet2HiPin 14
#define jet1ButPin 30
#define jet2ButPin 21
#define upButPin 22
#define downButPin 23
#define jet1LedPin 24
#define jet2LedPin 25



// prototype definitions for the worker threads
void *TempThread(void *param);
void *ListenerThread(void *param);
void *HotTubLogic(void *param);
void setupGPIO();
void *UiThread(void *param);

// prototypes from common.c
int Sleep(int millisecs);
int read_line(FILE *fp, char *bp, int mx);
int ReadConfigString(char *var, char *defaultVal, char *out, int sz, char *file);
int WriteConfigString(char *var, char *out, char *file);
void Log(char *format, ... );
void LogDbg(char *format, ... );
int sendSimpleMail( char *eserver, char *toaddr, char *fromaddr, char *subject, char *body);
long long int getTicks();

// GLOBAL variables.  A lock needs to be used to prevent any 
// simultaneous access from multiple threads
EXTERN int			kicked;						// flag for shutdown or restart
EXTERN int 			debug;						// flag to allow debug log output
EXTERN char         ThingSpeakAPIkey[32];       // 
EXTERN char         NoticeToAddress[32];
EXTERN char         NoticeFromAddress[32];
EXTERN char         MTA[32];
EXTERN char 		waterSensorID[32];			// 1-wire device ID for water temp 
EXTERN char 		heaterSensorID[32];			// 1-wire device ID for heater temp
EXTERN char			equipSensorID[32];          // 1-wire device ID for equipment temp
EXTERN char         outdoorSensorID[32];        // 1-wire device ID for outdoor temp
EXTERN double 		desiredTemp;				// where I want the water to be
EXTERN double 		currentTemp;				// where the water is now
EXTERN double 		heaterTemp;					// temperature in the heater
EXTERN double		equipmentTemp;              // equipment temperature
EXTERN double		outdoorTemp;                // Outdoor temperature
EXTERN double       minEquipmentTemp;           // Minimum Equipment Temperature to issue Freeze Warning
EXTERN double 		maxHeaterTemp;				// fail temperature of the heater
EXTERN double 		slopTemp;					// prevents constant "seeking"
EXTERN double 		maxDesired;					// highest allowed setting
EXTERN double 		minDesired;					// lowest allowed setting
EXTERN int			pumpOnDuration;				// seconds to run pump
EXTERN int			pumpOffDuration;			// seconds to not run pump
EXTERN int			filterOnDuration;			// seconds to run filter
EXTERN int			filterOffDuration;			// seconds to not run filter
EXTERN int          textThrottle;				// min seconds between text messages
EXTERN int 			heatIsOn;
EXTERN int 			pumpIsOn;
EXTERN int 			jetsLevel;					// pump level for jets: 0=off, 1=high
EXTERN int 			jet2Level;
EXTERN char			failMessage[40];			// what failed
EXTERN char			freezeWarning[40];			// freezeWarning

EXTERN long long int butDebounce; 				// for SW debounce