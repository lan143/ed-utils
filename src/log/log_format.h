#pragma once

#include <stddef.h>
#include <stdint.h>

namespace EDUtils
{
    enum NetLogLevel
    {
        NET_LOG_ERROR = 0,
        NET_LOG_WARN = 1,
        NET_LOG_INFO = 2,
        NET_LOG_DEBUG = 3,
        NET_LOG_VERBOSE = 4
    };

    /*
     * Normalize one log line for network transmission.
     *
     * src/srcLen: raw formatted line, may contain ANSI CSI sequences and may
     * lack a trailing LF. src is not required to be NUL-terminated.
     * lostBytes: bytes already known to be cut off before this call (0 if none).
     *
     * Strips ANSI color sequences when LOG_NET_STRIP_ANSI is enabled, removes
     * trailing CR/LF, appends exactly one LF and NUL-terminates the result.
     * Returns the content length excluding the NUL.
     */
    size_t logNormalizeLine(char* dst, size_t dstCap, const char* src,
                            size_t srcLen, uint32_t lostBytes);

    /* Detect the level of a raw line (ANSI sequences allowed). Unknown -> INFO. */
    NetLogLevel logDetectLevel(const char* line, size_t len);

    /* Send when level is at least as severe as minLevel (ERROR is smallest). */
    bool logLevelAllowed(NetLogLevel level, NetLogLevel minLevel);

    /* Write "[netlog] dropped <count> records\n"; 0 if dstCap is too small. */
    size_t logDroppedMarker(char* dst, size_t dstCap, uint32_t count);
}
