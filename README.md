# ed-utils

Небольшая библиотека утилит для Arduino/ESP32 из семейства проектов домашней автоматизации.
Внутри: работа со строками и числами, JSON с учётом свободной кучи, «nullable»-обёртка,
менеджер состояния с публикацией, отправка логов по сети, идентификаторы устройства и разбор semver.

## Требования

- ESP32 и Arduino framework (библиотека рассчитана на `espressif32`).
- PlatformIO (или Arduino IDE).
- Зависимость **ArduinoJson v6**. Версия v7 **несовместима**: в ней удалён `DynamicJsonDocument`, на котором построен `src/Json.cpp`.

## Установка

### PlatformIO

Через git-репозиторий:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    https://github.com/lan143/ed-utils.git
    ArduinoJson@^6
```

Локальная разработка (подключение рабочей копии вместо репозитория):

```ini
lib_deps =
    file:///absolute/path/to/ed-utils
    ArduinoJson@^6
```

### Arduino IDE

Склонируйте репозиторий и установите библиотеку через
**Sketch → Include Library → Add .ZIP Library…**, указав ZIP-архив с библиотекой.
Зависимость ArduinoJson v6 нужно установить отдельно через Library Manager.

## Модули

### Utils

Идентификаторы устройства: MAC-адрес Wi-Fi STA и chip ID из eFuse.

```cpp
#include <Utils.h>

std::string mac = EDUtils::getMacAddress();  // "AA:BB:CC:DD:EE:FF"
const char* chipId = EDUtils::getChipID();   // "0x1a2b3c4d5e6f"
```

`getChipID()` возвращает указатель на статический буфер — не освобождайте его и не рассчитывайте на потокобезопасность.

### ExtStrings

Строгие преобразования строк в числа, printf-форматирование и разбиение строки.

```cpp
#include <ExtStrings.h>

int value = 0;
if (EDUtils::str2int(&value, "42", 10) == EDUtils::STR2INT_SUCCESS) { /* value == 42 */ }

float_t ratio = 0;
if (EDUtils::str2float(&ratio, "3.14") == EDUtils::STR2INT_SUCCESS) { /* ratio == 3.14f */ }

std::string text = EDUtils::formatString("pin=%d", 4);
std::vector<std::string> parts = EDUtils::split("a,,b", ",");  // {"a", "", "b"}
```

`str2int`/`str2float` отвергают пустую строку, ведущие пробелы и «хвост» после
числа (возвращают `STR2INT_INCONVERTIBLE`), а переполнение — `STR2INT_OVERFLOW`.

### Json

Разбор и сборка JSON с выделением документа под размер данных и свободную кучу.

```cpp
#include <Json.h>

bool ok = EDUtils::parseJson("{\"value\":1}", [](JsonObject obj) {
    int v = obj["value"].as<int>();
    return v == 1;
});

std::string out = EDUtils::buildJson([](JsonObject obj) {
    obj["value"] = 1;
});
```

Лямбда для `parseJson` возвращает `bool` (результат разбора), для `buildJson` —
`void`. При нехватке памяти `parseJson` возвращает `false`, а `buildJson` —
строку `"{}"`.

### Nullable

Обёртка над значением с флагом валидности.

```cpp
#include <nullable.h>

struct Sensor { int value; };

EDUtils::Nullable<Sensor> sensor;          // невалидно
sensor.setValidValue(Sensor{42});          // теперь валидно

if (sensor.Valid()) {
    Sensor s = sensor.Value();
    // s.value == 42
}
```

Двухаргументный конструктор: `EDUtils::Nullable<Sensor> other(true, Sensor{7});`.
Значение по умолчанию (`_value`) не инициализируется до `setValidValue`
или двухаргументного конструктора. Сравнение `==`/`!=` учитывает и значение, и валидность.

### State

Менеджер состояния: сравнивает новое состояние со старым и публикует изменения
через `StateProducer` не чаще, чем раз в 500 мс.

```cpp
#include <state/state_mgr.h>
#include <Json.h>

struct DeviceState {
    int value = 0;
    bool operator!=(const DeviceState& other) const { return value != other.value; }
};

class DeviceProducer : public EDUtils::StateProducer<DeviceState> {
public:
    bool publish(DeviceState* state) override {
        std::string payload = EDUtils::buildJson([state](JsonObject obj) {
            obj["value"] = state->value;
        });
        // ... отправить payload ...
        return true;  // false — повторить на следующем цикле
    }
};

DeviceProducer producer;
EDUtils::StateMgr<DeviceState> stateMgr(&producer);

void setup() { stateMgr.getState().value = 1; }

void loop() { stateMgr.loop(); }
```

`T` должен поддерживать `operator!=` и копирующее присваивание. `getState()`
возвращает изменяемую ссылку на новое состояние.

### Log

Макросы логирования и отправка накопленных логов по HTTP.

```cpp
#include <log/log.h>

