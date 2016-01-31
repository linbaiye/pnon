#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "log.h"
#include "protocol.h"
#include "client.h"


int main(void)
{
    if (log_init("/var/log/mytunnel.log", DEBUG) < 0) {
        fprintf(stderr, "failed to init log:%s", strerror(errno));
        return -1;
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
