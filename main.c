////////
//
//  main.c
//  CS537 FTP Client
//
//  Created by Christopher Erdelyi on 4/7/18.
//	Contributed to by John Wu.
//  Copyright © 2018 Christopher Erdelyi, John Wu. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>


#define MAXLINE 2000

/* Creates data connection with FTP server after initial control
   connection has been established. Data connections are opened and closed
   for each command sent (for example, RETR or NLST).
   This function works in conjunction with the PASV command sent by the client. */
int dataConnSetup(char* pasvSetting)
{
	int dataSocket, dataConn, gethost, connectReturn, port1, port2;
	char *token, *ip1, *ip2, *ip3, *ip4, *p1, *p2, *dataPortString, addr[16];
	
	// ftp.gnu.org and ftp.ucsd.edu return string containing IP in parenthesis
	token = strtok(pasvSetting, "(");
	
	ip1 = strtok(NULL, ",");
	ip2 = strtok(NULL, ",");
	ip3 = strtok(NULL, ",");
	ip4 = strtok(NULL, ",");
	
	p1 = strtok(NULL, ",");
	p2 = strtok(NULL, ")");
	port1 = atoi(p1);
	port2 = atoi(p2);
	
	snprintf(addr, 16, "%s.%s.%s.%s", ip1, ip2, ip3, ip4);
	
	// Socket number must be calculated as follows below
	dataSocket = ((port1 * 256) + port2);
	
	// set up IPv4 socket to initiate new connection for data transfer,
	// using port specified by server in response to PASV request.
	struct sockaddr_in pass;
	pass.sin_family		= AF_INET;
	pass.sin_addr.s_addr   = inet_addr(addr);
	pass.sin_port		  = htons(dataSocket);
	
	dataConn = socket(pass.sin_family, SOCK_STREAM, 0);
	connectReturn = connect(dataConn, (struct sockaddr *)&pass, sizeof(pass));
	if (connectReturn != -1)
	{
		printf(" --Data connection made. IP %s, Socket %i \n", addr, dataSocket);		// Success
	}
	else
	{
		fprintf(stderr, "Error connection request refused, errno = %d (%s) \n",
				errno, strerror(errno));
	}
	
	// return the handle of the socket
	return dataConn;
}

//takes entire server response and checks line by line
//returns response code of last line (except if find a >= 400 code, then returns that)
int handleResponse(int cSock, char* servResp)
{
	int respNum;
	char* copy;
	char* responseLine;
	char* QUIT = "QUIT\r\n";
	
	memset(&servResp[0], 0, MAXLINE);
	sleep(1);
	read(cSock, servResp, MAXLINE);
	servResp[strlen(servResp)+1] = '\0';
	copy = (char*) malloc(strlen(servResp));	//copy for tokenizing
	strcpy(copy, servResp);
	responseLine = strtok(copy, "\r\n");
	while(responseLine != NULL)
	{
		//print line, get response code
		printf("%s\n", responseLine);
		respNum = atoi(responseLine);
		if(respNum >= 400)
			break;
		responseLine = strtok(NULL, "\r\n");
	}
	return respNum;
}

//given server response code and expected code
//   if not match, error message and quit program. else nothing
void errorQuit(int cSock, int rCode, int expected)
{
	char* QUIT = "QUIT\r\n";
	if (rCode != expected)
	{
		printf("ERROR: received %d (was expecting %d).\n", rCode, expected);
		write(cSock, QUIT, strlen(QUIT));
		exit(EXIT_FAILURE);
	}
}

