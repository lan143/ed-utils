#include "log.h"

#include <string.h>
#include <time.h>
#include <utility>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>

/* HTTPClient and WiFi are implementation details of log.cpp only. */
#include <HTTPClient.h>
#include <WiFi.h>

#if defined(__has_include)
#if __has_include(<ETH.h>)
#include <ETH.h>
#define ED_LOG_HAS_ETH 1
#endif
#endif

/* Single definition of the global logger (see AGENTS.md). */
EDUtils::NetworkLogger networkLogger;

volatile TaskHandle_t EDUtils::NetworkLogger::_senderTask = NULL;

namespace EDUtils
{
    /*
     * Private accessor for the sender task body. Keeping the FreeRTOS entry
     * point a plain void(*)(void*) avoids exposing task internals in log.h.
     */
    struct NetworkLoggerTaskAccess
    {
        static void task(void* param)
        {
            run(static_cast<NetworkLogger*>(param));
        }

        static void run(NetworkLogger* logger);
    };
}

namespace
{
    bool s_senderStarted = false;
}

static EDUtils::NetworkLogger* s_hookTarget = nullptr;
static int (*s_prevVprintf)(const char*, va_list) = nullptr;

namespace
{
    /*
     * Normalization headroom.
     *
     * LOG_NET_LINE_MAX is the maximum ring element size, LF included, NUL
     * excluded. We format at most LOG_NET_LINE_MAX - 48 content bytes, leaving
     * room for the appended LF and a possible "[ ...+N]" truncation marker.
     * logNormalizeLine() is also given a LOG_NET_LINE_MAX-byte destination and
     * never returns more than dstCap - 1 bytes, so the final outLen is always
     * <= LOG_NET_LINE_MAX. Wave 3 may rely on this bound when draining.
     */
    const size_t kFormatCap = LOG_NET_LINE_MAX - 48;
    const size_t kFormatBufSize = kFormatCap + 1;

    bool configValid(const EDUtils::LogConfig& config)
    {
        return config.host[0] != '\0' && config.uri[0] != '\0' && config.port != 0;
    }

#if defined(ED_LOG_HAS_ETH)
    /*
     * FR-11 names ETH.connected(), but some Arduino-ESP32 releases expose only
     * ETH.linkUp(). Pick whichever exists at compile time.
     */
    template <typename T>
    auto ethReady(T& eth, int) -> decltype(eth.connected(), bool())
    {
        return eth.connected();
    }

    template <typename T>
    bool ethReady(T& eth, long)
    {
        return eth.linkUp();
    }
#endif

    bool defaultNetworkReady()
    {
        if (WiFi.isConnected()) {
            return true;
        }
#if defined(ED_LOG_HAS_ETH)
        return ethReady(ETH, 0);
#else
        return false;
#endif
    }
}

namespace EDUtils
{
    /*
     * Friend accessor for the allocation-free vprintf hook. Keeping the hook
     * itself a plain static function avoids exposing implementation details in
     * the public header.
     */
    struct NetworkLoggerHookAccess
    {
        static int handle(const char* fmt, va_list args)
        {
            NetworkLogger* target = s_hookTarget;

            /* Fast path: no logger, disabled, or no ring -> console only. */
            if (target == nullptr || !target->netEnabled() || target->netRing() == nullptr) {
                if (s_prevVprintf != nullptr) {
                    return s_prevVprintf(fmt, args);
                }
                return 0;
            }

            /*
             * Format on the caller's stack. va_copy keeps the original args
             * untouched for the previous hook below. No allocation, no locks.
             */
            char lineBuf[LOG_NET_LINE_MAX + 1];
            va_list copy;
            va_copy(copy, args);
            const int needed = vsnprintf(lineBuf, kFormatBufSize, fmt, copy);
            va_end(copy);

            int result = needed;
            if (s_prevVprintf != nullptr) {
                result = s_prevVprintf(fmt, args);
            }

            if (needed <= 0) {
                return result;
            }

            size_t len = static_cast<size_t>(needed);
            uint32_t lostBytes = 0;
            if (len > kFormatCap) {
                lostBytes = static_cast<uint32_t>(len - kFormatCap);
                len = kFormatCap;
                lineBuf[len] = '\0';
            }

            /* Never touch the ring from an ISR or before the scheduler runs. */
            if (xPortInIsrContext() != 0) {
                return result;
            }
            if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
                return result;
            }

