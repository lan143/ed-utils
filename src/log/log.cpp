#include "log.h"

EDUtils::NetworkLogger networkLogger;

static int logFunc(const char *fmt, va_list args)
{
    static char loc_buf[64];
    char * temp = loc_buf;
    uint32_t len;
    va_list copy;
    va_copy(copy, args);
    len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (len >= sizeof(loc_buf)) {
        temp = (char*)malloc(len+1);

        if(temp == NULL) {
            return 0;
        }
    }
    
    vsnprintf(temp, len+1, fmt, args);
    ets_printf("%s", temp);
    networkLogger.addLogRecord(temp);

    if (len >= sizeof(loc_buf)) {
        free(temp);
    }

    return len;
}

void EDUtils::NetworkLogger::init(EDUtils::LogConfig config, std::string system, std::string instance)
{
    if (strlen(config.host) == 0 || strlen(config.uri) == 0 || config.port == 0) {
        return;
    }

    _config = config;
    _system = system;
    _instance = instance;

    _init = true;
}

void EDUtils::NetworkLogger::update()
{
    if ((_lastUpdateTime + LOG_FLUSH_TIME) < esp_timer_get_time()) {
        if (_init && _enable) {
            std::string ready;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                const int readIndex = _writeIndex;
                _writeIndex ^= 1;
                ready.swap(_buffers[readIndex]);
            }

            HTTPClient http;
            http.begin(_config.host, _config.port, _config.uri);
            http.addHeader("Content-Type", "text/plain");
            http.addHeader("X-SYSTEM", _system.c_str());
            http.addHeader("X-INSTANCE", _instance.c_str());

            int statusCode = http.POST(ready.c_str());
            if (statusCode != 200) {
                LOGE("log", "failed to send logs. code: %d", statusCode);
            }
            http.end();
        }

        _lastUpdateTime = esp_timer_get_time();
    }
}
