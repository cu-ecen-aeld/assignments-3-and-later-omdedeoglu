/*
* aesdsocket.c 
*/

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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <pthread.h>



#define DATA_FILE "/var/tmp/aesdsocketdata"
#define FILE_BUF_SIZE 1024
#define BUF_SIZE 500
#define PORT "9000"

/* global definitions */
bool caught_sigint = false;
bool caught_sigterm = false;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;


typedef struct thread_ctx {
    int client_fd;
    int finished;
    pthread_t tid;

    SLIST_ENTRY(thread_ctx) entries;
} thread_ctx_t;


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


static void daemonize(void)
{
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Parent exits
        exit(EXIT_SUCCESS);
    }

    // Child continues
    if (setsid() == -1) {
        syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Optional second fork (not strictly required, but safe)
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "second fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Reset file mode mask
    umask(0);

    // Change working directory
    if (chdir("/") != 0) {
        syslog(LOG_ERR, "chdir failed");
        exit(EXIT_FAILURE);
    }

    // Redirect standard files to /dev/null
    /*int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }*/
}


void *client_thread(void *arg)
{
    thread_ctx_t *ctx = arg;
    int numbytes=0;
    char buf[BUF_SIZE];

    while(1){
        if( caught_sigint ) {
            syslog(LOG_ERR, "Caught signal, exiting ");
            printf("\nCaught SIGINT!\n");
            goto close_socket;
        }
        if( caught_sigterm ) {
            syslog(LOG_ERR, "Caught signal, exiting ");
            printf("\nCaught SIGTERM!\n");
            goto close_socket;
        }
        char *packet = NULL;
        size_t packet_size = 0;
        bool newline_found = false;

        while (!newline_found) {
            numbytes = recv(ctx->client_fd, buf, BUF_SIZE, 0);
            if (numbytes == -1) {
                syslog(LOG_ERR, "recv failed: %s", strerror(errno));
                goto close_socket;
            }
            if (numbytes <= 0) {
                // client closed connection
                syslog(LOG_INFO, "Closed connection ");
                printf("Closed connection \n");
                goto close_socket;
            }

            char *new_packet = realloc(packet, packet_size + numbytes);
            if (!new_packet) {
                syslog(LOG_ERR, "realloc failed");
                free(packet);
                goto close_socket;
            }
            packet = new_packet;
            /*for(int i=0; i<numbytes; i++){
                printf("buf[%d]:0x%X \n",i,buf[i]);
            }*/
            memcpy(packet + packet_size, buf, numbytes);
            packet_size += numbytes;

            if (memchr(buf, '\n', numbytes)) {
                //printf("newline found\n");
                newline_found = true;
            }
        }

        
        //printf("open !\n");
        pthread_mutex_lock(&file_mutex);
        int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd == -1) {
            syslog(LOG_ERR, "open failed: %s", strerror(errno));
            free(packet);
            pthread_mutex_unlock(&file_mutex);
            goto close_socket;
        }

        if (write(fd, packet, packet_size) == -1) {
            syslog(LOG_ERR, "write failed: %s", strerror(errno));
        }
        close(fd);
        free(packet);
        packet = NULL;

        fd = open(DATA_FILE, O_RDONLY);
        if (fd == -1) {
            syslog(LOG_ERR, "open for read failed: %s", strerror(errno));
            pthread_mutex_unlock(&file_mutex);
            goto close_socket;
        }

        char filebuf[FILE_BUF_SIZE];
        ssize_t bytes_read;

        while ((bytes_read = read(fd, filebuf, FILE_BUF_SIZE)) > 0) {
            if (send(ctx->client_fd, filebuf, bytes_read, 0) == -1) {
                syslog(LOG_ERR, "send failed: %s", strerror(errno));
                break;
            }
        }
        close(fd);
        pthread_mutex_unlock(&file_mutex);
    }

    close_socket:
    if (ctx->client_fd != -1) {
        //syslog(LOG_INFO, "Closed connection from %s", host);
        syslog(LOG_INFO, "Closed connection");
        close(ctx->client_fd);
        ctx->client_fd = -1;
        ctx->finished = 1;
    }
    return NULL;
}

void *timestamp_thread(void *arg)
{
    (void)arg;

    while (!caught_sigint && !caught_sigterm) {

        int elapsed = 0;

        while (elapsed < 10) {
            if (caught_sigint || caught_sigterm) {
                break;
            }
            sleep(1);
            elapsed++;
        }

        // Only write if full 10 seconds completed
        if (elapsed < 10) {
            break;
        }

        // Safe to write timestamp
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        if (!tm_info)
            continue;

        char timebuf[128];
        strftime(timebuf, sizeof(timebuf),
                "%a, %d %b %Y %H:%M:%S %z", tm_info);

        char line[256];
        snprintf(line, sizeof(line),
                "timestamp:%s\n", timebuf);

        pthread_mutex_lock(&file_mutex);
        int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) {
            if (write(fd, line, strlen(line)) == -1) {
                syslog(LOG_ERR, "Timestamp_thread: write failed: %s", strerror(errno));
            }
            close(fd);
        }
        pthread_mutex_unlock(&file_mutex);
    }

    return NULL;
}


