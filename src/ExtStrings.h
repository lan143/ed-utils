#pragma once

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <cstdarg>
#include <stdexcept>

namespace EDUtils
{
    typedef enum {
        STR2INT_SUCCESS,
        STR2INT_OVERFLOW,
        STR2INT_UNDERFLOW,
        STR2INT_INCONVERTIBLE
    } str2int_errno;

    /* Convert string s to int out.
    *
    * @param[out] out The converted int. Cannot be NULL.
    *
    * @param[in] s Input string to be converted.
    *
    *     The format is the same as strtol,
    *     except that the following are inconvertible:
    *
    *     - empty string
    *     - leading whitespace
    *     - any trailing characters that are not part of the number
    *
    *     Cannot be NULL.
    *
    * @param[in] base Base to interpret string in. Same range as strtol (2 to 36).
    *
    * @return Indicates if the operation succeeded, or why it failed.
    */
    str2int_errno str2int(int *out, const char *s, int base);
    
    str2int_errno str2float(float_t* out, const char* str);

    extern std::string formatString(const char* format, ...);
    extern std::vector<std::string> split(const std::string& input, const std::string& delimiter);
}
