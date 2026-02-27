#include "log.h"

EDUtils::NetworkLogger networkLogger;

static int logFunc(const char *fmt, va_list args)
{
    static char loc_buf[64];
    char * temp = loc_buf;
    uint32_t len;
    va_list copy;
    va_copy(copy, args);
    len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (len >= sizeof(loc_buf)) {
        temp = (char*)malloc(len+1);

        if(temp == NULL) {
            return 0;
        }
    }
    
    vsnprintf(temp, len+1, fmt, args);
    ets_printf("%s", temp);
    networkLogger.addLogRecord(temp);

    if (len >= sizeof(loc_buf)) {
        free(temp);
    }

    return len;
}
