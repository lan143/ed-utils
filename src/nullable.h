#pragma once

namespace EDUtils
{
    template <typename T>
    struct Nullable {
        T _value;
        bool _valid;

        Nullable() : _valid(false) {}
        Nullable(bool valid, T value) : _valid(valid), _value(value) {}

        bool Valid() const { return _valid; }
        T Value() const { return _value; }

        void setValidValue(T value)
        {
            _valid = true;
            _value = value;
        }

        bool operator==(Nullable& other)
        {
            return _value == other._value && _valid == other._valid;
        }

        bool operator!=(Nullable& other)
        {
            return !((*this) == other);
        }
    };
}