//sets up anonymous login first
//loops getting user input and completing actions to accomplish requests
int controlSession(int controlSocket, int numPaths, char* serverDir, char* localDir)
{
	char serverResponse[MAXLINE+1], dataBlock[MAXLINE+1];
	char userinput[500];
	char* USER = "USER anonymous\r\n";
	char* PASS = "PASS \r\n";
	char* PASV = "PASV\r\n";
	char* TYPE = "TYPE I\r\n";
	char* CWD = "CWD ";
	char* NLST = "NLST ";
	char* RETR = "RETR ";
	char* HELP = "HELP ";
	char* QUIT = "QUIT\r\n";
	
	int responseCode;
	int dataSocket;
	ssize_t bytesIn = 0;
	
	memset(&dataBlock[0], 0, sizeof(dataBlock));
	
	//read initial server response
	responseCode = handleResponse(controlSocket, serverResponse);
	
	//attempt to do anonymous login
	while(responseCode != 230)
	{
		switch(responseCode)
		{
			case 220:
				write(controlSocket, USER, strlen(USER));
				break;
			case 331:
				write(controlSocket, PASS, strlen(PASS));
				break;
			default:
				errorQuit(controlSocket, responseCode, 230);
				break;
		}
		responseCode = handleResponse(controlSocket, serverResponse);
	}
	
	//CWD to change server directory if user included a command line arg for it
	if (numPaths > 0)
	{
		char* CWD_arg = malloc(strlen((CWD)) + strlen(serverDir) + 2);
		strcpy(CWD_arg, CWD);
		strcat(CWD_arg, serverDir);
		strcat(CWD_arg, "\r\n");
		write(controlSocket, CWD_arg, strlen(CWD_arg));
		
		responseCode = handleResponse(controlSocket, serverResponse);
		free(CWD_arg);
		errorQuit(controlSocket, responseCode, 250);
	}
	
	//get user input and handle requests
	printf("sftp> ");
	fgets(userinput, 500, stdin);
	while(strncmp(userinput, "quit", 4) != 0)
	{
		//directory listing request
		if(strncmp(userinput, "ls", 2) == 0)
		{
			char* path = strtok(userinput, " ");
			path = strtok(NULL, " ");
			path = strtok(path, "\n");
			char* NLST_arg;
			if (path != NULL)	//ls with path
			{
				NLST_arg = malloc(strlen((NLST)) + strlen(path) + 2);
				strcpy(NLST_arg, NLST);
				strcat(NLST_arg, path);
				strcat(NLST_arg, "\r\n");
			}
			else	//plain ls
			{
				NLST_arg = malloc(strlen((NLST)) + 2);
				strcpy(NLST_arg, NLST);
				strcat(NLST_arg, "\r\n");
			}
			
			//send PASV
			write(controlSocket, PASV, strlen(PASV));
			responseCode = handleResponse(controlSocket, serverResponse);
			errorQuit(controlSocket, responseCode, 227);
			//PASV success-> setup data connetion, send NLST
			dataSocket = dataConnSetup(serverResponse);
			write(controlSocket, NLST_arg, strlen(NLST_arg));
			free(NLST_arg);
			responseCode = handleResponse(controlSocket, serverResponse);
			errorQuit(controlSocket, responseCode, 226);
			//server sent directory success -> read and show data from data socket
			printf(" --File Directory-- \n");
			while (read(dataSocket, dataBlock, MAXLINE))
			{
				printf("%s", dataBlock);
				memset(&dataBlock[0], 0, sizeof(dataBlock));
			}
			close(dataSocket);
		}
		//get request
		else if (strncmp(userinput, "get", 3) == 0)
		{
			char* serverPathFile = strtok(userinput, " ");
			serverPathFile = strtok(NULL, " ");
			char* RETR_arg;
			if (serverPathFile == NULL)	//no filename provided
				printf(" --Usage: get [<directory>]/<filename> [<local directory>]/\n");
			else	//filename (and/or path given)
			{
				//build request for sending to server
				RETR_arg = malloc(strlen((RETR)) + strlen(serverPathFile) + 2);
				strcpy(RETR_arg, RETR);
				strcat(RETR_arg, serverPathFile);
				strcat(RETR_arg, "\r\n");
				
				//tok to save local directory
				char* localPath = strtok(NULL, " ");
				localPath = strtok(localPath, "\n");
				//parse serverPathFile to just get filename
				char* filename = serverPathFile;
				while(strchr(filename, '/'))
					filename = strchr(filename, '/') + 1;
				filename = strtok(filename, "\n");
				
				//setup binary data connection
				//send PASV
				write(controlSocket, PASV, strlen(PASV));
				responseCode = handleResponse(controlSocket, serverResponse);
				errorQuit(controlSocket, responseCode, 227);
				//PASV success: setup data connetion in binary, send RETR request
				dataSocket = dataConnSetup(serverResponse);
				write(controlSocket, TYPE, strlen(TYPE));
				responseCode = handleResponse(controlSocket, serverResponse);
				errorQuit(controlSocket, responseCode, 200);
				write(controlSocket, RETR_arg, strlen(RETR_arg));
				free(RETR_arg);
				responseCode = handleResponse(controlSocket, serverResponse);
				
				//check response code (large file might only send 150, smaller might have 150+226 together)
				int delayed226 = 0;
				if (responseCode == 150)
					delayed226 = 1;
				else
					errorQuit(controlSocket, responseCode, 226);
				
				//build path (if necessary)
				char* localPathFile;
				if (numPaths >= 2 || localPath != NULL)
				{
					//combine cmd line given directory + inputted path + filename
					char* fullPath;
					if (numPaths >= 2 && localPath != NULL)	//cmd line & input provided
					{
						fullPath = malloc(1 + strlen(localDir) + strlen(localPath));
						strcpy(fullPath, localDir);
						strcat(fullPath, "/");
						strcat(fullPath, localPath);
					}
					else if (numPaths >= 2)	//cmd line only
					{
						fullPath = malloc(strlen(localDir));
						strcpy(fullPath, localDir);
					}
					else if (localPath != NULL)	//input only
					{
						fullPath = malloc(strlen(localPath));
						strcpy(fullPath, localPath);
					}
					//create directories (rebuild path folder by folder)
					localPathFile = malloc(strlen(fullPath) + strlen(filename) + 3);
					strcpy(localPathFile, "./");
					char* folder = strtok(fullPath, "/");
					while(folder)
					{
						strcat(localPathFile, folder);
						strcat(localPathFile, "/");
						mkdir(localPathFile, 0777);
						folder = strtok(NULL, "/");
					}
					//add filename to path for fopen() parameter				
					strcat(localPathFile, filename);
					free(fullPath);
				}
				else	//no path, use current dir -> just filename
				{
					localPathFile = malloc(strlen(filename));
					strcpy(localPathFile, filename);
				}

				int currentTransfer = 0;
				//read data connection and write file
				FILE* file = fopen(localPathFile, "w");
				if(file!=NULL)
				{
					int datasize = read(dataSocket, dataBlock, MAXLINE);
					while (datasize > 0)
					{
						bytesIn += datasize;	//track total bytes
						currentTransfer += datasize;	//track bytes for this file
						fwrite(dataBlock, sizeof(char), datasize, file);
						memset(&dataBlock[0], 0, sizeof(dataBlock));
						datasize = read(dataSocket, dataBlock, MAXLINE);
					}
					memset(&dataBlock[0], 0, sizeof(dataBlock));	//make sure dataBlock is cleared
				}
				else
				{
					printf("ERROR: unable to create local file.\n");
					write(controlSocket, QUIT, strlen(QUIT));
					exit(EXIT_FAILURE);
				}
				fclose(file);
				
				//for if only 150 was sent initially (see above)
				if (delayed226)
				{
					responseCode = handleResponse(controlSocket, serverResponse);
					errorQuit(controlSocket, responseCode, 226);
				}
				close(dataSocket);
				printf(" --%d bytes received at %s\n", currentTransfer, localPathFile);
				free(localPathFile);
			}
		}
		//help request
		else if (strncmp(userinput, "help", 4) == 0)
		{
			char* topic = strtok(userinput, " ");
			topic = strtok(NULL, " ");
			topic = strtok(topic, "\n");
			char* HELP_arg;
			if (topic != NULL)	//help on specific FTP request
			{
				HELP_arg = malloc(strlen((HELP)) + strlen(topic) + 2);
				strcpy(HELP_arg, HELP);
				strcat(HELP_arg, topic);
				strcat(HELP_arg, "\r\n");
			}
			else	//general help
			{
				HELP_arg = malloc(strlen((HELP)) + 2);
				strcpy(HELP_arg, HELP);
				strcat(HELP_arg, "\r\n");
			}
			write(controlSocket, HELP_arg, strlen(HELP_arg));
			responseCode = handleResponse(controlSocket, serverResponse);
			free(HELP_arg);
			errorQuit(controlSocket, responseCode, 214);
		}
		//CWD request
		else if (strncmp(userinput, "cwd", 3) == 0)
		{
			char* newDir = strtok(userinput, " ");
			newDir = strtok(NULL, " ");
			newDir = strtok(newDir, "\n");
			char* CWD_arg;
			if(newDir == NULL)	// no directory provided
				printf(" --Usage: cwd [<directory>]\n");
			else
			{
				//build and send CWD request
				CWD_arg = malloc(strlen((CWD)) + strlen(newDir) + 2);
				strcpy(CWD_arg, CWD);
				strcat(CWD_arg, newDir);
				strcat(CWD_arg, "\r\n");
				write(controlSocket, CWD_arg, strlen(CWD_arg));
				free(CWD_arg);
				responseCode = handleResponse(controlSocket, serverResponse);
				errorQuit(controlSocket, responseCode, 250);
			}
		}
		else
			printf(" --user input not recognized.\n");
		
		//get new input before looping
		printf("sftp> ");
		fgets(userinput, 500, stdin);
	}
	
	//user is quiting
	write(controlSocket, QUIT, strlen(QUIT));
	responseCode = handleResponse(controlSocket, serverResponse);
	errorQuit(controlSocket, responseCode, 221);
	close(controlSocket);
	printf("OK: %d bytes copied.\n\n", bytesIn);
	
	return 0;
}



