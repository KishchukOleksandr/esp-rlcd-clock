(function () {
  "use strict";

  /* Вбудований офлайн-довідник населених пунктів України (002-location-config: FR-001, FR-002, FR-009) */
  const UA_SETTLEMENTS = [
    { name: "Біла Церква", lat: 49.7956, lon: 30.1167 },
    { name: "Бердянськ", lat: 46.7598, lon: 36.7989 },
    { name: "Бориспіль", lat: 50.3516, lon: 30.9508 },
    { name: "Бровари", lat: 50.5114, lon: 30.7903 },
    { name: "Буча", lat: 50.5434, lon: 30.2120 },
    { name: "Вінниця", lat: 49.2331, lon: 28.4682 },
    { name: "Дніпро", lat: 48.4647, lon: 35.0462 },
    { name: "Донецьк", lat: 48.0159, lon: 37.8028 },
    { name: "Дрогобич", lat: 49.3500, lon: 23.5000 },
    { name: "Житомир", lat: 50.2547, lon: 28.6587 },
    { name: "Запоріжжя", lat: 47.8388, lon: 35.1396 },
    { name: "Івано-Франківськ", lat: 48.9226, lon: 24.7111 },
    { name: "Ізмаїл", lat: 45.3500, lon: 28.8333 },
    { name: "Ірпінь", lat: 50.5218, lon: 30.2506 },
    { name: "Кам'янець-Подільський", lat: 48.6788, lon: 26.5852 },
    { name: "Кам'янське", lat: 48.5231, lon: 34.6136 },
    { name: "Київ", lat: 50.4501, lon: 30.5234 },
    { name: "Ковель", lat: 51.2167, lon: 24.7167 },
    { name: "Коломия", lat: 48.5306, lon: 25.0403 },
    { name: "Конотоп", lat: 51.2400, lon: 33.2000 },
    { name: "Краматорськ", lat: 48.7386, lon: 37.5842 },
    { name: "Кременчук", lat: 49.0685, lon: 33.4204 },
    { name: "Кривий Ріг", lat: 47.9105, lon: 33.3918 },
    { name: "Кропивницький", lat: 48.5079, lon: 32.2623 },
    { name: "Луганськ", lat: 48.5740, lon: 39.3078 },
    { name: "Луцьк", lat: 50.7472, lon: 25.3254 },
    { name: "Львів", lat: 49.8397, lon: 24.0297 },
    { name: "Маріуполь", lat: 47.0971, lon: 37.5434 },
    { name: "Мелітополь", lat: 46.8489, lon: 35.3653 },
    { name: "Миколаїв", lat: 46.9750, lon: 31.9946 },
    { name: "Мукачево", lat: 48.4414, lon: 22.7178 },
    { name: "Нікополь", lat: 47.5719, lon: 34.3964 },
    { name: "Одеса", lat: 46.4825, lon: 30.7233 },
    { name: "Олександрія", lat: 48.6667, lon: 33.1167 },
    { name: "Павлоград", lat: 48.5342, lon: 35.8700 },
    { name: "Полтава", lat: 49.5883, lon: 34.5514 },
    { name: "Рівне", lat: 50.6199, lon: 26.2516 },
    { name: "Севастополь", lat: 44.6166, lon: 33.5254 },
    { name: "Сімферополь", lat: 44.9521, lon: 34.1024 },
    { name: "Слов'янськ", lat: 48.8533, lon: 37.6250 },
    { name: "Суми", lat: 50.9077, lon: 34.7981 },
    { name: "Тернопіль", lat: 49.5535, lon: 25.5948 },
    { name: "Ужгород", lat: 48.6208, lon: 22.2879 },
    { name: "Умань", lat: 48.7484, lon: 30.2218 },
    { name: "Харків", lat: 49.9935, lon: 36.2304 },
    { name: "Херсон", lat: 46.6354, lon: 32.6169 },
    { name: "Хмельницький", lat: 49.4230, lon: 26.9871 },
    { name: "Черкаси", lat: 49.4444, lon: 32.0598 },
    { name: "Чернівці", lat: 48.2915, lon: 25.9403 },
    { name: "Чернігів", lat: 51.4982, lon: 31.2893 },
    { name: "Шостка", lat: 51.8631, lon: 33.4697 }
  ];

  const MANUAL_CITY_VALUE = "__manual__";

  const fsmBadge = document.getElementById("fsm-badge");
  const errorBanner = document.getElementById("error-banner");
  const staConnectedEl = document.getElementById("sta-connected");
  const staSsidEl = document.getElementById("sta-ssid");
  const staIpEl = document.getElementById("sta-ip");
  const mdnsLinkEl = document.getElementById("mdns-link");
  const staRssiEl = document.getElementById("sta-rssi");
  const apSsidEl = document.getElementById("ap-ssid");
  const apIpEl = document.getElementById("ap-ip");
  const apClientsEl = document.getElementById("ap-clients");
  const apChannelEl = document.getElementById("ap-channel");
  const sysUptimeEl = document.getElementById("sys-uptime");
  const sysHeapEl = document.getElementById("sys-heap");
  const sysMinHeapEl = document.getElementById("sys-min-heap");

  const btnRefresh = document.getElementById("btn-refresh-status");
  const btnScan = document.getElementById("btn-scan");
  const scanContainer = document.getElementById("scan-results-container");
  const scanList = document.getElementById("scan-list");

  const formSta = document.getElementById("form-sta-connect");
  const inputStaSsid = document.getElementById("input-sta-ssid");
  const inputStaPass = document.getElementById("input-sta-password");
  const staFeedback = document.getElementById("sta-form-feedback");
  const btnStaSubmit = document.getElementById("btn-sta-submit");

  const formLocation = document.getElementById("form-location");
  const selectUaCity = document.getElementById("select-ua-city");
  const inputLocName = document.getElementById("input-loc-name");
  const inputLocLat = document.getElementById("input-loc-lat");
  const inputLocLon = document.getElementById("input-loc-lon");
  const locStatusBadge = document.getElementById("loc-status-badge");
  const locFeedback = document.getElementById("loc-form-feedback");
  const btnLocSubmit = document.getElementById("btn-loc-submit");

  const locOnlineBadge = document.getElementById("loc-online-badge");
  const locSearchInput = document.getElementById("loc-search-input");
  const locSearchBtn = document.getElementById("loc-search-btn");
  const locSearchHint = document.getElementById("loc-search-hint");
  const locSearchFeedback = document.getElementById("loc-search-feedback");
  const locSearchResults = document.getElementById("loc-search-results");

  const formAp = document.getElementById("form-ap-password");
  const inputApPass = document.getElementById("input-ap-password");
  const apFeedback = document.getElementById("ap-form-feedback");
  const btnApSubmit = document.getElementById("btn-ap-submit");
  const btnFactoryReset = document.getElementById("btn-factory-reset");

  let locFormDirty = false;
  let onlineSearchAvailable = false;
  let onlineSearchInFlight = false;

  function showAlert(el, message, isError) {
    el.textContent = message;
    el.className = "alert " + (isError ? "alert-error" : "alert-success");
  }

  function hideAlert(el) {
    el.textContent = "";
    el.className = "alert hidden";
  }

  function getUtf8ByteLength(str) {
    if (typeof TextEncoder !== "undefined") {
      return new TextEncoder().encode(str).length;
    }
    return unescape(encodeURIComponent(str)).length;
  }

  function initSettlementDropdown() {
    if (!selectUaCity) {
      return;
    }
    UA_SETTLEMENTS.forEach(function (city, idx) {
      const opt = document.createElement("option");
      opt.value = String(idx);
      opt.textContent = city.name + " (" + city.lat.toFixed(4) + ", " + city.lon.toFixed(4) + ")";
      selectUaCity.appendChild(opt);
    });
    const manualOpt = document.createElement("option");
    manualOpt.value = MANUAL_CITY_VALUE;
    manualOpt.textContent = "Інший населений пункт (ввести вручну)";
    selectUaCity.appendChild(manualOpt);
  }

  function findSettlementIndex(name, lat, lon) {
    for (let i = 0; i < UA_SETTLEMENTS.length; ++i) {
      const item = UA_SETTLEMENTS[i];
      if (item.name === name &&
          Math.abs(item.lat - lat) < 0.0005 &&
          Math.abs(item.lon - lon) < 0.0005) {
        return i;
      }
    }
    return -1;
  }

  function updateLocationBadge(configured, name) {
    if (!locStatusBadge) {
      return;
    }
    if (configured && name) {
      locStatusBadge.textContent = "Встановлено: " + name;
      locStatusBadge.className = "badge badge-success";
    } else {
      locStatusBadge.textContent = "Не налаштовано";
      locStatusBadge.className = "badge badge-neutral";
    }
  }

  function syncLocationFormFromStatus(loc) {
    if (!loc) {
      return;
    }
    updateLocationBadge(Boolean(loc.configured), loc.name);

    if (locFormDirty) {
      return;
    }

    if (loc.configured && loc.name) {
      const latNum = Number(loc.latitude);
      const lonNum = Number(loc.longitude);
      inputLocName.value = loc.name;
      inputLocLat.value = Number.isFinite(latNum) ? String(Number(latNum.toFixed(6))) : "0";
      inputLocLon.value = Number.isFinite(lonNum) ? String(Number(lonNum.toFixed(6))) : "0";

      const matchIdx = findSettlementIndex(loc.name, latNum, lonNum);
      selectUaCity.value = matchIdx >= 0 ? String(matchIdx) : MANUAL_CITY_VALUE;
    } else {
      selectUaCity.value = "";
      inputLocName.value = "";
      inputLocLat.value = "";
      inputLocLon.value = "";
    }
  }

  function formatUptime(seconds) {
    const s = Number(seconds) || 0;
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const rem = s % 60;
    if (h > 0) {
      return h + " год " + m + " хв " + rem + " с";
    }
    if (m > 0) {
      return m + " хв " + rem + " с";
    }
    return rem + " с";
  }

  function formatBytes(bytes) {
    const b = Number(bytes) || 0;
    return (b / 1024).toFixed(1) + " КБ (" + b + " Б)";
  }

  function updateFsmBadge(state, connected) {
    if (connected || state === "CONNECTED_STA") {
      fsmBadge.textContent = "Підключено до роутера (AP+STA)";
      fsmBadge.className = "badge badge-success";
    } else if (state === "CONNECTING" || state === "RECONNECT_BACKOFF") {
      fsmBadge.textContent = "Підключення до роутера…";
      fsmBadge.className = "badge badge-warn";
    } else {
      fsmBadge.textContent = "Точка доступу активна";
      fsmBadge.className = "badge badge-neutral";
    }
  }

  function updateInternetUi(internet) {
    if (!locOnlineBadge || !locSearchInput || !locSearchBtn) {
      return;
    }

    const available = Boolean(internet && internet.available);
    const checking = Boolean(internet && internet.checking);
    onlineSearchAvailable = available;

    if (available) {
      locOnlineBadge.textContent = "Онлайн-пошук по Україні доступний";
      locOnlineBadge.className = "badge badge-success";
      locSearchInput.disabled = onlineSearchInFlight;
      locSearchBtn.disabled = onlineSearchInFlight;
      if (locSearchHint) {
        locSearchHint.textContent = "Введіть назву міста, селища чи села (від 2 символів) і натисніть «Пошук» або Enter.";
      }
    } else if (checking) {
      locOnlineBadge.textContent = "Перевірка доступу до Інтернету…";
      locOnlineBadge.className = "badge badge-warn";
      locSearchInput.disabled = true;
      locSearchBtn.disabled = true;
      if (locSearchHint) {
        locSearchHint.textContent = "Перевіряємо підключення пристрою до Інтернету… Офлайн-список і ручний ввід доступні нижче.";
      }
    } else {
      locOnlineBadge.textContent = "Офлайн-режим: вбудований список і ручний ввід";
      locOnlineBadge.className = "badge badge-neutral";
      locSearchInput.disabled = true;
      locSearchBtn.disabled = true;
      if (locSearchHint) {
        locSearchHint.textContent = "Розширений онлайн-пошук недоступний без Інтернету на платі ESP32. Скористайтеся списком або ручним вводом нижче.";
      }
    }
  }

  function clearOnlineSearchResults() {
    if (!locSearchResults) {
      return;
    }
    locSearchResults.innerHTML = "";
    locSearchResults.classList.add("hidden");
  }

  function formatAdminContext(item) {
    const parts = [];
    if (item.admin1) {
      parts.push(item.admin1);
    }
    if (item.admin2 && item.admin2 !== item.admin1) {
      parts.push(item.admin2);
    }
    if (Number.isFinite(Number(item.latitude)) && Number.isFinite(Number(item.longitude))) {
      parts.push(Number(item.latitude).toFixed(4) + ", " + Number(item.longitude).toFixed(4));
    }
    return parts.join(" • ");
  }

  async function runOnlineLocationSearch() {
    if (!locSearchInput || !locSearchBtn || onlineSearchInFlight) {
      return;
    }

    hideAlert(locSearchFeedback);
    clearOnlineSearchResults();

    if (!onlineSearchAvailable) {
      showAlert(locSearchFeedback, "Онлайн-пошук недоступний без підключення пристрою до Інтернету. Скористайтеся офлайн-списком або ручним вводом.", true);
      return;
    }

    const q = locSearchInput.value.trim();
    const charCount = Array.from(q).length;
    if (charCount < 2) {
      showAlert(locSearchFeedback, "Введіть щонайменше 2 символи для пошуку населеного пункту України.", true);
      return;
    }
    if (getUtf8ByteLength(q) > 64) {
      showAlert(locSearchFeedback, "Пошуковий запит занадто довгий (максимум 64 байти UTF-8).", true);
      return;
    }

    onlineSearchInFlight = true;
    locSearchInput.disabled = true;
    locSearchBtn.disabled = true;
    locSearchBtn.textContent = "Пошук…";

    try {
      const res = await fetch("/api/location/search?q=" + encodeURIComponent(q), { cache: "no-store" });
      const payload = await res.json();

      if (!res.ok) {
        showAlert(locSearchFeedback, payload.message || "Не вдалося виконати онлайн-пошук. Скористайтеся офлайн-списком або ручним вводом.", true);
        if (payload.error === "OFFLINE") {
          updateInternetUi({ available: false, checked: true, checking: false });
        }
        return;
      }

      const results = Array.isArray(payload.results) ? payload.results : [];
      if (results.length === 0) {
        showAlert(locSearchFeedback, "Населених пунктів України за запитом «" + q + "» не знайдено.", true);
        return;
      }

      locSearchResults.innerHTML = "";
      results.forEach(function (item) {
        const li = document.createElement("li");
        li.className = "search-result-item";
        li.setAttribute("role", "option");

        const nameEl = document.createElement("span");
        nameEl.className = "search-result-name";
        nameEl.textContent = item.name;

        const metaEl = document.createElement("span");
        metaEl.className = "search-result-meta";
        metaEl.textContent = formatAdminContext(item);

        li.appendChild(nameEl);
        li.appendChild(metaEl);

        li.addEventListener("click", function () {
          locFormDirty = true;
          inputLocName.value = item.name;
          inputLocLat.value = Number(item.latitude).toFixed(4);
          inputLocLon.value = Number(item.longitude).toFixed(4);
          const matchIdx = findSettlementIndex(item.name, Number(item.latitude), Number(item.longitude));
          selectUaCity.value = matchIdx >= 0 ? String(matchIdx) : MANUAL_CITY_VALUE;
          clearOnlineSearchResults();
          hideAlert(locFeedback);
          showAlert(locSearchFeedback, "Підставлено: " + item.name + (item.admin1 ? " (" + item.admin1 + ")" : "") + ". Натисніть «Зберегти місце знаходження».", false);
        });

        locSearchResults.appendChild(li);
      });

      locSearchResults.classList.remove("hidden");
    } catch (err) {
      showAlert(locSearchFeedback, "Помилка з'єднання під час онлайн-пошуку. Перевірте мережу або скористайтеся офлайн-списком.", true);
    } finally {
      onlineSearchInFlight = false;
      locSearchBtn.textContent = "Пошук";
      locSearchInput.disabled = !onlineSearchAvailable;
      locSearchBtn.disabled = !onlineSearchAvailable;
    }
  }

  async function fetchStatus() {
    if (onlineSearchInFlight) {
      return;
    }
    try {
      const res = await fetch("/api/status", { cache: "no-store" });
      if (!res.ok) {
        throw new Error("HTTP " + res.status);
      }
      const data = await res.json();

      updateFsmBadge(data.fsm_state, data.sta && data.sta.connected);

      if (data.sta) {
        staConnectedEl.textContent = data.sta.connected ? "Підключено" : "Відключено";
        staSsidEl.textContent = data.sta.ssid || "Не налаштовано";
        staIpEl.textContent = data.sta.ip || "0.0.0.0";
        staRssiEl.textContent = data.sta.connected ? data.sta.rssi + " dBm" : "—";

        if (data.sta.last_error) {
          showAlert(errorBanner, data.sta.last_error, true);
        } else {
          hideAlert(errorBanner);
        }

        if (!inputStaSsid.value && data.sta.ssid) {
          inputStaSsid.value = data.sta.ssid;
        }
      }

      if (data.ap) {
        apSsidEl.textContent = data.ap.ssid || "ESP Weather Clock";
        apIpEl.textContent = data.ap.ip || "192.168.4.1";
        apClientsEl.textContent = String(data.ap.client_count ?? 0);
        apChannelEl.textContent = String(data.ap.channel ?? 1);
      }

      if (data.location) {
        syncLocationFormFromStatus(data.location);
      }

      if (data.internet) {
        updateInternetUi(data.internet);
      } else {
        updateInternetUi({ available: false, checked: false, checking: false });
      }

      if (data.system) {
        sysUptimeEl.textContent = formatUptime(data.system.uptime_sec);
        sysHeapEl.textContent = formatBytes(data.system.free_heap_bytes);
        sysMinHeapEl.textContent = formatBytes(data.system.min_free_heap_bytes);
        if (data.system.mdns_host) {
          mdnsLinkEl.textContent = data.system.mdns_host;
          mdnsLinkEl.href = "http://" + data.system.mdns_host + "/";
        }
      }
    } catch (err) {
      fsmBadge.textContent = "Очікування зв'язку…";
      fsmBadge.className = "badge badge-warn";
    }
  }

  async function scanNetworks() {
    btnScan.disabled = true;
    btnScan.textContent = "Сканування…";
    scanContainer.classList.remove("hidden");
    scanList.innerHTML = "";

    const loadingItem = document.createElement("li");
    loadingItem.className = "scan-item";
    loadingItem.textContent = "Пошук доступних мереж 2.4 ГГц…";
    scanList.appendChild(loadingItem);

    try {
      const res = await fetch("/api/wifi/scan", { cache: "no-store" });
      const data = await res.json();
      scanList.innerHTML = "";

      const networks = Array.isArray(data.networks) ? data.networks : [];
      if (networks.length === 0) {
        const emptyItem = document.createElement("li");
        emptyItem.className = "scan-item";
        emptyItem.textContent = "Мереж не знайдено. Введіть SSID вручну.";
        scanList.appendChild(emptyItem);
      } else {
        networks.forEach(function (net) {
          const li = document.createElement("li");
          li.className = "scan-item";
          li.setAttribute("role", "option");

          const nameSpan = document.createElement("span");
          nameSpan.textContent = (net.secure ? "🔒 " : "🔓 ") + net.ssid;

          const metaSpan = document.createElement("span");
          metaSpan.textContent = net.rssi + " dBm (Ch " + net.channel + ")";

          li.appendChild(nameSpan);
          li.appendChild(metaSpan);

          li.addEventListener("click", function () {
            inputStaSsid.value = net.ssid;
            inputStaPass.focus();
          });

          scanList.appendChild(li);
        });
      }
    } catch (err) {
      scanList.innerHTML = "";
      const errItem = document.createElement("li");
      errItem.className = "scan-item";
      errItem.textContent = "Не вдалося виконати сканування мереж.";
      scanList.appendChild(errItem);
    } finally {
      btnScan.disabled = false;
      btnScan.textContent = "Сканувати мережі";
    }
  }

  if (selectUaCity) {
    selectUaCity.addEventListener("change", function () {
      locFormDirty = true;
      hideAlert(locFeedback);
      const val = selectUaCity.value;
      if (val === "" || val === MANUAL_CITY_VALUE) {
        if (val === MANUAL_CITY_VALUE) {
          inputLocName.focus();
        }
        return;
      }
      const idx = Number(val);
      const city = UA_SETTLEMENTS[idx];
      if (city) {
        inputLocName.value = city.name;
        inputLocLat.value = city.lat.toFixed(4);
        inputLocLon.value = city.lon.toFixed(4);
      }
    });
  }

  function onManualLocationInput() {
    locFormDirty = true;
    const name = inputLocName.value.trim();
    const lat = Number(inputLocLat.value.trim().replace(",", "."));
    const lon = Number(inputLocLon.value.trim().replace(",", "."));
    if (name && Number.isFinite(lat) && Number.isFinite(lon)) {
      const idx = findSettlementIndex(name, lat, lon);
      selectUaCity.value = idx >= 0 ? String(idx) : MANUAL_CITY_VALUE;
    } else {
      selectUaCity.value = MANUAL_CITY_VALUE;
    }
  }

  if (inputLocName && inputLocLat && inputLocLon) {
    inputLocName.addEventListener("input", onManualLocationInput);
    inputLocLat.addEventListener("input", onManualLocationInput);
    inputLocLon.addEventListener("input", onManualLocationInput);
  }

  if (formLocation) {
    formLocation.addEventListener("submit", async function (e) {
      e.preventDefault();
      hideAlert(locFeedback);

      const name = inputLocName.value.trim();
      const latRaw = inputLocLat.value.trim().replace(",", ".");
      const lonRaw = inputLocLon.value.trim().replace(",", ".");

      const nameBytes = getUtf8ByteLength(name);
      if (name.length === 0 || nameBytes < 1 || nameBytes > 64) {
        showAlert(locFeedback, "Вкажіть назву населеного пункту (до 64 байтів UTF-8).", true);
        return;
      }

      if (latRaw === "" || lonRaw === "") {
        showAlert(locFeedback, "Вкажіть числові значення широти (-90..90) та довготи (-180..180).", true);
        return;
      }

      const latitude = Number(latRaw);
      const longitude = Number(lonRaw);

      if (!Number.isFinite(latitude) || latitude < -90 || latitude > 90) {
        showAlert(locFeedback, "Широта (Latitude) має бути числом у межах від -90 до 90.", true);
        return;
      }

      if (!Number.isFinite(longitude) || longitude < -180 || longitude > 180) {
        showAlert(locFeedback, "Довгота (Longitude) має бути числом у межах від -180 до 180.", true);
        return;
      }

      inputLocLat.value = String(latitude);
      inputLocLon.value = String(longitude);
      btnLocSubmit.disabled = true;

      try {
        const res = await fetch("/api/location", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            name: name,
            latitude: latitude,
            longitude: longitude
          })
        });
        const payload = await res.json();
        if (!res.ok) {
          showAlert(locFeedback, payload.message || "Помилка збереження місця знаходження.", true);
        } else {
          locFormDirty = false;
          if (payload.location) {
            syncLocationFormFromStatus(payload.location);
          } else {
            updateLocationBadge(true, name);
          }
          showAlert(locFeedback, payload.message || "Місце знаходження успішно збережено.", false);
        }
      } catch (err) {
        showAlert(locFeedback, "Не вдалося з'єднатися з пристроєм для збереження локації.", true);
      } finally {
        btnLocSubmit.disabled = false;
      }
    });
  }

  formSta.addEventListener("submit", async function (e) {
    e.preventDefault();
    hideAlert(staFeedback);

    const ssid = inputStaSsid.value.trim();
    const password = inputStaPass.value;

    if (ssid.length < 1 || ssid.length > 32) {
      showAlert(staFeedback, "SSID має містити від 1 до 32 символів.", true);
      return;
    }
    if (password.length > 0 && (password.length < 8 || password.length > 63)) {
      showAlert(staFeedback, "Пароль Wi-Fi має містити від 8 до 63 символів або бути порожнім для відкритої мережі.", true);
      return;
    }

    btnStaSubmit.disabled = true;
    try {
      const res = await fetch("/api/wifi/connect", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ssid: ssid, password: password })
      });
      const payload = await res.json();
      if (!res.ok) {
        showAlert(staFeedback, payload.message || "Помилка збереження налаштувань.", true);
      } else {
        showAlert(staFeedback, payload.message || "Налаштування збережено. Виконується підключення…", false);
        setTimeout(fetchStatus, 1500);
      }
    } catch (err) {
      showAlert(staFeedback, "Запит відправлено. Якщо радіоканал змінився, перепідключіться до точки доступу.", false);
    } finally {
      btnStaSubmit.disabled = false;
    }
  });

  formAp.addEventListener("submit", async function (e) {
    e.preventDefault();
    hideAlert(apFeedback);

    const password = inputApPass.value;
    if (password.length < 8 || password.length > 63) {
      showAlert(apFeedback, "Пароль точки доступу має містити від 8 до 63 символів.", true);
      return;
    }

    btnApSubmit.disabled = true;
    try {
      const res = await fetch("/api/wifi/ap-password", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ password: password })
      });
      const payload = await res.json();
      if (!res.ok) {
        showAlert(apFeedback, payload.message || "Помилка оновлення пароля AP.", true);
      } else {
        showAlert(apFeedback, payload.message || "Пароль точки доступу успішно оновлено.", false);
        inputApPass.value = "";
      }
    } catch (err) {
      showAlert(apFeedback, "Пароль оновлено. Перепідключіться до 'ESP Weather Clock' з новим паролем.", false);
    } finally {
      btnApSubmit.disabled = false;
    }
  });

  btnFactoryReset.addEventListener("click", async function () {
    if (!window.confirm("Скинути всі налаштування Wi-Fi та місця знаходження до заводських (пароль точки доступу: weather1)?")) {
      return;
    }
    hideAlert(apFeedback);
    hideAlert(locFeedback);
    btnFactoryReset.disabled = true;
    try {
      const res = await fetch("/api/wifi/reset", { method: "POST" });
      const payload = await res.json();
      showAlert(apFeedback, payload.message || "Налаштування скинуто до заводських.", false);
      inputStaSsid.value = "";
      inputStaPass.value = "";
      locFormDirty = false;
      syncLocationFormFromStatus({ configured: false, name: "", latitude: 0, longitude: 0 });
      setTimeout(fetchStatus, 1500);
    } catch (err) {
      showAlert(apFeedback, "Налаштування скинуто. Перепідключіться з паролем weather1.", false);
    } finally {
      btnFactoryReset.disabled = false;
    }
  });

  document.querySelectorAll(".btn-toggle-pass").forEach(function (btn) {
    btn.addEventListener("click", function () {
      const targetId = btn.getAttribute("data-target");
      const input = document.getElementById(targetId);
      if (input) {
        input.type = input.type === "password" ? "text" : "password";
      }
    });
  });

  initSettlementDropdown();
  btnRefresh.addEventListener("click", fetchStatus);
  btnScan.addEventListener("click", scanNetworks);

  if (locSearchBtn) {
    locSearchBtn.addEventListener("click", runOnlineLocationSearch);
  }
  if (locSearchInput) {
    locSearchInput.addEventListener("keydown", function (e) {
      if (e.key === "Enter") {
        e.preventDefault();
        runOnlineLocationSearch();
      }
    });
  }

  fetchStatus();
  setInterval(fetchStatus, 4000);
})();
