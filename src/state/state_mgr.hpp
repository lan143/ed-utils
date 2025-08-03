#pragma once

#include "state_producer.h"

namespace EDUtils
{
    template<class T>
    class StateMgr
    {
    public:
        StateMgr(StateProducer<T>* producer) : _producer(producer) {}

        T& getState() { return _newState; }
        void loop()
        {
            if (_nextUpdateTime < millis()) {
                if (_oldState != _newState && _producer->publish(&_newState)) {
                    _oldState = _newState;
                }

                _nextUpdateTime = millis() + 500;
            }
        }


    private:
        unsigned int _nextUpdateTime = 0;

        T _oldState;
        T _newState;

    private:
        StateProducer<T>* _producer;
    };
}
