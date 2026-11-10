# Interface Contracts: Вбудований HTTP веб-сервер та REST API

**Feature**: `001-wifi-apsta-web-config`  
**Date**: 2026-11-10  
**Interfaces**:
- **SoftAP Interface**: `http://192.168.4.1` (або автоматичне перенаправлення Captive Portal)
- **STA (LAN) Interface**: `http://<router-assigned-ip>` або `http://weather-clock.local`
- **Authentication**: Відсутня (доступ відкритий у межах захищеної паролем точки доступу `"ESP Weather Clock"` та довіреної локальної мережі згідно з `FR-003`).

---

## 1. Веб-інтерфейс та Captive Portal Endpoints

### `GET /`
Повертає головну вбудовану веб-сторінку налаштувань (`text/html; charset=utf-8`), скомпільовану у Flash-пам'ять прошивки. Сторінка містить адаптивний інтерфейс (без зовнішніх CDN-залежностей, щоб працювати без доступу до Інтернету) для:
- Перегляду поточного статусу клієнтського підключення, IP-адреси в локальній мережі, RSSI, стану точки доступу та системної діагностики (`FR-004`, `FR-011`).
- Сканування та вибору Wi-Fi мережі зі списку або ручного введення SSID і пароля (`FR-005`).
- Зміни пароля точки доступу `"ESP Weather Clock"` (`FR-002`).

### Captive Portal Detection Routes (`GET /generate_204`, `GET /hotspot-detect.html`, `GET /connecttest.txt`, HTTP 404 Handler)
- При зверненні через інтерфейс точки доступу повертають `HTTP 302 Found` із заголовком `Location: http://192.168.4.1/`, автоматично відкриваючи вікно налаштування на смартфонах і комп'ютерах (`FR-003`).

---

## 2. REST API Endpoints (`application/json`)

### 2.1 `GET /api/status` (також доступний за аліасом `GET /api/health`)

Повертає поточний стан мережевих інтерфейсів та діагностичні метрики системи (`FR-004`, `FR-011`, Constitution Principle V).

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
  "system": {
    "uptime_sec": 1420,
    "free_heap_bytes": 284120,
    "min_free_heap_bytes": 241080,
    "mdns_host": "weather-clock.local"
  }
}
```

---

### 2.2 `GET /api/wifi/scan`

Запускає сканування доступних Wi-Fi мереж 2.4 ГГц та повертає список знайдених точок доступу, відсортованих за рівнем сигналу (`FR-005`).

**Response `200 OK`**:
```json
{
  "networks": [
    {
      "ssid": "Home_WiFi_2.4G",
      "rssi": -54,
      "channel": 6,
      "secure": true
    },
    {
      "ssid": "Guest_Net",
      "rssi": -78,
      "channel": 1,
      "secure": false
    }
  ]
}
```

---

### 2.3 `POST /api/wifi/connect`

Зберігає нові облікові дані клієнтської мережі (STA) у NVS та негайно ініціює підключення до вказаної мережі без перезавантаження пристрою (`FR-006`, `FR-007`, `FR-008`).

**Request Body**:
```json
{
  "ssid": "Home_WiFi_2.4G",
  "password": "mySecretPassword123"
}
```

**Правила валідації (`FR-006`)**:
- `ssid`: обов'язковий рядок, від `1` до `32` байтів.
- `password`: рядок довжиною `0` (для відкритих мереж) або від `8` до `63` ASCII символів.

**Response `200 OK`**:
```json
{
  "status": "connecting",
  "message": "Параметри збережено. Виконується підключення до Home_WiFi_2.4G..."
}
```

**Response `400 Bad Request`** (при порушенні правил валідації):
```json
{
  "error": "INVALID_INPUT",
  "message": "SSID має містити від 1 до 32 символів, а пароль — від 8 до 63 символів (або бути порожнім для відкритої мережі)."
}
```

---

### 2.4 `POST /api/wifi/ap-password`

Змінює пароль точки доступу `"ESP Weather Clock"`, зберігає його у NVS та оновлює конфігурацію SoftAP (`FR-002`, `FR-006`, `FR-007`).

**Request Body**:
```json
{
  "password": "newWeatherPassword1"
}
```

**Правила валідації (`FR-002`, `FR-006`)**:
- `password`: обов'язковий рядок довжиною від `8` до `63` друкованих ASCII символів.

**Response `200 OK`**:
```json
{
  "status": "updated",
  "message": "Пароль точки доступу 'ESP Weather Clock' успішно оновлено."
}
```

**Response `400 Bad Request`**:
```json
{
  "error": "INVALID_PASSWORD",
  "message": "Пароль точки доступу має містити від 8 до 63 символів."
}
```

---

### 2.5 `POST /api/wifi/reset`

Скидає налаштування мережі до заводських значень (відновлює пароль точки доступу `"weather1"` та видаляє збережену клієнтську мережу STA) — програмний аналог утримання кнопки `BOOT` на платі (`FR-012`).

**Response `200 OK`**:
```json
{
  "status": "reset",
  "message": "Налаштування скинуто до заводських (пароль точки доступу: weather1)."
}
```
