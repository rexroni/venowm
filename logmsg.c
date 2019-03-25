#include <stdio.h>

#include "logmsg.h"

void logmsg(const char *fmt, ...){
    {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }

    {
        va_list ap;
        FILE *f = fopen("logmsg", "a");
        if(!f) return;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fclose(f);
    }
}

int vlogmsg(const char *fmt, va_list ap){
    va_list ap2;
    va_copy(ap2, ap);
    int ret = vfprintf(stderr, fmt, ap2);
    va_end(ap2);

    FILE *f = fopen("logmsg2", "a");
    if(!f) return ret;
    vfprintf(f, fmt, ap);
    fclose(f);
    return ret;
}
