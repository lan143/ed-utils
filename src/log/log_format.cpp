#include "log_format.h"

#include "log_options.h"

namespace EDUtils
{
namespace
{
    const char ANSI_ESC = '\x1B';

    /*
     * Number of bytes to skip for an ANSI sequence starting at src[i].
     * Only "ESC [ <params> m" is recognized; anything else drops just ESC.
     */
    size_t ansiSkipLen(const char* src, size_t srcLen, size_t i)
    {
        if (i + 1 >= srcLen || src[i + 1] != '[') {
            return 1;
        }

        const size_t limit = i + 2 + 16;
        size_t j = i + 2;

        while (j < srcLen && j < limit) {
            const unsigned char c = static_cast<unsigned char>(src[j]);
            if (c == 'm') {
                return (j - i) + 1;
            }
            if ((c >= '0' && c <= '9') || c == ';' || c == '?') {
                ++j;
                continue;
            }
            break;
        }

        return 1;
    }

    bool isLevelChar(char c)
    {
        return c == 'E' || c == 'W' || c == 'I' || c == 'D' || c == 'V';
    }

    NetLogLevel levelForChar(char c)
    {
        switch (c) {
            case 'E': return NET_LOG_ERROR;
            case 'W': return NET_LOG_WARN;
            case 'I': return NET_LOG_INFO;
            case 'D': return NET_LOG_DEBUG;
            case 'V': return NET_LOG_VERBOSE;
            default:  return NET_LOG_INFO;
        }
    }

    /* Length of the stripped content, excluding trailing CR/LF characters. */
    size_t contentLength(const char* src, size_t srcLen)
    {
        size_t stripped = 0;
        size_t trailingNewlines = 0;
        size_t i = 0;

        while (i < srcLen) {
            if (static_cast<unsigned char>(src[i]) == static_cast<unsigned char>(ANSI_ESC)) {
#if LOG_NET_STRIP_ANSI
                i += ansiSkipLen(src, srcLen, i);
                continue;
#else
                ++i;
                ++stripped;
                trailingNewlines = 0;
                continue;
#endif
            }

            if (src[i] == '\r' || src[i] == '\n') {
                ++trailingNewlines;
            } else {
                trailingNewlines = 0;
            }
            ++stripped;
            ++i;
        }

        return stripped - trailingNewlines;
    }

    /* Copy at most outLimit stripped content bytes from src to dst. */
    void copyContent(char* dst, const char* src, size_t srcLen, size_t outLimit)
    {
        size_t out = 0;
        size_t i = 0;

        while (i < srcLen && out < outLimit) {
            if (static_cast<unsigned char>(src[i]) == static_cast<unsigned char>(ANSI_ESC)) {
#if LOG_NET_STRIP_ANSI
                i += ansiSkipLen(src, srcLen, i);
                continue;
#else
                /* Without stripping, ESC is ordinary content. */
                dst[out++] = src[i++];
                continue;
#endif
            }
            dst[out++] = src[i++];
        }
    }

    size_t decimalDigits(uint32_t value)
    {
        size_t digits = 1;
        while (value >= 10) {
            value /= 10;
            ++digits;
        }
        return digits;
    }

    /* "[ ...+N]" has 7 fixed characters plus the decimal digits of N. */
    size_t markerLength(uint32_t count)
    {
        return 7 + decimalDigits(count);
    }

    size_t writeMarker(char* dst, uint32_t count)
    {
        char digits[10];
        size_t digitCount = 0;

        do {
            digits[digitCount++] = static_cast<char>('0' + (count % 10));
            count /= 10;
        } while (count != 0);

        size_t pos = 0;
        dst[pos++] = ' ';
        dst[pos++] = '[';
        dst[pos++] = '.';
        dst[pos++] = '.';
        dst[pos++] = '.';
        dst[pos++] = '+';
        while (digitCount > 0) {
            dst[pos++] = digits[--digitCount];
        }
        dst[pos++] = ']';

        return pos;
    }
}
}