            /* Feedback-loop guard: never enqueue records produced by the sender. */
            TaskHandle_t sender = static_cast<TaskHandle_t>(NetworkLogger::_senderTask);
            if (sender != NULL && xTaskGetCurrentTaskHandle() == sender) {
                return result;
            }

            target->enqueueFormatted(lineBuf, len, lostBytes);
            return result;
        }
    };
}

static int edLogVprintfHook(const char* fmt, va_list args)
{
    return EDUtils::NetworkLoggerHookAccess::handle(fmt, args);
}

EDUtils::NetworkLogger::NetworkLogger() : _pendingLen(0)
{
    /*
     * Deliberately side-effect free: the ring, hook and sender task are all
     * created from init(). _pending itself is only written by the sender.
     */
}

bool EDUtils::NetworkLogger::ensureResourcesLocked()
{
    static bool hookInstalled = false;

    /*
     * Single-global invariant: only the first NetworkLogger instance owns the
     * process-wide hook. A second instance must never steal it.
     */
    if (hookInstalled && s_hookTarget != this) {
        return false;
    }

    if (_ring.load(std::memory_order_acquire) == nullptr) {
        RingbufHandle_t ring = xRingbufferCreate(LOG_NET_RING_SIZE, RINGBUF_TYPE_NOSPLIT);
        if (ring != nullptr) {
            _ring.store(ring, std::memory_order_release);
        }
    }

    if (!hookInstalled) {
        s_prevVprintf = esp_log_set_vprintf(edLogVprintfHook);
        s_hookTarget = this;
        hookInstalled = true;
    }

    /*
     * Start the sender once, after a ring exists. A failed ring creation still
     * leaves the hook installed (console keeps flowing, the hook fast path
     * skips the network), so a later init() can retry both.
     */
    if (_ring.load(std::memory_order_acquire) != nullptr && !s_senderStarted &&
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        TaskHandle_t handle = NULL;
        const BaseType_t created = xTaskCreatePinnedToCore(
            &EDUtils::NetworkLoggerTaskAccess::task,
            "netlog",
            LOG_NET_TASK_STACK,
            this,
            LOG_NET_TASK_PRIO,
            &handle,
            LOG_NET_TASK_CORE);

        if (created == pdPASS && handle != NULL) {
            _senderTask = handle;
            s_senderStarted = true;
        }
        /* On failure leave _senderTask NULL; a later init() retries. */
    }

    return _ring.load(std::memory_order_acquire) != nullptr && s_hookTarget == this;
}

void EDUtils::NetworkLogger::purgeLocked()
{
    RingbufHandle_t ring = _ring.load(std::memory_order_acquire);
    if (ring != nullptr) {
        size_t itemSize = 0;
        void* item = nullptr;
        while ((item = xRingbufferReceive(ring, &itemSize, 0)) != nullptr) {
            vRingbufferReturnItem(ring, item);
        }
    }

    /*
     * The public ringbuf API has no fill query, so fill is approximated from
     * byte counters. Make the counters agree after a purge.
     */
    _consumedBytes.store(_producedBytes.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);

    /*
     * A sender may be sleeping (backoff) or have a batch in flight. Mark any
     * sender-owned pending batch stale so it is dropped on the next wake
     * instead of transmitting pre-disable data (FR-4).
     */
    _pendingInvalid.store(true, std::memory_order_relaxed);
}

