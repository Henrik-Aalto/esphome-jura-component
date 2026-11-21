(() => {
  const CSS_URL = "/0.css";
  const styled = new WeakSet();
  let cssText = "";
  const store = new Map();
  let powerOn = false;

  const ICONS = {
    logo: '<path d="M5 8h11v5a5 5 0 0 1-5 5H10a5 5 0 0 1-5-5V8Z"/><path d="M16 9h2.5a2.5 2.5 0 0 1 0 5H16"/><path d="M8 4c0-1 .8-1 .8-2M12 4c0-1 .8-1 .8-2M16 4c0-1 .8-1 .8-2"/>',
    espresso: '<path d="M6 10h9v3.5A4.5 4.5 0 0 1 10.5 18A4.5 4.5 0 0 1 6 13.5V10Z"/><path d="M15 11h2a2 2 0 0 1 0 4h-2"/><path d="M8 6c0-1 1-1 1-2M11 6c0-1 1-1 1-2M14 6c0-1 1-1 1-2"/>',
    coffee: '<path d="M5 9h11v4a5 5 0 0 1-5 5h-1a5 5 0 0 1-5-5V9Z"/><path d="M16 10h2a2.5 2.5 0 0 1 0 5h-2"/><path d="M7 5c0-1 1-1 1-2M11 5c0-1 1-1 1-2M15 5c0-1 1-1 1-2"/>',
    double: '<path d="M2 14h8v2.5A3.5 3.5 0 0 1 6.5 20h-1A3.5 3.5 0 0 1 2 16.5V14Z"/><path d="M10 14h8v2.5A3.5 3.5 0 0 1 14.5 20h-1A3.5 3.5 0 0 1 10 16.5V14Z"/><path d="M4 9h12"/>',
    milk: '<path d="M9 3h6l1 4v12a2 2 0 0 1-2 2H10a2 2 0 0 1-2-2V7l1-4Z"/><path d="M8 11h8"/><path d="M10 15h4"/>',
    cappuccino: '<path d="M5 10h11v3.5A5.5 5.5 0 0 1 10.5 19A5.5 5.5 0 0 1 5 13.5V10Z"/><path d="M16 11h2a2 2 0 0 1 0 4h-2"/><path d="M6 10c1-2 3-2 4 0 1-2 3-2 4 0"/>',
    rinse: '<path d="M12 3c3 4 6 7 6 10a6 6 0 0 1-12 0c0-3 3-6 6-10Z"/><path d="M9 14c0 2 1.5 3 3 3"/>',
    clean: '<path d="M12 3l1.4 4.1L17.5 8.5l-4.1 1.4L12 14l-1.4-4.1L6.5 8.5l4.1-1.4L12 3Z"/><path d="M18 14l.8 2.2L21 17l-2.2.8L18 20l-.8-2.2L15 17l2.2-.8L18 14Z"/>',
    descale: '<path d="M4 8h16"/><path d="M6 8v11a2 2 0 0 0 2 2h8a2 2 0 0 0 2-2V8"/><path d="M9 12c0 2 6 2 6 0"/><path d="M12 4v2"/>',
    milkclean: '<path d="M4 16c2-2 4-2 6 0s4 2 6 0 4-2 4 0"/><path d="M4 20c2-2 4-2 6 0s4 2 6 0 4-2 4 0"/><path d="M12 3v5"/><path d="M9 6l3 3 3-3"/>',
    filter: '<path d="M5 4h14l-5.5 7v5.5L10.5 19v-8L5 4Z"/><path d="M9 8h6"/>',
    power: '<path d="M12 3v9"/><path d="M6.5 6.5a8 8 0 1 0 11 0"/>',
    terminal: '<path d="M4 5h16v14H4z"/><path d="M7 9l3 3-3 3"/><path d="M12 15h5"/>',
    counter: '<path d="M4 20V10M10 20V4M16 20v-7M22 20H2"/>',
    tray: '<path d="M4 15h16l-2 5H6l-2-5Z"/><path d="M8 11h8"/>',
    firmware: '<path d="M6 6h12v12H6z"/><path d="M9 9h6v6H9z"/><path d="M3 10h3M3 14h3M18 10h3M18 14h3M10 3v3M14 3v3M10 18v3M14 18v3"/>',
  };

  const PRODUCTS = [
    { outlet: "right", label: "Milchschaum", icon: "milk", entity: "button/Milchschaum" },
    { outlet: "right", label: "Latte Macchiato", icon: "milk", entity: "button/Latte Macchiato" },
    { outlet: "right", label: "Kaffee", icon: "coffee", entity: "button/Kaffee" },
    { outlet: "mid", label: "Espresso", icon: "espresso", entity: "button/Espresso" },
    { outlet: "mid", label: "Heißwasser", icon: "rinse", entity: "button/Heißwasser" },
  ];

  const CARE = [
    { label: "Spülen", icon: "rinse", entity: "button/Spülen" },
  ];

  const STATS = [
    { id: "sensor/Espressos gesamt", label: "Espresso", icon: "espresso", outlet: "mitte" },
    { id: "sensor/Doppelte Espressos ⁄ Ristretto", label: "Doppelter Espresso", icon: "double", outlet: "mitte" },
    { id: "sensor/Kaffees gesamt", label: "Kaffee", icon: "coffee", outlet: "beide" },
    { id: "sensor/Doppelte Kaffees ⁄ Flat White", label: "Doppelter Kaffee", icon: "double", outlet: "mitte" },
    { id: "sensor/Cappuccini ⁄ Ristretto", label: "Cappuccino / Ristretto", icon: "cappuccino", outlet: "beide" },
    { id: "sensor/Latte Macchiati ⁄ Cappuccini", label: "Latte / Cappuccino", icon: "milk", outlet: "rechts" },
    { id: "sensor/Doppelte Ristretti", label: "Doppelter Ristretto", icon: "double", outlet: "mitte" },
    { id: "sensor/Brühgruppen-⁄Spülzyklen", label: "Brüh-/Spülzyklen", icon: "counter", outlet: "system" },
    { id: "sensor/Reinigungen gesamt", label: "Reinigungen", icon: "clean", outlet: "system" },
    { id: "sensor/Entkalkungen gesamt", label: "Entkalkungen", icon: "descale", outlet: "system" },
    { id: "sensor/Kaffeesatz-Füllstand", label: "Kaffeesatz", icon: "tray", outlet: "system" },
  ];

  function icon(name) {
    return (
      '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" ' +
      'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' +
      (ICONS[name] || "") +
      "</svg>"
    );
  }

  function inject(root) {
    if (!root) return;
    if (!styled.has(root)) {
      styled.add(root);
      try {
        const style = document.createElement("style");
        style.setAttribute("data-jura-theme", "");
        style.textContent = cssText;
        if (root === document) {
          document.head.appendChild(style);
        } else {
          root.appendChild(style);
        }
      } catch (e) {}
    }
    try {
      root.querySelectorAll("*").forEach((el) => {
        if (el.shadowRoot) inject(el.shadowRoot);
      });
    } catch (e) {}
  }

  function sweep() {
    if (!cssText) return;
    inject(document);
  }

  function meta() {
    document.title = "Jura Z2";
    let theme = document.querySelector('meta[name="theme-color"]');
    if (!theme) {
      theme = document.createElement("meta");
      theme.name = "theme-color";
      document.head.appendChild(theme);
    }
    theme.content = "#15100d";
    const link = document.querySelector('link[rel="icon"]');
    if (link) {
      const svg =
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">' +
        '<rect width="64" height="64" rx="14" fill="#1b120e"/>' +
        '<path d="M16 24h26v12a11 11 0 0 1-11 11h-4a11 11 0 0 1-11-11V24z" fill="#c99a67"/>' +
        '<path d="M42 27h4a6 6 0 0 1 0 12h-4" fill="none" stroke="#c99a67" stroke-width="4" stroke-linecap="round"/>' +
        '<path d="M23 18c0-3 3-3 3-6M32 18c0-3 3-3 3-6M41 18c0-3 3-3 3-6" stroke="#8a6244" stroke-width="3" stroke-linecap="round" fill="none"/>' +
        "</svg>";
      link.href = "data:image/svg+xml," + encodeURIComponent(svg);
    }
  }

  function entityUrl(entity, action) {
    const slash = entity.indexOf("/");
    if (slash < 0) return "";
    const domain = entity.slice(0, slash);
    const name = entity.slice(slash + 1);
    return "/" + domain + "/" + encodeURIComponent(name) + "/" + action;
  }

  function stateText(data) {
    let value;
    if (data.state !== undefined && data.state !== null) value = data.state;
    else if (data.value !== undefined && data.value !== null) value = data.value;
    else return "–";
    const text = String(value);
    return text === "" ? "–" : text;
  }

  function setPower(on) {
    powerOn = on;
    const button = document.getElementById("jura-power");
    if (!button) return;
    button.classList.toggle("is-on", on);
  }

  function updateNotice(value) {
    const notice = document.getElementById("jura-notice");
    const text = document.getElementById("jura-notice-text");
    if (!notice || !text) return;
    const quiet = !value || value === "–" || value === "Bereit" || value === "Ausgeschaltet";
    notice.hidden = quiet;
    text.textContent = quiet ? "" : value;
    notice.classList.toggle(
      "is-warning",
      !quiet && /füll|leer|fehlt|voll|sp(ü|ue)l|pflege|reinig|entkalk/i.test(value)
    );
  }

  function renderState(data) {
    const id = data.id;
    if (!id) return;
    const value = stateText(data);
    document.querySelectorAll("[data-state-id]").forEach((el) => {
      if (el.getAttribute("data-state-id") === id) el.textContent = value;
    });
    if (id === "switch/Kaffeeautomat") setPower(data.state === "ON");
    if (id === "text_sensor/Jura Status") updateNotice(value);
  }

  async function runAction(button, entity, action) {
    const url = entityUrl(entity, action);
    if (!url) return;
    button.classList.add("is-busy");
    button.classList.remove("is-ok", "is-error");
    try {
      const response = await fetch(url, { method: "POST" });
      if (!response.ok) throw new Error("HTTP " + response.status);
      button.classList.add("is-ok");
      window.setTimeout(() => button.classList.remove("is-ok"), 1200);
    } catch (error) {
      button.classList.add("is-error");
      window.setTimeout(() => button.classList.remove("is-error"), 1800);
    } finally {
      button.classList.remove("is-busy");
    }
  }

  function productButton(product) {
    if (!product.entity) {
      return (
        '<button type="button" class="jura-product is-disabled" disabled>' +
        '<span class="jura-icon">' + icon(product.icon) + "</span>" +
        '<span class="jura-name">' + product.label + "</span>" +
        '<span class="jura-sub">Code folgt</span>' +
        "</button>"
      );
    }
    return (
      '<button type="button" class="jura-product" data-entity="' + product.entity + '" data-action="press">' +
      '<span class="jura-icon">' + icon(product.icon) + "</span>" +
      '<span class="jura-name">' + product.label + "</span>" +
      '<span class="jura-sub">Bezug starten</span>' +
      "</button>"
    );
  }

  function careButton(item) {
    if (!item.entity) {
      return (
        '<button type="button" class="jura-action is-disabled" disabled>' +
        '<span class="jura-icon">' + icon(item.icon) + "</span>" +
        '<span class="jura-name">' + item.label + "</span>" +
        '<span class="jura-sub">Code folgt</span>' +
        "</button>"
      );
    }
    return (
      '<button type="button" class="jura-action" data-entity="' + item.entity + '" data-action="press">' +
      '<span class="jura-icon">' + icon(item.icon) + "</span>" +
      '<span class="jura-name">' + item.label + "</span>" +
      '<span class="jura-sub">Auslösen</span>' +
      "</button>"
    );
  }

  function statCard(stat) {
    return (
      '<div class="jura-stat">' +
      '<span class="jura-stat-icon">' + icon(stat.icon) + "</span>" +
      '<span class="jura-stat-label">' + stat.label + "</span>" +
      '<span class="jura-value" data-state-id="' + stat.id + '">–</span>' +
      '<span class="jura-stat-outlet">' + stat.outlet + "</span>" +
      "</div>"
    );
  }

  function buildDashboard() {
    const old = document.getElementById("jura-dash");
    if (old) old.remove();

    const right = PRODUCTS.filter((p) => p.outlet === "right").map(productButton).join("");
    const mid = PRODUCTS.filter((p) => p.outlet === "mid").map(productButton).join("");
    const care = CARE.map(careButton).join("");
    const stats = STATS.map(statCard).join("");

    const html =
      '<div id="jura-dash" class="jura-shell">' +
      '<header class="jura-hero">' +
      '<div class="jura-brand">' +
      '<div class="jura-logo">' + icon("logo") + "</div>" +
      "<div><h1>Jura Z2</h1><p id=\"jura-subtitle\">Kaffeemaschine · Küche</p></div>" +
      "</div>" +
      '<div class="jura-hero-actions">' +
      '<button type="button" id="jura-power" class="jura-power">' +
      '<span class="jura-power-dot"></span><span class="jura-power-icon">' + icon("power") + "</span>" +
      "</button>" +
      '<button type="button" id="jura-log-toggle" class="jura-ghost">' + icon("terminal") + "<span>Log</span></button>" +
      "</div>" +
      "</header>" +
      '<div class="jura-statusbar">' +
      '<span class="jura-pill">Status <b data-state-id="text_sensor/Jura Status">–</b></span>' +
      '<span class="jura-pill">Schale <b data-state-id="text_sensor/Auffangschale">–</b></span>' +
      '<span class="jura-pill">Tank <b data-state-id="text_sensor/Wassertank">–</b></span>' +
      '<span class="jura-pill">Kaffeesatz <b data-state-id="sensor/Kaffeesatz-Füllstand">–</b></span>' +
      '<span class="jura-pill">Firmware <b data-state-id="text_sensor/Jura Firmware⁄Modell">–</b></span>' +
      "</div>" +
      '<div id="jura-notice" class="jura-notice" hidden>' +
      '<span class="jura-notice-dot"></span><span id="jura-notice-text">–</span>' +
      "</div>" +
      '<section class="jura-panel">' +
      '<div class="jura-panel-head"><h2>' + icon("espresso") + "Produkte</h2><span class=\"jura-hint\">Zwei Ausläufe</span></div>" +
      '<div class="jura-outlets">' +
      '<div class="jura-outlet jura-outlet-right">' +
      '<div class="jura-outlet-head"><span class="jura-dot"></span>Rechter Auslauf</div>' +
      '<div class="jura-card-grid">' + right + "</div>" +
      "</div>" +
      '<div class="jura-outlet jura-outlet-mid">' +
      '<div class="jura-outlet-head"><span class="jura-dot"></span>Mittlerer Auslauf</div>' +
      '<div class="jura-card-grid">' + mid + "</div>" +
      "</div>" +
      "</div>" +
      "</section>" +
      '<section class="jura-panel">' +
      '<div class="jura-panel-head"><h2>' + icon("clean") + "Pflege</h2><span class=\"jura-hint\">Manuell spülen</span></div>" +
      '<div class="jura-card-grid jura-care">' + care + "</div>" +
      "</section>" +
      '<section class="jura-panel">' +
      '<div class="jura-panel-head"><h2>' + icon("counter") + "Bezüge</h2><span class=\"jura-hint\">Gesamtzähler</span></div>" +
      '<div class="jura-stats">' + stats + "</div>" +
      "</section>" +
      '<section class="jura-panel jura-diag-panel">' +
      "<details><summary>" + icon("firmware") + "Diagnose &amp; Befehl</summary>" +
      '<div class="jura-diag-grid">' +
      '<div class="jura-diag"><span>Ereignis</span><b data-state-id="text_sensor/Jura Ereignis">–</b></div>' +
      '<div class="jura-diag"><span>IC-Bits</span><b data-state-id="text_sensor/Jura IC-Bits">–</b></div>' +
      '<div class="jura-diag"><span>HZ-Status</span><b data-state-id="text_sensor/Jura HZ-Status">–</b></div>' +
      '<div class="jura-diag"><span>CS-Status</span><b data-state-id="text_sensor/Jura CS-Status">–</b></div>' +
      '<div class="jura-diag"><span>Zähleränderung</span><b data-state-id="text_sensor/Jura Zähleränderung">–</b></div>' +
      "</div>" +
      '<div class="jura-diag-actions">' +
      '<button type="button" class="jura-action jura-compact" data-entity="button/Firmware⁄Modell lesen" data-action="press">' +
      icon("firmware") + "Firmware lesen</button>" +
      "</div>" +
      '<form id="jura-cmd-form" class="jura-cmd">' +
      '<input id="jura-cmd" maxlength="32" placeholder="z. B. FA:04" autocomplete="off">' +
      '<button type="submit" class="jura-action jura-compact">Senden</button>' +
      "</form>" +
      '<p class="jura-note">Nur zur Diagnose. Unbekannte FA-Codes können unerwünschte Aktionen auslösen.</p>' +
      "</details>" +
      "</section>" +
      '<section id="jura-log-panel" class="jura-panel jura-log-panel" hidden>' +
      '<div class="jura-panel-head"><h2>' + icon("terminal") + "Log</h2>" +
      '<button type="button" id="jura-log-clear" class="jura-ghost jura-small">Leeren</button></div>' +
      '<div id="jura-log-host" class="jura-log-host"></div>' +
      "</section>" +
      "</div>";

    document.body.insertAdjacentHTML("beforeend", html);
  }

  function mountLog() {
    const host = document.getElementById("jura-log-host");
    if (!host) return;
    const element = document.createElement("esp-log");
    element.setAttribute("rows", "120");
    host.appendChild(element);
    const applyRows = () => {
      try { element.rows = 120; } catch (e) {}
    };
    if (window.customElements && window.customElements.whenDefined) {
      window.customElements.whenDefined("esp-log").then(applyRows);
    }
    applyRows();
  }

  function connectSource() {
    try {
      if (window.source && window.source.close) window.source.close();
    } catch (e) {}
    const source = new EventSource("/events");
    window.source = source;
    source.addEventListener("state", (event) => {
      let data;
      try { data = JSON.parse(event.data); } catch (e) { return; }
      if (!data || !data.id) return;
      store.set(data.id, data);
      renderState(data);
    });
    source.addEventListener("open", () => {
      const subtitle = document.getElementById("jura-subtitle");
      if (subtitle) subtitle.textContent = "Kaffeemaschine · Küche";
    });
    source.addEventListener("error", () => {
      const subtitle = document.getElementById("jura-subtitle");
      if (subtitle) subtitle.textContent = "Verbindung wird wiederhergestellt …";
    });
  }

  function wire() {
    const powerButton = document.getElementById("jura-power");
    if (powerButton) {
      powerButton.addEventListener("click", () => {
        runAction(powerButton, "switch/Kaffeeautomat", powerOn ? "turn_off" : "turn_on");
      });
    }

    const logToggle = document.getElementById("jura-log-toggle");
    const logPanel = document.getElementById("jura-log-panel");
    if (logToggle && logPanel) {
      logToggle.addEventListener("click", () => {
        logPanel.hidden = !logPanel.hidden;
        logToggle.classList.toggle("is-active", !logPanel.hidden);
        if (!logPanel.hidden) logPanel.scrollIntoView({ behavior: "smooth", block: "start" });
      });
    }

    const logClear = document.getElementById("jura-log-clear");
    if (logClear) {
      logClear.addEventListener("click", () => {
        const host = document.getElementById("jura-log-host");
        if (!host) return;
        host.innerHTML = "";
        mountLog();
      });
    }

    const form = document.getElementById("jura-cmd-form");
    if (form) {
      form.addEventListener("submit", async (event) => {
        event.preventDefault();
        const input = document.getElementById("jura-cmd");
        if (!input) return;
        const value = input.value.trim();
        if (!value) return;
        const url = "/text/" + encodeURIComponent("Jura Kommando") + "/set?value=" + encodeURIComponent(value);
        try {
          const response = await fetch(url, { method: "POST" });
          if (!response.ok) throw new Error("HTTP " + response.status);
          input.value = "";
        } catch (e) {
          input.classList.add("is-error");
          window.setTimeout(() => input.classList.remove("is-error"), 1500);
        }
      });
    }

    document.addEventListener("click", (event) => {
      const target = event.target;
      if (!target || !target.closest) return;
      const button = target.closest("[data-entity][data-action]");
      if (!button || button.disabled) return;
      event.preventDefault();
      runAction(button, button.getAttribute("data-entity"), button.getAttribute("data-action"));
    });
  }

  async function boot() {
    meta();
    try {
      const response = await fetch(CSS_URL, { cache: "no-store" });
      if (response.ok) cssText = await response.text();
    } catch (e) {}
    sweep();
    const hideBuiltIn = document.createElement("style");
    hideBuiltIn.textContent = "esp-app{display:none!important}";
    document.head.appendChild(hideBuiltIn);
    buildDashboard();
    wire();
    connectSource();
    mountLog();
    new MutationObserver(sweep).observe(document.documentElement, {
      childList: true,
      subtree: true,
    });
    window.setInterval(sweep, 2000);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", boot);
  } else {
    boot();
  }
})();
