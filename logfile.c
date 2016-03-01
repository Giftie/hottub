/***********************************************************************
   Filename:   logfile.c

	 antiquity   Ted Hale  created

************************************************************************/

/* system includes */
#include <stdio.h>
#include <stdlib.h>  
#include <stdarg.h>
#include <time.h>
#include <string.h>

// needed for the debug flag
#include "hottub.h"

/* global data*/

FILE *logfp;      
unsigned long LineNum;
int Julian;
char FileName[100];


/* ------------------------- Log file functions -------------------------- */


/*
  CloseLogFile

  Closes the current log file if it is in use.
*/
void LogClose(void) 
{
      fclose(logfp); 
	  logfp = NULL;
}     



/*
   OpenLogFile

   Opens the logfile with the specified name and julian day 
   Level = level of messages - 
           0 = errors only, 1 status messages also, 2 = detailed log

   RETURNS: 1 if an error occurred creating the file
            0 for success

  filename should include full path but not suffix

*/
int LogOpen(char *filename) 
{
    char temp[200];
    struct tm *today;
    time_t now;

   if (filename == NULL) {
      return (1);
   }

	time(&now);
    today = localtime(&now);

	// save params globally for later
	strncpy(FileName, filename, strlen(filename));

	// save this in static variable so we knw when day changes
	Julian = today->tm_yday+1;

	// put together the file name
	sprintf(temp,"%s_%04d-%02d-%02d.log",filename,
		             today->tm_year+1900,today->tm_mon+1,today->tm_mday);
   logfp = fopen (temp, "a" );

   if ( logfp == NULL ) {
      return (1);
   }

   LineNum = 0;
   return (0);
}



/*
   Log

   Writes a message to the previously opened log file.

   Passed in a format string and variable number of parameters,
   in the format used by printf.
*/
void Log(char *format, ... ) 
{
	char dtbuf[30];
	va_list arglist;
    struct tm *today;
    time_t now;

	time(&now);
    today = localtime(&now);

	if( Julian != today->tm_yday+1 ) {
		LogClose();
		LogOpen( FileName );
	}

	if (logfp==NULL) return;

	sprintf(dtbuf,"%02d/%02d/%04d %02d:%02d:%02d",today->tm_mon+1,today->tm_mday,
		today->tm_year+1900,today->tm_hour,today->tm_min,today->tm_sec);
	fprintf(logfp, "%5lu  %s ",LineNum++,dtbuf);
	va_start ( arglist, format );
	vfprintf (logfp, format, arglist );
	//if (format[strlen(format)-1]<' ')
		//format[strlen(format)-1] = 0;
	fprintf(logfp,"\r\n");
	fflush ( logfp );
}

void LogDbg(char *format, ... ) 
{
	char dtbuf[30];
	va_list arglist;
    struct tm *today;
    time_t now;

	if (!debug)
		return;
		
	time(&now);
    today = localtime(&now);

	if( Julian != today->tm_yday+1 ) {
		LogClose();
		LogOpen( FileName );
	}

	if (logfp==NULL) return;

	sprintf(dtbuf,"%02d/%02d/%04d %02d:%02d:%02d",today->tm_mon+1,today->tm_mday,
		today->tm_year+1900,today->tm_hour,today->tm_min,today->tm_sec);
	fprintf(logfp, "%5lu  %s ",LineNum++,dtbuf);
	va_start ( arglist, format );
	vfprintf (logfp, format, arglist );
	//if (format[strlen(format)-1]<' ')
		//format[strlen(format)-1] = 0;
	fprintf(logfp,"\r\n");
	fflush ( logfp );
}
