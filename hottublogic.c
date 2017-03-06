/*---------------------------------------------------------------------------
   hottublogic.c   handle logic for hot tub control
         Ted Hale
	2013-12-22   initial edits
	2013-12-23   ! Can't pass/return floats or doubles on ARM?!?!
	             using global variable now anyway
	2013-12-24   add cover sensor
	2014-01-03   move heater pulse to separate thread
	2014-04-07   add push to xively
	2015-01-22   mod for new interface (no pulse needed)
	2015-02-05   add heater timeout logic
	2015-02-08   add email alert
	2015-03-03   add control of jets
	2015-03-08   poll for jet buttons to avoid spurious interrupts
	2015-08-20   mods for V2

	
NOTES:	
sudo modprobe w1-gpio
sudo modprobe w1-therm
cd /sys/bus/w1/devices/
ls  (to get ID string for sensor)   28-000004610260

cd (ID String)
cat w1_slave	
first line must end with YES
second line will end with t=12345    milli degrees C

---------------------------------------------------------------------------*/

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/timeb.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>
#include <wiringPi.h>

#include "hottub.h"

int sendSimpleMail( char *eserver, char *toaddr, char *fromaddr, char *subject, char *body);
int UpdateThingSpeak(char *api_key, char *field_name, char *value, char *field_name2, char *value2, char *field_name3, char *value3, char *field_name4, char *value4);

double BADTEMP = 999.0;
time_t pumpTurnedOnTime = 0;
time_t pumpTurnedOffTime = 0;
	
//**************************************************************************
// get temperature in Degrees F
void getTemperature(char *id, double *value)
{
	char buff[80];
	FILE *fd;
	char *p;
	double c;
	
	if (strlen(id)<1)
	{
		*value = 999.9;
		//*valuec = 999.99;
		return;
	}
	
	sprintf(buff,"/sys/bus/w1/devices/%s/w1_slave",id);
	///Log(buff);
	fd = fopen(buff,"r");
	if (fd==NULL)
	{
		Log("Error %d opening %s",errno,buff);
		*value = BADTEMP;
		//*valuec = BADTEMP;
		return;
	}
	read_line(fd,buff,sizeof(buff));
	///Log(buff);
	if (strcmp(&buff[36],"YES")!=0)
	{
		Log("error on 1-wire read: %s",buff);
		fclose(fd);
		*value = BADTEMP;
		//*valuec = BADTEMP;
		return;
	}
	read_line(fd,buff,sizeof(buff));
	p = strstr(buff,"t=");
	///Log(buff);
	if (p==NULL)
	{
		Log("error on 1-wire read: %s",buff);
		fclose(fd);
		*value = BADTEMP;
		//*valuec = BADTEMP;
		return;
	}
	fclose(fd);
	c = atof(p+2);
	*value = ((c/1000.0) * 1.8) + 32.0;
	//*valuec = (c/1000);
	return;
}

//**************************************************************************
PumpOn()
{
	if ( (pumpIsOn) || (jetsLevel) ) return; // If pump is already on, or Jets are on don't turn on low speed
	pumpIsOn = 1;
	Log("HotTubLogic> Turn pump ON");
	time(&pumpTurnedOnTime);
	piLock(0);
	butDebounce=getTicks();
	digitalWrite(pumpPin,1);
	piUnlock(0);
}

PumpOff()
{
	pumpIsOn = 0;
	Log("HotTubLogic> Turn pump OFF");
	time(&pumpTurnedOffTime);
	piLock(0);
	butDebounce=getTicks();
	digitalWrite(pumpPin,0);
	piUnlock(0);
}

//**************************************************************************
HeatOn()
{
	if ( (heatIsOn) || (jetsLevel) ) return; // If heat is already on, or Jets are on, don't turn on the heat
	heatIsOn = 1;		
	Log("HotTubLogic> Turn heat ON");
	piLock(0);
	butDebounce=getTicks();
	digitalWrite(heaterPin,1);
	piUnlock(0);
}

HeatOff()
{
	if (!heatIsOn) return;
	time(&pumpTurnedOnTime);  	// make pump run full cycle after heater turns off
	heatIsOn = 0;				// flag heater off
	Log("HotTubLogic> Turn heat OFF");
	piLock(0);
	butDebounce=getTicks();
	digitalWrite(heaterPin,0);
	piUnlock(0);
}

//**************************************************************************
// can't use fabs function on ARM (can't pass FP arguments, so use pointers)
int isSpurious(double *a, double *b)
{
	if ((*b==-123.456) || (*b==999.0)) return 0;
	return ( ((*a-*b)>10.0) || ((*b-*a)>10.0) );
}

