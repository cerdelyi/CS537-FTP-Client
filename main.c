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

 int controlSession()
{
    printf("This passed through \n");
    return 1;
};


int main(int argc, char *argv[])
{
    int    requestfd, gethost, connVal;
    pthread_t tid;
    struct addrinfo *result, *resultIter, hints;
   
    bzero(&result, sizeof(result));
    
    hints.ai_family = AF_INET;
    
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
        gethost = getaddrinfo("ftp://ftp.dlptest.com/", NULL, &hints, &result);
        if(gethost == -1)
        {
            fprintf(stderr, "Error unable to create socket, errno = %d (%s) \n",
                    errno, strerror(errno));
            return -1;
        }
        
    }
    printf("Gethost works \n");
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */
    
    requestfd = socket(AF_INET, SOCK_STREAM, 0);
    if (requestfd == -1)
    {
        fprintf(stderr, "Error unable to create socket, errno = %d (%s) \n",
                errno, strerror(errno));
        return -1;
    }
    
    printf("created socket %i \n", requestfd);
    
    for (resultIter = result; resultIter != NULL; resultIter = resultIter->ai_next) {
        connVal = socket(resultIter->ai_family, resultIter->ai_socktype,
                     resultIter->ai_protocol);
        if (connVal == -1)
            continue;
        
        if (connect(connVal, resultIter->ai_addr, resultIter->ai_addrlen) != -1)
            break;                  /* Success */
        
        close(connVal);
    }
    
    if (resultIter == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }
    
    freeaddrinfo(result);
    printf("Connect call performed \n");
    
    if ((requestfd < 0 )) {
        if (errno != EINTR)
        {
            fprintf(stderr, "Error connection request refused, errno = %d (%s) \n",
                    errno, strerror(errno));
        }
    }
        
        if (pthread_create(&tid, NULL, controlSession(), (void *)result) != 0) {
            fprintf(stderr, "Error unable to create thread, errno = %d (%s) \n",
                    errno, strerror(errno));
        }
        
}