void setup() {
    LOGI("app", "pin=%d", 4);

    EDUtils::LogConfig cfg;
    strncpy(cfg.host, "192.168.1.10", LOG_HOST_LEN - 1);
    strncpy(cfg.uri, "/logs", LOG_HOST_LEN - 1);
    cfg.port = 8080;

    networkLogger.init(cfg, "thermostat", "kitchen");
    networkLogger.enable(true);
}

void loop() { networkLogger.update(); }
```

Доступны макросы `LOGE`, `LOGW`, `LOGI`, `LOGD`, `LOGV`. `networkLogger` —
глобальный объект-синглтон, определённый в `src/log/log.cpp`; не создавайте второй
`EDUtils::NetworkLogger`. Отправка выключена по умолчанию (`enable(false)`),
`init()` молча ничего не делает при пустых `host`/`uri` или `port == 0`.
`update()` отправляет данные не чаще, чем раз в 10 секунд (`LOG_FLUSH_TIME`
равен 10 000 000 микросекунд), с заголовками `X-SYSTEM` и `X-INSTANCE`.

#### HTTP API настроек логгера

Заголовочный обработчик `src/log/log_handlers.hpp` (зависимость **ESPAsyncWebServer**)
публикует настройки логгера по адресу `/api/log/settings`:

- `GET /api/log/settings` — текущие значения.
- `POST /api/log/settings` — применить новые значения.

Тело запроса и ответа — один и тот же JSON-объект:

```json
{"host": "192.168.1.10", "uri": "/logs", "port": 8080, "enabled": true}
```

Поля: `host` и `uri` — непустые строки короче `LOG_HOST_LEN`, `port` — целое в
диапазоне 1..65535, `enabled` — необязательное булево (если не передано,
текущее значение не меняется). `POST` требует `Content-Type: application/json`.
Ошибки валидации полей возвращаются как JSON `{"message": "..."}` с кодом 422;
пустое тело, некорректный JSON или тело больше 1024 байт отклоняются
библиотекой ESPAsyncWebServer с кодами 400/413 без JSON-тела.

```cpp
#include <log/log_handlers.hpp>

AsyncWebServer server(80);
EDUtils::LogSettingsHandler logSettings(networkLogger);
logSettings.registerHandlers(&server);
```

Поля `system`/`instance` задаются только в `init()` и через API не редактируются.

Сохранение настроек опционально: колбэк `onApplied(...)` вызывается после применения
настроек в контексте задачи AsyncWebServer с новыми действующими значениями
(`config`, `enabled`). Вернув `false`, прошивка сигнализирует о сбое сохранения —
ответ будет `500 {"message": "failed to store settings"}`, при этом применённые
значения остаются активны. Без колбэка настройки не сохраняются и живут только
в runtime-состоянии `NetworkLogger`.

```cpp
logSettings.onApplied([&configMgr](const EDUtils::LogConfig& config, bool enabled) {
    configMgr.getData()->log = config;
    configMgr.getData()->logEnabled = enabled;
    return configMgr.store();
});
```

`logEnabled` — пример поля в Config прошивки; если такого поля нет, можно сохранять
только `host`/`uri`/`port`.

Для управления этими настройками из веб-интерфейса в `frontend/` есть пакет
`edu-log-ui`: Lit-элемент `<edu-log-settings-page>` с формой `host`/`uri`/`port`/`enabled`.
Подробности подключения — в `frontend/INTEGRATION.md`.

### Version

Разбор semver-строки и сравнение версий (глобальное пространство имён, без `EDUtils`).

```cpp
#include <version/version.h>

auto result = parseVersion("1.2.3");
if (result.second) {
    Version v = result.first;      // {1, 2, 3}
    Version minimum{1, 0, 0};
    bool ok = v >= minimum;        // true
}
```

`parseVersion` возвращает `std::pair<Version, bool>`; `bool` равен `true` только
если все три компонента разобраны.

## Ограничения и заметки

- Тестов и CI в репозитории пока нет; единственная проверка — компиляция библиотеки в проекте-потребителе. Лицензия не указана.
- `NetworkLogger` резервирует два буфера по 16 КБ каждый (всего ~32 КБ RAM)
  и дополнительно использует `HTTPClient` во время отправки.
- Отправка логов выполняется не чаще раза в 10 секунд.
- Локальная версия библиотеки (`0.1.0`) задаётся в `library.json` и
  `library.properties`; `Version`/`parseVersion` — это runtime-парсер, а не версия библиотеки.

## Автор

Artem Kravchenko — <https://lan143.ru>

- Репозиторий: <https://github.com/lan143/ed-utils>; пакет: `ed-utils` (PlatformIO), `EDUtils` (Arduino IDE)
