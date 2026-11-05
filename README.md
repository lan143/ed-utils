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

Макросы логирования и неблокирующая отправка логов по HTTP. Логгер перехватывает
весь вывод `esp_log` через `esp_log_set_vprintf`, копит строки в кольцевом буфере и
отправляет их отдельной задачей `netlog`; прикладной код и `loop()` при этом не
блокируются.

```cpp
#include <log/log.h>

void setup() {
    EDUtils::LogConfig cfg;
    strncpy(cfg.host, "192.168.1.10", LOG_HOST_LEN - 1);
    strncpy(cfg.uri, "/logs", LOG_HOST_LEN - 1);
    cfg.port = 8080;

    networkLogger.init(cfg, "thermostat", "kitchen");
    networkLogger.enable(true);
}

void loop() {
    LOGI("app", "pin=%d", 4);
    networkLogger.update();  // устаревший no-op, оставлен для совместимости
}
```

`networkLogger` — глобальный объект-синглтон, определённый в `src/log/log.cpp`;
не создавайте второй `EDUtils::NetworkLogger`. Конструктор глобального объекта
ничего не делает: хук, ring и sender-задача создаются в `init()`. `init()` всегда
сохраняет `system`, `instance` и конфиг, даже если тот невалиден; готовность
конфига проверяется через `isConfigured()`, отправка идёт только при
`isConfigured() && isEnabled()` и готовой сети. Отправка выключена по умолчанию
(`enable(false)`).

`update()` больше не выполняет работу: это deprecated no-op, сохранённый, чтобы
старый `loop()` продолжал собираться. `addLogRecord(const char*)` остаётся
обёрткой над записью в ring buffer.

#### Архитектура

1. **Хук `vprintf`.** Устанавливается один раз в `init()`. Форматирует строку в
   стековый буфер `LOG_NET_LINE_MAX`, вызывает предыдущий хук (консольный вывод не
   меняется) и кладёт нормализованную копию в ring buffer через
   `xRingbufferSend` с таймаутом 0. В хуке нет `malloc`/`new`, `std::string`,
   mutex, `LOGx` и блокирующих вызовов. Записи из sender-задачи в сеть не
   попадают (защита от петли обратной связи).
2. **Ring buffer.** `RINGBUF_TYPE_NOSPLIT` размером `LOG_NET_RING_SIZE`
   (по умолчанию 24 КБ), создаётся один раз и не растёт. При переполнении
   отбрасывается новая запись и увеличивается счётчик `recordsDropped`.
3. **Sender-задача `netlog`.** Низкий приоритет, отдельный стек
   `LOG_NET_TASK_STACK`. Ждёт до `LOG_NET_FLUSH_INTERVAL_MS` или внеочередного
   уведомления, собирает пачку в буфер `LOG_NET_CHUNK_SIZE` и отправляет
   HTTP POST. Пустые пачки не отправляются.

Нормализация: ANSI-последовательности вырезаются при `LOG_NET_STRIP_ANSI=1`,
завершающие CRLF сводятся к одному LF, каждая строка заканчивается LF. Если
строка не помещается в лимит, она усекается, а перед LF добавляется маркер
` [...+N]` с числом потерянных байт.

Накопленные потери не теряются молча: перед следующей пачкой в неё добавляется
первая строка `[netlog] dropped N records`, а счётчик `recordsDropped` доступен
через `getStats()`. Маркер становится частью пачки и повторяется при ретрае
вместе с ней.

#### Макросы

Доступны `LOGE`, `LOGW`, `LOGI`, `LOGD`, `LOGV`; все они раскрываются в
`do { ... } while (0)`. Макросы фильтруются на этапе компиляции по
`CORE_DEBUG_LEVEL` (по умолчанию `ARDUHAL_LOG_LEVEL_INFO`), поэтому при
стандартной сборке `LOGD`/`LOGV` компилируются в no-op — чтобы они работали,
соберите с `-DCORE_DEBUG_LEVEL=4` или `5`. Дополнительно сеть фильтруется
в рантайме по `LOG_NET_MIN_LEVEL`; консольный вывод этим фильтром не
ограничивается.

#### Параметры сборки

Все параметры задаются через `#ifndef` в `src/log/log_options.h` и
переопределяются флагами вида `-DLOG_NET_RING_SIZE=32768`.

