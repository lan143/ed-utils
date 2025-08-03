#pragma once

namespace EDUtils
{
    template<class T>
    class StateProducer
    {
    public:
        virtual bool publish(T* state) = 0;
    };
}
