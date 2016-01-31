#ifndef _LOG_H_
#define _LOG_H_

#define DEBUG 0
#define INFO  1
#define WARN  2
#define ERROR 3
extern void __log(const char *file, const char *fn, int line, int level, const char *fmt, ...);
extern int log_init(const char *, int l);
#define log_debug(fmt...) __log(__FILE__, __FUNCTION__, __LINE__, DEBUG, ##fmt)
#define log_info(fmt...)  __log(__FILE__, __FUNCTION__, __LINE__, INFO, ##fmt)
#define log_warn(fmt...)  __log(__FILE__, __FUNCTION__, __LINE__, WARN, ##fmt)
#define log_error(fmt...) __log(__FILE__, __FUNCTION__, __LINE__, ERROR, ##fmt)
#endif