int main(int argc, char *argv[]) {
    
    bool daemon_mode = false;

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    } else if (argc > 1) {
        fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    openlog("aesdsocketApp", LOG_PID | LOG_CONS, LOG_USER);
    unlink(DATA_FILE); 
    
    if (daemon_mode) {
        daemonize();
    }

    syslog(LOG_INFO, "Starting aesdsocket server");

    printf("Hello from aesdsocket.c! number of arguments: %d arguments: %s \n",argc, argv[0]);
    
    SLIST_HEAD(thread_list, thread_ctx);

    struct thread_list threads;
    SLIST_INIT(&threads);

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
    if( (status = getaddrinfo(NULL, PORT, &hints, &result)) != 0){
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

    if( listen(socketfd, 10) != 0){
        syslog(LOG_ERR, "listen Failed! %s", strerror(errno));
        printf("listen Failed! \n");
        return -1;
    }

    printf("server waiting connection over localhost:9000 \n");

    /*start timer thread*/
    pthread_t timestamp_tid;
    if(pthread_create(&timestamp_tid, NULL, timestamp_thread, NULL) != 0){
        syslog(LOG_ERR, "Timer thread create Failed! %s", strerror(errno));
        printf("Timer thread create Failed! \n");
        return -1;
    }

    while(1){
        if( caught_sigint ) {
            syslog(LOG_ERR, "Caught signal, exiting ");
            printf("\nCaught SIGINT!\n");
            goto cleanup_connection;
        }
        if( caught_sigterm ) {
            syslog(LOG_ERR, "Caught signal, exiting ");
            printf("\nCaught SIGTERM!\n");
            goto cleanup_connection;
        }

        addr_size = sizeof(their_addr);
        acceptfd = accept(socketfd, (struct sockaddr *)&their_addr, &addr_size);
        if (acceptfd != -1) {
            
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

            thread_ctx_t *ctx = calloc(1, sizeof(*ctx));
            ctx->client_fd = acceptfd;
            pthread_create(&ctx->tid, NULL, client_thread, ctx);
            SLIST_INSERT_HEAD(&threads, ctx, entries);
        }

        /* reap finished threads */
        thread_ctx_t *curr = SLIST_FIRST(&threads);
        thread_ctx_t *tmp = NULL;

        while (curr) {
            
            tmp = SLIST_NEXT(curr, entries);
            
            if (curr->finished) {
                pthread_join(curr->tid, NULL);
                SLIST_REMOVE(&threads, curr, thread_ctx, entries);
                free(curr);

            }

            curr = tmp;
        }
    }

    cleanup_connection:
    //printf("cleanup_connection\n");
    syslog(LOG_INFO, "Ending aesdsocket server! cleanup_connection");
    if (acceptfd != -1) {
        syslog(LOG_INFO, "Closed connection from %s", host);
        close(acceptfd);
        acceptfd = -1;
    }

    if (socketfd != -1){
        syslog(LOG_INFO, "Closed Socket %s", PORT);
        close(socketfd);
    }

    thread_ctx_t *ctx; 
    SLIST_FOREACH(ctx, &threads, entries) {
        shutdown(ctx->client_fd, SHUT_RDWR);
    }

    /* reap finished threads */
    thread_ctx_t *curr = SLIST_FIRST(&threads);
    thread_ctx_t *tmp = NULL;

    while (curr) {
        
        tmp = SLIST_NEXT(curr, entries);
        
        if (curr->finished) {
            pthread_join(curr->tid, NULL);
            SLIST_REMOVE(&threads, curr, thread_ctx, entries);
            free(curr);

        }

        curr = tmp;
    }

    syslog(LOG_INFO, "Reap Finished waiting timestamp thread..");
    //pthread_cancel(timestamp_tid);
    pthread_join(timestamp_tid, NULL);
    syslog(LOG_INFO, "Timestamp thread joined, starting to delete the file..");

    if (unlink(DATA_FILE) == 0) {
        syslog(LOG_INFO, "%s File deleted successfully", DATA_FILE);
        printf("File deleted successfully\n");
    } else {
        syslog(LOG_ERR, "unlink failed: %s", strerror(errno));
    }

    

    return 0;

}