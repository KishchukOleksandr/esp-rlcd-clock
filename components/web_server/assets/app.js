(function () {
  "use strict";

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

  const formAp = document.getElementById("form-ap-password");
  const inputApPass = document.getElementById("input-ap-password");
  const apFeedback = document.getElementById("ap-form-feedback");
  const btnApSubmit = document.getElementById("btn-ap-submit");
  const btnFactoryReset = document.getElementById("btn-factory-reset");

  function showAlert(el, message, isError) {
    el.textContent = message;
    el.className = "alert " + (isError ? "alert-error" : "alert-success");
  }

  function hideAlert(el) {
    el.textContent = "";
    el.className = "alert hidden";
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

  async function fetchStatus() {
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
    if (!window.confirm("Скинути всі налаштування Wi-Fi до заводських (пароль точки доступу: weather1)?")) {
      return;
    }
    hideAlert(apFeedback);
    btnFactoryReset.disabled = true;
    try {
      const res = await fetch("/api/wifi/reset", { method: "POST" });
      const payload = await res.json();
      showAlert(apFeedback, payload.message || "Налаштування скинуто до заводських.", false);
      inputStaSsid.value = "";
      inputStaPass.value = "";
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

  btnRefresh.addEventListener("click", fetchStatus);
  btnScan.addEventListener("click", scanNetworks);

  fetchStatus();
  setInterval(fetchStatus, 4000);
})();
