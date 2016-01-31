#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include "log.h"
#include "protocol.h"
#include "event.h"


int main(void)
{
    if (log_init("/var/log/mytunnel.log", DEBUG) < 0) {
        fprintf(stderr, "failed to init log.");
        return -1;
    }
    if (even_init() < 0) {
        log_error("Failed to init event.");
        return 1;
    }
    prot_init();
    if (event_loop() != 0) {
        return 1;
    }
    return 0;
}