void EDUtils::NetworkLogger::enqueueFormatted(const char* data, size_t len,
                                              uint32_t lostBytes)
{
    if (data == nullptr || len == 0) {
        return;
    }

    RingbufHandle_t ring = _ring.load(std::memory_order_acquire);
    if (ring == nullptr) {
        return;
    }

    const NetLogLevel level = logDetectLevel(data, len);
    if (!logLevelAllowed(level, LOG_NET_MIN_LEVEL)) {
        return;
    }

    char sendBuf[LOG_NET_LINE_MAX];
    const size_t outLen = logNormalizeLine(sendBuf, sizeof(sendBuf), data, len, lostBytes);
    if (outLen == 0 || outLen > LOG_NET_LINE_MAX) {
        return;
    }

    if (xRingbufferSend(ring, sendBuf, outLen, 0) != pdTRUE) {
        _recordsDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    _recordsProduced.fetch_add(1, std::memory_order_relaxed);
    _producedBytes.fetch_add(static_cast<uint32_t>(outLen), std::memory_order_relaxed);

    /*
     * Urgent wakeup. _senderTask is NULL until the sender is created, so this
     * branch is dormant before init(). The modulo-2^32 difference is exact
     * while the real fill is below LOG_NET_RING_SIZE.
     */
    TaskHandle_t sender = static_cast<TaskHandle_t>(_senderTask);
    if (sender != NULL) {
        const uint32_t ringSize = static_cast<uint32_t>(LOG_NET_RING_SIZE);
        const uint32_t produced = _producedBytes.load(std::memory_order_relaxed);
        const uint32_t consumed = _consumedBytes.load(std::memory_order_relaxed);
        const uint32_t fill = produced - consumed;
        const uint32_t freeBytes = fill >= ringSize ? 0 : (ringSize - fill);

        if (freeBytes < LOG_NET_FLUSH_THRESHOLD || level == NET_LOG_ERROR) {
            const uint32_t nowMs = static_cast<uint32_t>(esp_timer_get_time() / 1000);
            const uint32_t lastMs = _lastUrgentMs.load(std::memory_order_relaxed);
            if (static_cast<uint32_t>(nowMs - lastMs) >= LOG_NET_URGENT_MIN_GAP_MS) {
                _lastUrgentMs.store(nowMs, std::memory_order_relaxed);
                xTaskNotifyGive(sender);
            }
        }
    }
}

void EDUtils::NetworkLogger::init(EDUtils::LogConfig config, std::string system,
                                  std::string instance)
{
    std::lock_guard<std::mutex> lock(_mutex);

    /* Always store caller data, even when the config is invalid (FR-14). */
    _config = config;
    _system = std::move(system);
    _instance = std::move(instance);

    const bool valid = configValid(_config);
    _configured.store(valid, std::memory_order_relaxed);

    if (valid) {
        ensureResourcesLocked();
    }
}

bool EDUtils::NetworkLogger::setConfig(const EDUtils::LogConfig& config)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _config = config;
    const bool valid = configValid(_config);
    _configured.store(valid, std::memory_order_relaxed);

    if (valid) {
        ensureResourcesLocked();
    }

    return valid;
}

EDUtils::LogConfig EDUtils::NetworkLogger::getConfig()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _config;
}

void EDUtils::NetworkLogger::enable(bool value)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _enable.store(value, std::memory_order_relaxed);
    if (!value) {
        purgeLocked();
    } else if (_configured.load(std::memory_order_relaxed)) {
        /* Non-blocking; safe while holding _mutex. */
        TaskHandle_t sender = static_cast<TaskHandle_t>(_senderTask);
        if (sender != NULL) {
            xTaskNotifyGive(sender);
        }
    }
}

bool EDUtils::NetworkLogger::isEnabled()
{
    return _enable.load(std::memory_order_relaxed);
}

bool EDUtils::NetworkLogger::applySettings(const EDUtils::LogConfig& config)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _config = config;
    const bool valid = configValid(_config);
    _configured.store(valid, std::memory_order_relaxed);

    if (!_enable.load(std::memory_order_relaxed)) {
        purgeLocked();
    }
    if (valid) {
        ensureResourcesLocked();
    }

    if (valid && _enable.load(std::memory_order_relaxed)) {
        /* Pick up a changed target promptly (non-blocking). */
        TaskHandle_t sender = static_cast<TaskHandle_t>(_senderTask);
        if (sender != NULL) {
            xTaskNotifyGive(sender);
        }
    }

    return _configured.load(std::memory_order_relaxed);
}

bool EDUtils::NetworkLogger::applySettings(const EDUtils::LogConfig& config, bool enabled)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _config = config;
    const bool valid = configValid(_config);
    _configured.store(valid, std::memory_order_relaxed);
    _enable.store(enabled, std::memory_order_relaxed);

    if (!enabled) {
        purgeLocked();
    }
    if (valid) {
        ensureResourcesLocked();
    }

    if (valid && enabled) {
        /* Pick up a changed target promptly (non-blocking). */
        TaskHandle_t sender = static_cast<TaskHandle_t>(_senderTask);
        if (sender != NULL) {
            xTaskNotifyGive(sender);
        }
    }

    return _configured.load(std::memory_order_relaxed);
}

