#pragma once

namespace EDUtils
{
    template <typename T>
    struct Nullable {
        T _value;
        bool _valid;

        Nullable(bool valid, T value) : _valid(valid), _value(value) {}

        bool Valid() const { return _valid; }
        T Value() const { return _value; }
    };
}
