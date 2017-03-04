/*---------------------------------------------------------------------------
   uithread.c   handle logic for hot tub user interface
                initialize WiringPi (GPIO and Interrupt setup)
				
         Ted Hale
	2015-08-19   initial edits, new for V2
	2015-12-01   add button lock
	2015-12-06   blank display lat at night
	2015-02-01   slow down display update so it doesn't flash on video
---------------------------------------------------------------------------*/

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/timeb.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <wiringPi.h>

//#include "TBH_LED7Backpack.h"
#include "hottub.h"

static time_t tButPressTime;	// to timeout desired temp display
static time_t tButLock;			// time to allow buttons unlocked
#define butBounce 5555

//***************************************************************************
void upButton_Interrupt(void)
{
	Log("UP BUTTON PRESSED");
	if (tButLock==0) {
		if (digitalRead(downButPin)==0)
		{
			Log("UiThread> Unlock buttons");
			time(&tButLock);
		}
		return;
	}
	if (abs(getTicks()-butDebounce)<butBounce) return;
	if (strcmp(failMessage,"OK")!=0) return;
	if (desiredTemp<maxDesired) 
	{
		desiredTemp += 0.5;
		saveDesiredTemp();
	}
	time(&tButPressTime);
	butDebounce=getTicks();
}	

//***************************************************************************
void downButton_Interrupt(void)
{
	Log("DOWN BUTTON PRESSED");
	if (tButLock==0) return;
	if (abs(getTicks()-butDebounce)<butBounce) return;
	if (strcmp(failMessage,"OK")!=0) return;
	if (desiredTemp>minDesired) 
	{
		desiredTemp -= 0.5;
		saveDesiredTemp();
	}
	time(&tButPressTime);
	butDebounce=getTicks();
}	

//***************************************************************************
void jet1Button_Interrupt(void)
{
	Log("JET 1 BUTTON PRESSED");
	if (abs(getTicks()-butDebounce)<butBounce) return;
	jetsLevel++;
	if (jetsLevel>1)
		jetsLevel=0;
	butDebounce=getTicks();
}	

//***************************************************************************
void jet2Button_Interrupt(void)
{
	Log("SOCKET 1 BUTTON PRESSED");
	if (abs(getTicks()-butDebounce)<butBounce) return;
	socket1Level++;
	if (socket1Level>1)
		socket1Level=0;
	butDebounce=getTicks();
}	

//**************************************************************************
// init a GPIO output pin
void initGpioOutput(int pin, int val)
{
	pinMode (pin, OUTPUT);
	digitalWrite(pin, val);
}

//**************************************************************************
// set up GPIO and interrupts for user interface
void setupGPIO()
{
	Log("Set up user interface");
	if (wiringPiSetup() == -1)
	{
		Log("Error on wiringPiSetup.");
		return;
	}

	piLock(0);
		
		initGpioOutput(pumpPin,0);
		initGpioOutput(heaterPin,0);
		initGpioOutput(jetsPin,0);
		initGpioOutput(socket1Pin,0);
		initGpioOutput(socket2Pin,0);
		initGpioOutput(socket3Pin,0);
		initGpioOutput(jetsLedPin,0);
		initGpioOutput(socket1LedPin,0);
		
		if ( wiringPiISR (upButPin, INT_EDGE_FALLING, &upButton_Interrupt) < 0 ) {
			Log("Unable to setup ISR\n");
			return;
		}
		if ( wiringPiISR (downButPin, INT_EDGE_FALLING, &downButton_Interrupt) < 0 ) {
			Log("Unable to setup ISR\n");
			return;
		}
		if ( wiringPiISR (jetsButPin, INT_EDGE_FALLING, &jet1Button_Interrupt) < 0 ) {
			Log("Unable to setup ISR\n");
			return;
		}
		if ( wiringPiISR (Socket1ButPin, INT_EDGE_FALLING, &jet2Button_Interrupt) < 0 ) {
			Log("Unable to setup ISR\n");
			return;
		}
		
	piUnlock(0);

	// open the LED display
//	TBH_LED7_Open(0x70);
}

//**************************************************************************
// blink jet1 button LED
void *jet1Thread(void *param)
{
	while (!kicked)
	{
		digitalWrite(jetsLedPin, 1);
		if (jetsLevel==0)
		{
			Sleep(50);
		} else {
			Sleep(150);
			digitalWrite(jetsLedPin, 0);
			Sleep(150);
		}
	}
}

//**************************************************************************
// blink jet2 button LED
void *socket1Thread(void *param)
{
	while (!kicked)
	{
		digitalWrite(socket1LedPin, 1);
		if (socket1Level==0)
		{
			Sleep(50);
		} else {
			Sleep(500);
			digitalWrite(socket1LedPin, 0);
			Sleep(500);
		}
	}
}

//**************************************************************************
// output to display (or not)
void LEDoutput(char *s)
{
	time_t now;
    struct tm *today;
	
	time(&now);
    today = localtime(&now);
	if ((today->tm_hour>21)||(today->tm_hour<6))
	{
	//	TBH_LED7_Show("   ");
	} else {
	//	TBH_LED7_Show(s);
	}
}

//**************************************************************************
// Thread entry point, param is not used
void *UiThread(void *param)
{
	pthread_t	tid;
	char tmp[10];
	time_t now;
	
	tButPressTime = 0;
	pthread_create(&tid, NULL, jet1Thread, NULL);
	pthread_create(&tid, NULL, socket1Thread, NULL);

	while (!kicked)
	{
		if (!tButPressTime)
		{
			sprintf(tmp,"%4.1f",currentTemp);
/*			LEDoutput(tmp);	*/
		}
		else
		{
			sprintf(tmp,"%4.1f",desiredTemp);
/*			LEDoutput(tmp);	*/
			// timeout showing desired temp
			time(&now);
			if ((now-tButPressTime)>1)
			{
				tButPressTime = 0;
			}
			if ((now-tButLock)>10)
			{
				tButLock = 0;
			}
		}
		
		Sleep(1000);
	}
	// quitting, clear display and stop blinking
/*	TBH_LED7_Show("----");
	TBH_LED7_blinkRate(0); */
	Log("UiThread> thread exiting");
}