size_t EDUtils::logNormalizeLine(char* dst, size_t dstCap, const char* src,
                                 size_t srcLen, uint32_t lostBytes)
{
    if (dst == 0 || dstCap < 2) {
        if (dst != 0 && dstCap == 1) {
            dst[0] = '\0';
        }
        return 0;
    }

    if (src == 0) {
        srcLen = 0;
    }

    const size_t contentLen = contentLength(src, srcLen);
    const size_t maxBody = dstCap - 2; /* keep room for LF and NUL */

    const bool contentFits = contentLen <= maxBody;
    const bool needMarker = (lostBytes > 0) || !contentFits;

    if (!needMarker) {
        copyContent(dst, src, srcLen, contentLen);
        dst[contentLen] = '\n';
        dst[contentLen + 1] = '\0';
        return contentLen + 1;
    }

    /* If even the shortest marker cannot fit, fall back to plain truncation. */
    if (markerLength(lostBytes) > maxBody) {
        size_t kept = contentLen < maxBody ? contentLen : maxBody;
        copyContent(dst, src, srcLen, kept);
        dst[kept] = '\n';
        dst[kept + 1] = '\0';
        return kept + 1;
    }

    size_t kept = contentLen;
    size_t markerLen = markerLength(lostBytes);

    for (size_t iteration = 0; iteration < 16 && kept + markerLen > maxBody; ++iteration) {
        const size_t overflow = (kept + markerLen) - maxBody;
        kept = (overflow >= kept) ? 0 : (kept - overflow);
        const uint32_t dropped = lostBytes + static_cast<uint32_t>(contentLen - kept);
        markerLen = markerLength(dropped);
    }

    if (kept + markerLen > maxBody) {
        /* Degenerate case: keep the bound invariant by trimming plain content. */
        size_t fallback = contentLen < maxBody ? contentLen : maxBody;
        copyContent(dst, src, srcLen, fallback);
        dst[fallback] = '\n';
        dst[fallback + 1] = '\0';
        return fallback + 1;
    }

    copyContent(dst, src, srcLen, kept);
    const uint32_t dropped = lostBytes + static_cast<uint32_t>(contentLen - kept);
    dst[kept + writeMarker(dst + kept, dropped)] = '\n';
    const size_t length = kept + markerLength(dropped);
    dst[length + 1] = '\0';

    return length + 1;
}

EDUtils::NetLogLevel EDUtils::logDetectLevel(const char* line, size_t len)
{
    if (line == 0 || len == 0) {
        return NET_LOG_INFO;
    }

    size_t pos = 0;
    while (pos < len &&
           static_cast<unsigned char>(line[pos]) == static_cast<unsigned char>(ANSI_ESC)) {
        pos += ansiSkipLen(line, len, pos);
    }

    if (pos < len && isLevelChar(line[pos])) {
        if (pos + 1 < len && (line[pos + 1] == '(' || line[pos + 1] == ' ')) {
            return levelForChar(line[pos]);
        }
    }

    size_t contentBytes = 0;
    size_t p = pos;
    while (p < len && contentBytes < 24) {
        if (static_cast<unsigned char>(line[p]) == static_cast<unsigned char>(ANSI_ESC)) {
            p += ansiSkipLen(line, len, p);
            continue;
        }

        if (line[p] == '[' && p + 2 < len && line[p + 2] == ']' && isLevelChar(line[p + 1])) {
            return levelForChar(line[p + 1]);
        }

        ++contentBytes;
        ++p;
    }

    return NET_LOG_INFO;
}

bool EDUtils::logLevelAllowed(NetLogLevel level, NetLogLevel minLevel)
{
    return static_cast<int>(level) <= static_cast<int>(minLevel);
}

size_t EDUtils::logDroppedMarker(char* dst, size_t dstCap, uint32_t count)
{
    char digits[10];
    size_t digitCount = 0;
    uint32_t value = count;

    do {
        digits[digitCount++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value != 0);

    /* "[netlog] dropped " + digits + " records\n" + NUL */
    const size_t contentLen = 26 + digitCount;
    if (dst == 0 || dstCap < contentLen + 1) {
        return 0;
    }

    size_t pos = 0;
    dst[pos++] = '[';
    dst[pos++] = 'n';
    dst[pos++] = 'e';
    dst[pos++] = 't';
    dst[pos++] = 'l';
    dst[pos++] = 'o';
    dst[pos++] = 'g';
    dst[pos++] = ']';
    dst[pos++] = ' ';
    dst[pos++] = 'd';
    dst[pos++] = 'r';
    dst[pos++] = 'o';
    dst[pos++] = 'p';
    dst[pos++] = 'p';
    dst[pos++] = 'e';
    dst[pos++] = 'd';
    dst[pos++] = ' ';
    while (digitCount > 0) {
        dst[pos++] = digits[--digitCount];
    }
    dst[pos++] = ' ';
    dst[pos++] = 'r';
    dst[pos++] = 'e';
    dst[pos++] = 'c';
    dst[pos++] = 'o';
    dst[pos++] = 'r';
    dst[pos++] = 'd';
    dst[pos++] = 's';
    dst[pos++] = '\n';
    dst[pos] = '\0';

    return pos;
}
