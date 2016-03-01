/*************************************************** 

  C Library for Adafruit LED backpack 4 digit 7-segment display
  for use with WiringPi
  by Ted Hale  ted.b.hale@gmail.com
  19-Aug-2015
  
  based on Adafruit-LED-Backpack-Library
  that license follows:  

  This is a library for our I2C LED Backpacks

  Designed specifically to work with the Adafruit LED Matrix backpacks 
  ----> http://www.adafruit.com/products/
  ----> http://www.adafruit.com/products/

  These displays use I2C to communicate, 2 pins are required to 
  interface. There are multiple selectable I2C addresses. For backpacks
  with 2 Address Select pins: 0x70, 0x71, 0x72 or 0x73. For backpacks
  with 3 Address Select pins: 0x70 thru 0x77

  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  MIT license, all text above must be included in any redistribution
  
 ****************************************************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#define HT16K33_CMD_BRIGHTNESS 0xE0
#define HT16K33_BLINK_CMD 0x80
#define HT16K33_BLINK_DISPLAYON 0x01

static int fd_LED = -1;

static uint8_t displaybuffer[11]; 

// Hexadecimal character lookup table (row 1 = 0..9, row 2 = A..F)
static const uint8_t DigitTable[] = {
//  0     1     2     3     4     5     6     7     8     9
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 
//  A     B     C     D     E     F     -  BLANK  
  0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x40, 0x00 };
  
static const char ValidChars[] = "0123456789ABCDEF- .";

// define where in the diaplay buffer each digit data goes (5 is the colon)
static const uint8_t digitOffset[4] = { 1, 3, 7, 9 };

void TBH_LED7_setBrightness(uint8_t b) 
{
  if (b > 15) b = 15;
  unsigned char val = HT16K33_CMD_BRIGHTNESS | b;
  write(fd_LED, &val, 1);
}

void TBH_LED7_blinkRate(uint8_t b) 
{
  if (b > 3) b = 0; // turn off if not sure
  unsigned char val = HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | (b << 1);
  write(fd_LED, &val, 1);
}

void TBH_LED7_Clear()
{
	uint8_t i;
	for (i=0; i<11; i++) 	// clear internal buffer
	{
		displaybuffer[i] = 0;
	}
							// clear the hardware
	write(fd_LED, displaybuffer, sizeof(displaybuffer));
}

void TBH_LED7_Open(uint8_t addr)
{
	fd_LED = wiringPiI2CSetup(addr);
	
	if (fd_LED!=-1)
	{
		// init display
		wiringPiI2CWrite(fd_LED, 0x21);
		wiringPiI2CWrite(fd_LED, 0x81);
		wiringPiI2CWrite(fd_LED, 0xEF);
	}
}

static void TBH_LED7_writeDisplay() 
{
	write(fd_LED, displaybuffer, 11);
}

void TBH_LED7_Show(char *buff)
{
	int i=0, n=0, offset;
	char *p;
	memset(displaybuffer,0,sizeof(displaybuffer));
	while ((i<strlen(buff))&&(n<4))
	{
		p = strchr(ValidChars,buff[i]);
		if (p)
		{
			if (buff[i]!='.')
			{
				offset = p-ValidChars;
				displaybuffer[digitOffset[n]] = DigitTable[offset];
			}
			if (buff[i+1]=='.')
			{
				displaybuffer[digitOffset[n]] |= (1<<7);
				i += 2;
			} else {
				i++;
			}
			n++;
		}
		else 
		{
			printf("%d  '%c' not valid\n",i,buff[i]);
			i++;
		}
	}
	TBH_LED7_writeDisplay();
}

void TBH_LED7_ShowDiag()
{
	int i;
	for (i=1; i<12; i++)
	{
		displaybuffer[i] = 0xFF;
	}
	TBH_LED7_writeDisplay();
}
