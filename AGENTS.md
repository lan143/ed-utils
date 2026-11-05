# AGENTS.md

Operating guide for AI agents working in this repository.

## Overview

- `ed-utils` / `EDUtils` is a small Arduino framework library for ESP32, used by the
  home-automation project family.
- Stack: PlatformIO library, C++ (Arduino ESP32), dependency **ArduinoJson v6**.
- Version source of truth: `library.json` **and** `library.properties` (both currently
  `0.3.0`). Bump both in the same change and add an annotated git tag.
- The `Version` / `parseVersion` API is a **runtime semver parser**, not the library
  version. Do not tie it to the manifest version.

## Layout

- `src/Utils.{h,cpp}` — `getMacAddress()` (Wi-Fi STA MAC) and `getChipID()` (static buffer).
- `src/ExtStrings.{h,cpp}` — strict `str2int` / `str2float`, `formatString`, `split`.
- `src/Json.{h,cpp}` — heap-aware `parseJson` / `buildJson` over `DynamicJsonDocument`.
- `src/nullable.h` — header-only `Nullable<T>` with validity flag.
- `src/state/state_producer.h` — pure-virtual `StateProducer<T>::publish(T*)`.
- `src/state/state_mgr.{h,hpp}` — `StateMgr<T>`: 500 ms change-publishing loop.
- `src/log/log_config.h` — `LogConfig` (`host`, `uri`, `port`).
- `src/log/log_options.h` — compile-time `LOG_NET_*` options (ring/chunk/line
  sizes, timeouts, sender task, backoff, filters).
- `src/log/log_format.{h,cpp}` — pure, host-testable normalization, level
  detection and dropped-marker helpers.
- `src/log/log.{h,cpp}` — log macros, `NetworkLogger` (vprintf hook -> NOSPLIT
  ring -> `netlog` sender task; public API includes `applySettings`, `getStats`,
  `setNetworkReadyCheck`) and the single global `networkLogger`.
- `src/log/log_handlers.hpp` — `LogSettingsHandler` for `/api/log/settings` and
  `/api/log/stats`.
- `tests/host/` — C++11 host unit tests for `src/log/log_format.cpp`.
- `src/version/version.{h,cpp}` — global `Version` comparisons and `parseVersion`.
- Root manifests: `library.json` (PlatformIO), `library.properties` (Arduino IDE).
- No `platformio.ini`, `examples/`, CI, or LICENSE in this repo; executable host
  tests live in `tests/host/`.

## Commands

Use `rtk` as the output-condensing proxy (see `RTK.md` for the full cheatsheet).

- Browse: `rtk ls`, `rtk tree src`, `rtk find . -name "*.h"`.
- Read: `rtk read src/Json.cpp`.
- Search: `rtk rg 'EDUtils::' src` or `rtk grep 'EDUtils::' src`.
- Git: `rtk git status`, `rtk git log --oneline -10`, `rtk git diff`, `rtk git branch`.
- Review: `rtk diff`.

### Validation (host tests + consumer compile)

PlatformIO Core is installed globally (`pio` / `platformio`); nothing to install in
this repo. There is no linter or CI. The gates are the host unit tests plus a
consumer compile check.

Run the host tests first (C++11, no ESP32 required):

```sh
sh tests/host/run_host_tests.sh
```

Then compile the library in a throwaway consumer project:

1. Create a throwaway consumer project **outside** this repo:

   ```sh
   mkdir -p /tmp/ed-utils-check && cd /tmp/ed-utils-check
   pio project init --board esp32dev
   ```

2. In its `platformio.ini` add:

   ```ini
   lib_deps =
       file:///absolute/path/to/ed-utils
       ArduinoJson@^6
   ```

3. Add a small `src/main.cpp` that includes the headers and calls the symbols you
   changed, then run:

   ```sh
   rtk err pio run
   ```

Assumption: consumer board is `esp32dev`. Do not add a `platformio.ini` to this
library root just to build it.

## Conventions

- Everything lives in `namespace EDUtils`, **except** `Version` and `parseVersion`
  (global namespace).
- `#pragma once` in every header.
- Free functions are declared with the existing `extern` style in headers.
- Prefer `std::string`, `std::vector`, `std::function`; `<mutex>` is fine on ESP32.
- 4-space indentation; keep existing method naming (`getState`, `Valid`, `loop`, ...).
- Commits are short and imperative: `Add X` / `Implement X` (see `git log --oneline`).
- Docs language: `README.md` in Russian, `AGENTS.md` and `RTK.md` in English.
  Code snippets stay C++.

## Invariants and risks

- **ArduinoJson v6 API lock.** `src/Json.cpp` uses `DynamicJsonDocument`, removed in
  v7. Do not upgrade ArduinoJson.
- **Heap-aware sizing.** Parse starts at `min(largest_free_block, strlen(data) * 1.5)`
  and doubles on `NoMemory` while below free heap; build starts at
  `min(free_heap, 512)`, doubles capped at free heap, else returns `"{}"`. Capacity 0
  yields `false` / `"{}"`. Preserve these bounds when touching `Json.cpp`.
- **Single logger definition.** `networkLogger` is defined once in `src/log/log.cpp`
  (`extern` declaration in `log.h`). Never instantiate another one — ODR/link errors.
- **The vprintf hook is allocation- and lock-free.** It runs in whichever task
  called `esp_log_write`: never add `std::string`, `new`/`malloc`, mutexes, `LOGx`
  or blocking calls to that path. It formats into a stack buffer and only uses a
  non-blocking ring send.
- **Ring lifecycle.** Created once in `init()` (`RINGBUF_TYPE_NOSPLIT`,
  `LOG_NET_RING_SIZE`) and never recreated or grown. A later `init()` only updates
  config/system/instance and retries missing resources.
- **Sender task.** Named `netlog`, stack `LOG_NET_TASK_STACK`. Success is any 2xx;
  failure retries the same batch/`X-SEQ` with backoff
  `LOG_NET_BACKOFF_MIN_MS`..`LOG_NET_BACKOFF_MAX_MS`. Its own log output is
  excluded from the hook, so there is no feedback loop.
- **Legacy logger macros.** `LOG_BUFFER_SIZE` and `LOG_FLUSH_TIME` remain only for
  source compatibility; the new core uses `LOG_NET_*` from `log_options.h` and
  ignores them. `update()` is a deprecated no-op.
- **StateMgr cadence.** Compares states every `loop()` but publishes at most every
  500 ms and only on change; a `false` return retries next cycle. `T` needs
  `operator!=` and copy assignment.
- **Nullable** default construction leaves `_value` uninitialized until
  `setValidValue()` or the two-argument constructor.
- **`getChipID()`** returns a pointer to the internal `char[19]` in `src/Utils.cpp`;
  stable across calls but not thread-safe.
- **Strict parsing.** `str2int` / `str2float` reject empty input, leading whitespace,
  and trailing garbage; overflow maps to `STR2INT_OVERFLOW`.

## Change checklist

1. Modify `src/` only as needed; keep unrelated files untouched.
2. Run the host tests and the compile check above
   (`sh tests/host/run_host_tests.sh`, `rtk err pio run`).
3. If the public API changed, update the matching `README.md` module section.
4. If releasing, bump **both** `library.json` and `library.properties`, then tag.
5. Never conflate the library version with the `Version` runtime parser.

## Do not

- Do not add `platformio.ini` to this repo root unless explicitly asked.
- Do not upgrade ArduinoJson or replace `DynamicJsonDocument`.
- Do not reformat unrelated files.
- Do not `git add`, commit, or push unless instructed (remote is SSH:
  `git@github.com:lan143/ed-utils.git`).
