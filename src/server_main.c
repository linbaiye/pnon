#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include "log.h"
#include "protocol.h"
#include "server.h"


static int daemonize(void)
{
    pid_t pid = fork();
    struct sigaction sa;
    struct rlimit rl;
    umask(0);
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        return -1;
    }
    if (pid > 0) {
        exit(0);
    } else if (pid < 0) {
        return -1;
    }
    setsid();
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        return -1;
    }
    if ((pid = fork()) < 0) {
        return -1;
    } else if (pid > 0) {
        exit(0);
    }
    int limit = rl.rlim_max == RLIM_INFINITY ? 1024 : rl.rlim_max;
    for (int i = 0; i < limit; i++) {
        close(i);
    }
    int fd0 = open("/dev/null", O_RDWR);
    int fd1 = dup(0);
    int fd2 = dup(0);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        return -1;
    }
    return 0;
}


int main(void)
{
    if (daemonize() < 0) {
        fprintf(stderr, "failed to daemonize:%s", strerror(errno));
        return 1;
    }
    if (log_init("/var/log/mytunnel.log", DEBUG) < 0) {
        fprintf(stderr, "failed to init log:%s", strerror(errno));
        return 1;
    }
    if (event_init() < 0) {
        log_error("Failed to init event.");
        return 1;
    }
    prot_init();
    if (event_loop() != 0) {
        return 1;
    }
    return 0;
}
