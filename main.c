//
//  main.c
//  CS537 FTP Client
//
//  Created by Christopher Erdelyi on 4/7/18.
//  Copyright © 2018 Christopher Erdelyi. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
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
    printf("Inside data conn setup \n");
    int dataSocket, dataConn, gethost, connectReturn, port1, port2;
    char *token, *ip1, *ip2, *ip3, *ip4, *p1, *p2, *dataPortString, addr[16];
    
    
    printf("String is %s \n", pasvSetting);
    token = strtok(pasvSetting, "(");
    printf("Token found: %s\n", token);
    
    
    
        ip1 = strtok(NULL, ",");
        ip2 = strtok(NULL, ",");
        ip3 = strtok(NULL, ",");
        ip4 = strtok(NULL, ",");
    
    p1 = strtok(NULL, ",");
    printf("P1 is %s \n", p1);
    p2 = strtok(NULL, ")");
    printf("P2 is %s \n", p2);
    port1 = atoi(p1);
    port2 = atoi(p2);
    
    snprintf(addr, 16, "%s.%s.%s.%s", ip1, ip2, ip3, ip4);
    printf("Full IP is %s \n", addr);
    
    dataSocket = ((port1 * 256) + port2);
    printf("DataSocket is %i \n", dataSocket);
   
    struct sockaddr_in pass;
    pass.sin_family        = AF_INET;
    pass.sin_addr.s_addr   = inet_addr(addr);
    pass.sin_port          = htons(dataSocket);
    
        dataConn = socket(pass.sin_family, SOCK_STREAM, 0);
        connectReturn = connect(dataConn, (struct sockaddr_in*)&pass, sizeof(pass));
        printf("ConnectReturn value is %i \n", connectReturn);
        if (connectReturn != -1)
        {
            
            printf("Successfully connected to socket %i via connect() \n", dataConn);                  // Success
        }
        else
        {
            fprintf(stderr, "Error connection request refused, errno = %d (%s) \n",
                    errno, strerror(errno));
        }
    
    return dataConn;
}

int controlSession(int controlSocket)
{
   
    printf("Entered control session \n");
    printf("controlSocket is %i \n", controlSocket);
    char serverResponse[MAXLINE+1], originalResponse[MAXLINE+1], dataBlock[MAXLINE+1];
    char* servNumbers;
    char* userQuit = "221";
    char* USER = "USER anonymous\r\n";
    char* PASS = "PASS \r\n";
    char* PASV = "PASV\r\n";
    char* TYPE = "TYPE I\r\n";
    char* CWD = "CWD /tmp\r\n";
    char* NLST = "NLST /\r\n";
    char* HELP = "HELP\r\n";
    char* QUIT = "QUIT\r\n";
    
    int responseCode;
    int dataSocket;
    
    ssize_t bytesIn;
    memset(&serverResponse[0], 0, sizeof(serverResponse));
    memset(&dataBlock[0], 0, sizeof(dataBlock));
    
    sleep(1);
    read(controlSocket, serverResponse, MAXLINE);
            serverResponse[strlen(serverResponse)+1] = '\0';
            printf("Server: %s \n", serverResponse);
            fflush(stdout);
    
    servNumbers = strtok(serverResponse, " ");
    responseCode = atoi(servNumbers);
    
    while(responseCode != 221){
        switch(responseCode) {
            case 220:   write(controlSocket, USER, strlen(USER));
                        printf("Sent USER \n");
                        break;
            
            case 331:   write(controlSocket, PASS, strlen(PASS));
                        printf("Sent PASS \n");
                        break;
                
            case 230:   write(controlSocket, PASV, strlen(PASV));
                        printf("Sent PASV \n");
                        break;
                
            case 227:   //use function to parse string to get port number
                        dataSocket = dataConnSetup(originalResponse);
                        printf("FTP data connection established. \n");
						write(controlSocket, NLST, strlen(NLST));
						printf("Sent NLST \n");
						while (read(dataSocket, dataBlock, MAXLINE))
						{
							printf("DIR LISTING:\n %s \n", dataBlock);
						}
						memset(&dataBlock[0], 0, sizeof(dataBlock));
                        break;
			default:	write(controlSocket, QUIT, strlen(QUIT));
						break;

        
        }
 
        memset(&serverResponse[0], 0, sizeof(serverResponse));
        sleep(1);
        
        bytesIn = read(controlSocket, serverResponse, MAXLINE);
        serverResponse[strlen(serverResponse)+1] = '\0';
        printf("Server: %s \n", serverResponse);
        printf(" ***** read %i bytes ******* \n\n", bytesIn);
        fflush(stdout);
        strcpy(originalResponse, serverResponse);
        servNumbers = strtok(serverResponse, " ");
        responseCode = atoi(servNumbers);
        printf("Server Response by case loop is %s\n", serverResponse);
    
    }
    
    close(controlSocket);
    
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
    
    if(argc == 2)
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
            printf("Gethost works: canonname is %s \n", result->ai_canonname);
        }
        
    }
    
    for (resultIter = result; resultIter != NULL; resultIter = resultIter->ai_next)
    {
        connVal = socket(resultIter->ai_family, resultIter->ai_socktype, resultIter->ai_protocol);
        
        if(resultIter != NULL)
        {
            printf("Result is not null \n");
            printf("ConnVal: %i \n", connVal);
        }
        if (connVal == -1)
        {
            continue;
        }
        
        connectReturn = connect(connVal, (struct sockaddr *)resultIter->ai_addr, resultIter->ai_addrlen);
        printf("ConnectReturn value is %i \n", connectReturn);
        if (connectReturn != -1)
        {
            
            printf("Successfully connected to socket %i via connect() \n", connVal);
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
    printf("Connect call performed \n");
    char* host = argv[1];
    controlSession(connVal);
    
    return 0;
}