bool EDUtils::NetworkLogger::isConfigured()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _configured.load(std::memory_order_relaxed);
}

void EDUtils::NetworkLogger::addLogRecord(const char* message)
{
    /*
     * Ring send is not ISR-safe; same context rules as the hook (minus the
     * sender-task check, which only exists to prevent implicit feedback
     * loops — explicit addLogRecord calls are intentional).
     */
    if (xPortInIsrContext() != 0) {
        return;
    }
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return;
    }

    if (message == nullptr || !_enable.load(std::memory_order_relaxed)) {
        return;
    }
    if (_ring.load(std::memory_order_acquire) == nullptr) {
        return;
    }

    const size_t len = strlen(message);
    if (len == 0) {
        return;
    }

    enqueueFormatted(message, len, 0);
}

void EDUtils::NetworkLogger::update()
{
    /* Deprecated no-op kept for source compatibility (all work is in the task). */
}

void EDUtils::NetworkLogger::setNetworkReadyCheck(std::function<bool()> check)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _readyCheck = std::move(check);
}

EDUtils::LogStats EDUtils::NetworkLogger::getStats()
{
    LogStats stats;

    stats.recordsProduced = _recordsProduced.load(std::memory_order_relaxed);
    stats.recordsDropped = _recordsDropped.load(std::memory_order_relaxed);
    stats.batchesSent = _batchesSent.load(std::memory_order_relaxed);
    stats.batchesFailed = _batchesFailed.load(std::memory_order_relaxed);
    stats.bytesSent = _bytesSent.load(std::memory_order_relaxed);
    stats.lastHttpStatus = static_cast<int16_t>(_lastHttpStatus.load(std::memory_order_relaxed));

    if (_ring.load(std::memory_order_acquire) == nullptr) {
        stats.ringFreeBytes = 0;
    } else {
        /*
         * Intentional approximation: the public ringbuf API has no fill query
         * (xRingbufferGetInfo is not public ESP-IDF API), so use the byte
         * counters maintained by the producer and consumer.
         */
        const uint32_t ringSize = static_cast<uint32_t>(LOG_NET_RING_SIZE);
        const uint32_t produced = _producedBytes.load(std::memory_order_relaxed);
        const uint32_t consumed = _consumedBytes.load(std::memory_order_relaxed);
        const uint32_t fill = produced - consumed;
        if (fill >= ringSize) {
            stats.ringFreeBytes = 0;
        } else {
            stats.ringFreeBytes = ringSize - fill;
        }
    }

    return stats;
}