//**************************************************************************
// Thread for reading temperatures
void *TempThread(void *param)
{
	double lastTemp = -123.456;
	double lastHeaterTemp = -123.456;
	double lastoutdoorTemp = -123.456;
	double lastequipmentTemp = -123.456;
	char tmp[80];
	
	do
	{	
		// read outdoor temperature
		LogDbg("TempThread> read outdoor temperature");
		getTemperature(outdoorSensorID,&outdoorTemp);

		//ignore spurious data
		if (isSpurious(&outdoorTemp,&lastoutdoorTemp))
		{
			sprintf(tmp,"TempThread> ignore spurious current temp: %6.1f",outdoorTemp);
			Log(tmp);
			outdoorTemp = lastoutdoorTemp;
		}
		lastoutdoorTemp = outdoorTemp;
		
		// read equipment temperature
		LogDbg("TempThread> read equipment temperature");
		getTemperature(equipSensorID,&equipmentTemp);
		
		//ignore spurious data
		if (isSpurious(&equipmentTemp,&lastequipmentTemp))
		{
			sprintf(tmp,"TempThread> ignore spurious current temp: %6.1f",equipmentTemp);
			Log(tmp);
			equipmentTemp = lastequipmentTemp;
		}
		lastequipmentTemp = equipmentTemp;
		
		// read water temperature
		LogDbg("TempThread> read water temperature");
		getTemperature(waterSensorID,&currentTemp);
		
		// ignore spurious data
		if (isSpurious(&currentTemp,&lastTemp))
		{
			sprintf(tmp,"TempThread> ignore spurious current temp: %6.1f",currentTemp);
			Log(tmp);
			currentTemp = lastTemp;
		}
		lastTemp = currentTemp;

		// read heater temperature
		LogDbg("TempThread> read heater temperature");
		getTemperature(heaterSensorID,&heaterTemp);
		
		// ignore spurious data
		if (isSpurious(&heaterTemp,&lastHeaterTemp))
		{
			sprintf(tmp,"TempThread> ignore spurious heater temp: %6.1f",heaterTemp);
			Log(tmp);
			heaterTemp = lastHeaterTemp;
		}
		lastHeaterTemp = heaterTemp;
		
		// let other threads run
        Sleep(1000);
    } while (kicked==0);  // exit loop if flag set
	Log("TempThread> thread exiting");
}

