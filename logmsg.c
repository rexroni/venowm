#include <stdio.h>

#include "logmsg.h"

void logmsg(const char *fmt, ...){
    {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }

    //{
    //    va_list ap;
    //    FILE *f = fopen("logmsg", "a");
    //    if(!f) return;
    //    va_start(ap, fmt);
    //    vfprintf(f, fmt, ap);
    //    va_end(ap);
    //    fclose(f);
    //}
}

