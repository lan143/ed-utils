#pragma once

#include <Arduino.h>

struct Version
{
    int major;
    int minor;
    int patch;

    bool operator==(Version& other) const
    {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool operator!=(Version& other) const
    {
        return !(*this == other);
    }

    bool operator<(const Version& other) const
    {
        if (major != other.major) {
            return major < other.major;
        }

        if (minor != other.minor) {
            return minor < other.minor;
        }

        return patch < other.patch;
    }

    bool operator>(const Version& other) const
    {
        return other < *this;
    }

    bool operator<=(const Version& other) const
    {
        return !(other < *this);
    }

    bool operator>=(const Version& other) const
    {
        return !(*this < other);
    }
};

extern std::pair<Version, bool> parseVersion(std::string versionStr);
