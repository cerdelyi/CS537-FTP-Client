////////
//
//  main.c
//  CS537 FTP Client
//
//  Created by Christopher Erdelyi on 4/7/18.
//    Contributed to by John Wu.
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

const int backlog = 4;

int dataConnSetup(char* pasvSetting)
{
    int dataSocket, dataConn, gethost, connectReturn, port1, port2;
    char *token, *ip1, *ip2, *ip3, *ip4, *p1, *p2, *dataPortString, addr[16];
    
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
    
    dataSocket = ((port1 * 256) + port2);
    
    struct sockaddr_in pass;
    pass.sin_family        = AF_INET;
    pass.sin_addr.s_addr   = inet_addr(addr);
    pass.sin_port          = htons(dataSocket);
    
    dataConn = socket(pass.sin_family, SOCK_STREAM, 0);
    connectReturn = connect(dataConn, (struct sockaddr *)&pass, sizeof(pass));
    if (connectReturn != -1)
    {
        printf(" --Data connection made. IP %s, Socket %i \n", addr, dataSocket);        // Success
    }
    else
    {
        fprintf(stderr, "Error connection request refused, errno = %d (%s) \n",
                errno, strerror(errno));
    }
    
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
    copy = (char*) malloc(strlen(servResp));
    strcpy(copy, servResp);
    responseLine = strtok(copy, "\r\n");
    while(responseLine != NULL)
    {
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

//sets up anonymous login
//loops through allowing user requests and appropriate ftp actions
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
            if (path != NULL)    //ls with path
            {
                NLST_arg = malloc(strlen((NLST)) + strlen(path) + 2);
                strcpy(NLST_arg, NLST);
                strcat(NLST_arg, path);
                strcat(NLST_arg, "\r\n");
            }
            else    //plain ls
            {
                NLST_arg = malloc(strlen((NLST)) + 2);
                strcpy(NLST_arg, NLST);
                strcat(NLST_arg, "\r\n");
            }
            
            //send PASV
            write(controlSocket, PASV, strlen(PASV));
            responseCode = handleResponse(controlSocket, serverResponse);
            errorQuit(controlSocket, responseCode, 227);
            //PASV success: setup data connetion, send NLST
            dataSocket = dataConnSetup(serverResponse);
            write(controlSocket, NLST_arg, strlen(NLST_arg));
            responseCode = handleResponse(controlSocket, serverResponse);
            errorQuit(controlSocket, responseCode, 226);
            //server sent directory success -> read and show data from data socket
            printf(" --File Directory-- \n");
            while (read(dataSocket, dataBlock, MAXLINE))
            {
                printf("%s", dataBlock);
                memset(&dataBlock[0], 0, sizeof(dataBlock));
            }
        }
        //get request
        else if (strncmp(userinput, "get", 3) == 0)
        {
            char* serverPathFile = strtok(userinput, " ");
            serverPathFile = strtok(NULL, " ");
            char* RETR_arg;
            if (serverPathFile == NULL)    //no filename provided
                printf(" --Usage: get [<directory>]/<filename> [<local directory>]/\n");
            else    //filename (and/or path given
            {
                //build request for sending to server
                RETR_arg = malloc(strlen((RETR)) + strlen(serverPathFile) + 2);
                strcpy(RETR_arg, RETR);
                strcat(RETR_arg, serverPathFile);
                strcat(RETR_arg, "\r\n");
                
                //tok for local directory
                char* localPath = strtok(NULL, " ");
                localPath = strtok(localPath, "\n");
                //tok serverPathFile to get filename
                char* filename = serverPathFile;
                while(strchr(filename, '/'))
                    filename = strchr(filename, '/') + 1;
                filename = strtok(filename, "\n");
                //combine cmd line local + input local + filename to open filelength
                char* localPathFile;
                if (numPaths >= 2 && localPath != NULL)    //local dir and path provided
                {
                    localPathFile = malloc(2 + strlen(localDir) + strlen(localPath) + strlen(filename));
                    strcpy(localPathFile, "./");
                    strcat(localPathFile, localDir);
                    strcat(localPathFile, localPath);
                    mkdir(localPathFile, 0777);        //create dir if doesn't exit
                    strcat(localPathFile, filename);
                }
                else if (numPaths >= 2)    //local dir only
                {
                    localPathFile = malloc(2 + strlen(localDir) + strlen(filename));
                    strcpy(localPathFile, "./");
                    strcat(localPathFile, localDir);
                    mkdir(localPathFile, 0777);        //create dir if doesn't exit
                    strcat(localPathFile, filename);
                }
                else if (localPath != NULL)    //local path only
                {
                    localPathFile = malloc(2 + strlen(localPath) + strlen(filename));
                    strcpy(localPathFile, "./");
                    strcat(localPathFile, localPath);
                    mkdir(localPathFile, 0777);        //create dir if doesn't exit
                    strcat(localPathFile, filename);
                }
                else    //no local dir and no local path
                {
                    localPathFile = malloc(strlen(filename));
                    strcpy(localPathFile, filename);
                }
				
                //open Image data connection
                //send PASV
                write(controlSocket, PASV, strlen(PASV));
                responseCode = handleResponse(controlSocket, serverResponse);
                errorQuit(controlSocket, responseCode, 227);
                //PASV success: setup data connetion in binary, send RETR
                dataSocket = dataConnSetup(serverResponse);
                write(controlSocket, TYPE, strlen(TYPE));
                responseCode = handleResponse(controlSocket, serverResponse);
                errorQuit(controlSocket, responseCode, 200);
                write(controlSocket, RETR_arg, strlen(RETR_arg));
                responseCode = handleResponse(controlSocket, serverResponse);
                
                int delayed226 = 0;
                if (responseCode == 150)
                    delayed226 = 1;
                else
                    errorQuit(controlSocket, responseCode, 226);
                
                int currentTransfer = 0;
                //read data connection and write file
                FILE* file = fopen(localPathFile, "w");
                if(file!=NULL)
                {
                    int datasize = read(dataSocket, dataBlock, MAXLINE);
                    while (datasize > 0)
                    {
                        bytesIn += datasize;
                        currentTransfer += datasize;
                        fwrite(dataBlock, sizeof(char), datasize, file);
                        memset(&dataBlock[0], 0, sizeof(dataBlock));
                        datasize = read(dataSocket, dataBlock, MAXLINE);
                    }
                    memset(&dataBlock[0], 0, sizeof(dataBlock));
                }
                else
                {
                    printf("ERROR: unable to create local file.\n");
                    write(controlSocket, QUIT, strlen(QUIT));
                    exit(EXIT_FAILURE);
                }
                fclose(file);
                
                if (delayed226)
                {
                    responseCode = handleResponse(controlSocket, serverResponse);
                    errorQuit(controlSocket, responseCode, 226);
                }
                printf(" --%d bytes received at %s\n", currentTransfer, localPathFile);
            }
        }
        //help request
        else if (strncmp(userinput, "help", 4) == 0)
        {
            char* topic = strtok(userinput, " ");
            topic = strtok(NULL, " ");
            topic = strtok(topic, "\n");
            char* HELP_arg;
            if (topic != NULL)    //help on specific FTP request
            {
                HELP_arg = malloc(strlen((HELP)) + strlen(topic) + 2);
                strcpy(HELP_arg, HELP);
                strcat(HELP_arg, topic);
                strcat(HELP_arg, "\r\n");
            }
            else    //general help
            {
                HELP_arg = malloc(strlen((HELP)) + 2);
                strcpy(HELP_arg, HELP);
                strcat(HELP_arg, "\r\n");
            }
            write(controlSocket, HELP_arg, strlen(HELP_arg));
            responseCode = handleResponse(controlSocket, serverResponse);
            errorQuit(controlSocket, responseCode, 214);
        }
        //CWD request
        else if (strncmp(userinput, "cwd", 3) == 0)
        {
            char* newDir = strtok(userinput, " ");
            newDir = strtok(NULL, " ");
            newDir = strtok(newDir, "\n");
            char* CWD_arg;
			if(newDir == NULL)
				printf(" --Usage: cwd [<directory>]\n");
			else
			{
				CWD_arg = malloc(strlen((CWD)) + strlen(newDir) + 2);
				strcpy(CWD_arg, CWD);
				strcat(CWD_arg, newDir);
				strcat(CWD_arg, "\r\n");
				write(controlSocket, CWD_arg, strlen(CWD_arg));
				responseCode = handleResponse(controlSocket, serverResponse);
				errorQuit(controlSocket, responseCode, 250);
			}
        }
        else
            printf(" --user input not recognized.\n");
        
        printf("sftp> ");
        fgets(userinput, 500, stdin);
    }
    
    //user is quiting
    write(controlSocket, QUIT, strlen(QUIT));
    responseCode = handleResponse(controlSocket, serverResponse);
    errorQuit(controlSocket, responseCode, 221);
    printf("OK: %d bytes copied.\n\n", bytesIn);
    
    return 0;
}



