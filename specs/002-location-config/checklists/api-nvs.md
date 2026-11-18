# API & NVS Data Checklist: Поля налаштування поточного місця знаходження

**Purpose**: Pre-implementation requirements quality gate for HTTP REST API contracts, payload validation rules, and NVS persistence/migration (`schema_ver: 1 → 2`)
**Created**: 2026-11-18
**Feature**: [spec.md](file:///d:/Pets/esp-rlcd-clock/specs/002-location-config/spec.md) | [data-model.md](file:///d:/Pets/esp-rlcd-clock/specs/002-location-config/data-model.md) | [http-api.md](file:///d:/Pets/esp-rlcd-clock/specs/002-location-config/contracts/http-api.md)

**Note**: Цей чекліст згенеровано командою `/speckit-checklist` для перевірки якості, повноти та однозначності вимог до API та збереження даних у NVS перед початком реалізації.

## Requirement Completeness (Повнота вимог до API та NVS)

- [x] CHK001 Чи повністю визначено набір полів сутності `Location Settings` (ознака налаштованості, назва населеного пункту, широта та довгота) та їхні значення за замовчуванням у стані «не налаштовано»? [Completeness, Spec §Key Entities, Data Model §1]
- [x] CHK002 Чи задокументовано вимоги до всіх HTTP-маршрутів читання та запису місця знаходження (`GET /api/location`, `POST /api/location` та розширення `GET /api/status`) включно з кодами відповідей та структурою JSON? [Completeness, Spec §FR-003, Contract §2.1–§2.3]
- [x] CHK003 Чи визначено правила прямої міграції схеми NVS із версії `1` до версії `2` із гарантією збереження раніше налаштованих параметрів Wi-Fi (`sta_*`, `ap_password`)? [Completeness, Plan §Technical Context, Data Model §3]
- [x] CHK004 Чи специфіковано вимогу повного очищення збереженого місця знаходження разом із мережевими параметрами під час апаратного скидання кнопкою `BOOT` та виклику `POST /api/wifi/reset`? [Completeness, Spec §FR-010, Contract §2.4]

## Requirement Clarity & Precision (Чіткість та однозначність правил валідації)

- [x] CHK005 Чи однозначно квантифіковано допустимі діапазони географічних координат (`-90.0 <= latitude <= 90.0`, `-180.0 <= longitude <= 180.0`) включно з інклюзивністю граничних значень? [Clarity, Spec §FR-004, Spec §Edge Cases]
- [x] CHK006 Чи чітко розмежовано обмеження довжини поля назви населеного пункту (`loc_name`) у байтах UTF-8 (`1..64` байти) та у кількості кириличних символів для уникнення переповнення буфера? [Clarity, Spec §FR-004, Data Model §1]
- [x] CHK007 Чи явно сформульовано правила відхилення рядків назви населеного пункту, що містять керівні ASCII-символи (`< 0x20`) або складаються виключно з пробільних символів? [Clarity, Data Model §1, Contract §2.3]
- [x] CHK008 Чи визначено точність представлення координат при серіалізації у NVS (мікроградуси `int32_t`, множник `1e6`) та правила округлення при перетворенні між `double` і `int32_t`? [Clarity, Research §2, Data Model §1]

## Requirement Consistency (Узгодженість між артефактами)

- [x] CHK009 Чи узгоджена структура JSON-об'єкта `location` (`configured`, `name`, `latitude`, `longitude`) між відповідями `GET /api/status`, `GET /api/location` та `POST /api/location`? [Consistency, Contract §2.1–§2.3]
- [x] CHK010 Чи не суперечать вимоги незалежного збереження місця знаходження (`POST /api/location` без перепідключення Wi-Fi) правилам роботи скінченного автомата `wifi_manager`? [Consistency, Spec §FR-007, Plan §Constitution Check II]
- [x] CHK011 Чи узгоджені ключі NVS (`"loc_set"`, `"loc_name"`, `"loc_lat_u6"`, `"loc_lon_u6"`) з обмеженням довжини ключів ESP-IDF NVS (до 15 символів) та існуючим простором імен `"wifi_cfg"`? [Consistency, Data Model §1]

## Edge Case & Exception Coverage (Покриття граничних умов та помилок)

- [x] CHK012 Чи визначено поведінку системи та інваріант незмінності попередніх даних у NVS/RAM при отриманні некоректного JSON, відсутніх полів або нечислових типів для `latitude`/`longitude` у `POST /api/location`? [Edge Case, Spec §FR-005, Data Model §3]
- [x] CHK013 Чи враховано у вимогах валідації відхилення нескінченних або невизначених значень чисел з плаваючою комою (`NaN`, `Infinity`) та обробку нульових координат (`0.0, 0.0`) як валідних? [Edge Case, Spec §Edge Cases, Data Model §1]
- [x] CHK014 Чи визначено вимоги до обробки десяткового розділювача (нормалізація коми на крапку на боці клієнта та очікування числового типу JSON на боці сервера)? [Coverage, Spec §Edge Cases, Contract §1]
- [x] CHK015 Чи визначено поведінку при читанні пошкоджених або частково відсутніх ключів локації у NVS (безпечний відкат до стану `configured: false` без втрати працездатності)? [Recovery, Plan §Constitution Check IV, Gap]

## Non-Functional & Constitutional Requirements (Нефункціональні вимоги та пам'ять)

- [x] CHK016 Чи задокументовано вимоги до статичного розміщення довідника населених пунктів України у Flash та використання фіксованих буферів без динамічної фрагментації купи SRAM (512KB)? [Non-Functional, Plan §Constitution Check III, Research §1]
- [x] CHK017 Чи визначено вимоги щодо обов'язкового звільнення об'єктів `cJSON` на всіх гілках виконання HTTP-обробників та синхронізації доступу до кешованої конфігурації? [Non-Functional, Plan §Constitution Check III]
- [x] CHK018 Чи можна об'єктивно перевірити вимоги валідації NVS і JSON-парсера модулями модульного тестування (Unity) ізольовано від апаратних драйверів та мережі? [Measurability, Spec §SC-002, Spec §SC-003, Plan §Constitution Check V]

## Notes

- Усі 18 пунктів якості вимог підтверджені в `spec.md`, `data-model.md`, `contracts/http-api.md` та `tasks.md`.
