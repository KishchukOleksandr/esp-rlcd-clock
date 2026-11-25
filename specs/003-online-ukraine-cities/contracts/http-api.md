# Interface Contracts: Онлайн-пошук населених пунктів України

**Feature**: `003-online-ukraine-cities`  
**Date**: 2026-11-25  
**Base Contracts**: Розширює [specs/002-location-config/contracts/http-api.md](file:///d:/Pets/esp-rlcd-clock/specs/002-location-config/contracts/http-api.md) із повною зворотною сумісністю (`FR-006`).

---

## 1. REST API (ESP32 Web Server — `application/json`)

### 1.1 Розширення `GET /api/status` (та `GET /api/health`)

До існуючої JSON-відповіді додається верхньорівневий об'єкт `internet` (`FR-002`):

**Response `200 OK`**:
```json
{
  "fsm_state": "CONNECTED_STA",
  "sta": {
    "configured": true,
    "connected": true,
    "ssid": "Home_WiFi_2.4G",
    "ip": "192.168.1.45",
    "rssi": -56,
    "last_error": ""
  },
  "ap": {
    "ssid": "ESP Weather Clock",
    "ip": "192.168.4.1",
    "client_count": 1,
    "channel": 6
  },
  "internet": {
    "available": true,
    "checked": true,
    "checking": false
  },
  "location": {
    "configured": true,
    "name": "Київ",
    "latitude": 50.4501,
    "longitude": 30.5234
  },
  "system": {
    "uptime_sec": 1420,
    "free_heap_bytes": 268000,
    "min_free_heap_bytes": 195000,
    "mdns_host": "weather-clock.local"
  }
}
```

Стани об'єкта `internet`:
- SoftAP без STA або STA відключено: `{ "available": false, "checked": false, "checking": false }`
- STA підключено, триває первинна перевірка: `{ "available": false, "checked": false, "checking": true }`
- STA підключено, роутер без Інтернету: `{ "available": false, "checked": true, "checking": false }`
- STA підключено, Інтернет доступний: `{ "available": true, "checked": true, "checking": false }`

---

### 1.2 Новий маршрут `GET /api/location/search?q=<encoded_query>`

Виконує онлайн-пошук населених пунктів України через інтернет-з'єднання плати ESP32 (`FR-001`, `FR-003`, `FR-004`, `FR-008`, `FR-009`, `FR-011`).

**Query Parameters**:
- `q` *(required)*: URL-кодований рядок UTF-8 (назва або початок назви населеного пункту України; після декодування та trim: від `2` символів UTF-8 до `64` байтів, без керівних символів).

**Response `200 OK`** (знайдено 1..10 результатів):
```json
{
  "query": "Вишневе",
  "count": 2,
  "results": [
    {
      "name": "Вишневе",
      "admin1": "Київська область",
      "admin2": "Бучанський район",
      "latitude": 50.3891,
      "longitude": 30.3705
    },
    {
      "name": "Вишневе",
      "admin1": "Дніпропетровська область",
      "admin2": "Кам'янський район",
      "latitude": 48.4383,
      "longitude": 33.9175
    }
  ]
}
```

**Response `200 OK`** (нічого не знайдено по Україні):
```json
{
  "query": "НеіснуючеМісто",
  "count": 0,
  "results": []
}
```

**Response `400 Bad Request`** (порожній, коротший за 2 символи UTF-8, задовгий > 64 байти або невалідний запит — `FR-011`):
```json
{
  "error": "INVALID_QUERY",
  "message": "Введіть щонайменше 2 символи для пошуку населеного пункту."
}
```

**Response `429 Too Many Requests`** (інший онлайн-пошук уже виконується на ESP32 — `FR-011`):
```json
{
  "error": "SEARCH_BUSY",
  "message": "Пошук уже виконується. Зачекайте завершення поточного запиту."
}
```

**Response `503 Service Unavailable`** (пристрій не підключений до Wi-Fi / немає доступу до Інтернету / таймаут або збій зовнішнього сервісу — `FR-007`, `FR-008`):
```json
{
  "error": "OFFLINE",
  "message": "Онлайн-пошук недоступний без підключення пристрою до Інтернету. Скористайтеся офлайн-списком або ручним вводом."
}
```
Можливі коди помилок для `503`:
- `OFFLINE` — пристрій не підключений до зовнішньої мережі Wi-Fi або Інтернет недоступний.
- `SEARCH_TIMEOUT` — перевищено час очікування відповіді зовнішнього сервісу (`4.5 с`).
- `UPSTREAM_ERROR` — зовнішній сервіс тимчасово недоступний або повернув некоректну відповідь.

---

### 1.3 Існуючі маршрути без змін (`FR-006`, `FR-010`)

- `GET /api/location` — без змін.
- `POST /api/location` — без змін (приймає `name`, `latitude`, `longitude`, зберігає в NVS без розриву Wi-Fi).
- `POST /api/wifi/reset` — додатково скидає кешований стан `internet` у `false`.

---

## 2. Зовнішній контракт (ESP32 → Open-Meteo Geocoding API)

- **Endpoint**: `GET https://geocoding-api.open-meteo.com/v1/search?name=<percent_encoded_q>&countryCode=UA&language=uk&count=10`
- **Transport**: HTTPS (TLS із `esp_crt_bundle_attach`), таймаут `4500 мс`, `Accept-Encoding: identity` (без gzip, щоб мінімізувати RAM і складність парсингу).
- **Expected Fields**:
  - `results[]` (опціонально — відсутній, якщо збігів немає):
    - `name` (string)
    - `country_code` (string, перевіряється на `"UA"`)
    - `admin1` (string, опціонально)
    - `admin2` (string, опціонально)
    - `latitude` (number)
    - `longitude` (number)

---

## 3. Контракт веб-інтерфейсу (`components/web_server/assets/index.html`, `app.js`, `style.css`)

У картці **«Місце знаходження»**:
1. **Індикатор режиму (`#loc-online-badge`)**:
   - Якщо `status.internet.available === true`: зелений індикатор «Онлайн-пошук по Україні доступний».
   - Якщо `status.internet.checking === true`: нейтральний індикатор «Перевірка доступу до Інтернету…».
   - Інакше: приглушений індикатор «Офлайн-режим: доступний вбудований список та ручний ввід».
2. **Блок онлайн-пошуку (`#loc-online-search-box`)**:
   - Поле введення `#loc-search-input` (`maxlength="64"`, placeholder: `Введіть назву міста, селища чи села…`) + кнопка `#loc-search-btn` (**«Пошук»**).
   - Активний при `internet.available === true`; при `false` поле і кнопка неактивні (`disabled`) з підказкою про офлайн-режим.
   - Запуск пошуку — виключно по кліку на кнопку **«Пошук»** або натисканню `Enter` у `#loc-search-input` (`FR-011`).
   - Локальна перевірка перед відправкою: якщо після `trim()` довжина `< 2` символів, показується підказка без HTTP-запиту (`FR-011`).
   - Під час виконання запиту кнопка блокується та показується індикатор «Пошук…» (`FR-011`).
   - Список результатів `#loc-search-results`: кожен пункт відображає `name` та адміністративний контекст (`admin1`, і `admin2` якщо присутній і не дублює `admin1`).
   - При кліку на пункт списку:
     - у `#loc-name` підставляється **лише `name`** (`FR-005`),
     - у `#loc-lat` та `#loc-lon` підставляються `latitude` та `longitude` (округлені до 4–5 знаків або як отримано),
     - випадаючий офлайн-список `#loc-city-select` переводиться у стан ручного/обраного значення,
     - список результатів згортається з коротким підтвердженням, що координати підставлено і можна натиснути **«Зберегти місце знаходження»**.
3. **Офлайн-довідник, ручні поля та кнопка збереження**: залишаються повністю незмінними й завжди доступними (`FR-006`, `FR-007`).
