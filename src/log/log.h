#pragma once

#include <Arduino.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <functional>
#include <mutex>
#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>

#include "log_config.h"
#include "log_options.h"
#include "log_format.h"

/*
 * Legacy build-time constants, kept for source compatibility. The new core
 * uses LOG_NET_RING_SIZE / LOG_NET_LINE_MAX from log_options.h instead.
 */
#define LOG_BUFFER_SIZE (16 * 1024)
#define LOG_FLUSH_TIME 10000000

/*
 * The LOGx macros are filtered at compile time through CORE_DEBUG_LEVEL.
 * Keep the same default as Arduino-ESP32 when the consumer doesn't set it.
 */
#ifndef CORE_DEBUG_LEVEL
#define CORE_DEBUG_LEVEL ARDUHAL_LOG_LEVEL_INFO
#endif

#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
#define EDLOG_E(format, ...) esp_log_write(ESP_LOG_ERROR, "", ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#else
#define EDLOG_E(...) ((void)0)
#endif

#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
#define EDLOG_W(format, ...) esp_log_write(ESP_LOG_WARN, "", ARDUHAL_LOG_FORMAT(W, format), ##__VA_ARGS__)
#else
#define EDLOG_W(...) ((void)0)
#endif

#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
#define EDLOG_I(format, ...) esp_log_write(ESP_LOG_INFO, "", ARDUHAL_LOG_FORMAT(I, format), ##__VA_ARGS__)
#else
#define EDLOG_I(...) ((void)0)
#endif

#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
#define EDLOG_D(format, ...) esp_log_write(ESP_LOG_DEBUG, "", ARDUHAL_LOG_FORMAT(D, format), ##__VA_ARGS__)
#else
#define EDLOG_D(...) ((void)0)
#endif

#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
#define EDLOG_V(format, ...) esp_log_write(ESP_LOG_VERBOSE, "", ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__)
#else
#define EDLOG_V(...) ((void)0)
#endif

#define LOGE(tag, format, ...) do { EDLOG_E("[%s] " format, tag, ##__VA_ARGS__); } while (0)
#define LOGW(tag, format, ...) do { EDLOG_W("[%s] " format, tag, ##__VA_ARGS__); } while (0)
#define LOGI(tag, format, ...) do { EDLOG_I("[%s] " format, tag, ##__VA_ARGS__); } while (0)
#define LOGD(tag, format, ...) do { EDLOG_D("[%s] " format, tag, ##__VA_ARGS__); } while (0)
#define LOGV(tag, format, ...) do { EDLOG_V("[%s] " format, tag, ##__VA_ARGS__); } while (0)

namespace EDUtils
{
    struct LogStats
    {
        uint32_t recordsProduced = 0;
        uint32_t recordsDropped = 0;
        uint32_t batchesSent = 0;
        uint32_t batchesFailed = 0;
        uint64_t bytesSent = 0;
        int16_t lastHttpStatus = 0;
        uint32_t ringFreeBytes = 0;
    };

    class NetworkLogger
    {
    public:
        /* Global construction must stay side-effect free (FR-13). */
        NetworkLogger();

        void init(LogConfig config, std::string system, std::string instance);

        bool setConfig(const LogConfig& config);
        LogConfig getConfig();
        void enable(bool enable);
        bool isEnabled();

        bool applySettings(const LogConfig& config);
        bool applySettings(const LogConfig& config, bool enabled);
        bool isConfigured();

        void addLogRecord(const char* message);
        void update();

        void setNetworkReadyCheck(std::function<bool()> check);
        LogStats getStats();

    private:
        friend struct NetworkLoggerHookAccess;
        friend struct NetworkLoggerTaskAccess;

        /*
         * Friend-facing implementation helpers (not public API): lock-free and
         * intentionally cheap, the hook must never take the configuration
         * mutex.
         */
        bool netEnabled() const { return _enable.load(std::memory_order_relaxed); }
        RingbufHandle_t netRing() const { return _ring.load(std::memory_order_acquire); }

        bool ensureResourcesLocked();
        void purgeLocked();
        void enqueueFormatted(const char* data, size_t len, uint32_t lostBytes);

        LogConfig _config;
        std::string _system;
        std::string _instance;
        std::function<bool()> _readyCheck;
        std::mutex _mutex;
        std::atomic<bool> _enable{false};
        std::atomic<bool> _configured{false};

        std::atomic<RingbufHandle_t> _ring{nullptr};
        std::atomic<uint32_t> _recordsProduced{0};
        std::atomic<uint32_t> _recordsDropped{0};
        /*
         * Byte counters are deliberately 32-bit: produced - consumed is exact
         * modulo 2^32 while the real fill is always below LOG_NET_RING_SIZE,
         * and this keeps the hot hook path cheap. _bytesSent stays 64-bit.
         */
        std::atomic<uint32_t> _producedBytes{0};
        std::atomic<uint32_t> _consumedBytes{0};
        std::atomic<uint32_t> _batchesSent{0};
        std::atomic<uint32_t> _batchesFailed{0};
        std::atomic<uint64_t> _bytesSent{0};
        std::atomic<int> _lastHttpStatus{0};
        std::atomic<uint32_t> _lastUrgentMs{0};

        /*
         * Sender-owned chunk buffer. It is a fixed array inside the global
         * NetworkLogger object, so it lands in .bss (~4 KB with defaults).
         * Together with the one-time ring allocation (~24 KB heap) and the
         * sender task stack (~6 KB heap) this is the logger's fixed footprint;
         * nothing grows afterwards. _pending is only touched by the sender
         * task; _pendingLen is initialized in the constructor.
         */
        char _pending[LOG_NET_CHUNK_SIZE];
        size_t _pendingLen;
        /* Set under _mutex by purgeLocked(); consumed by the sender task. */
        std::atomic<bool> _pendingInvalid{false};

        /* NULL until the sender task is created successfully. */
        static volatile TaskHandle_t _senderTask;
    };
}

extern EDUtils::NetworkLogger networkLogger;
