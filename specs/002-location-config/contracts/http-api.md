# Interface Contracts: API налаштування поточного місця знаходження

**Feature**: `002-location-config`  
**Date**: 2026-11-18  
**Interfaces**:
- **SoftAP Interface**: `http://192.168.4.1` (працює офлайн без доступу до Інтернету — `FR-009`, `SC-005`)
- **STA (LAN) Interface**: `http://<router-assigned-ip>` або `http://weather-clock.local`

---

## 1. Веб-інтерфейс (`GET /`)

Головна сторінка налаштувань (`components/web_server/assets/index.html`) доповнюється окремою карткою/секцією **«Місце знаходження»**:
- Випадаючий список (`<select>`) із вбудованим переліком населених пунктів України та опцією ручного вводу (`FR-001`).
- Поле введення/редагування назви населеного пункту (`type="text"`, `maxlength="64"`).
- Поля введення/редагування широти (`Latitude`, діапазон `-90`..`90`, крок `any`) та довготи (`Longitude`, діапазон `-180`..`180`, крок `any`) з автоматичною підстановкою при виборі міста зі списку та автоматичною заміною коми на крапку перед відправкою (`FR-002`).
- Окрема кнопка **«Зберегти місце знаходження»**, яка відправляє `POST /api/location` без зачіпання форми Wi-Fi (`FR-001`, `FR-007`).

---

## 2. REST API Endpoints (`application/json`)

### 2.1 Розширення `GET /api/status` (та `GET /api/health`)

До існуючої відповіді `GET /api/status` додається об'єкт `location` (`FR-003`, `FR-008`):

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
  "location": {
    "configured": true,
    "name": "Київ",
    "latitude": 50.4501,
    "longitude": 30.5234
  },
  "system": {
    "uptime_sec": 1420,
    "free_heap_bytes": 284000,
    "min_free_heap_bytes": 241000,
    "mdns_host": "weather-clock.local"
  }
}
```

Якщо місце знаходження ще не налаштовано (`configured: false`):
```json
  "location": {
    "configured": false,
    "name": "",
    "latitude": 0.0,
    "longitude": 0.0
  }
```

---

### 2.2 `GET /api/location`

Повертає поточні збережені параметри місця знаходження (`FR-003`, `FR-008`).

**Response `200 OK`**:
```json
{
  "configured": true,
  "name": "Львів",
  "latitude": 49.8397,
  "longitude": 24.0297
}
```

---

### 2.3 `POST /api/location`

Валідує та зберігає нове поточне місце знаходження у NVS (`"wifi_cfg"`) без перепідключення Wi-Fi (`FR-004`, `FR-005`, `FR-006`, `FR-007`).

**Request Body**:
```json
{
  "name": "Київ",
  "latitude": 50.4501,
  "longitude": 30.5234
}
```

**Правила валідації (`FR-004`)**:
- `name`: обов'язковий непорожній рядок (`1..64` байти UTF-8), без керівних символів (`< 0x20`), не може складатися лише з пробілів.
- `latitude`: обов'язкове число (JSON number) у діапазоні `-90.0 <= latitude <= 90.0`.
- `longitude`: обов'язкове число (JSON number) у діапазоні `-180.0 <= longitude <= 180.0`.

**Response `200 OK`**:
```json
{
  "status": "saved",
  "message": "Місце знаходження успішно збережено.",
  "location": {
    "configured": true,
    "name": "Київ",
    "latitude": 50.4501,
    "longitude": 30.5234
  }
}
```

**Response `400 Bad Request`** (при порушенні правил валідації — `FR-005`):
```json
{
  "error": "INVALID_LOCATION",
  "message": "Вкажіть назву населеного пункту (1–64 символи), широту (-90..90) та довготу (-180..180)."
}
```

---

### 2.4 Вплив на `POST /api/wifi/reset` та апаратне скидання `BOOT` (`FR-010`)

Виклик `POST /api/wifi/reset` або утримання апаратної кнопки `BOOT` (`GPIO9`) протягом $\ge 3$ секунд додатково очищує збережене місце знаходження (`configured: false`, `name: ""`, `latitude: 0.0`, `longitude: 0.0`).
