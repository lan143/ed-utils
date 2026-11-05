#pragma once

//
// HTTP endpoints for the network logger.
//
// GET  /api/log/settings -> current logging target and enabled flag
// POST /api/log/settings -> validate and atomically apply target/enabled
// GET  /api/log/stats    -> runtime counters (records, batches, bytes, ring)
//
// POST applies target and enabled flag in one NetworkLogger::applySettings()
// call, so the logger never observes a partially updated configuration. The
// sender notices the changed target and closes then re-opens its keep-alive
// connection on the next send.
//
// POST requires Content-Type: application/json; empty or malformed JSON is
// rejected by ESPAsyncWebServer's AsyncCallbackJsonWebHandler with bare 400, oversized
// body (> 1024 bytes) with bare 413, before field validation runs.
//
// Persistence is optional: a consumer can register an onApplied() callback to
// store the settings. It receives the logger's final state (not the request
// echo). The applied values stay active even if storing fails.
// system/instance are set once by NetworkLogger::init() and are not editable
// through this handler.
//

#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <functional>
#include <string.h>
#include <string>
#include <utility>

#include "log.h"

namespace EDUtils
{
    class LogSettingsHandler
    {
    public:
        explicit LogSettingsHandler(NetworkLogger& logger,
                                    const char* path = "/api/log/settings",
                                    const char* statsPath = "/api/log/stats")
            : _logger(logger),
              _path(path != nullptr ? path : "/api/log/settings"),
              _statsPath(statsPath != nullptr ? statsPath : "/api/log/stats")
        {
        }

        using on_applied_t = std::function<bool(const LogConfig&, bool)>;

        // Called after settings are applied to the logger, in the AsyncWebServer
        // task context, with the new effective state (config, enabled). Return
        // false to signal persistence failure (responds 500; logger keeps the
        // applied values). Unset = no persistence, success responds 200.
        void onApplied(on_applied_t fn)
        {
            _onApplied = std::move(fn);
        }

        void registerHandlers(AsyncWebServer* server)
        {
            server->on(_path.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
                handleGet(request);
            });

            auto* postHandler = new AsyncCallbackJsonWebHandler(
                _path.c_str(),
                [this](AsyncWebServerRequest* request, JsonVariant& json) {
                    handlePost(request, json);
                },
                LOG_SETTINGS_JSON_SIZE);
            postHandler->setMethod(HTTP_POST);
            postHandler->setMaxContentLength(LOG_SETTINGS_MAX_BODY);
            server->addHandler(postHandler);

            server->on(_statsPath.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
                handleGetStats(request);
            });
        }

    private:
        void handleGet(AsyncWebServerRequest* request)
        {
            sendState(request, 200);
        }

        void handleGetStats(AsyncWebServerRequest* request)
        {
            const LogStats stats = _logger.getStats();

            StaticJsonDocument<256> doc;
            doc["recordsProduced"] = stats.recordsProduced;
            doc["recordsDropped"] = stats.recordsDropped;
            doc["batchesSent"] = stats.batchesSent;
            doc["batchesFailed"] = stats.batchesFailed;
            doc["bytesSent"] = stats.bytesSent;
            doc["lastHttpStatus"] = stats.lastHttpStatus;
            doc["ringFreeBytes"] = stats.ringFreeBytes;

            std::string body;
            body.reserve(256);
            serializeJson(doc, body);

            request->send(200, "application/json", body.c_str());
        }

        void handlePost(AsyncWebServerRequest* request, JsonVariant& json)
        {
            if (!json.is<JsonObject>()) {
                sendError(request, 422, "invalid json");
                return;
            }

            const JsonVariant hostValue = json["host"];
            if (hostValue.isNull() || !hostValue.is<const char*>() || strlen(hostValue.as<const char*>()) == 0) {
                sendError(request, 422, "host is required");
                return;
            }

            if (strlen(hostValue.as<const char*>()) >= LOG_HOST_LEN) {
                sendError(request, 422, "host too long");
                return;
            }

            /* Content checks run after presence/length and before port/enabled. */
            const char* host = hostValue.as<const char*>();
            if (strchr(host, ' ') != nullptr || strstr(host, "://") != nullptr) {
                sendError(request, 422, "invalid host");
                return;
            }

            const JsonVariant uriValue = json["uri"];
            if (uriValue.isNull() || !uriValue.is<const char*>() || strlen(uriValue.as<const char*>()) == 0) {
                sendError(request, 422, "uri is required");
                return;
            }

            if (strlen(uriValue.as<const char*>()) >= LOG_HOST_LEN) {
                sendError(request, 422, "uri too long");
                return;
            }

            const char* uri = uriValue.as<const char*>();
            if (uri[0] != '/') {
                sendError(request, 422, "invalid uri");
                return;
            }

            const JsonVariant portValue = json["port"];
            if (!portValue.is<int>() || portValue.as<int>() < 1 || portValue.as<int>() > 65535) {
                sendError(request, 422, "invalid port");
                return;
            }

            if (json.containsKey("enabled") && !json["enabled"].is<bool>()) {
                sendError(request, 422, "invalid enabled");
                return;
            }

            LogConfig config;
            strncpy(config.host, hostValue.as<const char*>(), LOG_HOST_LEN - 1);
            config.host[LOG_HOST_LEN - 1] = '\0';
            strncpy(config.uri, uriValue.as<const char*>(), LOG_HOST_LEN - 1);
            config.uri[LOG_HOST_LEN - 1] = '\0';
            config.port = static_cast<uint16_t>(portValue.as<int>());

            const bool ok = json.containsKey("enabled")
                                ? _logger.applySettings(config, json["enabled"].as<bool>())
                                : _logger.applySettings(config);
            if (!ok) {
                sendError(request, 422, "invalid settings");
                return;
            }

            if (_onApplied) {
                /* Persist the logger's final state, not the request echo. */
                const LogConfig applied = _logger.getConfig();
                const bool enabled = _logger.isEnabled();
                if (!_onApplied(applied, enabled)) {
                    sendError(request, 500, "failed to store settings");
                    return;
                }
            }

            sendState(request, 200);
        }

        void sendState(AsyncWebServerRequest* request, int status)
        {
            // Two atomic snapshots; a concurrent change may blend them — harmless for an echo.
            const LogConfig config = _logger.getConfig();

            StaticJsonDocument<256> doc;
            doc["host"] = config.host;
            doc["uri"] = config.uri;
            doc["port"] = config.port;
            doc["enabled"] = _logger.isEnabled();

            std::string body;
            body.reserve(256);
            serializeJson(doc, body);

            request->send(status, "application/json", body.c_str());
        }

        void sendError(AsyncWebServerRequest* request, int status, const char* message)
        {
            StaticJsonDocument<128> doc;
            doc["message"] = message;

            std::string body;
            body.reserve(128);
            serializeJson(doc, body);

            request->send(status, "application/json", body.c_str());
        }

        NetworkLogger& _logger;
        std::string _path;
        std::string _statsPath;
        on_applied_t _onApplied;

        static constexpr size_t LOG_SETTINGS_JSON_SIZE = 1024;
        static constexpr int LOG_SETTINGS_MAX_BODY = 1024;
    };
}
