#include "../../src/log/log_format.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

    void reportFailure(const char* label, int line, const char* detail)
    {
        ++g_failures;
        std::fprintf(stderr, "FAIL %s (line %d): %s\n", label, line, detail);
    }

#define CHECK(cond)                                                            \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            reportFailure(#cond, __LINE__, "condition false");                 \
        }                                                                      \
    } while (0)

    void expectNormalizedCap(const char* label, size_t dstCap, const char* src,
                             size_t srcLen, uint32_t lostBytes, const char* expected)
    {
        char dst[512];
        std::memset(dst, 0x7F, sizeof(dst));

        const size_t n = EDUtils::logNormalizeLine(dst, dstCap, src, srcLen, lostBytes);
        const size_t expectedLen = std::strlen(expected);

        ++g_checks;
        if (n != expectedLen) {
            reportFailure(label, __LINE__, "returned length mismatch");
        }
        ++g_checks;
        if (std::strcmp(dst, expected) != 0) {
            reportFailure(label, __LINE__, "content mismatch");
        }
        ++g_checks;
        if (dst[n] != '\0') {
            reportFailure(label, __LINE__, "result not NUL-terminated");
        }
        ++g_checks;
        if (n == 0 || dst[n - 1] != '\n') {
            reportFailure(label, __LINE__, "result does not end with LF");
        }
        ++g_checks;
        if (n > dstCap - 1) {
            reportFailure(label, __LINE__, "result exceeds dstCap - 1");
        }
    }

    void expectNormalized(const char* label, const char* src, size_t srcLen,
                          uint32_t lostBytes, const char* expected)
    {
        expectNormalizedCap(label, static_cast<size_t>(512), src, srcLen, lostBytes, expected);
    }

    void testNormalize(void)
    {
        expectNormalized("plain gains LF", "hello", 5, 0, "hello\n");
        expectNormalized("trailing CRLF", "hello\r\n", 7, 0, "hello\n");
        expectNormalized("trailing lone CR", "hello\r", 6, 0, "hello\n");
        expectNormalized("trailing lone LF", "hello\n", 6, 0, "hello\n");
        expectNormalized("trailing multiple newlines", "hello\r\n\r\n", 9, 0, "hello\n");
        expectNormalized("ANSI at start", "\x1b[0;32mhello", 12, 0, "hello\n");
        expectNormalized("ANSI in middle", "he\x1b[1;31mllo", 12, 0, "hello\n");
        expectNormalized("ANSI at end", "hello\x1b[0m", 9, 0, "hello\n");
        expectNormalized("stray ESC dropped", "he\x1bllo", 6, 0, "hello\n");
        expectNormalized("empty input", "", 0, 0, "\n");
        expectNormalized("lostBytes that fits", "hello", 5, 7, "hello [...+7]\n");

        {
            const char raw[] = {'h', 'i', '\n'};
            expectNormalized("non NUL-terminated input", raw, sizeof(raw), 0, "hi\n");
        }

        expectNormalizedCap("truncation with marker", 24,
                            "abcdefghijklmnopqrstuvwxyz", 26, 0,
                            "abcdefghijklm [...+13]\n");
        expectNormalizedCap("truncation with lostBytes", 24,
                            "abcdefghijklmnopqrstuvwxyz", 26, 5,
                            "abcdefghijklm [...+18]\n");
        expectNormalizedCap("too small for marker", 9,
                            "abcdefghij", 10, 0, "abcdefg\n");
    }

    void testDetectLevel(void)
    {
        const char arduino[] = "[E][main.cpp:20] x";
        const char ansiIdf[] = "\x1b[0;32mI (12) tag: hi";
        const char idfWarn[] = "W (5) wifi:...";
        const char idfError[] = "E (0) gpio:...";
        const char plain[] = "hello";
        const char verbose[] = "[V]verbose";
        const char debug[] = "[D][a:1]";

        CHECK(EDUtils::logDetectLevel(arduino, std::strlen(arduino)) == EDUtils::NET_LOG_ERROR);
        CHECK(EDUtils::logDetectLevel(ansiIdf, std::strlen(ansiIdf)) == EDUtils::NET_LOG_INFO);
        CHECK(EDUtils::logDetectLevel(idfWarn, std::strlen(idfWarn)) == EDUtils::NET_LOG_WARN);
        CHECK(EDUtils::logDetectLevel(idfError, std::strlen(idfError)) == EDUtils::NET_LOG_ERROR);
        CHECK(EDUtils::logDetectLevel(plain, std::strlen(plain)) == EDUtils::NET_LOG_INFO);
        CHECK(EDUtils::logDetectLevel(verbose, std::strlen(verbose)) == EDUtils::NET_LOG_VERBOSE);
        CHECK(EDUtils::logDetectLevel(debug, std::strlen(debug)) == EDUtils::NET_LOG_DEBUG);

        char beyond[40];
        std::memset(beyond, 'x', 24);
        beyond[24] = '[';
        beyond[25] = 'E';
        beyond[26] = ']';
        beyond[27] = '!';
        CHECK(EDUtils::logDetectLevel(beyond, 28) == EDUtils::NET_LOG_INFO);
    }

    void testLevelAllowed(void)
    {
        CHECK(EDUtils::logLevelAllowed(EDUtils::NET_LOG_ERROR, EDUtils::NET_LOG_VERBOSE));
        CHECK(EDUtils::logLevelAllowed(EDUtils::NET_LOG_ERROR, EDUtils::NET_LOG_ERROR));
        CHECK(!EDUtils::logLevelAllowed(EDUtils::NET_LOG_DEBUG, EDUtils::NET_LOG_INFO));
        CHECK(EDUtils::logLevelAllowed(EDUtils::NET_LOG_VERBOSE, EDUtils::NET_LOG_VERBOSE));
        CHECK(!EDUtils::logLevelAllowed(EDUtils::NET_LOG_VERBOSE, EDUtils::NET_LOG_DEBUG));
    }

    void testDroppedMarker(void)
    {
        char dst[64];
        std::memset(dst, 0x7F, sizeof(dst));

        size_t n = EDUtils::logDroppedMarker(dst, sizeof(dst), 42);
        CHECK(n == std::strlen("[netlog] dropped 42 records\n"));
        CHECK(std::strcmp(dst, "[netlog] dropped 42 records\n") == 0);
        CHECK(dst[n] == '\0');

        n = EDUtils::logDroppedMarker(dst, sizeof(dst), 0);
        CHECK(n == std::strlen("[netlog] dropped 0 records\n"));
        CHECK(std::strcmp(dst, "[netlog] dropped 0 records\n") == 0);
        CHECK(dst[n] == '\0');

        char small[10];
        std::memset(small, 0x7F, sizeof(small));
        n = EDUtils::logDroppedMarker(small, sizeof(small), 42);
        CHECK(n == 0);
        CHECK(static_cast<unsigned char>(small[0]) == 0x7F);
        CHECK(static_cast<unsigned char>(small[9]) == 0x7F);
    }
}

int main(void)
{
    testNormalize();
    testDetectLevel();
    testLevelAllowed();
    testDroppedMarker();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d of %d checks failed\n", g_failures, g_checks);
        return g_failures;
    }

    std::printf("ALL LOG_FORMAT TESTS PASSED (%d checks)\n", g_checks);
    return 0;
}
