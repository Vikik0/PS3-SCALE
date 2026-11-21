import "./styles.css";

const API_BASE = (import.meta.env.VITE_API_BASE_URL || "").replace(/\/$/, "");
const STORAGE_KEY = "crash-bash-online-session-v1";
const app = document.querySelector("#app");

const state = {
  session: readSession(),
  room: null,
  loading: false,
  error: "",
  health: "checking",
  polling: false
};

function readSession() {
  try {
    return JSON.parse(localStorage.getItem(STORAGE_KEY) || "null");
  } catch {
    return null;
  }
}

function saveSession(session) {
  state.session = session;
  localStorage.setItem(STORAGE_KEY, JSON.stringify(session));
}

function clearSession() {
  state.session = null;
  localStorage.removeItem(STORAGE_KEY);
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>"']/g, char => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
  })[char]);
}

function initials(name) {
  return String(name || "P").trim().slice(0, 2).toUpperCase();
}

async function api(path, { method = "GET", body } = {}) {
  if (!API_BASE) throw new Error("The matchmaking API URL has not been configured yet.");
  const headers = { "Content-Type": "application/json" };
  if (state.session?.token) headers.Authorization = `Bearer ${state.session.token}`;
  const response = await fetch(API_BASE + path, {
    method,
    headers,
    body: body === undefined ? undefined : JSON.stringify(body)
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    if (response.status === 401) clearSession();
    throw new Error(data.message || data.error || `Request failed (${response.status})`);
  }
  return data;
}

function setError(message) {
  state.error = message || "";
  render();
}

function roomCard() {
  if (!state.room) {
    return `
      <div class="empty-room">
        <div class="empty-emblem"><span>?</span></div>
        <p class="eyebrow">YOUR PARTY</p>
        <h3>No room yet</h3>
        <p>Create a private room or enter a friend's code. Up to four players can join.</p>
        <div class="empty-lines"><i></i><i></i><i></i><i></i></div>
      </div>`;
  }

  const room = state.room;
  const players = [...(room.players || [])].sort((a, b) => a.slot - b.slot);
  const me = players.find(player => player.player_id === state.session?.session_id);
  const isHost = Boolean(me?.is_host);
  const allReady = players.length >= 2 && players.every(player => player.ready);
  const slots = Array.from({ length: room.max_players || 4 }, (_, index) => {
    const player = players.find(item => item.slot === index + 1);
    return player
      ? `<div class="player-slot occupied">
          <div class="avatar avatar-${index + 1}">${escapeHtml(initials(player.name))}</div>
          <div class="player-meta"><strong>${escapeHtml(player.name)}${player.is_host ? ' <span class="host-tag">HOST</span>' : ""}</strong><small>PLAYER ${player.slot}</small></div>
          <span class="ready-pill ${player.ready ? "is-ready" : ""}">${player.ready ? "READY" : "NOT READY"}</span>
        </div>`
      : `<div class="player-slot vacant">
          <div class="avatar avatar-empty">+</div>
          <div class="player-meta"><strong>Waiting for player</strong><small>PLAYER ${index + 1}</small></div>
          <span class="vacant-pill">OPEN SLOT</span>
        </div>`;
  }).join("");

  return `
    <div class="room-active">
      <div class="room-code-row">
        <div><p class="eyebrow">ROOM CODE</p><strong class="room-code">${escapeHtml(room.room_code)}</strong></div>
        <button class="icon-button" data-action="copy-code" aria-label="Copy room code" title="Copy room code">▣</button>
      </div>
      <div class="room-status-row"><span class="status-dot"></span><span>${room.status === "running" ? "MATCH STARTED" : "WAITING IN LOBBY"}</span><span class="room-count">${players.length}/${room.max_players} PLAYERS</span></div>
      <div class="player-list">${slots}</div>
      <div class="room-actions">
        <button class="button button-secondary" data-action="leave">LEAVE ROOM</button>
        <button class="button button-primary" data-action="ready" ${room.status !== "waiting" ? "disabled" : ""}>${me?.ready ? "CANCEL READY" : "I'M READY"}</button>
        ${isHost ? `<button class="button button-start" data-action="start" ${!allReady || room.status !== "waiting" ? "disabled" : ""}>START MATCH <span>→</span></button>` : ""}
      </div>
      <p class="room-footnote">Room relay prototype is available for transport testing. PS3 and browser gameplay clients are still being connected.</p>
    </div>`;
}

function render() {
  const connected = state.health === "online";
  app.innerHTML = `
    <div class="site-shell">
      <header class="topbar">
        <a class="brand" href="#" aria-label="Crash Bash Enhanced home">
          <span class="brand-mark"><b>C</b><i></i></span>
          <span class="brand-copy"><strong>CRASH BASH</strong><small>ENHANCED ONLINE</small></span>
        </a>
        <nav class="top-nav"><a href="#play" class="active">PLAY ONLINE</a><a href="#how">HOW IT WORKS</a><a href="#rankings">RANKINGS</a></nav>
        <div class="top-actions">
          <span class="connection-chip ${connected ? "online" : ""}"><i></i>${connected ? "SERVICE ONLINE" : "SERVICE OFFLINE"}</span>
          <button class="profile-button" data-action="profile">${state.session ? escapeHtml(initials(state.session.display_name)) : "GUEST"}</button>
        </div>
      </header>

      <main>
        <section class="hero">
          <div class="hero-copy">
            <div class="hero-kicker"><span class="kicker-line"></span> THE PARTY IS BACK</div>
            <h1>GOOD TIMES.<br/><em>NO DISTANCE.</em></h1>
            <p>Bring the chaos back online. Create a room, invite your friends, and get the party started — no PSN sign-in required.</p>
            <div class="hero-stats">
              <div><strong>04</strong><span>PLAYER ROOMS</span></div>
              <div><strong>00</strong><span>PSN REQUIRED</span></div>
              <div><strong>∞</strong><span>REMATCHES</span></div>
            </div>
          </div>
          <div class="hero-art" aria-hidden="true">
            <div class="sunburst"></div><div class="orbit orbit-one"></div><div class="orbit orbit-two"></div>
            <div class="hero-disc disc-one"></div><div class="hero-disc disc-two"></div><div class="hero-disc disc-three"></div>
            <div class="hero-badge"><small>ONLINE</small><strong>PARTY<br/>MODE</strong><span>EST. 1999</span></div>
            <div class="hero-caption">PLAY TOGETHER<br/><b>FROM ANYWHERE</b></div>
          </div>
        </section>

        <section class="feature-strip">
          <div><span class="feature-icon">◎</span><span><strong>GUEST ACCESS</strong><small>No PSN account needed</small></span></div>
          <div><span class="feature-icon">♧</span><span><strong>PRIVATE ROOMS</strong><small>Invite friends with a code</small></span></div>
          <div><span class="feature-icon">✦</span><span><strong>UP TO 4 PLAYERS</strong><small>Bring the whole party</small></span></div>
          <div><span class="feature-icon">⌁</span><span><strong>LIGHTWEIGHT SERVICE</strong><small>Matchmaking, not game streaming</small></span></div>
        </section>

        <section class="play-section" id="play">
          <div class="section-heading"><div><p class="eyebrow">READY WHEN YOU ARE</p><h2>JUMP INTO THE <em>LOBBY</em></h2></div><span class="section-index">01 / PLAY</span></div>
          <div class="play-grid">
            <div class="join-panel">
              <div class="panel-top"><span class="panel-label">01 — START A PARTY</span><span class="panel-symbol">＋</span></div>
              <h3>MAKE A ROOM</h3>
              <p>Set up a private lobby and share your room code with friends.</p>
              <label for="display-name">PLAYER NAME</label>
              <input id="display-name" maxlength="16" placeholder="Enter your name" value="${escapeHtml(state.session?.display_name || "")}" />
              <label for="player-limit">PLAYER SLOTS</label>
              <select id="player-limit"><option value="4">4 PLAYERS — FULL PARTY</option><option value="3">3 PLAYERS</option><option value="2">2 PLAYERS — DUEL</option></select>
              <button class="button button-primary full-button" data-action="create" ${!connected || state.loading ? "disabled" : ""}>CREATE PRIVATE ROOM <span>→</span></button>
              <div class="panel-divider"><span>OR JOIN A FRIEND</span></div>
              <label for="room-code-input">ROOM CODE</label>
              <div class="join-code-row"><input id="room-code-input" maxlength="6" placeholder="ABC123" autocomplete="off" /><button class="button button-outline" data-action="join" ${!connected || state.loading ? "disabled" : ""}>JOIN</button></div>
            </div>

            <div class="lobby-panel">
              <div class="panel-top"><span class="panel-label">02 — YOUR PARTY</span><span class="panel-symbol">◉</span></div>
              ${roomCard()}
            </div>
          </div>
          ${state.error ? `<div class="error-banner" role="alert"><span>!</span>${escapeHtml(state.error)}<button data-action="dismiss-error" aria-label="Dismiss error">×</button></div>` : ""}
        </section>

        <section class="how-section" id="how">
          <div class="section-heading"><div><p class="eyebrow">THREE STEPS. ZERO FUSS.</p><h2>HOW IT <em>WORKS</em></h2></div><span class="section-index">02 / GUIDE</span></div>
          <div class="steps-grid">
            <article class="step-card"><span class="step-number">01</span><div class="step-icon">◉</div><h3>CREATE OR JOIN</h3><p>Start a guest session and create a private room, or enter your friend's six-character code.</p></article>
            <article class="step-card"><span class="step-number">02</span><div class="step-icon">♧</div><h3>GET YOUR CREW READY</h3><p>Wait for up to three friends to join. Everyone marks ready before the host starts.</p></article>
            <article class="step-card"><span class="step-number">03</span><div class="step-icon">✦</div><h3>LET THE PARTY BEGIN</h3><p>The room relay prototype is in place. Browser and PS3 gameplay synchronization is still under development.</p></article>
          </div>
        </section>

        <section class="rank-section" id="rankings">
          <div><p class="eyebrow">THE HALL OF FAME</p><h2>RANKINGS <em>COMING NEXT</em></h2><p>Ranked results will appear after match verification and game synchronization are implemented. No placeholder scores or fake player stats.</p></div>
          <div class="rank-placeholder"><span>★</span><strong>LEADERBOARD</strong><small>WAITING FOR VERIFIED MATCH DATA</small></div>
        </section>
      </main>

      <footer class="footer"><a class="brand footer-brand" href="#"><span class="brand-mark"><b>C</b><i></i></span><span class="brand-copy"><strong>CRASH BASH</strong><small>ENHANCED ONLINE</small></span></a><p>COMMUNITY PROJECT · NOT AFFILIATED WITH THE ORIGINAL GAME PUBLISHER</p><span>BUILD 0.1.0</span></footer>
    </div>`;

  if (!API_BASE) {
    const chip = app.querySelector(".connection-chip");
    if (chip) chip.textContent = "API NOT CONFIGURED";
  }
}

async function ensureSession(displayName = "") {
  if (state.session?.token) return state.session;
  const name = displayName || document.querySelector("#display-name")?.value?.trim() || "Player";
  const session = await api("/v1/sessions", {
    method: "POST",
    body: { mode: "guest", display_name: name }
  });
  saveSession(session);
  return session;
}

async function runAction(action) {
  if (state.loading) return;
  const displayName = document.querySelector("#display-name")?.value?.trim() || "";
  const selectedLimit = document.querySelector("#player-limit")?.value || "4";
  const enteredCode = document.querySelector("#room-code-input")?.value?.trim().toUpperCase() || "";
  state.loading = true;
  state.error = "";
  render();
  try {
    if (action === "create") {
      await ensureSession(displayName);
      const maxPlayers = Number(selectedLimit || 4);
      state.room = await api("/v1/rooms", { method: "POST", body: { max_players: maxPlayers, is_private: true, mode: "party" } });
      beginPolling();
    } else if (action === "join") {
      await ensureSession(displayName);
      const code = enteredCode;
      if (!/^[A-Z0-9]{6}$/.test(code)) throw new Error("Enter a valid six-character room code.");
      state.room = await api(`/v1/rooms/${code}/join`, { method: "POST", body: {} });
      beginPolling();
    } else if (action === "ready") {
      const me = state.room?.players?.find(p => p.player_id === state.session?.session_id);
      state.room = await api(`/v1/rooms/${state.room.room_code}/ready`, { method: "POST", body: { ready: !me?.ready } });
    } else if (action === "start") {
      state.room = await api(`/v1/rooms/${state.room.room_code}/start`, { method: "POST", body: {} });
    } else if (action === "leave") {
      await api(`/v1/rooms/${state.room.room_code}/leave`, { method: "POST", body: {} });
      state.room = null;
      stopPolling();
    } else if (action === "copy-code") {
      await navigator.clipboard.writeText(state.room.room_code);
      state.error = "Room code copied.";
    } else if (action === "profile") {
      if (state.session) {
        clearSession();
        state.room = null;
        stopPolling();
      } else {
        await ensureSession(displayName);
      }
    } else if (action === "dismiss-error") {
      state.error = "";
    }
  } catch (error) {
    state.error = error.message || "Something went wrong.";
  } finally {
    state.loading = false;
    render();
  }
}

let pollTimer = null;
function beginPolling() {
  stopPolling();
  pollTimer = setInterval(async () => {
    if (!state.room || state.polling) return;
    state.polling = true;
    try {
      state.room = await api(`/v1/rooms/${state.room.room_code}`);
      render();
    } catch (error) {
      state.error = error.message;
      render();
    } finally {
      state.polling = false;
    }
  }, 2200);
}

function stopPolling() {
  if (pollTimer) clearInterval(pollTimer);
  pollTimer = null;
}

app.addEventListener("click", event => {
  const button = event.target.closest("[data-action]");
  if (!button) return;
  runAction(button.dataset.action);
});

app.addEventListener("input", event => {
  if (event.target.id === "room-code-input") {
    const start = event.target.selectionStart;
    event.target.value = event.target.value.toUpperCase().replace(/[^A-Z0-9]/g, "").slice(0, 6);
    event.target.setSelectionRange(start, start);
  }
});

async function init() {
  render();
  if (!API_BASE) {
    state.health = "offline";
    render();
    return;
  }
  try {
    const health = await fetch(API_BASE + "/health", { cache: "no-store" });
    if (!health.ok) throw new Error("API health check failed.");
    state.health = "online";
  } catch {
    state.health = "offline";
  }
  render();
  if (state.session?.token) {
    try {
      const response = await api("/v1/rooms/current");
      state.room = response.room || null;
    } catch {
      // An expired or roomless guest session is harmless; create a new one on demand.
    }
    if (state.room) beginPolling();
  }
}

init();
