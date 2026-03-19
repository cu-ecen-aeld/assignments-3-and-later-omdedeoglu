#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#define BUF_SIZE 500

bool caught_sigint = false;
bool caught_sigterm = false;

static void signal_handler ( int signal_number )
{
    int errno_saved = errno;
    if ( signal_number == SIGINT ) {
        caught_sigint = true;
    } else if ( signal_number == SIGTERM ) {
        caught_sigterm = true;
    }
    errno = errno_saved;
}

int main(int argc, char *argv[]) {
    openlog("aesdsocketApp", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting aesdsocket server");

    printf("Hello from aesdsocket.c! number of arguments: %d arguments: %s \n",argc, argv[0]);
    
    struct sigaction new_action;
    bool success = true;
    memset(&new_action,0,sizeof(struct sigaction));
    new_action.sa_handler=signal_handler;
    if( sigaction(SIGTERM, &new_action, NULL) != 0 ) {
        printf("Error %d (%s) registering for SIGTERM",errno,strerror(errno));
        success = false;
    }
    if( sigaction(SIGINT, &new_action, NULL) ) {
        printf("Error %d (%s) registering for SIGINT",errno,strerror(errno));
        success = false;
    }
    if(!success) {
        syslog(LOG_ERR, "Error registering signal handlers");
        return -1;
    }

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int status, socketfd=-1, acceptfd=-1;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    char host[NI_MAXHOST];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if( (status = getaddrinfo(NULL, "9000", &hints, &result)) != 0){
        syslog(LOG_ERR, "getaddrinfo Failed! %s", strerror(errno));
        printf("getaddrinfo Failed! \n");
        return -1;
    }

    int yes=1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        socketfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
        if (socketfd == -1)
            continue;

        setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

        if (bind(socketfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; //Success

        close(socketfd);
    }
    freeaddrinfo(result);
    
    if (rp == NULL) {   // No address succeeded
        syslog(LOG_ERR, "bind Failed! %s", strerror(errno));
        printf("bind Failed! \n");
        return -1;
    }

    if( listen(socketfd, 5) != 0){
        syslog(LOG_ERR, "listen Failed! %s", strerror(errno));
        printf("listen Failed! \n");
        return -1;
    }

    printf("server waiting connection over localhost:9000 \n");
    while(1){
        if( caught_sigint ) {
            syslog(LOG_ERR, "Caught signal, exiting ");
            printf("\nCaught SIGINT!\n");
            close(socketfd);
            close(acceptfd);
            break;
        }
        if( caught_sigterm ) {
            syslog(LOG_ERR, "Caught signal, exiting ");
            printf("\nCaught SIGTERM!\n");
            close(socketfd);
            close(acceptfd);
            break;
        }

        addr_size = sizeof(their_addr);
        acceptfd = accept(socketfd, (struct sockaddr *)&their_addr, &addr_size);
        if (acceptfd == -1) {
            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }

        int rc = getnameinfo(
            (struct sockaddr *)&their_addr,
            addr_size,
            host,
            sizeof(host),
            NULL,
            0,
            NI_NUMERICHOST
        );

        if (rc == 0) {
            printf("Accepted connection from %s\n", host);
            syslog(LOG_INFO, "Accepted connection from %s", host);
        }


    }

    return 0;

}