/* Main */

int main(int argc, char *argv[])
{
	int	gethost, connVal, connectReturn;
	/*
	  The addrinfo struct holds the result of the query to a URL, which contains the IP address.
	   getaddrinfo returns a linked list of these possible addresses. The list must be iterated through to
	   find a connection that works.
	*/
	struct addrinfo *result = malloc(sizeof(*result)), *resultIter = malloc(sizeof(*resultIter)), hints;
	
	bzero(&result, sizeof(result));
   
	/*
	 The hints addrinfo struct passes parameters to set up how getaddrinfo should behave.
	 ai_family sets whether to allow IPv4, IPv6, or both;
	 ai_socktype should be SOCK_STREAM for TCP connections;
	 ai_flags are useful for debugging the connection.
	 */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;	/* Allow IPv4 only */
	hints.ai_socktype = SOCK_STREAM; /* Stream socket */
	hints.ai_flags = AI_CANONNAME | AI_ADDRCONFIG | AI_PASSIVE;
	hints.ai_protocol = 0;		  /* Any protocol */
	
	
	if (argc == 1) {
		printf("Usage: sftp <address>. <Address> may be IPv4 or URL. \n");
		return -1;
	}
	
	if(argc >= 2)
	{
		/*
		 This function takes in the following parameters: const char hostname (URL or IP), const char port,
		 struct addrinfo hints (can be NULL), struct addrinfo result (pass by reference).
		 */
		gethost = getaddrinfo(argv[1], "ftp", &hints, &result);
		if(gethost != 0)
		{
			fprintf(stderr, "Error unable to get host, errno = %d (%s) \n",
					errno, strerror(errno));
			return -1;
		}
		
		if(gethost == 0)
		{
			//printf(" --Gethost works: canonname is %s \n", result->ai_canonname);
		}
		
	}
	/*
	  Getaddrinfo provides linked list of possible IP addresses associated with a URL.
	  Use a For loop to iterate over the list, stopping at the first address encountered
	  that successfully connects.
	 */
	for (resultIter = result; resultIter != NULL; resultIter = resultIter->ai_next)
	{
		connVal = socket(resultIter->ai_family, resultIter->ai_socktype, resultIter->ai_protocol);
		
		if(resultIter != NULL)	  //Additional structs to check for connection in linked list.
		{
			//printf(" --Result is not null \n");
			//printf(" --ConnVal: %i \n", connVal);
		}
		if (connVal == -1)		  //no connection. Try next addrinfo struct in linked list.
		{
			continue;
		}
		
		connectReturn = connect(connVal, (struct sockaddr *)resultIter->ai_addr, resultIter->ai_addrlen);
		//printf(" --ConnectReturn value is %i \n", connectReturn);
		if (connectReturn != -1)
		{
			
			//printf(" --Successfully connected to socket %i via connect() \n", connVal);
			break;				  /* Success */
		}
		else
		{
			fprintf(stderr, "Error connection request refused, errno = %d (%s) \n",
					errno, strerror(errno));
		}
		// close the attempted connection that did not work, and try next struct if not NULL.
		close(connVal);
	}
	
	if (resultIter == NULL) {			   /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	
	//Free memory; we no longer need the info in struct result.
	freeaddrinfo(result);
	//printf(" --Connect call performed \n");
	
	int checkNumPaths = 0;
	char* serverDirectory, *localDirectory;
	
	//Assumption: last argument typed will always be server pathname.
	if(argc >=3)
	{
		serverDirectory = argv[2];
		checkNumPaths++;
	}
	//Assumption: 4th argument will always be local directory the user
	//wants to download files into. 
	if(argc == 4)
	{
		localDirectory = argv[3];
		checkNumPaths++;
	}
	controlSession(connVal, checkNumPaths, serverDirectory, localDirectory);
	
	close(connVal);
	
	return 0;
}
