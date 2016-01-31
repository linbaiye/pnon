#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include "log.h"

static FILE *logfp = NULL;
static pid_t pid;
static const char *const log_level[] = {
    "D",                    /* Debug    */
    "I",                    /* Info     */
    "W",                    /* Warning */
    "E"                     /* Error    */
};

static int lowest_level = DEBUG;
#define MAX_PATH_LEN 128


/* Never call this method directly, see log.h.*/
void __log(const char *file, const char *fn, int line, int level, const char *fmt, ...)
{
    char timestr[64], str[256];
    struct tm *tm = NULL;
    time_t tim;
    va_list ap;
    if (!logfp || lowest_level > level) {
        return;
    }
    time(&tim);
    tm = localtime(&tim);
    strftime(timestr, 64, "%F %T", tm);
    va_start(ap, fmt);
    vsnprintf(str, 256, fmt, ap);
    va_end(ap);
    fprintf(logfp, "[%d] [%s] %s [%s:%s:%d] %s\n", pid, timestr, log_level[level], file, fn, line, str);
    fflush(logfp);
}


int log_init(const char *path, int l)
{
    /* Must leave a room for '\0'. */
    if (path == NULL || strnlen(path, MAX_PATH_LEN) > MAX_PATH_LEN) {
        return -1;
    }
    if (l >= DEBUG && l <= ERROR) {
      lowest_level = l;
    }
    logfp = fopen(path, "a");
    if (logfp == NULL) {
        return -1;
    }
    pid = getpid();
    return 0;
}