| Define | По умолчанию | Назначение |
| --- | --- | --- |
| `LOG_NET_RING_SIZE` | `24576` | размер ring buffer |
| `LOG_NET_CHUNK_SIZE` | `4096` | размер пачки |
| `LOG_NET_LINE_MAX` | `256` | максимальная длина одной записи (с LF, без NUL) |
| `LOG_NET_FLUSH_INTERVAL_MS` | `2000` | максимальная задержка перед отправкой |
| `LOG_NET_FLUSH_THRESHOLD` | `(LOG_NET_RING_SIZE * 3 / 4)` | порог свободного места для досрочного пробуждения sender |
| `LOG_NET_URGENT_MIN_GAP_MS` | `500` | минимальный интервал между внеочередными пробуждениями |
| `LOG_NET_TASK_STACK` | `6144` | стек sender-задачи |
| `LOG_NET_TASK_PRIO` | `(tskIDLE_PRIORITY + 1)` | приоритет sender-задачи |
| `LOG_NET_TASK_CORE` | `tskNO_AFFINITY` | ядро sender-задачи |
| `LOG_NET_CONNECT_TIMEOUT_MS` | `1500` | таймаут соединения |
| `LOG_NET_TIMEOUT_MS` | `3000` | таймаут запроса |
| `LOG_NET_BACKOFF_MIN_MS` | `1000` | нижняя граница backoff |
| `LOG_NET_BACKOFF_MAX_MS` | `60000` | верхняя граница backoff |
| `LOG_NET_STRIP_ANSI` | `1` | вырезать ANSI-коды |
| `LOG_NET_MIN_LEVEL` | `NET_LOG_VERBOSE` | минимальный уровень для сети |

#### Публичный API и статистика

Помимо `init`, `enable`, `isEnabled`, `setConfig`, `getConfig`, `addLogRecord` и
`update` доступны:

- `applySettings(config)` / `applySettings(config, enabled)` — атомарно применяют
  цель и флаг включения.
- `isConfigured()` — валиден ли текущий конфиг (`host`, `uri`, `port`).
- `setNetworkReadyCheck(std::function<bool()>)` — свой критерий готовности сети.
- `getStats()` — счётчики `LogStats`.

`LogStats` содержит `recordsProduced`, `recordsDropped`, `batchesSent`,
`batchesFailed`, `bytesSent`, `lastHttpStatus` и `ringFreeBytes` (свободное место
в ring, приблизительно; `0`, если ring не создан).

По умолчанию сеть считается готовой, когда `WiFi.isConnected()` или — если
доступен `<ETH.h>` — Ethernet подключён (`ETH.connected()`, либо `ETH.linkUp()`
в старых ядрах). Свой критерий можно задать так:

```cpp
networkLogger.setNetworkReadyCheck([] {
    return WiFi.isConnected() || (ETH.linkUp() && ETH.localIP() != IPAddress());
});
```

Пока сеть не готова, sender ничего не отправляет, а записи остаются в ring.

#### Формат HTTP-запроса

`POST http://<host>:<port><uri>` с `Content-Type: text/plain`. Тело — пачка
строк, разделённых LF, передаётся с явной длиной (указатель + размер).

| Заголовок | Значение |
| --- | --- |
| `X-SYSTEM` | система, заданная в `init()` |
| `X-INSTANCE` | экземпляр, заданный в `init()` |
| `X-BOOT-ID` | случайный 32-битный идентификатор загрузки в hex (`%08x`) |
| `X-SEQ` | номер пачки в рамках загрузки; при ретрае не меняется |
| `X-UPTIME-MS` | `esp_timer_get_time() / 1000` на момент отправки пачки |
| `X-EPOCH-MS` | Unix-время в мс; только если `time(nullptr) > 1700000000` |
| `X-DROPPED-TOTAL` | накопленное число потерянных записей с момента загрузки |

Успехом считается любой код 2xx. При ошибке та же пачка с тем же `X-SEQ`
повторяется с экспоненциальным backoff от 1 с до 60 с; pending и маркер потерь
при этом не меняются. При выключении логгера ring и pending очищаются.

Каждая строка сохраняет собственное время от старта внутри префикса Arduino/IDF
(например, `I (12345) ...`), а `X-UPTIME-MS` — момент отправки всей пачки.
Приёмник пересчитывает время строки по формуле:

```text
t_line = X-EPOCH-MS - (X-UPTIME-MS - uptime_of_line)
```

Если `X-EPOCH-MS` отсутствует, приёмник использует собственное время получения.

#### Память

- ring buffer — около 24 КБ heap, выделяется один раз в `init()`;
- chunk-буфер пачки — 4 КБ в `.bss` (внутри глобального объекта логгера);
- стек sender-задачи — 6 КБ, выделяется один раз при создании задачи.

После `init()` heap не растёт. Если ring не удалось выделить, консольный вывод
продолжает работать, сетевая часть остаётся неактивной, а
`getStats().ringFreeBytes == 0`; повторный `init()` может попробовать снова.
Макросы `LOG_BUFFER_SIZE` и `LOG_FLUSH_TIME` оставлены только для совместимости
и новым ядром не используются.

#### HTTP API настроек и статистики логгера

