# Tasks: Поля налаштування поточного місця знаходження на сторінці підключення

**Input**: Design documents from `/specs/002-location-config/`  
**Prerequisites**: [plan.md](file:///d:/Pets/esp-rlcd-clock/specs/002-location-config/plan.md) (required), [spec.md](file:///d:/Pets/esp-rlcd-clock/specs/002-location-config/spec.md) (required), [research.md](file:///d:/Pets/esp-rlcd-clock/specs/002-location-config/research.md), [data-model.md](file:///d:/Pets/esp-rlcd-clock/specs/002-location-config/data-model.md), [contracts/http-api.md](file:///d:/Pets/esp-rlcd-clock/specs/002-location-config/contracts/http-api.md)

**Organization**: Задачі згруповано за користувацькими історіями (User Stories) для забезпечення незалежної реалізації та перевірки кожної історії.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Може виконуватися паралельно (різні файли, відсутність блокуючих залежностей від незавершених задач)
- **[Story]**: Ідентифікатор користувацької історії (`[US1]`, `[US2]`, `[US3]`)
- Кожна задача містить точний шлях до файлу у репозиторії

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Оголошення констант схеми NVS `v2`, структур даних місця знаходження та заголовкових контрактів між компонентами

- [X] T001 Define location constants (`CONFIG_STORE_SCHEMA_VERSION = 2U`, `LOC_NAME_MAX_LEN = 64U`, `LOC_LAT_MIN`, `LOC_LAT_MAX`, `LOC_LON_MIN`, `LOC_LON_MAX`) and extend `wifi_stored_config_t` with `loc_configured`, `loc_name`, `latitude`, and `longitude` fields in `components/config_store/include/config_store.h`
- [X] T002 [P] Declare `web_server_parse_location_json` contract for `/api/location` JSON payload parsing in `components/web_server/include/web_server.h`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Базова персистентність у NVS (міграція схеми `v1 → v2`, серіалізація координат у мікроградусах `int32_t`, заводське скидання) та доступ до активної локації з ядра застосунку

**⚠️ CRITICAL**: Жодна користувацька історія не може бути розпочата до завершення цієї фази

- [X] T003 Declare `config_store_validate_location` and `config_store_save_location` API functions in `components/config_store/include/config_store.h`
- [X] T004 Implement location defaults (`config_store_get_defaults`), NVS schema `v1 → v2` forward migration with safe fallback on missing/corrupted keys (`CHK015`), microdegree `int32_t` read/write (`"loc_set"`, `"loc_name"`, `"loc_lat_u6"`, `"loc_lon_u6"`), and location clearing in `config_store_factory_reset` (`FR-006`, `FR-010`) in `components/config_store/config_store.c`
- [X] T005 [P] Implement thread-safe runtime location cache and getter/update API (`app_core_get_location` / `config_store_get_location`) for downstream subsystems (`FR-008`) in `components/app_core/include/app_core.h` and `components/app_core/app_core.c`

**Checkpoint**: Базова інфраструктура NVS `v2` та кешування локації готові — можна розпочинати реалізацію користувацьких історій

---

## Phase 3: User Story 1 - Налаштування поточного місця знаходження під час первинного підключення (Priority: P1) 🎯 MVP

**Goal**: Користувач відкриває вбудовану веб-сторінку (через точку доступу `"ESP Weather Clock"` офлайн або в локальній мережі), обирає населений пункт України зі вбудованого списку (з автоматичною підстановкою широти й довготи) та зберігає місце знаходження в енергонезалежній пам'яті.

**Independent Test**: Підключитися до точки доступу `"ESP Weather Clock"` без доступу до Інтернету, відкрити `http://192.168.4.1/`, обрати місто зі списку (наприклад, «Львів»), переконатися в автопідстановці координат (`49.8397`, `24.0297`), натиснути «Зберегти місце знаходження» та перевірити, що після перезавантаження пристрою вибране місто й координати відображаються на сторінці (`SC-001`, `SC-002`, `SC-005`).

### Implementation for User Story 1

- [X] T006 [US1] Implement `GET /api/location` handler and extend `GET /api/status` (and `GET /api/health`) JSON response with the `"location"` object (`configured`, `name`, `latitude`, `longitude`) (`FR-003`, `FR-008`) in `components/web_server/web_server.c`
- [X] T007 [US1] Implement `POST /api/location` URI handler invoking `web_server_parse_location_json`, `config_store_save_location`, and updating `app_core` runtime location (`FR-006`, `FR-008`) in `components/web_server/web_server.c`
- [X] T008 [P] [US1] Add dedicated «Місце знаходження» card section with Ukrainian settlement `<select>`, settlement name input, latitude/longitude inputs, status badge, and «Зберегти місце знаходження» button (`FR-001`) in `components/web_server/assets/index.html`
- [X] T009 [P] [US1] Add responsive CSS styling for the location configuration card, two-column coordinate inputs, and status feedback messages in `components/web_server/assets/style.css`
- [X] T010 [US1] Embed offline Ukrainian settlements directory (regional centers, Kyiv, Sevastopol, Simferopol, major cities + manual option), implement coordinate auto-fill on city selection, load saved location from `/api/status`, and submit `POST /api/location` (`FR-001`, `FR-002`, `FR-003`, `FR-009`) in `components/web_server/assets/app.js`

**Checkpoint**: User Story 1 повністю функціональна та може бути протестована незалежно (MVP вибору та збереження населеного пункту України)

---

## Phase 4: User Story 2 - Оновлення місця знаходження без повторного налаштування Wi-Fi (Priority: P2)

**Goal**: Користувач на пристрої з уже налаштованим Wi-Fi змінює населений пункт або вручну вводить/коригує назву населеного пункту України та його координати й зберігає їх окремою кнопкою без розриву Wi-Fi з'єднання та без повторного введення пароля мережі.

**Independent Test**: На підключеному до роутера пристрої відкрити `http://weather-clock.local/`, обрати інше місто або вибрати ручний ввід і вказати власний населений пункт України та координати, натиснути «Зберегти місце знаходження» і переконатися, що `sta.connected` залишилося `true` без перепідключення до роутера (`SC-004`).

### Implementation for User Story 2

- [X] T011 [US2] Ensure `config_store_save_location` in `components/config_store/config_store.c` and `POST /api/location` in `components/web_server/web_server.c` commit only location NVS keys while preserving `sta_*`/`ap_password` and never triggering `wifi_manager` state transitions (`FR-007`, `SC-004`)
- [X] T012 [P] [US2] Implement manual settlement name and coordinate override logic in `components/web_server/assets/app.js` (automatically switching `<select>` to manual mode when user edits name/latitude/longitude and updating location UI state independently of the Wi-Fi form) (`FR-002`, `FR-007`)
- [X] T013 [US2] Synchronize factory reset handling across `POST /api/wifi/reset` UI feedback in `components/web_server/assets/app.js` and `BOOT` button long-press callback in `components/app_core/app_core.c` so location state resets to unconfigured alongside network settings (`FR-010`)

**Checkpoint**: User Stories 1 та 2 працюють разом — локація оновлюється та уточнюється вручну абсолютно незалежно від Wi-Fi з'єднання

---

## Phase 5: User Story 3 - Валідація некоректних значень місця знаходження (Priority: P2)

**Goal**: Система перевіряє назву населеного пункту та географічні координати перед збереженням, відхиляє некоректні або неповні дані з інформативним повідомленням про помилку та захищає попередні збережені налаштування від перезапису.

**Independent Test**: Спробувати зберегти порожню назву, рядок лише з пробілів, назву довшу за 64 байти або координати поза межами (`latitude = 105`, `longitude = -200`) через веб-форму або `POST /api/location`; переконатися у поверненні `HTTP 400 Bad Request`, появі повідомлення про помилку та збереженні попередніх валідних даних (`SC-003`).

### Implementation for User Story 3

- [X] T014 [US3] Implement strict validation in `config_store_validate_location` (UTF-8 byte length `1..64`, rejection of control chars `< 0x20` and whitespace-only strings, `isfinite(latitude)`, `isfinite(longitude)`, `-90.0 <= latitude <= 90.0`, `-180.0 <= longitude <= 180.0`) (`FR-004`, `FR-005`) in `components/config_store/config_store.c`
- [X] T015 [US3] Implement `web_server_parse_location_json` with strict `cJSON_IsString`/`cJSON_IsNumber` checks, deterministic `cJSON_Delete` cleanup on all execution paths, and `HTTP 400 Bad Request` (`INVALID_LOCATION`) error responses (`FR-004`, `FR-005`) in `components/web_server/web_server.c`
- [X] T016 [P] [US3] Implement client-side decimal comma-to-dot normalization (`50,4501` → `50.4501`), input bounds checking, and inline error message display without clearing prior valid state (`FR-004`, `FR-005`) in `components/web_server/assets/app.js`

**Checkpoint**: Усі три користувацькі історії реалізовано з повною серверною та клієнтською валідацією

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Модульне тестування Unity (Principle V Конституції), перевірка чистоти компіляції (`-Wall -Wextra`) та фінальна валідація

- [X] T017 [P] Add Unity unit tests for `config_store_validate_location` (valid Ukrainian UTF-8 names, boundary coordinates `-90`, `90`, `-180`, `180`, `0.0`, out-of-range values, `NaN`/`Inf`, empty/whitespace/overlong names), NVS `v1 → v2` migration, and factory reset in `test/unit/test_config_store.c`
- [X] T018 [P] Add Unity unit tests for `web_server_parse_location_json` (valid JSON, missing fields, string-instead-of-number coordinates, out-of-bounds coordinates, malformed JSON) in `test/unit/test_http_validation.c`
- [X] T019 Verify zero compiler warnings (`-Wall -Wextra`), FreeRTOS stack/heap watermark integrity, and end-to-end validation scenarios per `specs/002-location-config/quickstart.md` in `components/app_core/app_core.c`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: Не має залежностей — виконується першою (`T001`, `T002`).
- **Foundational (Phase 2)**: Залежить від Phase 1 — БЛОКУЄ всі користувацькі історії (`T003`–`T005`).
- **User Story 1 (Phase 3 - P1 MVP)**: Залежить від завершення Phase 2 (`T006`–`T010`).
- **User Story 2 (Phase 4 - P2)**: Залежить від Phase 2 та розширює UI/API з Phase 3 (`T011`–`T013`).
- **User Story 3 (Phase 5 - P2)**: Залежить від Phase 2; серверна валідація (`T014`, `T015`) та клієнтська валідація (`T016`) інтегруються з API та формою.
- **Polish (Phase 6)**: Виконується після завершення реалізації користувацьких історій (`T017`–`T019`).
