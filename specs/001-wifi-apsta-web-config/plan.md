# Implementation Plan: Суміщений режим Wi-Fi (AP+STA) та вбудована веб-сторінка налаштування

**Branch**: `001-wifi-apsta-web-config` | **Date**: 2026-11-10 | **Spec**: [spec.md](file:///d:/Pets/esp-rlcd-clock/specs/001-wifi-apsta-web-config/spec.md)

**Input**: Feature specification from `/specs/001-wifi-apsta-web-config/spec.md`

## Summary

Реалізація прошивки для плати **Waveshare ESP32-C6-DEV-KIT-N8**, яка забезпечує постійну роботу Wi-Fi у суміщеному режимі `WIFI_MODE_APSTA` (одночасна робота точки доступу `"ESP Weather Clock"` із паролем за замовчуванням `"weather1"` та Wi-Fi клієнта локальної мережі). Керування налаштуваннями клієнта, сканування мереж, перегляд діагностики та зміна пароля точки доступу здійснюються через вбудовану адаптивну веб-сторінку (`esp_http_server`), доступну одночасно як через точку доступу (`192.168.4.1` + Captive Portal), так і в локальній мережі роутера (за виданою DHCP IP-адресою та mDNS-іменем `http://weather-clock.local`). Налаштування зберігаються у NVS із підтримкою апаратного скидання до заводських параметрів утриманням кнопки `BOOT` (`GPIO9`).

## Technical Context

**Language/Version**: C11 (ESP-IDF v6.1)

**Primary Dependencies**: `esp_wifi`, `esp_netif`, `esp_event`, `esp_http_server`, `nvs_flash`, `cJSON`, `espressif/mdns`, FreeRTOS (`esp_timer`, `driver/gpio`)

**Storage**: ESP-IDF Non-Volatile Storage (`nvs_flash`), виділений простір імен `"wifi_cfg"` на 8MB Quad SPI Flash

**Testing**: Unity Test Framework (модульні тести для валідації конфігурації, переходів FSM та HTTP/JSON парсерів)

**Target Platform**: Waveshare ESP32-C6-DEV-KIT-N8 (32-bit RISC-V одноядерний @ 160 MHz, 512KB SRAM, 8MB Flash, 2.4 GHz Wi-Fi 6 / 802.11ax/b/g/n)

**Project Type**: Embedded Firmware (ESP-IDF CMake project з модульною архітектурою компонентів)

**Performance Goals**: Старт точки доступу `< 10 с` після подачі живлення (`SC-001`), повне завантаження веб-сторінки `< 3 с` у AP та LAN (`SC-002`), авто-перепідключення до збереженої мережі `< 30 с` (`SC-004`)

**Constraints**: 512KB SRAM (відсутність витоків пам'яті та динамічної фрагментації купи, статика веб-інтерфейсу у Flash через `EMBED_TXTFILES`), один фізичний радіомодуль 2.4 ГГц для `AP+STA` (призупинення фонових реконектів STA при `ap_client_count > 0`)

**Scale/Scope**: Одночасне обслуговування до 4 клієнтів на точці доступу SoftAP та паралельний доступ з локальної мережі, 5 модульних компонентів (`hal`, `config_store`, `wifi_manager`, `web_server`, `app_core`)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **I. Modular Architecture & HAL**: Драйвер апаратної кнопки скидання (`GPIO9` / `BOOT`) та системного таймера ізольовано у компоненті `hal`; логіка збереження конфігурації (`config_store`), мережевий автомат (`wifi_manager`) та HTTP-сервер (`web_server`) розділені чистими C-заголовками та придатні до модульного тестування з моками.
- [x] **II. Resilient Network & FSM**: Реалізовано явний подієво-орієнтований скінченний автомат (`AP_PROVISIONING_MODE`, `CONNECTING`, `CONNECTED_STA`, `DISCONNECTED`, `RECONNECT_BACKOFF`) з експоненційною затримкою та захистом активних сесій SoftAP (`ap_client_count > 0`) і підтримкою Captive Portal.
- [x] **III. Memory & Concurrency Safety (NON-NEGOTIABLE)**: Статичні файли веб-сторінки розміщуються у Flash (`EMBED_TXTFILES`); результати сканування та конфігурація використовують фіксовані буфери; усі об'єкти `cJSON` звільняються на всіх гілках виконання; спільний стан `wifi_runtime_status_t` захищений м'ютексом FreeRTOS; TWDT залишається активним.
- [x] **IV. Safe Config & NVS Persistence**: Усі вхідні параметри (SSID 1–32 байти, пароль STA 0 або 8–63 символи, пароль AP 8–63 символи) проходять сувору валідацію до запису в NVS; реалізовано версіонування схеми (`schema_ver = 1`), атомарний `nvs_commit()` та скидання до заводських значень (`"ESP Weather Clock"` / `"weather1"`).
- [x] **V. Testability, Observability & Diagnostics**: Кожен модуль використовує власний тег `ESP_LOGx`; веб-сервер надає діагностичний ендпоінт `GET /api/status` (та аліас `/api/health`) з `uptime_sec`, `free_heap_bytes`, `min_free_heap_bytes`, станом обох інтерфейсів та RSSI; логіка переходів FSM, валідації NVS та розбору JSON покривається тестами Unity.

## Project Structure

### Documentation (this feature)

```text
specs/001-wifi-apsta-web-config/
├── plan.md              # Цей файл (план реалізації)
├── research.md          # Результати дослідження Phase 0
├── data-model.md        # Модель даних, схема NVS та переходи FSM (Phase 1)
├── quickstart.md        # Інструкція зі збірки, прошивки та перевірки (Phase 1)
├── contracts/           # Контракти інтерфейсів (Phase 1)
│   └── http-api.md      # Специфікація HTTP маршрутів, Captive Portal та REST API
└── tasks.md             # Декомпозиція задач Phase 2 (створюється командою /speckit-tasks)
```

### Source Code (repository root)

```text
CMakeLists.txt
sdkconfig.defaults
partitions.csv
main/
├── CMakeLists.txt
├── idf_component.yml
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
│   │   └── config_store.h
│   └── config_store.c
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
│   │   └── web_server.h
│   ├── web_server.c
│   └── assets/
│       ├── index.html
│       ├── style.css
│       └── app.js
└── app_core/
    ├── CMakeLists.txt
    ├── include/
    │   └── app_core.h
    └── app_core.c
test/
└── unit/
    ├── CMakeLists.txt
    ├── test_config_store.c
    ├── test_wifi_fsm.c
    └── test_http_validation.c
```

**Structure Decision**: Обрано стандартну модульну архітектуру **ESP-IDF v6.1** з ізольованими компонентами у директорії `components/` (`hal`, `config_store`, `wifi_manager`, `web_server`, `app_core`) та директорією модульних тестів `test/unit/`, що безпосередньо реалізує вимоги **Principle I** Конституції проекту.

## Complexity Tracking

> Порушень Конституції проекту немає. Усі архітектурні рішення повністю відповідають п'яти базовим принципам.
