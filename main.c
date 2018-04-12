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
#include <pthread.h>

#define MAXLINE 1500

const int backlog = 4;

struct threadParams{
    struct addrinfo *passAddr;
    int socket;
};

void *controlSession(void *arg)
{
    printf("Entered control session \n");
    
    struct threadParams *passThru;
    bzero(&passThru, sizeof(passThru));
    passThru= (struct threadParams *) arg;
    struct addrinfo *result = passThru->passAddr;
    int socket = passThru->socket;
    
    printf("socket is %i \n", socket);
    
    return 0;
};


int main(int argc, char *argv[])
{
    int    gethost, connVal, connectReturn;
    pthread_t tid;
    struct addrinfo *result, *resultIter, hints;
    struct threadParams *passThru;
   
    bzero(&result, sizeof(result));
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 & IPv6 for test */
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
    
    if ((connVal < 0 )) {
        if (errno != EINTR)
        {
            fprintf(stderr, "Error connection request refused, errno = %d (%s) \n",
                    errno, strerror(errno));
        }
    }
    
    malloc(sizeof(passThru));
    bzero(&passThru, sizeof(passThru));
    passThru->passAddr = resultIter;
    passThru->socket = connVal;
    
        if (pthread_create(&tid, NULL, controlSession, (void *)passThru) != 0) {
            fprintf(stderr, "Error unable to create thread, errno = %d (%s) \n",
                    errno, strerror(errno));
        }
        
}


