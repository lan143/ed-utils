#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <stdio.h>
#include <mutex>
#include <utility>

#include "log_config.h"

#define LOG_BUFFER_SIZE 16 * 1024
#define LOG_FLUSH_TIME 10000000

#define LOGE(tag, format, ...)  __log_e("[%s] " format, tag, ##__VA_ARGS__)
#define LOGW(tag, format, ...)  __log_w("[%s] " format, tag, ##__VA_ARGS__)
#define LOGI(tag, format, ...)  __log_i("[%s] " format, tag, ##__VA_ARGS__)
#define LOGD(tag, format, ...)  __log_d("[%s] " format, tag, ##__VA_ARGS__)
#define LOGV(tag, format, ...)  __log_v("[%s] " format, tag, ##__VA_ARGS__)

#define __log_e(format, ...) esp_log_write(ESP_LOG_ERROR, "", ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#define __log_w(format, ...) esp_log_write(ESP_LOG_WARN, "", ARDUHAL_LOG_FORMAT(W, format), ##__VA_ARGS__)
#define __log_i(format, ...) esp_log_write(ESP_LOG_INFO, "", ARDUHAL_LOG_FORMAT(I, format), ##__VA_ARGS__)
#define __log_d(format, ...) esp_log_write(ESP_LOG_DEBUG, "", ARDUHAL_LOG_FORMAT(D, format), ##__VA_ARGS__)
#define __log_v(format, ...) esp_log_write(ESP_LOG_VERBOSE, "", ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__)

static int logFunc(const char *fmt, va_list args);

namespace EDUtils
{
    class NetworkLogger
    {
    public:
        NetworkLogger()
        {
            esp_log_set_vprintf(logFunc);

            _buffers[0].reserve(LOG_BUFFER_SIZE);
            _buffers[1].reserve(LOG_BUFFER_SIZE);
        }

        void init(LogConfig config)
        {
            if (strlen(config.host) == 0 || strlen(config.uri) == 0 || config.port == 0) {
                return;
            }

            _config = config;
            _init = true;
        }

        void addLogRecord(const char* message)
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (_buffers[_writeIndex].length() < _buffers[_writeIndex].capacity()) {
                _buffers[_writeIndex].append(message);
            }
        }

        void update()
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
                    int statusCode = http.POST(ready.c_str());
                    if (statusCode != 200) {
                        LOGE("log", "failed to send logs. code: %d", statusCode);
                    }
                    http.end();
                }

                _lastUpdateTime = esp_timer_get_time();
            }
        }

        void enable(bool enable)
        {
            _enable = enable;
        }

    private:
        LogConfig _config;
        uint64_t _lastUpdateTime = 0;
        std::string _buffers[2];
        uint8_t _writeIndex = 0;
        std::mutex _mutex;
        bool _enable = false;
        bool _init = false;
    };
}

extern EDUtils::NetworkLogger networkLogger;
