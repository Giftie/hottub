/*
 *  Email.cpp  -  Send Email
 *
 *  Jan 1997,  Ted Hale
 *  Feb 2015   make it a function
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <memory.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define EMAIL_PORT 25
#define NoticeToAddress "giftie61@gmail.com"
#define NoticeFromAddress "giftie@shaw.ca"

// You will need to set the address for your mail (SMTP) server here
#define MTA "imap.shaw.ca"

/****************************************************************************/

int SendCmd(int sk, char *line)
{
	int error;
	///printf(line);
	error = send(sk,line,strlen(line),0);
	if (error<0) {
	   printf(" error on send \n");
	   return(1);
	}
	return(0);
}

/****************************************************************************/

int ReadResp(int sk, char *line, int max)
{
	int error,statcode;
	memset(line,0,max);
	error = recv(sk,line,max,0);
	if (error<0) {
		printf(" error on recv\n");
		return(1);
	}
	sscanf(line,"%d",&statcode);
	if (statcode>499) {
		printf("Error status from SMTP Host:\n");
		printf("   %s\n",line);
		return(1);
	}
	///printf(line);
	return(0);
}

/****************************************************************************/
// return 0 if no error
int sendSimpleMail( char *toaddr, char *fromaddr, char *subject, char *body)
{
	int error, retval = 0;
	char line[1000];
	struct hostent *host;
	struct sockaddr_in hostaddr;
	int sk;
	int timeout = 1*60*1000;	
	do
	{
		if (!(host=gethostbyname(MTA))) {
		    printf(" Unable to get host address\n");
			retval = 2;
			break;
		}

		memcpy(&hostaddr.sin_addr,host->h_addr_list[0],4);

		sk = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sk<0) {
			printf("Unable to create socket\n");
			retval = 3;
			break;
		}
		hostaddr.sin_family = PF_INET;
		hostaddr.sin_port   = htons( EMAIL_PORT );

		if( connect( sk, (struct sockaddr *)&hostaddr, sizeof(hostaddr) ) != 0 ) {
			printf(" error on connect\n");
			retval = 3;
			break;
		}

		error = ReadResp(sk,line,sizeof(line));
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		
		strcpy(line,"HELO myhostname \n"); 
		error = SendCmd(sk,line);
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		error = ReadResp(sk,line,sizeof(line));
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		
		strcpy(line,"MAIL FROM: ");                
		strcat(line,fromaddr);
		strcat(line,"\n");
		error = SendCmd(sk,line);
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		error = ReadResp(sk,line,sizeof(line));
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		
		strcpy(line,"RCPT TO: ");
		strcat(line,toaddr);
		strcat(line,"\n");
		error = SendCmd(sk,line);
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		error = ReadResp(sk,line,sizeof(line));
		if (error!=0) 
		{
			retval = 99;
			break;
		}

		strcpy(line,"DATA\n");
		error = SendCmd(sk,line);
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		error = ReadResp(sk,line,sizeof(line));
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		
		strcpy(line,"To: ");
		strcat(line,toaddr);
		strcat(line,"\n");
		error = SendCmd(sk,line);
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		
		strcpy(line,"From: ");
		strcat(line,fromaddr);
		strcat(line,"\n");
		error = SendCmd(sk,line);
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		
		strcpy(line,"Subject: ");
		strcat(line,subject);
		strcat(line,"\n\n");
		error = SendCmd(sk,line);
		if (error!=0) 
		{
			retval = 99;
			break;
		}

		error = SendCmd(sk,body);
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		strcpy(line,"\r\n");
		error = SendCmd(sk,line);
		if (error!=0) 
		{
			retval = 99;
			break;
		}

		strcpy(line,".\r\n");
		error = SendCmd(sk,line);
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		error = ReadResp(sk,line,sizeof(line));
		if (error!=0) 
		{
			retval = 99;
			break;
		}

		strcpy(line,"quit\n");
		error = SendCmd(sk,line);
		if (error!=0) 
		{
			retval = 99;
			break;
		}
		error = ReadResp(sk,line,sizeof(line));
		if (error!=0) 
		{
			retval = 99;
			break;
		}
	
	} while (0);
	
	// clean up
	if (sk!=0)
		close(sk);
		
    return retval;
}

/*
int main()
{
	int err;
	
	err = sendSimpleMail("7575326135@vtext.com", 
					"ted.hale@cox.net", 
					"TEST", 
					"Here is the message.\nAnd some more\n");
	
	printf("Err = %d\n",err);
	return 0;
}
*/