Заголовочный обработчик `src/log/log_handlers.hpp` (зависимость **ESPAsyncWebServer**)
публикует настройки логгера по адресу `/api/log/settings` и статистику по
`/api/log/stats`:

- `GET /api/log/settings` — текущие значения.
- `POST /api/log/settings` — применить новые значения атомарно.
- `GET /api/log/stats` — runtime-счётчики из `getStats()` (не сохраняются и
  сбрасываются при перезагрузке).

Тело запроса и ответа настроек — один и тот же JSON-объект:

```json
{"host": "192.168.1.10", "uri": "/logs", "port": 8080, "enabled": true}
```

Поля: `host` и `uri` — непустые строки короче `LOG_HOST_LEN`, `port` — целое в
диапазоне 1..65535, `enabled` — необязательное булево (если не передано,
текущее значение не меняется). `POST` требует `Content-Type: application/json`.
`POST` применяет цель и флаг одним вызовом `applySettings`, поэтому sender не
видит частично обновлённую конфигурацию.

Ошибки валидации полей возвращаются как JSON `{"message": "..."}` с кодом 422:
`invalid json`, `host is required`, `host too long`, `invalid host`,
`uri is required`, `uri too long`, `invalid uri`, `invalid port`,
`invalid enabled`, `invalid settings`. В частности, `host` не должен содержать
пробелов и `://`, а `uri` должен начинаться с `/`. Пустое тело, некорректный JSON
или тело больше 1024 байт отклоняются библиотекой ESPAsyncWebServer с кодами
400/413 без JSON-тела.

`GET /api/log/stats` возвращает:

```json
{
  "recordsProduced": 1200,
  "recordsDropped": 3,
  "batchesSent": 42,
  "batchesFailed": 1,
  "bytesSent": 102400,
  "lastHttpStatus": 204,
  "ringFreeBytes": 20480
}
```

```cpp
#include <log/log_handlers.hpp>

AsyncWebServer server(80);
EDUtils::LogSettingsHandler logSettings(networkLogger);
// Третий аргумент конструктора — путь статистики, по умолчанию "/api/log/stats".
logSettings.registerHandlers(&server);
```

Поля `system`/`instance` задаются только в `init()` и через API не редактируются.

Сохранение настроек опционально: колбэк `onApplied(...)` вызывается после применения
настроек в контексте задачи AsyncWebServer с **итоговым** состоянием логгера
(`getConfig()`, `isEnabled()`), а не с эхом запроса. Вернув `false`, прошивка
сигнализирует о сбое сохранения — ответ будет
`500 {"message": "failed to store settings"}`, при этом применённые значения
остаются активны. Без колбэка настройки не сохраняются и живут только в
runtime-состоянии `NetworkLogger`.

```cpp
logSettings.onApplied([&configMgr](const EDUtils::LogConfig& config, bool enabled) {
    configMgr.getData()->log = config;
    configMgr.getData()->logEnabled = enabled;
    return configMgr.store();
});
```

`logEnabled` — пример поля в Config прошивки; если такого поля нет, можно сохранять
только `host`/`uri`/`port`.

#### Тестирование и приёмка

Host-тесты чистых функций нормализации, уровня и маркера потерь запускаются без
ESP32:

```sh
sh tests/host/run_host_tests.sh
```

Для проверки доставки на железе есть stdlib-приёмник `tools/mock_receiver.py`
(счётчики `X-SEQ`, пропусков и дублей, режимы `--status`, `--fail-first`,
`--fail-all`, `--hang`) и runbook `tools/STRESS_SCENARIOS.md`. Сборка библиотеки
по-прежнему проверяется компиляцией в проекте-потребителе.

Для управления настройками из веб-интерфейса в `frontend/` есть пакет
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

- CI в репозитории нет; есть host-тесты (`tests/host/run_host_tests.sh`),
  mock-приёмник и сценарии стресс-теста, а сборка проверяется компиляцией
  библиотеки в проекте-потребителе. Лицензия не указана.
- Память `NetworkLogger`: ring buffer ~24 КБ heap (выделяется один раз в `init()`),
  chunk-буфер 4 КБ в `.bss` и стек sender-задачи 6 КБ. После `init()` heap не растёт.
- Отправка выполняется sender-задачей: в штатном режиме она просыпается не реже
  чем раз в `LOG_NET_FLUSH_INTERVAL_MS` (2 с) или досрочно при заполнении ring;
  при ошибке та же пачка повторяется с backoff от 1 с до 60 с.
- Локальная версия библиотеки (`0.3.0`) задаётся в `library.json` и
  `library.properties`; `Version`/`parseVersion` — это runtime-парсер, а не версия библиотеки.

## Автор

Artem Kravchenko — <https://lan143.ru>

- Репозиторий: <https://github.com/lan143/ed-utils>; пакет: `ed-utils` (PlatformIO), `EDUtils` (Arduino IDE)
