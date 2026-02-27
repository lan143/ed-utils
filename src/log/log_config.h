#pragma once

#include <Arduino.h>

#define LOG_HOST_LEN 64

namespace EDUtils
{
    struct LogConfig
    {
        char host[LOG_HOST_LEN] = {0};
        char uri[LOG_HOST_LEN] = {0};
        uint16_t port = 0;
    };
}
