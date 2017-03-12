/*---------------------------------------------------------------------------
  listener.c
  By Ted B. Hale
	
	listen for simple commands from socket
	taken from controld project
	
	2013-12-22   initial edits
	2015-02-05   add failMessage
	2015-02-10   add reset command
	2015-03-06   add jet1 and jet2 commands

  ---------------------------------------------------------------------------*/

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h> 

#include "hottub.h"
void readDesiredTemp();

//***************************************************************************

int sendStr(int fd, char *str)
{
	return send(fd,str,strlen(str),0);
}

//***************************************************************************

char *jetlevel(int x)
{
	if (x==1) return "Low";
	if (x==2) return "High";
	return "Off";
}

//***************************************************************************

void DoStatus(int fd)
{
	int		i;
	char	buff[1000];

	sprintf(buff,"%6.1f\n",currentTemp);
	sendStr(fd,buff);
	sprintf(buff,"%6.1f\n",desiredTemp);
	sendStr(fd,buff);
	sprintf(buff,"%6.1f\n",heaterTemp);
	sendStr(fd,buff);
	sprintf(buff,"%s\n",heatIsOn?"On":"Off");
	sendStr(fd,buff);
	sprintf(buff,"%s\n",pumpIsOn?"On":"Off");
	sendStr(fd,buff);
	sprintf(buff,"0\n");  // removed coverIsOpen flag
	sendStr(fd,buff);
	sprintf(buff,"%s\n",failMessage);
	sendStr(fd,buff);
	sprintf(buff,"%s\n",jetlevel(jetsLevel));
	sendStr(fd,buff);
	sprintf(buff,"%s\n",jetlevel(socket1Level));
	sendStr(fd,buff);
	
	sendStr(fd,"\n");
}

//***************************************************************************
void Command(int fd, char *cmdline)
{
	char *params;
	char *cmd;
	int  i;
	char tmp[80];

	cmd = cmdline;
	params = strchr(cmdline,' ');
	if (!params)
		params = "";
	else
	{
		*params = 0;
		params++;
	}
	if (!strcasecmp(cmd,"stat")) {
		DoStatus(fd);
		close(fd);
		return;
	}
	if (!strcasecmp(cmd,"tempup")) {
		desiredTemp += 0.5;
		sendStr(fd,"OK\n");
		close(fd);
		sprintf(tmp,"%06.1f",desiredTemp);
		saveDesiredTemp();	
		return;
	}
	if (!strcasecmp(cmd,"tempdown")) {
		desiredTemp -= 0.5;
		sendStr(fd,"OK\n");
		close(fd);
		sprintf(tmp,"%06.1f",desiredTemp);
		saveDesiredTemp();
		return;
	}
	if (!strcasecmp(cmd,"reset")) {
		sendStr(fd,"OK\n");
		close(fd);
		Log("**** Reset Command ****");
		strcpy(failMessage,"OK");
		TBH_LED7_blinkRate(0);
		readDesiredTemp();
		return;
	}
	if (!strcasecmp(cmd,"jets")) {
		sendStr(fd,"OK\n");
		close(fd);
		if (jetsLevel==0)
		{
			jetsLevel = 1;
		}
		else 
		{
			jetsLevel = 0;
		}
		return;
	}
	if (!strcasecmp(cmd,"socket1")) {
		sendStr(fd,"OK\n");
		close(fd);
		if (socket1Level==0)
		{
			socket1Level = 1;
		}
		else 
		{
			socket1Level = 0;
		}
		return;
	}

	sendStr(fd,"unknown command: ");
	sendStr(fd,cmd);
	sendStr(fd,"\n");
	close(fd);
}

//***************************************************************************
// return # bytes read,  -1 on error
int ReadLine(int fd, char *buff, int buffsz)
{
	int  x, ret;
	char *p;

	// read one line 
	memset(buff,0,buffsz);
	p = buff;
	x = buffsz;
	while (x>0) {
		ret = recv(fd, p, 1, 0);
		if ((ret<=0) || kicked)
		{
			return -1;
			
		}
		if (*p=='\n')
			break;
		if (*p<' ')
			break;
		p++;
		x--;
	}
}

//***************************************************************************
// set linger and close socket. Should help guarantee delivery of last packet
void SocketClose(int s)
{
	#define TRUE    1
	#define FALSE   0
	struct linger so_linger;
	so_linger.l_onoff = TRUE;
	so_linger.l_linger = 30;
	setsockopt(s,SOL_SOCKET,SO_LINGER,&so_linger,sizeof so_linger);
	close(s);
}

//***************************************************************************

void *HandleClient(void *param)
{
	int *fd = (int*)param;
	char input[200];


	if (ReadLine(*fd,input,sizeof(input))<0)
	{
		SocketClose(*fd);
		free(param);
		return;
	}

	// null out any trailing newline or other special char
	if (input[strlen(input)-1]<' ')
		input[strlen(input)-1] = 0;

	Log("HandleClient Listener: got <%s>",input);
	// process command
	Command(*fd, input);

	SocketClose(*fd);
	free(param);
	Log("HandleClient done");
	return;
}


//***************************************************************************

void *ListenerThread(void *param)
{
	int sockfd,SelectCount;
	int *new_fd;
	struct sockaddr_in myaddr;
	struct sockaddr_in clientaddr;
	socklen_t sin_size;
	int	i;
	fd_set	socketset;
	struct	timeval tv;
	pthread_t tid;

	Log("Listener> thread starting");

	sockfd=socket(AF_INET,SOCK_STREAM,0);
	memset(&myaddr,0,sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(MYPORT);
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(sockfd,(struct sockaddr *)&myaddr,sizeof(struct sockaddr));
	listen(sockfd,10);

	while(!kicked)
	{
		FD_ZERO(&socketset);
		FD_SET(sockfd, &socketset);
		tv.tv_sec  =  0;  
		tv.tv_usec =  100;  
		if ((SelectCount = select(sockfd+1, &socketset, NULL, NULL, &tv)) <0) {
			Log("Listener> error %d in select call",errno);
			Sleep(1000);
			continue;
		}

		if (SelectCount == 0 ) {    // we are idle - just repeat
			///Log("Listener: IDLE");
			Sleep(100);
			continue;
		}

		// accept the new connection
		sin_size = sizeof(struct sockaddr_in);
		new_fd = malloc(sizeof(int));
		*new_fd = accept(sockfd,(struct sockaddr *)&clientaddr,&sin_size);
		Log("Listener> new connection from %s\n",inet_ntoa(clientaddr.sin_addr));
		// let a thread handle the commands
		pthread_create(&tid, NULL, HandleClient, new_fd);
		pthread_detach(tid);
	}
	Log("Listener> stopping");
	close(sockfd);
	Log("Listener> done.");
	return 0;
}

/****************************************************************************/
