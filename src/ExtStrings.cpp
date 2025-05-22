#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "ExtStrings.h"

EDUtils::str2int_errno EDUtils::str2int(int *out, const char *s, int base)
{
    char *end;
    if (s[0] == '\0' || isspace(s[0]))
        return EDUtils::STR2INT_INCONVERTIBLE;
    errno = 0;
    long l = strtol(s, &end, base);
    /* Both checks are needed because INT_MAX == LONG_MAX is possible. */
    if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX))
        return EDUtils::STR2INT_OVERFLOW;
    if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN))
        return EDUtils::STR2INT_UNDERFLOW;
    if (*end != '\0')
        return EDUtils::STR2INT_INCONVERTIBLE;
    *out = l;
    return EDUtils::STR2INT_SUCCESS;
}

EDUtils::str2int_errno EDUtils::str2float(float_t* out, const char* str)
{
    if (str == nullptr || *str == '\0') {
        return EDUtils::STR2INT_INCONVERTIBLE;
    }

    char* endPtr;
    errno = 0;
    float_t result = strtof(str, &endPtr);

    if (errno == ERANGE) {
        return EDUtils::STR2INT_OVERFLOW;
    }

    if (*endPtr != '\0') {
        return EDUtils::STR2INT_INCONVERTIBLE;
    }

    *out = result;

    return EDUtils::STR2INT_SUCCESS;
}

std::string EDUtils::formatString(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int size = std::vsnprintf(nullptr, 0, format, args);
    va_end(args);

    if (size < 0) {
        throw std::runtime_error("Error during formatting");
    }

    std::vector<char> buffer(size + 1);

    va_start(args, format);
    std::vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);

    return std::string(buffer.data());
}

std::vector<std::string> EDUtils::split(const std::string& input, const std::string& delimiter)
{
    std::string s = input;
    std::vector<std::string> tokens;
    size_t pos = 0;
    std::string token;

    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        tokens.push_back(token);
        s.erase(0, pos + delimiter.length());
    }

    tokens.push_back(s);

    return tokens;
}
