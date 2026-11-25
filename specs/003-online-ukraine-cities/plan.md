# Implementation Plan: Розширений онлайн-вибір будь-якого міста України

**Branch**: `003-online-ukraine-cities` | **Date**: 2026-11-25 | **Spec**: [spec.md](file:///d:/Pets/esp-rlcd-clock/specs/003-online-ukraine-cities/spec.md)

**Input**: Feature specification from `/specs/003-online-ukraine-cities/spec.md`

## Summary

Розширення підсистеми налаштування місця знаходження (`002-location-config`) на платі **Waveshare ESP32-C6-DEV-KIT-N8** можливістю онлайн-пошуку та вибору будь-якого населеного пункту України (міста, селища чи села) за умови фактичного підключення плати до мережі Інтернет. Додається новий ізольований компонент `geo_search`, який виконує неблокуючу фонову перевірку реальної доступності Інтернету (DNS + TCP до `geocoding-api.open-meteo.com:443`) при підключенні Wi-Fi STA та виконує захищені м'ютексом HTTPS-запити до **Open-Meteo Geocoding API** (`countryCode=UA`, `language=uk`, `count=10`) з використанням статичного буфера `12 КБ` і суворою фільтрацією результатів за Україною. Веб-сервер доповнюється полем `internet` у `GET /api/status` та новим маршрутом `GET /api/location/search?q=...`, а веб-інтерфейс — індикатором онлайн-доступності та блоком явного пошуку (кнопка «Пошук» / `Enter`, від 2 символів) зі списком результатів (назва + область/район) і підстановкою лише назви та координат у форму зі збереженням повної зворотної сумісності з офлайн-довідником і NVS-схемою `schema_ver = 2`.

## Technical Context

**Language/Version**: C11 (ESP-IDF v6.1)

**Primary Dependencies**: `esp_http_client`, `esp-tls` / `mbedtls` (`esp_crt_bundle`), `esp_http_server`, `cJSON`, `esp_wifi`, `esp_netif`, `lwip`, FreeRTOS (`esp_timer`)

**Storage**: Без змін схеми NVS — використовується існуюча підсистема `config_store` (`nvs_flash`, простір імен `"wifi_cfg"`, `schema_ver = 2`); результати онлайн-пошуку та статус Інтернету зберігаються лише в оперативній пам'яті (runtime)

**Testing**: Unity Test Framework (новий набір тестів `test/unit/test_geo_search.c` для UTF-8 валідації запитів, URL percent-encoding кирилиці, UTF-8 обрізання до 64 байтів та парсера JSON Open-Meteo з фільтрацією `country_code == "UA"`)

**Target Platform**: Waveshare ESP32-C6-DEV-KIT-N8 (32-bit RISC-V одноядерний @ 160 MHz, 512KB SRAM, 8MB Quad SPI Flash, 2.4 GHz Wi-Fi 6)

**Project Type**: Embedded Firmware (ESP-IDF CMake project із модульною архітектурою компонентів)

**Performance Goals**: Відображення результатів онлайн-пошуку `< 5 с` у 95% випадків (`SC-002`, таймаут зовнішнього HTTP-клієнта `4.5 с`), перевірка доступності Інтернету `< 2.5 с` у фоновій задачі без блокування Wi-Fi FSM та основного циклу (`FR-002`, `FR-008`)

**Constraints**: 512KB SRAM (максимум 1 активний TLS-запит одночасно через м'ютекс `s_search_mutex`, статичний буфер відповіді `12 КБ`, `httpd` стек `8192` байтів для mbedTLS), повна автономність в офлайн-режимі без Інтернету (`SC-004`), нуль попереджень компілятора при `-Wall -Wextra`

**Scale/Scope**: Новий компонент `components/geo_search/`, мінімальні точкові інтеграції в `components/wifi_manager/`, `components/web_server/` (REST + `assets/index.html`, `app.js`, `style.css`), `components/app_core/`, `sdkconfig.defaults` та новий файл модульних тестів `test/unit/test_geo_search.c`

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **I. Modular Architecture & HAL**: Вся логіка геокодування, перевірки доступності Інтернету, UTF-8 кодування та парсингу Open-Meteo JSON інкапсульована в окремому компоненті `geo_search` із чистим заголовком `geo_search.h`; чисті C-функції валідації/кодування/парсингу не залежать від апаратної частини та тестуються автономно в Unity.
- [x] **II. Resilient Network & FSM**: Перевірка Інтернету виконується асинхронно у фоновій FreeRTOS-задачі `geo_probe_task` за сповіщенням від `wifi_manager` (ніколи не блокує обробники подій Wi-Fi чи FSM); втрата Інтернету, таймаут або недоступність Open-Meteo повертають контрольовану помилку `503` без розриву Wi-Fi та без деградації офлайн-функціональності.
- [x] **III. Memory & Concurrency Safety (NON-NEGOTIABLE)**: М'ютекс `s_search_mutex` гарантує не більше 1 одночасного TLS-з'єднання; відповідь Open-Meteo читається у статичний буфер `12 КБ` без динамічного перерозподілу; `cJSON` та `esp_http_client` звільняються на всіх гілках виконання; стан `geo_internet_status_t` захищений м'ютексом; детерміновані розміри стеків (`geo_probe_task`: 4096 байтів, `httpd`: 8192 байтів) із моніторингом `uxTaskGetStackHighWaterMark`; TWDT активний.
- [x] **IV. Safe Config & NVS Persistence**: Вхідний параметр `q` валідується й санітизується до формування зовнішнього запиту; зовнішні дані фільтруються за `country_code == "UA"`, перевіряються на допустимі координати та обрізаються по межі UTF-8 символу до 64 байтів; запис у NVS відбувається виключно через існуючу валідовану та атомарну процедуру `POST /api/location` (`config_store`).
- [x] **V. Testability, Observability & Diagnostics**: Усі модульні події (перевірка Інтернету, тривалість/код відповіді Open-Meteo, помилки таймауту/парсингу) логуються через `ESP_LOGx` з тегом `"geo_search"`; статус Інтернету експортується у `GET /api/status` та `GET /api/health`; додаються модульні тести Unity у `test/unit/test_geo_search.c`.

## Project Structure

### Documentation (this feature)

```text
specs/003-online-ukraine-cities/
├── plan.md              # Цей файл (план реалізації)
├── research.md          # Результати технічного дослідження Phase 0
├── data-model.md        # Структури даних, стани доступності Інтернету та правила валідації (Phase 1)
├── quickstart.md        # Інструкція зі збірки, тестів та перевірки сценаріїв на платі (Phase 1)
├── contracts/           # Контракти інтерфейсів (Phase 1)
│   └── http-api.md      # REST API (/api/status, /api/location/search), Open-Meteo та UI-контракт
└── tasks.md             # Декомпозиція задач Phase 2 (створюється командою /speckit-tasks)
```

### Source Code (repository root)

```text
CMakeLists.txt
sdkconfig.defaults                  # Увімкнення CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
main/
├── CMakeLists.txt
└── main.c
components/
├── board_hal/
│   ├── CMakeLists.txt
│   ├── include/hal_button.h
│   └── hal_button.c
├── config_store/                   # Без змін (збереження локації schema_ver = 2)
│   ├── CMakeLists.txt
│   ├── include/config_store.h
│   └── config_store.c
├── geo_search/                     # НОВИЙ компонент: перевірка Інтернету + Open-Meteo геокодування
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── geo_search.h            # Публічний C API + чисті функції для Unity-тестів
│   ├── geo_search_parser.c         # Чисті функції: UTF-8 перевірка/обрізання, URL encode, Open-Meteo JSON parser
│   └── geo_search.c                # Фонова задача перевірки Інтернету, TLS HTTPS клієнт, синхронізація м'ютексом
├── wifi_manager/
│   ├── CMakeLists.txt              # Додавання залежності на geo_search (або callback)
│   ├── include/wifi_manager.h
│   ├── wifi_manager.c              # Сповіщення geo_search при STA_GOT_IP / STA_DISCONNECTED / Factory Reset
│   └── captive_dns.c
├── web_server/
│   ├── CMakeLists.txt              # Додавання залежності на geo_search
│   ├── include/web_server.h
│   ├── web_server.c                # Розширення GET /api/status (internet), новий GET /api/location/search, стек httpd 8192
│   └── assets/
│       ├── index.html              # Індикатор онлайн-пошуку, поле + кнопка «Пошук», контейнер результатів
│       ├── style.css               # Стилі індикатора доступності та списку результатів пошуку
│       └── app.js                  # Логіка явного пошуку (кнопка/Enter), блокування дублікатів, вибір результату
└── app_core/
    ├── CMakeLists.txt              # Додавання залежності на geo_search
    ├── include/app_core.h
    └── app_core.c                  # Ініціалізація geo_search_init() до старту Wi-Fi + діагностика стеку
test/
└── unit/
    ├── CMakeLists.txt              # Додавання test_geo_search.c та залежності geo_search
    ├── test_config_store.c
    ├── test_http_validation.c
    ├── test_wifi_fsm.c
    └── test_geo_search.c           # НОВІ модульні тести Unity для geo_search_parser.c
```

**Structure Decision**: Додається один новий ізольований компонент `components/geo_search/` із розділенням на чисту тестовану логіку (`geo_search_parser.c`) та ESP-IDF мережевий шар (`geo_search.c`). Така структура строго відповідає Принципу I Конституції, не перевантажує `web_server` чи `wifi_manager` зовнішньою TLS/HTTP-логікою та зберігає 100% зворотну сумісність із `config_store`.

## Complexity Tracking

> Порушень Конституції проєкту немає. Таблиця обґрунтування ускладнень не потрібна.
