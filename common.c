/*---------------------------------------------------------------------------
   common.c   misc shared functions
         Ted Hale

---------------------------------------------------------------------------*/

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/timeb.h>
#include <pthread.h>
#include <wiringPi.h>

#include "hottub.h"

//************************************************************************
/*
void Log(char *format, ... ) 
{
	va_list arglist;
	va_start ( arglist, format );
	vprintf (format, arglist );
}
*/

//***************************************************************************

int Sleep(int millisecs)
{
	return usleep(1000*millisecs);
}

//************************************************************************

long long int getTicks()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return (1000000*now.tv_sec)+now.tv_usec;
}

//************************************************************************
// read chars into buffer until EOF or newline
int read_line(FILE *fp, char *bp, int mx)
{   char c = '\0';
    int i = 0;
	memset(bp,0,mx);
    /* Read one line from the source file */
    while ( ( (c = getc(fp)) != '\n' ) && (i<mx) )
    {   
		if ((c == EOF)||(c == 255))         /* return -1 on EOF */
		{
			LogDbg("read_line> got EOF");
			sleep(1);
			if (i>0) break;
            else return(-1);
		}
        bp[i++] = c;
    }
    bp[i] = '\0';
    return(i);
}

//************************************************************************
// get var=value from config file
int ReadConfigString(char *var, char *defaultVal, char *out, int sz, char *file)
{
	FILE	*f;
	char	line[100];
	char	*p;

	LogDbg("ReadConfigString> get %s from %s ",var,file);
	piLock(1);
	f = fopen(file,"r");
	if (!f)
	{
		Log("ReadConfigString> error %d opening %s",errno,file);
		piUnlock(1);
		return 1;
	}

	while (read_line(f,line,sizeof(line))>=0)
	{
		LogDbg("ReadConfigString> read: %s",line);
		if ((line[0]==';')||(line[0]=='#')) 
			continue;
		p = strchr(line,'=');
		if (!p)
			continue;
		*p=0;
		p++;
		if (!strcmp(line,var))
		{
			strncpy(out,p,sz);
			fclose(f);
			Log("ReadConfigString> return %s=%s",var,out);
			piUnlock(1);
			return 1;
		}
	}
	fclose(f);
	strncpy(out,defaultVal,sz);
	Log("ReadConfigString> return %s=%s",var,out);
	piUnlock(1);
	return 0;
}

//************************************************************************
// save the desired temp to file
void saveDesiredTemp()
{
	FILE *f;
	char temp[20];
	
	piLock(1);
	f = fopen(DESIREDTEMPFILE,"w");
	if (f==NULL)
	{
		piUnlock(1);
		Log("saveDesiredTemp> error %d opening %s",errno,DESIREDTEMPFILE);
		return;
	}
	Log("saveDesiredTemp> set to %6.1f degrees",desiredTemp);
	sprintf(temp,"%6.1f",desiredTemp);
	fwrite(temp,strlen(temp),1,f);
	fclose(f);
	piUnlock(1);
}
