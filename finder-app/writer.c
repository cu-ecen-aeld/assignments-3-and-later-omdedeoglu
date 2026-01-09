#include <fcntl.h>      // for open(), O_* flags
#include <sys/stat.h>   // for mode constants (e.g., 0644)
#include <unistd.h>     // for close(), write()
#include <errno.h>      // for errno
#include <stdio.h>      // for perror()
#include <syslog.h>     // for syslog functions
#include <string.h>     // for strlen(), strerror()
#include <stdbool.h>    // for bool type


int main(int argc, char *argv[]) {

    // Check for the correct number of arguments
    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments. Usage: %s <filename> <message>", argv[0]);

        closelog();
        return 1;
    }

    // Initialize syslog
    openlog("writerApp", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "writer app started");

    // Get the filename and message from command line arguments
    const char *filename = argv[1];
    const char *message = argv[2];

    // Open the file for writing
    int fd;
    fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open file %s open:%s", filename, strerror(errno));
        closelog();
        return 1;
    }

    // Write the message to the file
    ssize_t ret;
    size_t len = strlen(message);
    const char *buf = message;
    bool blWriteSuccess = true;
    while (len != 0 && (ret = write (fd, buf, len)) != 0) {
        if (ret == -1) {
            if (errno == EINTR)
                    continue;
            syslog(LOG_ERR, "Failed to write file %s write:%s", filename, strerror(errno));
            blWriteSuccess = false;
            break;
        }

        len -= ret;
        buf += ret;
    }
    
    if(blWriteSuccess) {
        syslog(LOG_DEBUG, "Writing %s to %s", message, filename);
    }
    closelog();
    return 0;
}
