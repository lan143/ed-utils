#pragma once

//
// HTTP settings endpoint for the network logger.
//
// GET  /api/log/settings -> current logging target and enabled flag
// POST /api/log/settings -> validate and apply a new target/enabled flag
// POST requires Content-Type: application/json; empty or malformed JSON is
// rejected by ESPAsyncWebServer's AsyncCallbackJsonWebHandler with bare 400, oversized
// body (> 1024 bytes) with bare 413, before field validation runs.
//
// Persistence is optional: a consumer can register an onApplied() callback to
// store the settings. The applied values stay active even if storing fails.
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
        explicit LogSettingsHandler(NetworkLogger& logger, const char* path = "/api/log/settings")
            : _logger(logger),
              _path(path != nullptr ? path : "/api/log/settings")
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
        }

    private:
        void handleGet(AsyncWebServerRequest* request)
        {
            sendState(request, 200);
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

            const JsonVariant uriValue = json["uri"];
            if (uriValue.isNull() || !uriValue.is<const char*>() || strlen(uriValue.as<const char*>()) == 0) {
                sendError(request, 422, "uri is required");
                return;
            }

            if (strlen(uriValue.as<const char*>()) >= LOG_HOST_LEN) {
                sendError(request, 422, "uri too long");
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

            if (!_logger.setConfig(config)) {
                sendError(request, 422, "invalid settings");
                return;
            }

            if (json.containsKey("enabled")) {
                _logger.enable(json["enabled"].as<bool>());
            }

            if (_onApplied) {
                bool enabled = json.containsKey("enabled")
                                   ? json["enabled"].as<bool>()
                                   : _logger.isEnabled();
                if (!_onApplied(config, enabled)) {
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
        on_applied_t _onApplied;

        static constexpr size_t LOG_SETTINGS_JSON_SIZE = 1024;
        static constexpr int LOG_SETTINGS_MAX_BODY = 1024;
    };
}
