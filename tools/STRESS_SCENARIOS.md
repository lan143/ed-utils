# Стресс-сценарии сетевого логгера

Runbook для приёмки `EDUtils::NetworkLogger` на железе (Wi-Fi и Ethernet).
Требуются: плата с прошивкой-тестом, хост с приёмником в той же сети и
`tools/mock_receiver.py` (только stdlib python3).

## Общие команды

```sh
# Приёмник: 204 по умолчанию, 500 первым N запросам, зависание, всегда 500
python3 tools/mock_receiver.py --port 8080 --status 204
python3 tools/mock_receiver.py --port 8080 --fail-first 1
python3 tools/mock_receiver.py --port 8080 --fail-all
python3 tools/mock_receiver.py --port 8080 --hang 10
python3 tools/mock_receiver.py --help
```

`--help` печатает все флаги и правило учёта строк. На SIGINT приёмник печатает
итоговую сводку и завершается с кодом `0`, если не было пропусков и дублей
`X-SEQ`, иначе с кодом `1`. В статистику идут только принятые (2xx) пачки;
неудачные POST'ы повторяются с тем же `X-SEQ` и не считаются дублями.

Настройки и статистика устройства:

```sh
curl -s http://<device>/api/log/settings
curl -s -X POST http://<device>/api/log/settings \
  -H 'Content-Type: application/json' \
  -d '{"host":"192.168.1.10","uri":"/logs","port":8080,"enabled":true}'
curl -s http://<device>/api/log/stats
```

## Тестовый скетч и измерения

Четыре задачи-производителя по ~100 байт; при задержке 40 мс это ~100 строк/с.
Стек sender-задачи и прикладной задачи проверяется
`uxTaskGetStackHighWaterMark(...)`, куча — `heap_caps_get_free_size(MALLOC_CAP_8BIT)`
(плюс `heap_caps_get_minimum_free_size` для минимума).

```cpp
#include <log/log.h>

static void producer(void* arg) {
    const char* tag = static_cast<const char*>(arg);
    char payload[100];
    memset(payload, 'x', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = '\0';
    for (;;) {
        LOGI(tag, "%s", payload);          // ~100 байт
        vTaskDelay(pdMS_TO_TICKS(40));     // 4 задачи * 25/с = 100 строк/с
    }
}

TaskHandle_t g_producer = nullptr;

void setup() {
    // networkLogger.init(cfg, "stress", "board"); networkLogger.enable(true);
    xTaskCreatePinnedToCore(producer, "p1", 4096, (void*)"t1", 1, &g_producer, 1);
    xTaskCreatePinnedToCore(producer, "p2", 4096, (void*)"t2", 1, nullptr, 1);
    xTaskCreatePinnedToCore(producer, "p3", 4096, (void*)"t3", 1, nullptr, 0);
    xTaskCreatePinnedToCore(producer, "p4", 4096, (void*)"t4", 1, nullptr, 0);
}

void loop() {
    static uint32_t last = 0;
    if (millis() - last >= 60000) {
        last = millis();
        UBaseType_t netlogHw = uxTaskGetStackHighWaterMark(xTaskGetHandle("netlog"));
        UBaseType_t appHw = uxTaskGetStackHighWaterMark(g_producer);
        LOGI("measure", "heap=%u min=%u netlog_hw=%u app_hw=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
             (unsigned)netlogHw, (unsigned)appHw);
    }
    delay(10);
}
```

Единицы high-water mark зависят от версии FreeRTOS/ESP-IDF — переведите их в
байты перед сравнением с критерием.

## 1. Базовый поток

- **Цель:** штатная доставка без потерь.
- **Подготовка:** 4 задачи, 100 записей/с по ~100 байт, 30 минут,
  приёмник `--status 204`.
- **Измерения:** `/api/log/stats`, сводка приёмника, heap-лог раз в минуту.
- **Критерий:** 0 потерь (`recordsDropped == 0`, нет маркеров dropped), порядок
  записей внутри каждой задачи сохранён.

## 2. Всплеск

- **Цель:** проверить ограниченные и посчитанные потери.
- **Подготовка:** сгенерировать 1000 записей за 100 мс, приёмник `--status 204`.
- **Измерения:** `recordsProduced`, `recordsDropped`, сводка приёмника
  (`lines`, `marker_lines`, `marker_sum`).
