import "./styles.css";

const API_BASE = (import.meta.env.VITE_API_BASE_URL || "").replace(/\/$/, "");
const API_ORIGIN = API_BASE || window.location.origin;
const STORAGE_KEY = "crash-bash-online-session-v1";
try { localStorage.setItem("crash-bash-online-api-origin-v1", API_ORIGIN); } catch {}
const app = document.querySelector("#app");

const state = {
  session: readSession(),
  room: null,
  publicRooms: [],
  loading: false,
  error: "",
  health: "checking",
  relayStatus: "idle",
  relayPackets: 0
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
  const headers = { "Content-Type": "application/json" };
  if (state.session?.token) headers.Authorization = `Bearer ${state.session.token}`;
  const response = await fetch(API_ORIGIN + path, {
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
        <p>Join a room created by the PS3 host with its six-character code. Public PS3 room discovery is not enabled yet.</p>
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
        <a class="button button-secondary" href="/play.html?room=${encodeURIComponent(room.room_code)}" target="_blank" rel="noopener">OPEN BROWSER PLAYER ↗</a>
        ${isHost && me?.platform === "ps3" ? `<button class="button button-start" data-action="start" ${!allReady || room.status !== "waiting" ? "disabled" : ""}>START MATCH <span>→</span></button>` : ""}
        ${isHost && me?.platform !== "ps3" ? `<p class="room-footnote">Browser-created rooms cannot host this match. Create the room from the PS3 in-game menu.</p>` : ""}
      </div>
      <p class="room-footnote">${state.relayStatus === "connected" ? (state.relayPackets > 0 ? "ROOM RELAY CONNECTED · " + state.relayPackets + " GAME PACKETS RECEIVED" : "ROOM RELAY CONNECTED · WAITING FOR GAME INPUTS") : state.relayStatus === "connecting" ? "CONNECTING TO ROOM RELAY..." : state.relayStatus === "error" ? "ROOM RELAY CONNECTION FAILED" : "CONNECT TO ROOM RELAY TO JOIN THE PARTY."}</p>
    </div>`;
}

function publicRoomsCard() {
  const connected = state.health === "online";
  const rooms = (state.publicRooms || []).filter(room => room.host_platform === "ps3");
  const items = rooms.map(room => `
    <article class="public-room-item">
      <div class="public-room-main">
        <strong>${escapeHtml(room.host_name)}'S ROOM</strong>
        <small>${escapeHtml(room.mode || "party").toUpperCase()} · ${room.player_count}/${room.max_players} PLAYERS</small>
      </div>
      <div class="public-room-actions">
        <span class="public-room-code">${escapeHtml(room.room_code)}</span>
        <button class="button button-outline" data-action="join-public" data-room-code="${escapeHtml(room.room_code)}" ${!connected || state.loading || state.room ? "disabled" : ""}>JOIN</button>
      </div>
    </article>`).join("");

  return `
    <section class="public-rooms-panel">
      <div class="public-rooms-heading">
        <div><p class="eyebrow">OPEN MATCHMAKING</p><h3>PUBLIC ROOMS</h3></div>
        <div class="public-rooms-toolbar">
          <button class="button button-primary" data-action="any-public" ${!connected || state.loading || state.room ? "disabled" : ""}>ANY PUBLIC ROOM</button>
          <button class="button button-secondary" data-action="refresh-public" ${!connected || state.loading ? "disabled" : ""}>REFRESH</button>
        </div>
      </div>
      ${items || `<p class="public-rooms-empty">${connected ? "No public rooms are open. Create one and let other players join." : "Public rooms will appear when the service is online."}</p>`}
    </section>`;
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
        <nav class="top-nav"><a href="#play" class="active">PLAY ONLINE</a><a href="/play.html">BROWSER PLAYER</a><a href="#how">HOW IT WORKS</a><a href="#rankings">RANKINGS</a></nav>
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
              <div><strong>04</strong><span>OPEN ROOMS</span></div>
              <div><strong>00</strong><span>PSN REQUIRED</span></div>
              <div><strong>01</strong><span>ACTIVE MATCH</span></div>
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
          <div><span class="feature-icon">♧</span><span><strong>PUBLIC + PRIVATE ROOMS</strong><small>Browse or invite with a code</small></span></div>
          <div><span class="feature-icon">✦</span><span><strong>UP TO 4 PLAYERS</strong><small>Bring the whole party</small></span></div>
          <div><span class="feature-icon">⌁</span><span><strong>BROWSER PLAYER BETA</strong><small>Run your own PS1 disc locally</small></span></div>
        </section>

        <section class="play-section" id="play">
          <div class="section-heading"><div><p class="eyebrow">READY WHEN YOU ARE</p><h2>JUMP INTO THE <em>LOBBY</em></h2></div><span class="section-index">01 / PLAY</span></div>
          <div class="play-grid">
            <div class="join-panel">
              <div class="panel-top"><span class="panel-label">01 — JOIN A PS3 HOST</span><span class="panel-symbol">↗</span></div>
              <h3>JOIN THE MATCH</h3>
              <p>The PS3 player creates the room from the in-game menu. Browser players join with the room code.</p>
              <label for="display-name">PLAYER NAME</label>
              <input id="display-name" maxlength="16" placeholder="Enter your name" value="${escapeHtml(state.session?.display_name || "")}" />
              <div class="notice-panel">PS3 HOST REQUIRED · Create the room on PS3, then share its code here.</div>
              <div class="panel-divider"><span>JOIN WITH A CODE</span></div>
              <label for="room-code-input">ROOM CODE</label>
              <div class="join-code-row"><input id="room-code-input" maxlength="6" placeholder="ABC123" autocomplete="off" /><button class="button button-outline" data-action="join" ${!connected || state.loading ? "disabled" : ""}>JOIN</button></div>
            </div>

            <div class="lobby-panel">
              <div class="panel-top"><span class="panel-label">02 — YOUR PARTY</span><span class="panel-symbol">◉</span></div>
              ${roomCard()}
            </div>
          </div>
          ${publicRoomsCard()}
          ${state.error ? `<div class="error-banner" role="alert"><span>!</span>${escapeHtml(state.error)}<button data-action="dismiss-error" aria-label="Dismiss error">×</button></div>` : ""}
        </section>

        <section class="how-section" id="how">
          <div class="section-heading"><div><p class="eyebrow">THREE STEPS. ZERO FUSS.</p><h2>HOW IT <em>WORKS</em></h2></div><span class="section-index">02 / GUIDE</span></div>
          <div class="steps-grid">
            <article class="step-card"><span class="step-number">01</span><div class="step-icon">◉</div><h3>PS3 CREATES THE ROOM</h3><p>The PS3 host opens Crash Bash Online in-game and shares the six-character room code.</p></article>
            <article class="step-card"><span class="step-number">02</span><div class="step-icon">♧</div><h3>BROWSER JOINS</h3><p>Browser players enter the code, load the same disc revision, and connect to the PS3 host.</p></article>
            <article class="step-card"><span class="step-number">03</span><div class="step-icon">✦</div><h3>SYNCED GAMEPLAY BETA</h3><p>The CBN1 browser adapter is integrated in source. A live PS3/browser match still needs hardware verification.</p></article>
          </div>
        </section>

        <section class="rank-section" id="rankings">
          <div><p class="eyebrow">THE HALL OF FAME</p><h2>RANKINGS <em>COMING NEXT</em></h2><p>Ranked results will appear after match verification and game synchronization are implemented. No placeholder scores or fake player stats.</p></div>
          <div class="rank-placeholder"><span>★</span><strong>LEADERBOARD</strong><small>WAITING FOR VERIFIED MATCH DATA</small></div>
        </section>
      </main>

      <footer class="footer"><a class="brand footer-brand" href="#"><span class="brand-mark"><b>C</b><i></i></span><span class="brand-copy"><strong>CRASH BASH</strong><small>ENHANCED ONLINE</small></span></a><p>FREE COMMUNITY SERVICE · ONE ACTIVE MATCH · NOT AFFILIATED WITH THE ORIGINAL GAME PUBLISHER</p><span>BUILD 0.1.0</span></footer>
    </div>`;

}

async function refreshPublicRooms({ quiet = true } = {}) {
  if (state.health !== "online") return;
  try {
    const response = await api("/v1/rooms/public");
    state.publicRooms = Array.isArray(response.rooms) ? response.rooms : [];
  } catch (error) {
    if (!quiet) state.error = error.message || "Could not refresh public rooms.";
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

async function runAction(action, selectedRoomCode = "") {
  if (state.loading) return;
  const displayName = document.querySelector("#display-name")?.value?.trim() || "";
  const selectedLimit = document.querySelector("#player-limit")?.value || "4";
  const enteredCode = document.querySelector("#room-code-input")?.value?.trim().toUpperCase() || "";
  state.loading = true;
  state.error = "";
  render();
  try {
    if (action === "create" || action === "create-public") {
      await ensureSession(displayName);
      const maxPlayers = Number(selectedLimit || 4);
      state.room = await api("/v1/rooms", {
        method: "POST",
        body: { max_players: maxPlayers, is_private: action === "create", mode: "party" }
      });
    } else if (action === "join" || action === "join-public") {
      await ensureSession(displayName);
      const code = (action === "join-public" ? selectedRoomCode : enteredCode).trim().toUpperCase();
      if (!/^[A-Z0-9]{6}$/.test(code)) throw new Error("Enter a valid six-character room code.");
      state.room = await api(`/v1/rooms/${code}/join`, { method: "POST", body: {} });
    } else if (action === "any-public") {
      await ensureSession(displayName);
      await refreshPublicRooms({ quiet: true });
      const room = state.publicRooms[0];
      if (!room) throw new Error("No public rooms are open right now. Try again in a moment.");
      state.room = await api(`/v1/rooms/${room.room_code}/join`, { method: "POST", body: {} });
    } else if (action === "ready") {
      const me = state.room?.players?.find(p => p.player_id === state.session?.session_id);
      state.room = await api(`/v1/rooms/${state.room.room_code}/ready`, { method: "POST", body: { ready: !me?.ready } });
    } else if (action === "start") {
      state.room = await api(`/v1/rooms/${state.room.room_code}/start`, { method: "POST", body: {} });
    } else if (action === "leave") {
      await api(`/v1/rooms/${state.room.room_code}/leave`, { method: "POST", body: {} });
      state.room = null;
      closeRelay();
    } else if (action === "refresh-public") {
      await refreshPublicRooms({ quiet: false });
      state.error = "Public rooms refreshed.";
    } else if (action === "copy-code") {
      await navigator.clipboard.writeText(state.room.room_code);
      state.error = "Room code copied.";
    } else if (action === "profile") {
      if (state.session) {
        clearSession();
        state.room = null;
        closeRelay();
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
    await refreshPublicRooms({ quiet: true });
    render();
    syncRelayConnection();
  }
}

let relaySocket = null;
let relayHeartbeatTimer = null;
let relayReconnectTimer = null;
let relayReconnectAttempts = 0;
let relayLastActivityAt = 0;

function closeRelay() {
  const socket = relaySocket;
  relaySocket = null;
  if (relayHeartbeatTimer) clearInterval(relayHeartbeatTimer);
  if (relayReconnectTimer) clearTimeout(relayReconnectTimer);
  relayHeartbeatTimer = null;
  relayReconnectTimer = null;
  relayReconnectAttempts = 0;
  relayLastActivityAt = 0;
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
    socket.close(1000, "Leaving room");
  }
  state.relayStatus = "idle";
  state.relayPackets = 0;
}

function syncRelayConnection() {
  if (!state.room || !state.session?.token) {
    if (relaySocket) closeRelay();
    return;
  }
  if (relaySocket && (relaySocket.readyState === WebSocket.OPEN || relaySocket.readyState === WebSocket.CONNECTING)) {
    return;
  }

  const socketUrl = API_ORIGIN.replace(/^http/, "ws") + `/v1/rooms/${state.room.room_code}/transport`;
  let socket;
  try {
    socket = new WebSocket(socketUrl, ["cbash-v1", `cbash-auth.${state.session.token}`]);
  } catch {
    state.relayStatus = "error";
    render();
    return;
  }
  relaySocket = socket;
  state.relayStatus = "connecting";
  socket.binaryType = "arraybuffer";
  render();

  socket.addEventListener("open", () => {
    if (relaySocket !== socket) return;
    state.relayStatus = "connected";
    relayReconnectAttempts = 0;
    relayLastActivityAt = Date.now();
    relayHeartbeatTimer = setInterval(() => {
      if (relaySocket === socket && socket.readyState === WebSocket.OPEN &&
          Date.now() - relayLastActivityAt >= 20000) {
        socket.send("PING");
        relayLastActivityAt = Date.now();
      }
    }, 20000);
    render();
  });

  socket.addEventListener("message", event => {
    if (relaySocket !== socket) return;
    relayLastActivityAt = Date.now();
    if (typeof event.data === "string") {
      if (event.data === "PONG") return;
      try {
        const message = JSON.parse(event.data);
        if (message.type === "room_update" && message.room) {
          state.room = message.room;
          render();
          syncRelayConnection();
        }
      } catch {
        // Ignore unknown relay control messages.
      }
      return;
    }
    if (!(event.data instanceof ArrayBuffer)) return;
    const packet = new Uint8Array(event.data);
    if (packet.length < 23 || packet[0] !== 0x43 || packet[1] !== 0x42 ||
        packet[2] !== 0x52 || packet[3] !== 0x31) return;
    state.relayPackets += 1;
    render();
  });

  socket.addEventListener("close", event => {
    if (relaySocket !== socket) return;
    relaySocket = null;
    if (relayHeartbeatTimer) clearInterval(relayHeartbeatTimer);
    relayHeartbeatTimer = null;
    state.relayStatus = "disconnected";
    render();
    if (state.room && event.code !== 1008 && relayReconnectAttempts < 3) {
      const delay = 3000 * (2 ** relayReconnectAttempts);
      relayReconnectAttempts += 1;
      relayReconnectTimer = setTimeout(async () => {
        relayReconnectTimer = null;
        const roomCode = state.room?.room_code;
        if (!roomCode) return;
        try {
          state.room = await api(`/v1/rooms/${roomCode}/join`, { method: "POST", body: {} });
          render();
          syncRelayConnection();
        } catch (error) {
          state.error = error.message || "Could not rejoin the room.";
          state.relayStatus = "error";
          render();
        }
      }, delay);
    } else if (state.room) {
      state.relayStatus = "error";
      render();
    }
  });

  socket.addEventListener("error", () => {
    if (relaySocket !== socket) return;
    state.relayStatus = "error";
    render();
  });
}
app.addEventListener("click", event => {
  const button = event.target.closest("[data-action]");
  if (!button) return;
  runAction(button.dataset.action, button.dataset.roomCode || "");
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
  try {
    const health = await fetch(API_ORIGIN + "/health", { cache: "no-store" });
    if (!health.ok) throw new Error("API health check failed.");
    state.health = "online";
  } catch {
    state.health = "offline";
  }
  if (state.health === "online") await refreshPublicRooms({ quiet: true });
  render();
  if (state.session?.token) {
    try {
      const response = await api("/v1/rooms/current");
      state.room = response.room || null;
    } catch {
      // An expired or roomless guest session is harmless; create a new one on demand.
    }
    if (state.room) syncRelayConnection();
  }
}

init();
