# Tasks: Суміщений режим Wi-Fi (AP+STA) та вбудована веб-сторінка налаштування

**Input**: Design documents from `/specs/001-wifi-apsta-web-config/`  
**Prerequisites**: [plan.md](file:///d:/Pets/esp-rlcd-clock/specs/001-wifi-apsta-web-config/plan.md) (required), [spec.md](file:///d:/Pets/esp-rlcd-clock/specs/001-wifi-apsta-web-config/spec.md) (required), [research.md](file:///d:/Pets/esp-rlcd-clock/specs/001-wifi-apsta-web-config/research.md), [data-model.md](file:///d:/Pets/esp-rlcd-clock/specs/001-wifi-apsta-web-config/data-model.md), [contracts/http-api.md](file:///d:/Pets/esp-rlcd-clock/specs/001-wifi-apsta-web-config/contracts/http-api.md)

**Organization**: Задачі згруповано за користувацькими історіями (User Stories) для забезпечення незалежної реалізації та перевірки кожної історії.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Може виконуватися паралельно (різні файли, відсутність блокуючих залежностей від незавершених задач)
- **[Story]**: Ідентифікатор користувацької історії (`[US1]`, `[US2]`, `[US3]`)
- Кожна задача містить точний шлях до файлу у репозиторії

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Ініціалізація структури проекту ESP-IDF v5.x для плати Waveshare ESP32-C6-DEV-KIT-N8 (8MB Flash)

- [X] T001 Create ESP-IDF root build configuration with `-Wall -Wextra` compiler flags in `CMakeLists.txt`
- [X] T002 Create 8MB Flash partition table with dedicated `nvs` and `factory` partitions in `partitions.csv`
- [X] T003 [P] Configure ESP32-C6 target defaults (8MB Quad SPI Flash, `WIFI_MODE_APSTA`, `esp_http_server`, Task Watchdog) in `sdkconfig.defaults`
- [X] T004 [P] Create main component build configuration and `espressif/mdns` dependency manifest in `main/CMakeLists.txt` and `main/idf_component.yml`
- [X] T005 [P] Create modular component CMake build files in `components/hal/CMakeLists.txt`, `components/config_store/CMakeLists.txt`, `components/wifi_manager/CMakeLists.txt`, `components/web_server/CMakeLists.txt`, and `components/app_core/CMakeLists.txt`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Базова інфраструктура збереження налаштувань (NVS), апаратної абстракції (HAL) та каркасу мережевого менеджера, яка блокує виконання всіх користувацьких історій

**⚠️ CRITICAL**: Жодна користувацька історія не може бути розпочата до завершення цієї фази

- [X] T006 Define `wifi_stored_config_t` struct, default constants (`"ESP Weather Clock"`, `"weather1"`), schema version (`v1`), and validation API in `components/config_store/include/config_store.h`
- [X] T007 Implement NVS flash initialization, SSID/password bounds validation (`FR-006`), atomic read/write (`nvs_commit`), and factory reset in `components/config_store/config_store.c`
- [X] T008 [P] Define hardware `BOOT` button (`GPIO9`) abstraction interface and long-press callback contract in `components/hal/include/hal_button.h`
- [X] T009 [P] Implement non-blocking `GPIO9` button driver with debounce and `>= 3000 ms` hold detection for factory reset (`FR-012`) in `components/hal/hal_button.c`
- [X] T010 Define `wifi_fsm_state_t`, `wifi_runtime_status_t`, `wifi_scan_item_t`, and thread-safe Wi-Fi manager contract in `components/wifi_manager/include/wifi_manager.h`
- [X] T011 Implement base `esp_netif` (`WIFI_AP_DEF` + `WIFI_STA_DEF`), `esp_event` loop registration, and FreeRTOS mutex protection for `WIFI_MODE_APSTA` in `components/wifi_manager/wifi_manager.c`

**Checkpoint**: Базова платформа готова — можна розпочинати реалізацію користувацьких історій

---

## Phase 3: User Story 1 - Первинне підключення до точки доступу пристрою та відкриття веб-сторінки (Priority: P1) 🎯 MVP

**Goal**: Пристрій після подачі живлення піднімає точку доступу `"ESP Weather Clock"` із паролем `"weather1"`, видає IP-адреси клієнтам і відкриває вбудовану веб-сторінку налаштувань (напряму за `http://192.168.4.1` або через Captive Portal).

**Independent Test**: Увімкнути пристрій, підключитися зі смартфона/ПК до Wi-Fi мережі `"ESP Weather Clock"` (пароль `"weather1"`), переконатися у спрацюванні Captive Portal або відкрити `http://192.168.4.1/` і побачити веб-сторінку зі станом пристрою та формою налаштування.

### Implementation for User Story 1

- [X] T012 [US1] Implement SoftAP configuration (`"ESP Weather Clock"`, password from `config_store` defaulting to `"weather1"`, WPA2/WPA3, IP `192.168.4.1/24`) and `AP_STACONNECTED`/`AP_STADISCONNECTED` client counter in `components/wifi_manager/wifi_manager.c`
- [X] T013 [P] [US1] Define non-blocking Captive Portal UDP DNS responder interface in `components/wifi_manager/include/captive_dns.h`
- [X] T014 [P] [US1] Implement lightweight UDP DNS responder on `192.168.4.1:53` resolving wildcard A-queries to `192.168.4.1` in `components/wifi_manager/captive_dns.c`
- [X] T015 [P] [US1] Create responsive embedded HTML configuration page layout (status card, Wi-Fi scan/connect form, AP password form) in `components/web_server/assets/index.html`
- [X] T016 [P] [US1] Create embedded CSS stylesheet for mobile and desktop viewports in `components/web_server/assets/style.css`
- [X] T017 [US1] Define embedded HTTP server lifecycle and URI handler registration contract in `components/web_server/include/web_server.h`
- [X] T018 [US1] Implement `esp_http_server` on `0.0.0.0:80` serving embedded static assets (`GET /`, `style.css`, `app.js`), Captive Portal detection redirects (`/generate_204`, `/hotspot-detect.html`, `/connecttest.txt`, HTTP 404 redirect), and diagnostic status endpoint (`GET /api/status` and `GET /api/health`) in `components/web_server/web_server.c`

**Checkpoint**: User Story 1 повністю функціональна та може бути протестована незалежно (MVP точкового підключення)

---

## Phase 4: User Story 2 - Налаштування клієнтського підключення до локальної Wi-Fi мережі (Priority: P1)

**Goal**: Користувач через вбудовану веб-сторінку сканує доступні мережі або вводить SSID і пароль вручну, зберігає їх у NVS та підключає пристрій до роутера без перезавантаження і без вимкнення точки доступу `"ESP Weather Clock"`, а також може змінити пароль точки доступу чи скинути налаштування кнопкою `BOOT`.

**Independent Test**: На веб-сторінці обрати домашню Wi-Fi мережу, ввести пароль та натиснути «Зберегти»; переконатися, що пристрій отримує IP-адресу від роутера, зберігає налаштування після перезавантаження, а при невірному паролі або втраті роутера точка доступу `"ESP Weather Clock"` залишається безперервно доступною без розривів сесії.

### Implementation for User Story 2

- [X] T019 [US2] Implement 2.4 GHz Wi-Fi scan routine (`wifi_manager_scan`) and STA FSM transitions (`CONNECTING`, `CONNECTED_STA`, `DISCONNECTED`, `RECONNECT_BACKOFF`) with exponential backoff and background reconnect pause when `ap_client_count > 0` (`FR-010`) in `components/wifi_manager/wifi_manager.c`
- [X] T020 [US2] Implement `GET /api/wifi/scan`, `POST /api/wifi/connect`, `POST /api/wifi/ap-password`, and `POST /api/wifi/reset` JSON handlers with strict input validation and deterministic `cJSON` memory cleanup in `components/web_server/web_server.c`
- [X] T021 [P] [US2] Implement frontend JavaScript client logic for network scanning, STA form submission, AP password update, and error message display in `components/web_server/assets/app.js`
- [X] T022 [US2] Implement application coordinator wiring `config_store`, `hal_button` long-press factory reset (`FR-012`), `wifi_manager`, and `web_server` in `components/app_core/include/app_core.h` and `components/app_core/app_core.c`
- [X] T023 [US2] Implement firmware entry point `app_main` initializing `app_core` and Task Watchdog Timer (TWDT) monitoring in `main/main.c`

**Checkpoint**: User Stories 1 та 2 працюють разом — пристрій повноцінно налаштовується та працює в режимі `WIFI_MODE_APSTA`

---

## Phase 5: User Story 3 - Доступ до веб-сторінки налаштувань через локальну мережу (Priority: P2)

**Goal**: Забезпечити одночасний та зручний доступ до вбудованої веб-сторінки з локальної мережі роутера як за отриманою DHCP IP-адресою, так і за локальним mDNS-іменем `http://weather-clock.local`.

**Independent Test**: Підключитися смартфоном або комп'ютером до домашнього роутера та відкрити у браузері локальну IP-адресу пристрою або `http://weather-clock.local/`; переконатися, що веб-сторінка налаштувань коректно завантажується паралельно з інтерфейсом точки доступу.

### Implementation for User Story 3

- [X] T024 [US3] Implement mDNS service initialization and hostname advertisement (`weather-clock.local`, service `_http._tcp` port 80) upon STA IP acquisition and SoftAP startup in `components/wifi_manager/wifi_manager.c`
- [X] T025 [US3] Refine HTTP request host/interface handling so requests arriving on the STA LAN IP or `weather-clock.local` serve the web UI and REST API directly without Captive Portal redirects to `192.168.4.1` in `components/web_server/web_server.c`
- [X] T026 [P] [US3] Update frontend UI to display both SoftAP (`192.168.4.1`) and LAN (`sta.ip` / `http://weather-clock.local`) access URLs, RSSI indicator, and system health telemetry in `components/web_server/assets/index.html` and `components/web_server/assets/app.js`

**Checkpoint**: Усі три користувацькі історії реалізовано та можна перевірити незалежно на обох мережевих інтерфейсах

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Модульне тестування (відповідно до Principle V Конституції), перевірка пам'яті/стеків FreeRTOS та фінальна валідація

- [X] T027 [P] Create Unity unit test component build configuration in `test/unit/CMakeLists.txt`
- [X] T028 [P] Implement unit tests for SSID/password bounds validation (`FR-006`), NVS schema defaults, and factory reset in `test/unit/test_config_store.c`
- [X] T029 [P] Implement unit tests for Wi-Fi FSM state transitions and `ap_client_count > 0` background reconnect pause (`FR-010`) in `test/unit/test_wifi_fsm.c`
- [X] T030 [P] Implement unit tests for HTTP JSON payload parsing and malformed payload rejection in `test/unit/test_http_validation.c`
- [X] T031 Add FreeRTOS stack high-water mark diagnostics (`uxTaskGetStackHighWaterMark`) and verify zero compiler warnings and end-to-end scenarios per `specs/001-wifi-apsta-web-config/quickstart.md` in `components/app_core/app_core.c`
