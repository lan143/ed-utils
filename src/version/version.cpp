#include <cstdio>

#include "version.h"

std::pair<Version, bool> parseVersion(std::string versionStr)
{
    Version version{};

    return std::make_pair(version, sscanf(versionStr.c_str(), "%d.%d.%d", &version.major, &version.minor, &version.patch) == 3);
}
