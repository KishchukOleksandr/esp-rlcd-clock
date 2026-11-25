# Tasks: Розширений онлайн-вибір будь-якого міста України

**Input**: Design documents from `/specs/003-online-ukraine-cities/`  
**Prerequisites**: [plan.md](file:///d:/Pets/esp-rlcd-clock/specs/003-online-ukraine-cities/plan.md), [spec.md](file:///d:/Pets/esp-rlcd-clock/specs/003-online-ukraine-cities/spec.md), [research.md](file:///d:/Pets/esp-rlcd-clock/specs/003-online-ukraine-cities/research.md), [data-model.md](file:///d:/Pets/esp-rlcd-clock/specs/003-online-ukraine-cities/data-model.md), [contracts/http-api.md](file:///d:/Pets/esp-rlcd-clock/specs/003-online-ukraine-cities/contracts/http-api.md), [quickstart.md](file:///d:/Pets/esp-rlcd-clock/specs/003-online-ukraine-cities/quickstart.md)

**Tests**: Включено модульні тести Unity відповідно до Принципу V Конституції проєкту (покриття парсерів, UTF-8 утиліт та валідації запитів).

**Organization**: Задачі згруповано за історіями користувача (User Stories) для незалежної реалізації та тестування кожного інкременту.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Може виконуватися паралельно (різні файли, немає залежностей від незавершених задач)
- **[Story]**: До якої історії користувача належить задача (`[US1]`, `[US2]`, `[US3]`)
- Усі задачі містять точні шляхи до файлів проєкту

## Path Conventions

- **Embedded ESP-IDF**: `main/`, `components/<module>/`, `test/unit/`, `sdkconfig.defaults` у корені репозиторію

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Підготовка конфігурації ESP-IDF та створення каркаса нового ізольованого компонента `geo_search`

- [X] T001 Enable `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y` for Open-Meteo HTTPS TLS verification in `sdkconfig.defaults`
- [X] T002 Create new `geo_search` component build file `components/geo_search/CMakeLists.txt` and public C header `components/geo_search/include/geo_search.h` defining `geo_search_item_t`, `geo_internet_status_t`, size/query constants (`GEO_SEARCH_MAX_RESULTS=10`, `GEO_QUERY_MIN_CHARS=2`, `GEO_QUERY_MAX_BYTES=64`, `GEO_ADMIN_MAX_LEN=64`), and public function prototypes
- [X] T003 [P] Register `geo_search` component dependency in `components/wifi_manager/CMakeLists.txt`, `components/web_server/CMakeLists.txt`, `components/app_core/CMakeLists.txt`, and `test/unit/CMakeLists.txt`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Базова неблокуюча інфраструктура перевірки Інтернету, UTF-8 утиліт і парсера Open-Meteo, необхідна для всіх історій користувача

**⚠️ CRITICAL**: Жодна історія користувача не може бути завершена до виконання цієї фази

- [X] T004 Implement pure UTF-8 and query helpers (`geo_search_utf8_char_count`, `geo_search_utf8_truncate`, `geo_search_validate_query`, `geo_search_url_encode`) in `components/geo_search/geo_search_parser.c`
- [X] T005 Implement pure Open-Meteo Geocoding JSON parser (`geo_search_parse_open_meteo_json`) with strict `country_code == "UA"` filtering, finite coordinate bounds checks (`-90..90`, `-180..180`), control-char sanitization, and UTF-8-safe 64-byte truncation in `components/geo_search/geo_search_parser.c`
- [X] T006 Implement `geo_search_init()`, mutex-protected `geo_internet_status_t` state (`geo_search_get_internet_status()`), `geo_search_notify_sta_connected()`, `geo_search_notify_sta_disconnected()`, and non-blocking FreeRTOS background task `geo_probe_task` (4096-byte stack, auxiliary task not subscribed to TWDT so 30 s `ulTaskNotifyTake` wait yields cleanly without triggering 10 s TWDT, DNS + TCP connect probe to `geocoding-api.open-meteo.com:443` with 2500 ms timeout and 30 s retry while STA connected without internet) in `components/geo_search/geo_search.c`
- [X] T007 Initialize `geo_search_init()` before `wifi_manager_init()` and log `geo_probe_task` stack high-water mark diagnostics in `components/app_core/app_core.c`
- [X] T008 Notify `geo_search_notify_sta_connected()` on `IP_EVENT_STA_GOT_IP` and `geo_search_notify_sta_disconnected()` on `WIFI_EVENT_STA_DISCONNECTED` and `wifi_manager_apply_factory_reset()` in `components/wifi_manager/wifi_manager.c`

**Checkpoint**: Фундамент готовий — фонова перевірка доступності Інтернету та чисті функції парсингу працюють, можна переходити до реалізації User Stories

---

## Phase 3: User Story 1 - Пошук та вибір будь-якого міста України за наявності доступу до Інтернету (Priority: P1) 🎯 MVP

**Goal**: Надати користувачу можливість шукати будь-яке місто/селище/село України через підключення ESP32 до Інтернету (явний запуск кнопкою «Пошук» / `Enter`, від 2 символів), автоматично підставляти назву та координати у форму й зберігати їх без розриву Wi-Fi

**Independent Test**: На платі ESP32 з активним доступом до Інтернету відкрити сторінку налаштувань, ввести назву невеликого міста України (наприклад, `Чортків`), натиснути «Пошук», обрати результат, зберегти локацію та перевірити відновлення назви і координат після перезавантаження

### Implementation for User Story 1

- [X] T009 [US1] Implement mutex-serialized HTTPS search client `geo_search_query_ukraine()` using `esp_http_client`, `esp_crt_bundle_attach`, static 12 KB response buffer, `Accept-Encoding: identity`, 4500 ms timeout, and `ESP_ERR_INVALID_STATE` when another search is active in `components/geo_search/geo_search.c`
- [X] T010 [US1] Increase `httpd_config.stack_size` to `8192` for mbedTLS safety and extend `GET /api/status` (and `GET /api/health`) JSON payload with `"internet": { "available", "checked", "checking" }` in `components/web_server/web_server.c`
- [X] T011 [US1] Implement `GET /api/location/search?q=...` route handler (query parameter extraction, URL decoding, UTF-8 validation, `400 INVALID_QUERY`, `429 SEARCH_BUSY`, `503` offline/timeout/upstream errors, `200 OK` JSON response) in `components/web_server/web_server.c`
- [X] T012 [P] [US1] Add online search availability badge (`#loc-online-badge`), search input (`#loc-search-input`), explicit «Пошук» button (`#loc-search-btn`), and results list container (`#loc-search-results`) to the Location card in `components/web_server/assets/index.html`
- [X] T013 [P] [US1] Add responsive styles for online/offline badge, search input row, loading indicator, and clickable search results list in `components/web_server/assets/style.css`
- [X] T014 [US1] Implement explicit online search interaction (button click and `Enter`, local >= 2 char check, in-flight button lock, pausing/deferring periodic `GET /api/status` polling while search is in-flight, `GET /api/location/search` fetch, populating `#loc-name` with settlement `name` only and `#loc-lat`/`#loc-lon` with coordinates on selection) in `components/web_server/assets/app.js`

**Checkpoint**: User Story 1 (MVP) повністю функціональна та незалежно тестована

---

## Phase 4: User Story 2 - Однозначний вибір серед міст і населених пунктів з однаковими назвами (Priority: P2)

**Goal**: Забезпечити відображення області (та за наявності району) для кожного знайденого населеного пункту України й суворе виключення закордонних локацій, щоб користувач міг безпомилково обрати потрібний населений пункт серед однойменних

**Independent Test**: Виконати онлайн-пошук за назвою `Вишневе` або `Олександрівка`, переконатися, що кожен варіант показує область (і район), закордонні результати відсутні, а вибір конкретного варіанта підставляє лише назву населеного пункту та саме його координати

### Implementation & Tests for User Story 2

- [X] T015 [US2] Ensure `GET /api/location/search` JSON serializer includes `admin1` and `admin2` strings alongside `name`, `latitude`, `longitude` for every returned Ukrainian settlement in `components/web_server/web_server.c`
- [X] T016 [US2] Render each item in `#loc-search-results` with primary settlement `name` and secondary administrative context (`admin1`, plus `admin2` when non-empty and distinct from `admin1`), keeping `#loc-name` populated with `name` only upon click in `components/web_server/assets/app.js`
- [X] T017 [P] [US2] Create Unity unit tests in `test/unit/test_geo_search.c` covering duplicate settlement disambiguation (`admin1`/`admin2` extraction), strict `country_code == "UA"` filtering (rejecting non-UA entries), and Ukrainian Cyrillic/apostrophe/hyphen city names (`Кам'янець-Подільський`, `Івано-Франківськ`, `Біла Церква`)

**Checkpoint**: User Stories 1 і 2 працюють разом і кожна незалежно тестована

---

## Phase 5: User Story 3 - Безперебійна робота в офлайн-режимі та при втраті доступу до Інтернету (Priority: P2)

**Goal**: Забезпечити коректну індикацію офлайн-стану (SoftAP або роутер без Інтернету), стійку обробку таймаутів/збоїв зовнішнього сервісу без зависань і втрати збережених даних, а також повну працездатність вбудованого офлайн-довідника й ручного вводу

**Independent Test**: Відкрити сторінку налаштувань у режимі SoftAP (без Інтернету) або на роутері без Інтернету, перевірити індикатор офлайн-режиму та успішно зберегти локацію з офлайн-довідника; також перевірити обробку помилок пошуку та граничних запитів

### Implementation & Tests for User Story 3

- [X] T018 [US3] Update `geo_search_query_ukraine()` in `components/geo_search/geo_search.c` to synchronize cached `geo_internet_status_t` on search success (`available = true`) or DNS/connect failure (`available = false`, waking background probe retry) and return distinct error codes for offline, timeout, and upstream errors
- [X] T019 [US3] Synchronize `#loc-online-badge` and enable/disable `#loc-search-input` and `#loc-search-btn` from `GET /api/status` (`internet.available`, `internet.checking`) while keeping `#loc-city-select`, manual fields, and «Зберегти місце знаходження» always operational in `components/web_server/assets/app.js`
- [X] T020 [US3] Add clear Ukrainian user-facing status/error messages for offline mode, search timeout, upstream failure, concurrent busy state, short query (< 2 chars), and zero Ukrainian matches without altering saved location fields in `components/web_server/assets/app.js`
- [X] T021 [P] [US3] Expand Unity unit tests in `test/unit/test_geo_search.c` for query validation edge cases (empty, spaces-only, 1 Cyrillic char = 2 UTF-8 bytes, > 64 bytes, control chars), RFC 3986 URL percent-encoding, UTF-8-safe 64-byte truncation, and empty/malformed/out-of-bounds Open-Meteo JSON responses

**Checkpoint**: Усі 3 історії користувача завершено та незалежно тестовано

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Перевірка якості коду, компіляція без попереджень, наскрізна валідація на платі та оновлення графа кодової бази

- [X] T022 [P] Add HTTP query decoding and bounds-safety checks for `GET /api/location/search` edge cases in `test/unit/test_http_validation.c`
- [X] T023 Build firmware for `esp32c6` (`idf.py build`) and verify zero compiler warnings under `-Wall -Wextra` and safe SRAM/Flash usage
- [ ] T024 Run quickstart validation scenarios A–D from `specs/003-online-ukraine-cities/quickstart.md` (including heap/stack diagnostics) and update knowledge graph with `graphify update .` (note: `graphify update .` completed; on-device scenarios A–D await flashing physical ESP32-C6 board)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: Не має залежностей — можна починати одразу (`T001` → `T002`; `T003` паралельно після `T002`).
- **Foundational (Phase 2)**: Залежить від завершення Phase 1 — **БЛОКУЄ** усі User Stories (`T004` → `T005`, `T006` паралельно з `T004`/`T005`, потім `T007` і `T008`).
- **User Story 1 (Phase 3 — P1 MVP)**: Залежить від Phase 2 (`T009` → `T010` → `T011`; `T012` і `T013` паралельно; `T014` після `T011`–`T013`).
- **User Story 2 (Phase 4 — P2)**: Залежить від Phase 2 (і розширює маршрути/UI з US1: `T015` після `T011`, `T016` після `T014`, `T017` повністю паралельна після `T005`).
- **User Story 3 (Phase 5 — P2)**: Залежить від Phase 2 (і розширює клієнт/UI з US1: `T018` після `T009`, `T019` і `T020` після `T014`, `T021` повністю паралельна після `T005`).
- **Polish (Phase 6)**: Залежить від завершення Phases 3–5.

### Parallel Opportunities

- У **Phase 1**: `T001` і `T002` стосуються різних файлів; `T003` — незалежні `CMakeLists.txt`.
- У **Phase 2**: `T004`/`T005` (`geo_search_parser.c`) можуть виконуватися паралельно з `T006` (`geo_search.c`).
- У **Phase 3 (US1)**: `T012` (`index.html`) і `T013` (`style.css`) виконуються паралельно з C-реалізацією `T009`–`T011`.
- У **Phase 4 & 5**: модульні тести `T017` і `T021` (`test/unit/test_geo_search.c`) можна писати паралельно з веб-інтерфейсом `T016`, `T019`, `T020`.

---

## Parallel Example: User Story 1

```text
# Паралельно з бекендом (T009–T011) можна оновлювати HTML і CSS:
Task T012: "Add online search badge, input, button, and results container in components/web_server/assets/index.html"
Task T013: "Add responsive styles for online search UI in components/web_server/assets/style.css"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Виконати **Phase 1: Setup** (`T001`–`T003`)
2. Виконати **Phase 2: Foundational** (`T004`–`T008`)
3. Виконати **Phase 3: User Story 1** (`T009`–`T014`)
4. **STOP & VALIDATE**: Перевірити онлайн-пошук невеликого міста України та збереження координат на ESP32-C6

### Incremental Delivery

1. Setup + Foundational → готова підсистема `geo_search` і фоновий моніторинг Інтернету
2. User Story 1 (MVP) → повноцінний онлайн-пошук будь-якого населеного пункту України
3. User Story 2 → розрізнення однойменних населених пунктів за областю/районом + Unity-тести
4. User Story 3 → стійкий офлайн-режим, синхронізація статусу Інтернету, зрозумілі повідомлення про помилки + Unity-тести граничних випадків
5. Polish → перевірка `-Wall -Wextra`, наскрізні тести `quickstart.md` та `graphify update .`