//**************************************************************************
// Thread entry point, param is not used
void *HotTubLogic(void *param)
{
	int pin, i, x, a0, a1, a2, showOutput, j1=-1, j2=-1;
	char tmp[80];
	char tmp2[80];
	char tmp3[80];
	char tmp4[80];
	FILE *fd;
	time_t now, lastOutput=0, lastPost=0, heatOnTime;
	time_t filterTime, filterOn, j1OnTime, j2OnTime;
	double startTemp;
	///float f;

	heatIsOn = 0;
	pumpIsOn = 0;
	freezeWarning = 0;
	time(&now);
	filterTime = now;
	strcpy(failMessage,"OK");
		
	// start polling loop
	Log("HotTubLogic> start polling loop.");
	Log("HotTubLogic> Water Sensor ID is %s",waterSensorID);
	Log("HotTubLogic> Heater Sensor ID is %s",heaterSensorID);
	Log("HotTubLogic> Outdoor Sensor ID is %s",outdoorSensorID);
	Log("HotTubLogic> Equipment Sensor ID is %s",equipSensorID);
    do
    {
		time(&now);
		// should we log this time?
		showOutput = ((now-lastOutput)>59);

		if (showOutput)
		{
			sprintf(tmp,"HotTubLogic> water: %6.1f   heater: %6.1f	   outdoor: %6.1f   Equipment: %6.1f",currentTemp,heaterTemp,outdoorTemp,equipmentTemp);
			Log(tmp);
		}
		// Should we update ThingSpeak?
		if (difftime(now,lastPost)>300)
		{
			sprintf(tmp,"%f",currentTemp);
			sprintf(tmp2,"%f",heaterTemp);
			sprintf(tmp3,"%f",outdoorTemp);
			sprintf(tmp4,"%f",equipmentTemp);			
			UpdateThingSpeak(ThingSpeakAPIkey, "field1", tmp, "field2", tmp2, "field3", tmp3, "field4", tmp4);		
			time(&lastPost);
		}		
		// check for equipment freeze warning
		if (equipmentTemp<minEquipmentTemp)
		{
			if (freezeWarning == 0)
			{
				freezeWarning = 1;
				sprintf(tmp,"HotTubLogic> ***** Equipment Freeze Warning ****** %6.1f",equipmentTemp);
				Log(tmp);
				sendSimpleMail(MTA,
						NoticeToAddress, 
						NoticeFromAddress, 
						"Equipment Freeze Warning", 
						tmp);
			    //TBH_LED7_blinkRate(1);
			}
		}
		// Check to see if equipment has is returned to above the minimun Equipment temp
		if (equipmentTemp>minEquipmentTemp)
		{
			if (freezeWarning != 0)
			{
				freezeWarning = 0;
				sprintf(tmp,"HotTubLogic> ***** Equipment Freeze Warning Cleared****** %6.1f",equipmentTemp);
				Log(tmp);
				sendSimpleMail(MTA,
						NoticeToAddress, 
						NoticeFromAddress, 
						"Equipment Freeze Warning Cleared", 
						tmp);
			}
		}		
		// check for over-temp on heater
		if (heaterTemp>maxHeaterTemp)
		{
			// DANGER - DANGER - shut it all down
			desiredTemp=-11.1;
			HeatOff();
			if (strcmp(failMessage,"OK")==0)
			{
				strcpy(failMessage,"Heater Over Temp");
				sprintf(tmp,"HotTubLogic> ***** Heater Over Temp ****** %6.1f",heaterTemp);
				Log(tmp);
				sendSimpleMail(MTA,
						NoticeToAddress, 
						NoticeFromAddress, 
						"Hottub Fail", 
						tmp);
				//TBH_LED7_blinkRate(1);
			}
		}
		
		// turn heat on/off as needed
		LogDbg("HotTubLogic> turn on/off heat as needed");
		if (!heatIsOn)
		{
			LogDbg("HotTubLogic> heat not on");
			if (currentTemp < (desiredTemp-slopTemp)) 
			{
				PumpOn(); // Pump always on when Heat is.
				HeatOn();
				time(&heatOnTime);
				startTemp = currentTemp;
			}
		}
		else
		{
			LogDbg("HotTubLogic> heat is on");
			if (currentTemp > desiredTemp)
			{
				HeatOff();
			}
			// if no increase in temp after 1 hour, then FAIL
			if (difftime(now,heatOnTime)>3600)
			{
				if ((currentTemp-startTemp)<1.0)
				{

					// DANGER - DANGER - shut it all down
					desiredTemp=-11.1;
					HeatOff();
					if (strcmp(failMessage,"OK")==0)
					{
						strcpy(failMessage,"Heater Timeout");
						sprintf(tmp,"HotTubLogic> ***** Heater timeout ****** %6.1f",currentTemp-startTemp);
						Log(tmp);
						sendSimpleMail(MTA,
								NoticeToAddress, 
								NoticeFromAddress, 
								"Hottub Fail", 
								"Heater timeout!\n");
						//TBH_LED7_blinkRate(1);
					}
				}
			}
		}
		// test for ecoMode and temperature
		if ( (ecoMode) && (outsideTemp > minEcoTemp) )
		{
			// turn on pump periodically
			LogDbg("HotTubLogic> turn on pump periodically");
			time(&now);
			if (pumpIsOn)
			{
				if (difftime(now,pumpTurnedOnTime)>pumpOnDuration)
				{
					// never turn off pump if heat is on
					if (!heatIsOn)
						PumpOff();
				}
			}
			else
			{
				if (difftime(now,pumpTurnedOffTime) > pumpOffDuration)
				{
					PumpOn();
				}
			}
		}
	    // otherwise run pump 
		else
		{
			PumpOn();
		}
		// change jet level if needed
		if (j1!=jetsLevel)
		{
			j1 = jetsLevel;
			switch (j1)
			{
			case 0:
				Log("HotTubLogic> Jets off");
				piLock(0);
				digitalWrite(jetsPin,0);
				piUnlock(0);
				PumpOn();
				break;
			case 1:
				Log("HotTubLogic> Jets High");
				PumpOff(); // Turn low speed off
				HeatOff(); // Turn heat off
				piLock(0);
				digitalWrite(jetsPin,1);
				piUnlock(0);
				time(&j1OnTime);
				break;
			}
		}
		// change socket 1 level if needed
		if (j2!=socket1Level)
		{
			j2 = socket1Level;
			switch (j2)
			{
			case 0:
				Log("HotTubLogic> Socket 1 Off");
				piLock(0);
				digitalWrite(socket1Pin,0);
				piUnlock(0);
				break;
			case 1:
				Log("HotTubLogic> Socket 1 On");
				piLock(0);
				digitalWrite(socket1Pin,1);
				piUnlock(0);
				time(&j2OnTime);
				break;
			}
		}
		// timeout jets
		if ((jetsLevel!=0) && (now-j1OnTime)>900)
		{
			Log("HotTubLogic> Time out Jets");
			jetsLevel = 0;
		}
		// turn on jet for filtering periodically
		time(&now);
		///Log("FilterLogic> now=%d  filterTime=%d  diff=%d",now,filterTime,(now-filterTime));
		if ((filterOn==0) && ((now-filterTime)>filterOffDuration))
		{
			Log("HotTubLogic> Filtering On");
			jetsLevel = 1;
			filterTime = now;
			filterOn = 1;
		}
		else
		{
			if ((filterOn==1) && ((now-filterTime)>filterOnDuration))
			{
				Log("HotTubLogic> Filtering Off");
				jetsLevel = 0;
				filterOn = 0;
			}
		}

		if (showOutput)
			lastOutput = now;
		
		// let other threads run
		for (i=0; i<10; i++)
		{
			if (kicked) break;
			Sleep(500);
		}
		
    } while (kicked==0);  // exit loop if flag set
	HeatOff();
	PumpOff();
	piLock(0);
	digitalWrite(jetsPin,0);
	digitalWrite(socket1Pin,0);
	digitalWrite(socket2Pin,0);
	digitalWrite(socket3Pin,0);
	digitalWrite(jetsLedPin,0);
	digitalWrite(socket1LedPin,0);
	piUnlock(0);
	Log("HotTubLogic> thread exiting");
}
