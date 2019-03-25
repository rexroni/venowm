#ifndef LOGMSG_H
#define LOGMSG_H

#include <stdarg.h>

void logmsg(const char *fmt, ...);

int vlogmsg(const char *fmt, va_list args);

#endif // LOGMSG_H