void EDUtils::NetworkLoggerTaskAccess::run(EDUtils::NetworkLogger* logger)
{
    if (logger == nullptr) {
        vTaskDelete(NULL);
        return;
    }

    /* Task-private HTTP client and state: nothing here is shared or locked. */
    HTTPClient http;

    char bootIdHex[9];
    snprintf(bootIdHex, sizeof(bootIdHex), "%08x",
             static_cast<unsigned>(esp_random()));

    char lastHost[LOG_HOST_LEN] = {0};
    char lastUri[LOG_HOST_LEN] = {0};
    uint16_t lastPort = 0;
    bool begun = false;
    bool epochHeaderAdded = false;

    uint32_t seq = 0;               /* seq of the pending batch; ++ on success */
    uint32_t droppedReported = 0;   /* watermark for the pending batch */
    uint32_t backoffMs = LOG_NET_BACKOFF_MIN_MS;
    int64_t lastReportUs = -10000000LL; /* allow the first failure to report */
    bool skipWait = false;

    for (;;) {
        if (!skipWait) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(LOG_NET_FLUSH_INTERVAL_MS));
        }
        skipWait = false;

        if (!logger->netEnabled()) {
            /*
             * Defensive drain. enable(false)/applySettings already purge under
             * the mutex, but a producer may have raced with that purge. Pending
             * is cleared as well (FR-4).
             */
            RingbufHandle_t ring = logger->netRing();
            if (ring != nullptr) {
                size_t sz = 0;
                void* item = nullptr;
                while ((item = xRingbufferReceive(ring, &sz, 0)) != nullptr) {
                    vRingbufferReturnItem(ring, item);
                    logger->_consumedBytes.fetch_add(
                        static_cast<uint32_t>(sz), std::memory_order_relaxed);
                }
            }
            logger->_pendingLen = 0;
            continue;
        }

        /* Snapshot under the mutex, then release before any I/O or LOGx. */
        LogConfig cfg;
        std::string system;
        std::string instance;
        std::function<bool()> readyCheck;
        {
            std::lock_guard<std::mutex> lock(logger->_mutex);
            cfg = logger->_config;
            system = logger->_system;
            instance = logger->_instance;
            readyCheck = logger->_readyCheck;
        }

        /* Drop a stale batch left over from before a disable/reconfigure. */
        if (logger->_pendingInvalid.exchange(false, std::memory_order_relaxed)) {
            logger->_pendingLen = 0;
        }

        if (!configValid(cfg)) {
            /* Records stay in the ring until a valid target is configured. */
            continue;
        }

        const bool ready = readyCheck ? readyCheck() : defaultNetworkReady();
        if (!ready) {
            /* FR-11: no send while the network is down; records stay queued. */
            continue;
        }

        bool ringMayHaveMore = false;

        if (logger->_pendingLen == 0) {
            const uint32_t droppedAtBuild =
                logger->_recordsDropped.load(std::memory_order_relaxed);
            const uint32_t newDrops = droppedAtBuild - droppedReported;

            if (newDrops > 0) {
                const size_t markerLen = logDroppedMarker(
                    logger->_pending, sizeof(logger->_pending), newDrops);
                if (markerLen > 0) {
                    logger->_pendingLen = markerLen;
                    /*
                     * Advance the watermark when the marker enters pending: a
                     * retry resends this same marker, while drops that happen
                     * later are counted for a subsequent marker.
                     */
                    droppedReported = droppedAtBuild;
                }
            }

            RingbufHandle_t ring = logger->netRing();
            if (ring != nullptr) {
                const size_t reserve = LOG_NET_LINE_MAX + 64;
                /*
                 * Check for space BEFORE receiving: a NOSPLIT ring item cannot
                 * be put back. If space blocks us while the ring may still hold
                 * data, remember it so the next batch is built immediately
                 * after a successful POST instead of waiting for the timeout.
                 */
                while (logger->_pendingLen + reserve <=
                       static_cast<size_t>(LOG_NET_CHUNK_SIZE)) {
                    size_t sz = 0;
                    void* item = xRingbufferReceive(ring, &sz, 0);
                    if (item == nullptr) {
                        break;
                    }
                    memcpy(logger->_pending + logger->_pendingLen, item, sz);
                    logger->_pendingLen += sz;
                    vRingbufferReturnItem(ring, item);
                    logger->_consumedBytes.fetch_add(
                        static_cast<uint32_t>(sz), std::memory_order_relaxed);
                }
                if (logger->_pendingLen + reserve >
                    static_cast<size_t>(LOG_NET_CHUNK_SIZE)) {
                    ringMayHaveMore = true;
                }
            }

            if (logger->_pendingLen == 0) {
                /* Empty batch: no records and no drops. */
                continue;
            }
        }

        /*
         * Open a fresh connection only when the target changed or a previous
         * end()/failure left the client unusable. Otherwise keep-alive is
         * preserved and only the headers below are refreshed.
         */
        const bool targetChanged =
            !begun ||
            strcmp(cfg.host, lastHost) != 0 ||
            cfg.port != lastPort ||
            strcmp(cfg.uri, lastUri) != 0;

        if (targetChanged) {
            http.setReuse(false);
            http.end();
            if (!http.begin(cfg.host, cfg.port, cfg.uri)) {
                begun = false;
                continue;
            }
            http.setReuse(true);
            http.setConnectTimeout(LOG_NET_CONNECT_TIMEOUT_MS);
            http.setTimeout(LOG_NET_TIMEOUT_MS);

            strncpy(lastHost, cfg.host, sizeof(lastHost));
            lastHost[sizeof(lastHost) - 1] = '\0';
            strncpy(lastUri, cfg.uri, sizeof(lastUri));
            lastUri[sizeof(lastUri) - 1] = '\0';
            lastPort = cfg.port;
            begun = true;
        }

        const time_t now = time(nullptr);
        const bool epochReady = now > static_cast<time_t>(1700000000);

        /*
         * X-EPOCH-MS is conditional. HTTPClient has no public clearHeaders(),
         * so if time ever becomes unsynchronised after the header was sent,
         * reset the client (its header buffer is cleared by begin()) before
         * reusing it.
         */
        if (epochHeaderAdded && !epochReady) {
            http.setReuse(false);
            http.end();
            begun = false;
            epochHeaderAdded = false;
            skipWait = true;
            continue;
        }

        char numBuf[32];
        /* addHeader() replaces by default, so refilling every POST cannot
         * accumulate duplicate X-* headers on a keep-alive connection. */
        http.addHeader("Content-Type", "text/plain");
        http.addHeader("X-SYSTEM", system.c_str());
        http.addHeader("X-INSTANCE", instance.c_str());
        http.addHeader("X-BOOT-ID", bootIdHex);

        snprintf(numBuf, sizeof(numBuf), "%u", static_cast<unsigned>(seq));
        http.addHeader("X-SEQ", numBuf);

        snprintf(numBuf, sizeof(numBuf), "%llu",
                 static_cast<unsigned long long>(esp_timer_get_time() / 1000));
        http.addHeader("X-UPTIME-MS", numBuf);

        if (epochReady) {
            snprintf(numBuf, sizeof(numBuf), "%llu",
                     static_cast<unsigned long long>(now) * 1000ULL);
            http.addHeader("X-EPOCH-MS", numBuf);
            epochHeaderAdded = true;
        }

        snprintf(numBuf, sizeof(numBuf), "%u",
                 static_cast<unsigned>(
                     logger->_recordsDropped.load(std::memory_order_relaxed)));
        http.addHeader("X-DROPPED-TOTAL", numBuf);

        /*
         * A purge may have happened during the backoff sleep above or while a
         * previous POST was in flight. Consume the marker again so this stale
         * batch is not transmitted.
         */
        if (logger->_pendingInvalid.exchange(false, std::memory_order_relaxed)) {
            logger->_pendingLen = 0;
        }

        const size_t sentLen = logger->_pendingLen;
        if (sentLen == 0) {
            continue;
        }
        /* FR-9: explicit pointer + length. The API takes a non-const pointer. */
        const int code = http.POST(
            reinterpret_cast<uint8_t*>(logger->_pending), sentLen);

        if (code >= 200 && code <= 299) {
            logger->_pendingLen = 0;
            logger->_batchesSent.fetch_add(1, std::memory_order_relaxed);
            logger->_bytesSent.fetch_add(static_cast<uint64_t>(sentLen),
                                         std::memory_order_relaxed);
            logger->_lastHttpStatus.store(code, std::memory_order_relaxed);
            backoffMs = LOG_NET_BACKOFF_MIN_MS;
            ++seq;

            /*
             * If the chunk filled up (ringMayHaveMore), or producers added
             * records while this POST was in flight (also the retry case,
             * where no drain happened this iteration), build the next batch
             * immediately. vTaskDelay(1) keeps lower-priority tasks alive.
             */
            const uint32_t fill =
                logger->_producedBytes.load(std::memory_order_relaxed) -
                logger->_consumedBytes.load(std::memory_order_relaxed);
            if (ringMayHaveMore || fill != 0) {
                vTaskDelay(1);
                skipWait = true;
            }
            continue;
        }

        logger->_batchesFailed.fetch_add(1, std::memory_order_relaxed);
        logger->_lastHttpStatus.store(code, std::memory_order_relaxed);

        const int64_t nowUs = esp_timer_get_time();
        if (nowUs - lastReportUs >= 10000000LL) {
            lastReportUs = nowUs;
            LOGE("netlog", "POST %s:%u%s failed (%d); retry in %u ms",
                 cfg.host, static_cast<unsigned>(cfg.port), cfg.uri, code,
                 static_cast<unsigned>(backoffMs));
        }

        if (code < 0) {
            /* Connection-level failure: force a fresh connect next time. */
            http.setReuse(false);
            http.end();
            begun = false;
        }

        vTaskDelay(pdMS_TO_TICKS(backoffMs));
        if (backoffMs < static_cast<uint32_t>(LOG_NET_BACKOFF_MAX_MS)) {
            const uint32_t doubled = backoffMs * 2U;
            backoffMs =
                doubled > static_cast<uint32_t>(LOG_NET_BACKOFF_MAX_MS)
                    ? static_cast<uint32_t>(LOG_NET_BACKOFF_MAX_MS)
                    : doubled;
        }

        /* We just slept for the backoff; retry without another flush wait. */
        skipWait = true;
    }
}
