# Implementation Plan: Поля налаштування поточного місця знаходження на сторінці підключення

**Branch**: `002-location-config` | **Date**: 2026-11-18 | **Spec**: [spec.md](file:///d:/Pets/esp-rlcd-clock/specs/002-location-config/spec.md)

**Input**: Feature specification from `/specs/002-location-config/spec.md`

## Summary

Розширення вбудованої веб-сторінки налаштування підключення та підсистеми збереження конфігурації на платі **Waveshare ESP32-C6-DEV-KIT-N8** окремим блоком налаштування поточного місця знаходження. Користувач може обрати населений пункт України зі вбудованого офлайн-довідника (з автоматичною підстановкою широти та довготи) або ввести/скоригувати назву населеного пункту й координати вручну та зберегти їх окремою кнопкою «Зберегти місце знаходження» без перепідключення Wi-Fi. Налаштування локації валідуються та атомарно зберігаються в енергонезалежній пам'яті NVS (зі збереженням зворотної сумісності схеми `schema_ver: 1 → 2`), повертаються у `GET /api/status` та `GET /api/location`, оновлюються через `POST /api/location` і скидаються до стану «не налаштовано» під час повного апаратного скидання кнопкою `BOOT` (`GPIO9`).

## Technical Context

**Language/Version**: C11 (ESP-IDF v6.1)

**Primary Dependencies**: `esp_http_server`, `nvs_flash`, `cJSON`, `esp_wifi`, `esp_netif`, FreeRTOS (`esp_timer`, `driver/gpio`)

**Storage**: ESP-IDF Non-Volatile Storage (`nvs_flash`), простір імен `"wifi_cfg"` (розширення схеми до `schema_ver = 2` із міграцією з версії `1`)

**Testing**: Unity Test Framework (модульні тести для валідації назви населеного пункту та координат, міграції схеми NVS та парсера JSON `POST /api/location`)

**Target Platform**: Waveshare ESP32-C6-DEV-KIT-N8 (32-bit RISC-V одноядерний @ 160 MHz, 512KB SRAM, 8MB Flash, 2.4 GHz Wi-Fi 6)

**Project Type**: Embedded Firmware (ESP-IDF CMake project з модульною архітектурою компонентів)

**Performance Goals**: Повне збереження та застосування місця знаходження `< 200 мс` (користувацький цикл налаштування `< 1 хв` згідно з `SC-001`), нульовий вплив на активне клієнтське з'єднання Wi-Fi (`SC-004`)

**Constraints**: 512KB SRAM (довідник населених пунктів України розміщується у статичних файлах у Flash через `EMBED_TXTFILES` без витрат оперативної пам'яті; фіксовані буфери для рядків UTF-8 до 64 байтів), автономна робота через SoftAP без доступу до Інтернету (`SC-005`)

**Scale/Scope**: Модифікація 3 основних компонентів (`config_store`, `web_server`, `app_core`), оновлення статичного веб-інтерфейсу (`components/web_server/assets/index.html`, `app.js`, `style.css`) та розширення модульних тестів `test/unit/test_config_store.c` і `test/unit/test_http_validation.c`

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **I. Modular Architecture & HAL**: Логіка валідації та збереження місця знаходження ізольована у компоненті `config_store`; HTTP-маршрути та JSON-парсер ізольовані у `web_server`; чисті C-заголовки дозволяють тестувати валідацію координат і парсинг JSON без апаратної частини.
- [x] **II. Resilient Network & FSM**: Збереження місця знаходження (`POST /api/location`) виконується незалежно від скінченного автомата Wi-Fi (`wifi_manager`) і ніколи не викликає перепідключення STA або перезапуск SoftAP (`FR-007`, `SC-004`).
- [x] **III. Memory & Concurrency Safety (NON-NEGOTIABLE)**: Вбудований список населених пунктів України зберігається у Flash-пам'яті (у складі статичного скрипта `app.js`, скомпільованого через `EMBED_TXTFILES`), не займаючи SRAM; у C-структурах використовуються фіксовані буфери (`char loc_name[65]`); об'єкти `cJSON` звільняються на всіх гілках виконання; спільний стан конфігурації захищений м'ютексом.
- [x] **IV. Safe Config & NVS Persistence**: Назва населеного пункту (1–64 байти UTF-8 без керівних символів) та координати (`-90.0 <= lat <= 90.0`, `-180.0 <= lon <= 180.0`, скінченні числа) проходять сувору перевірку до запису в NVS; реалізовано безпечну міграцію схеми `schema_ver = 1 → 2`, атомарний `nvs_commit()` та скидання локації під час `config_store_factory_reset()` (`FR-010`).
- [x] **V. Testability, Observability & Diagnostics**: Логування операцій збереження/помилок валідації через `ESP_LOGx`; стан місця знаходження включено до `GET /api/status` та `GET /api/location`; додаються модульні тести Unity для граничних координат, невалідних даних та JSON-парсера.

## Project Structure

### Documentation (this feature)

```text
specs/002-location-config/
├── plan.md              # Цей файл (план реалізації)
├── research.md          # Результати технічного дослідження Phase 0
├── data-model.md        # Модель даних, розширення схеми NVS та правила валідації (Phase 1)
├── quickstart.md        # Інструкція зі збірки, прошивки та перевірки сценаріїв (Phase 1)
├── contracts/           # Контракти інтерфейсів (Phase 1)
│   └── http-api.md      # Специфікація HTTP маршрутів та JSON-схем для місця знаходження
└── tasks.md             # Декомпозиція задач Phase 2 (створюється командою /speckit-tasks)
```

### Source Code (repository root)

```text
CMakeLists.txt
sdkconfig.defaults
main/
├── CMakeLists.txt
└── main.c
components/
├── board_hal/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── hal_button.h
│   └── hal_button.c
├── config_store/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── config_store.h          # Розширення структури та API збереження локації (schema_ver = 2)
│   └── config_store.c              # Валідація координат/назви, NVS серіалізація та міграція
├── wifi_manager/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── wifi_manager.h
│   │   └── captive_dns.h
│   ├── wifi_manager.c
│   └── captive_dns.c
├── web_server/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── web_server.h            # Додавання web_server_parse_location_json()
│   ├── web_server.c                # Обробники GET/POST /api/location та розширення GET /api/status
│   └── assets/
│       ├── index.html              # Окремий блок вибору населеного пункту України та полів координат
│       ├── style.css               # Стилізація блоку місця знаходження та станів помилок/успіху
│       └── app.js                  # Вбудований довідник населених пунктів України, автопідстановка, збереження
└── app_core/
    ├── CMakeLists.txt
    ├── include/
    │   └── app_core.h
    └── app_core.c                  # Кешування та надання активної локації іншим підсистемам
test/
└── unit/
    ├── CMakeLists.txt
    ├── test_config_store.c         # Розширення тестів Unity для валідації локації та міграції NVS
    ├── test_http_validation.c      # Розширення тестів Unity для web_server_parse_location_json()
    └── test_wifi_fsm.c
```

**Structure Decision**: Зберігається існуюча 5-компонентна модульна архітектура ESP-IDF (`hal`, `config_store`, `wifi_manager`, `web_server`, `app_core`). Зміни локалізовані в `config_store` (персистентність та валідація локації), `web_server` (REST API та статичний UI) і `test/unit` (модульні тести).

## Complexity Tracking

> Порушень Конституції проекту немає. Таблиця обґрунтування ускладнень не потрібна.