- **Критерий:** `(lines - marker_lines) + marker_sum == число сгенерированных
  записей`: маркер-строки уже учтены в `lines` (приёмник считает их как
  содержимое).

## 3. Приёмник недоступен 5 минут

- **Цель:** отсутствие блокировки и роста памяти.
- **Подготовка:** 10 записей/с, приёмник `--fail-all` или `--hang 10` (зависший
  приёмник), либо остановить процесс / оборвать связь; затем вернуть `--status 204`.
- **Измерения:** длительность итерации `loop()` (не более +5 мс к обычной),
  heap-лог, `ringFreeBytes`, порядок пачек после восстановления.
- **Критерий:** `loop()` не блокируется (добавка не более 5 мс), память не растёт;
  после восстановления приходит сохранённая часть, затем маркер потерь, затем
  поток без разрывов.

## 4. 500, затем 204

- **Цель:** ретрай той же пачки с тем же `X-SEQ`.
- **Подготовка:** приёмник `--fail-first 1` (первый POST — 500, далее 204).
- **Измерения:** строка `seq=`/`duplicate=` в сводке приёмника, `batchesFailed`,
  `lastHttpStatus`.
- **Критерий:** повтор идёт с тем же `X-SEQ`; после дедупликации по
  `(X-BOOT-ID, X-SEQ)` дублей нет (приёмник показывает `duplicates=0`).

## 5. Включение и выключение настроек

- **Цель:** корректное применение `/api/log/settings` без перезагрузки.
- **Подготовка:** `POST /api/log/settings` с `enabled=false`, затем `true`;
  сменить `host` и `port` на другой экземпляр приёмника.
- **Измерения:** `GET /api/log/stats`, heap-лог, поведение приёмника.
- **Критерий:** при `enabled=false` heap и ring не растут (ring пуст); смена
  `host`/`port` применяется без перезагрузки, новые пачки идут на новую цель.

## 6. Нет сети или Ethernet-линка

- **Цель:** sender не шлёт и не падает без сети.
- **Подготовка:** отключить Wi-Fi или выдернуть Ethernet, затем восстановить.
- **Измерения:** `batchesSent`, `batchesFailed`, `ringFreeBytes`, heap-лог,
  сводка после появления сети.
- **Критерий:** во время отсутствия сети POST'ов нет; после появления линка
  отправка начинается без перезагрузки и без падения.

## 7. Отсутствие петли обратной связи

- **Цель:** ошибки отправки не попадают в сеть и не лавинообразят трафик.
- **Подготовка:** приёмник `--fail-all`, 10 минут.
- **Измерения:** размер тел и частота POST'ов, строки в пачках, `batchesFailed`.
- **Критерий:** трафик не нарастает лавинообразно; в тело пачек не попадают
  сообщения об ошибках отправки (`POST ... failed`).

## 8. Стек

- **Цель:** запас стека sender-задачи и накладные расходы хука.
- **Подготовка:** сценарий 1, очередь задач с минимальным стеком.
- **Измерения:** `uxTaskGetStackHighWaterMark(xTaskGetHandle("netlog"))` и
  `uxTaskGetStackHighWaterMark(handle прикладной задачи)`.
- **Критерий:** запас sender-задачи не менее 1 КБ; расход стека хуком на самой
  маленькой прикладной задаче в пределах цели §5.2 (не более 1.5 КБ сверх штатного).

## 9. Heap за час

- **Цель:** отсутствие роста heap после `init()`.
- **Подготовка:** сценарий 1 в течение часа, приёмник `--status 204`.
- **Измерения:** `heap_caps_get_free_size(MALLOC_CAP_8BIT)` и
  `heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)` раз в минуту.
- **Критерий:** изменение free heap не более 512 байт за час; минимальный free
  heap не падает.

## Отчёт

Приложите: версию платы и среды, флаги `mock_receiver.py`, вывод
`/api/log/stats` до/после, таблицу heap по минутам, high-water sender и
прикладной задачи, длительность итерации `loop()`, итоговую сводку приёмника
(`gaps`, `duplicates`, `lines`, `marker_sum`).