/* Main */

int main(int argc, char *argv[])
{
    int    gethost, connVal, connectReturn;
    struct addrinfo *result = malloc(sizeof(*result)), *resultIter = malloc(sizeof(*resultIter)), hints;
    
    bzero(&result, sizeof(result));
    // bzero(&passThru, sizeof(passThru));
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow IPv4 & IPv6 for test */
    hints.ai_socktype = SOCK_STREAM; /* Stream socket */
    hints.ai_flags = AI_CANONNAME | AI_ADDRCONFIG | AI_PASSIVE;
    hints.ai_protocol = 0;          /* Any protocol */
    
    
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
    
    for (resultIter = result; resultIter != NULL; resultIter = resultIter->ai_next)
    {
        connVal = socket(resultIter->ai_family, resultIter->ai_socktype, resultIter->ai_protocol);
        
        if(resultIter != NULL)
        {
            //printf(" --Result is not null \n");
            //printf(" --ConnVal: %i \n", connVal);
        }
        if (connVal == -1)
        {
            continue;
        }
        
        connectReturn = connect(connVal, (struct sockaddr *)resultIter->ai_addr, resultIter->ai_addrlen);
        //printf(" --ConnectReturn value is %i \n", connectReturn);
        if (connectReturn != -1)
        {
            
            //printf(" --Successfully connected to socket %i via connect() \n", connVal);
            break;                  /* Success */
        }
        else
        {
            fprintf(stderr, "Error connection request refused, errno = %d (%s) \n",
                    errno, strerror(errno));
        }
        close(connVal);
    }
    
    if (resultIter == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }
    
    freeaddrinfo(result);
    //printf(" --Connect call performed \n");
    
    int checkNumPaths = 0;
    char* serverDirectory, *localDirectory;
    if(argc >=3)
    {
        serverDirectory = argv[2];
        checkNumPaths++;
    }
    if(argc == 4)
    {
        localDirectory = argv[3];
        checkNumPaths++;
    }
    controlSession(connVal, checkNumPaths, serverDirectory, localDirectory);
    
    close(connVal);
    
    return 0;
}

