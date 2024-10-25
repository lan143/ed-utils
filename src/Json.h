#pragma once

#include <ArduinoJson.h>

namespace EDUtils
{
    using json_parse_t = std::function<bool(JsonObject)>;
    using json_build_t = std::function<void(JsonObject)>;

    extern bool parseJson(const char* data, const json_parse_t &f);
    extern std::string buildJson(const json_build_t &f);
}
