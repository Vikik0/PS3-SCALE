import { applyPadStateToEmulator, createLocalPadCapture } from "/pad-bridge.js";
import { CrashBashRelayTransport } from "/relay-transport.js";
import { BrowserLockstepClient } from "/lockstep-client.js";

window.CBASH_CREATE_RELAY_TRANSPORT = options => new CrashBashRelayTransport(options);

const DATA_PATH = "https://cdn.emulatorjs.org/stable/data/";
const SESSION_KEY = "crash-bash-online-session-v1";
const API_ORIGIN = (() => {
  try { return localStorage.getItem("crash-bash-online-api-origin-v1") || window.location.origin; }
  catch { return window.location.origin; }
})();
const ROOM_CODE = new URLSearchParams(window.location.search).get("room")?.trim().toUpperCase() || "";
const gameInput = document.querySelector("#game-files");
const biosInput = document.querySelector("#bios-file");
const startButton = document.querySelector("#start");
const connectButton = document.querySelector("#connect-room");
const leaveButton = document.querySelector("#leave-room");
const statusLine = document.querySelector("#status");
const networkStatus = document.querySelector("#network-status");
const gameWrap = document.querySelector("#game-wrap");

let loaderScript = null;
let emulatorStarted = false;
let emulatorLoading = false;
let activeBlobUrls = [];
let loadTimeout = null;
let lockstepClient = null;
let lockstepActive = false;
let localPadCapture = null;

if (!ROOM_CODE) {
  connectButton.hidden = true;
  networkStatus.textContent = "LOCAL MODE · Open this page from a room in the lobby to connect to a PS3 host.";
}
leaveButton.hidden = true;

function releaseLocalObjectUrls() {
  for (const url of activeBlobUrls) URL.revokeObjectURL(url);
  activeBlobUrls = [];
  if (loadTimeout) clearTimeout(loadTimeout);
  loadTimeout = null;
}

function setStatus(message, error = false) {
  statusLine.textContent = message;
  statusLine.style.color = error ? "#ffaaa2" : "#d5d5da";
}

function setNetworkStatus(message, error = false) {
  networkStatus.textContent = message;
  networkStatus.style.color = error ? "#ffaaa2" : "#e9d6a0";
}

function extension(file) {
  return file.name.split(".").pop().toLowerCase();
}

function selectedGame(files) {
  const supported = new Set(["cue", "bin", "chd", "pbp", "iso", "img", "ccd"]);
  const candidates = files.filter(file => supported.has(extension(file)));
  if (!candidates.length) return null;
  return candidates.find(file => extension(file) === "cue") ||
    candidates.find(file => ["chd", "pbp", "iso", "img", "ccd"].includes(extension(file))) ||
    candidates[0];
}

function readSession() {
  try { return JSON.parse(localStorage.getItem(SESSION_KEY) || "null"); }
  catch { return null; }
}

async function api(path, token, { method = "GET", body } = {}) {
  const headers = { Accept: "application/json" };
  if (token) headers.Authorization = `Bearer ${token}`;
  if (body !== undefined) headers["Content-Type"] = "application/json";
  const response = await fetch(new URL(path, API_ORIGIN), {
    method, headers, body: body === undefined ? undefined : JSON.stringify(body)
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(data.message || data.error || `Room request failed (${response.status}).`);
  return data;
}

async function prepareRoomConnection() {
  if (!/^[A-Z0-9]{6}$/.test(ROOM_CODE)) throw new Error("Open the browser player from a valid six-character room link.");
  const session = readSession();
  if (!session?.token || !session?.session_id) throw new Error("Open the lobby first and join the room using Guest Mode.");
  const current = await api("/v1/rooms/current", session.token);
  let room = current.room;
  if (room && room.room_code !== ROOM_CODE) {
    throw new Error(`Your guest session is already in room ${room.room_code}. Leave that room before joining ${ROOM_CODE}.`);
  }
  if (!room) room = await api(`/v1/rooms/${ROOM_CODE}/join`, session.token, { method: "POST", body: {} });
  const hostId = room.host_session_id;
  let host = room.players?.find(player => player.player_id === hostId);
  let own = room.players?.find(player => player.player_id === session.session_id);
  if (!own) throw new Error("Your guest session is not listed in this room.");
  if (hostId === session.session_id) throw new Error("Browser-hosted matches are not supported yet. Join a room created by the PS3 host.");
  if (host?.platform !== "ps3") throw new Error("This room is not marked as a PS3-hosted room. Start a room from the PS3 in-game menu.");
  if (room.status !== "waiting") throw new Error("This match has already started. Join before the PS3 host starts the game.");
  if (room.player_count > room.max_players) throw new Error("The room player count is invalid.");
  if (!own.ready) {
    room = await api(`/v1/rooms/${ROOM_CODE}/ready`, session.token, { method: "POST", body: { ready: true } });
    own = room.players?.find(player => player.player_id === session.session_id);
    host = room.players?.find(player => player.player_id === hostId);
    if (!own?.ready || !host?.ready) throw new Error("The room did not confirm both players as ready.");
  }
  return { session, room };
}

async function buildExternalFiles(primary, files, biosFile = null) {
  const external = {};
  const selectedByName = new Map();
  const urlByFile = new Map();
  const blobUrls = [];
  let primaryUrl = "";
  let biosUrl = "";

  try {
    primaryUrl = URL.createObjectURL(primary);
    blobUrls.push(primaryUrl);
    if (biosFile) {
      biosUrl = URL.createObjectURL(biosFile);
      blobUrls.push(biosUrl);
    }
    for (const file of files) {
      if (file === primary) continue;
      const key = file.name.toLowerCase();
      if (selectedByName.has(key)) {
        throw new Error(`More than one selected file is named "${file.name}". Rename the duplicate before loading.`);
      }
      selectedByName.set(key, file);
      const url = URL.createObjectURL(file);
      blobUrls.push(url);
      urlByFile.set(file, url);
      external[file.name] = url;
    }

    if (extension(primary) === "cue") {
      const cue = await primary.text();
      const references = [...cue.matchAll(/^\s*FILE\s+(?:"([^"]+)"|(\S+))\s+\w+/gim)]
        .map(match => match[1] || match[2]);
      if (!references.length) throw new Error("The selected CUE file does not contain any FILE track entries.");
      for (const reference of references) {
        const basename = reference.split(/[\\/]/).pop().toLowerCase();
        const match = selectedByName.get(basename);
        if (!match) throw new Error(`The CUE file references "${reference}", but that file was not selected.`);
        external[reference] = urlByFile.get(match);
      }
    }
    return { external, blobUrls, primaryUrl, biosUrl };
  } catch (error) {
    blobUrls.forEach(url => URL.revokeObjectURL(url));
    throw error;
  }
}

function gameManager() { return window.EJS_emulator?.gameManager || null; }

function applyPadState(slot, buttons, sticks = [128, 128, 128, 128]) {
  return applyPadStateToEmulator(gameManager(), slot, buttons, sticks);
}

function exposeBrowserInputBridge() {
  const manager = gameManager();
  if (!manager?.functions?.simulateInput || typeof manager.getFrameNum !== "function") return false;
  window.CBASH_APPLY_PAD_STATE = applyPadState;
  window.CBASH_GET_EMULATOR_FRAME = () => {
    try { return manager.getFrameNum(); } catch { return -1; }
  };
  window.CBASH_EMULATOR_READY = true;
  return true;
}

function installLocalInputCapture() {
  if (localPadCapture) return true;
  const manager = gameManager();
  if (!manager || typeof manager.simulateInput !== "function") return false;
  localPadCapture = createLocalPadCapture(manager, () => lockstepActive);
  return true;
}

function advanceEmulatorFrames(count) {
  const manager = gameManager();
  if (!manager || typeof manager.getFrameNum !== "function" || typeof manager.toggleMainLoop !== "function") {
    return Promise.reject(new Error("The EmulatorJS frame-control API is not ready."));
  }
  manager.toggleMainLoop(0);
  const firstFrame = Number(manager.getFrameNum());
  if (!Number.isFinite(firstFrame)) return Promise.reject(new Error("Could not read the EmulatorJS frame counter."));
  return new Promise((resolve, reject) => {
    let raf = 0;
    const timeout = setTimeout(() => finish(new Error("The browser emulator stopped advancing frames.")), 5000);
    const finish = error => {
      clearTimeout(timeout);
      if (raf) cancelAnimationFrame(raf);
      manager.toggleMainLoop(0);
      if (error) reject(error); else resolve();
    };
    const poll = () => {
      const current = Number(manager.getFrameNum());
      if (Number.isFinite(current) && current - firstFrame >= count) { finish(); return; }
      raf = requestAnimationFrame(poll);
    };
    manager.toggleMainLoop(1);
    raf = requestAnimationFrame(poll);
  });
}

function stopLockstep(message = "Disconnected from the PS3 room.") {
  lockstepActive = false;
  try { gameManager()?.toggleMainLoop?.(0); } catch {}
  if (lockstepClient) {
    const old = lockstepClient;
    lockstepClient = null;
    if (old.phase !== "stopped" && old.phase !== "failed" && old.phase !== "closed") old.stop();
  }
  leaveButton.hidden = true;
  connectButton.disabled = !emulatorStarted || !ROOM_CODE;
  if (message) setNetworkStatus(message, true);
}

async function connectToRoom() {
  if (!emulatorStarted || !window.CBASH_EMULATOR_READY || !localPadCapture) {
    setNetworkStatus("Load the PlayStation game before connecting to a room.", true);
    return;
  }
  if (lockstepClient) return;
  connectButton.disabled = true;
  setNetworkStatus("Checking the room and guest session…");
  let pendingSession = null;
  try {
    const { session, room } = await prepareRoomConnection();
    pendingSession = session;
    const transport = new CrashBashRelayTransport({
      apiOrigin: API_ORIGIN, roomCode: ROOM_CODE, token: session.token
    });
    let client;
    client = new BrowserLockstepClient({
      transport,
      hostSessionId: room.host_session_id,
      capture: localPadCapture,
      applyPadState,
      advanceFrames: advanceEmulatorFrames,
      onStatus: (message, error) => setNetworkStatus(message, error),
      onMatchStart: () => {
        lockstepActive = true;
        leaveButton.hidden = false;
        setNetworkStatus("PS3 match started. Browser input is now synchronized through the relay.");
      },
      onStop: ({ message } = {}) => {
        lockstepActive = false;
        try { gameManager()?.toggleMainLoop?.(0); } catch {}
        leaveButton.hidden = true;
        if (lockstepClient === client) lockstepClient = null;
        connectButton.disabled = !emulatorStarted || !ROOM_CODE;
        if (message) setNetworkStatus(message, true);
      }
    });
    lockstepClient = client;
    await client.connect();
    pendingSession = null;
    leaveButton.hidden = false;
    setNetworkStatus(`Connected to room ${ROOM_CODE}. Waiting for the PS3 host to start the match…`);
  } catch (error) {
    lockstepClient = null;
    if (pendingSession?.token) {
      try {
        await api(`/v1/rooms/${ROOM_CODE}/ready`, pendingSession.token, { method: "POST", body: { ready: false } });
      } catch {}
    }
    connectButton.disabled = !emulatorStarted || !ROOM_CODE;
    setNetworkStatus(error.message || "Could not connect to the PS3 room.", true);
  }
}

function startEmulator(primaryUrl, biosUrl, externalFiles, blobUrls) {
  if (loaderScript) loaderScript.remove();
  if (lockstepClient) stopLockstep("Previous room connection closed.");
  releaseLocalObjectUrls();
  activeBlobUrls = blobUrls;
  emulatorStarted = false;
  emulatorLoading = true;
  localPadCapture = null;
  gameWrap.hidden = false;
  document.querySelector("#game").replaceChildren();

  window.CBASH_EMULATOR_READY = false;
  window.EJS_player = "#game";
  window.EJS_gameName = "Crash Bash";
  window.EJS_gameUrl = primaryUrl;
  window.EJS_biosUrl = biosUrl || "";
  window.EJS_core = "psx";
  window.EJS_defaultOptions = {
    ...(window.EJS_defaultOptions || {}),
    pcsx_rearmed_multitap: "port 1 only"
  };
  window.EJS_pathtodata = DATA_PATH;
  window.EJS_externalFiles = externalFiles;
  window.EJS_startOnLoaded = true;
  window.EJS_threads = false;
  window.EJS_askBeforeExit = false;
  window.EJS_AdUrl = "";
  window.EJS_onGameStart = () => {
    if (loadTimeout) clearTimeout(loadTimeout);
    loadTimeout = null;
    emulatorStarted = true;
    emulatorLoading = false;
    const manager = gameManager();
    try { manager?.toggleMainLoop?.(0); } catch {}
    const began = Date.now();
    const expose = () => {
      if (exposeBrowserInputBridge()) {
        installLocalInputCapture();
        releaseLocalObjectUrls();
        startButton.disabled = true;
        connectButton.disabled = !ROOM_CODE || !readSession()?.token;
        setStatus("Game loaded and paused. Connect to the PS3 room to begin synchronized play.");
        if (ROOM_CODE) setNetworkStatus(`Room ${ROOM_CODE} selected. Connect when the PS3 host is waiting.`);
        return;
      }
      if (Date.now() - began < 10000) setTimeout(expose, 100);
      else setStatus("Game started, but the browser input bridge did not initialize.", true);
    };
    expose();
  };
  window.EJS_ready = () => {
    window.EJS_emulator?.on("exit", () => {
      if (lockstepClient) stopLockstep("The browser emulator closed.");
      emulatorStarted = false;
      emulatorLoading = false;
      startButton.disabled = !selectedGame(Array.from(gameInput.files || []));
      connectButton.disabled = true;
      releaseLocalObjectUrls();
      window.CBASH_EMULATOR_READY = false;
      localPadCapture?.reset();
      localPadCapture = null;
      setStatus("Emulator closed. Your disc image and BIOS remain on this device.");
    });
  };

  loaderScript = document.createElement("script");
  loaderScript.src = DATA_PATH + "loader.js";
  loaderScript.async = true;
  loaderScript.onerror = () => {
    emulatorLoading = false;
    startButton.disabled = !selectedGame(Array.from(gameInput.files || []));
    releaseLocalObjectUrls();
    setStatus("Could not load the EmulatorJS runtime CDN. Check the internet connection and try again.", true);
  };
  document.body.appendChild(loaderScript);
  loadTimeout = setTimeout(() => {
    if (emulatorStarted) return;
    emulatorLoading = false;
    startButton.disabled = !selectedGame(Array.from(gameInput.files || []));
    if (loaderScript) loaderScript.remove();
    releaseLocalObjectUrls();
    setStatus("The PlayStation runtime did not finish loading. Check the selected disc files and try again.", true);
  }, 60000);
  setStatus("Loading the PlayStation runtime and your local disc image…");
}

gameInput.addEventListener("change", () => {
  const files = Array.from(gameInput.files || []);
  const primary = selectedGame(files);
  startButton.disabled = !primary || emulatorLoading || emulatorStarted;
  if (!primary) {
    setStatus("Choose a CUE, BIN, CHD, PBP, ISO, IMG, or CCD file.", true);
    return;
  }
  setStatus(`Selected ${primary.name} · ${(primary.size / (1024 * 1024)).toFixed(1)} MB. Files stay in this browser.`);
});

startButton.addEventListener("click", async () => {
  const files = Array.from(gameInput.files || []);
  const primary = selectedGame(files);
  if (!primary) { setStatus("Select a supported PlayStation disc file first.", true); return; }
  if (primary.size > 900 * 1024 * 1024) {
    setStatus("This disc image is over 900 MB. Use a CHD/PBP image or a desktop browser with more available memory.", true);
    return;
  }
  startButton.disabled = true;
  try {
    window.EJS_emulator?.gameManager?.exit?.();
    releaseLocalObjectUrls();
    const { external, blobUrls, primaryUrl, biosUrl } = await buildExternalFiles(primary, files, biosInput.files?.[0] || null);
    startEmulator(primaryUrl, biosUrl, external, blobUrls);
  } catch (error) {
    releaseLocalObjectUrls();
    emulatorLoading = false;
    startButton.disabled = !selectedGame(Array.from(gameInput.files || []));
    setStatus(error.message || "Could not prepare the selected disc files.", true);
  }
});

connectButton.addEventListener("click", connectToRoom);
leaveButton.addEventListener("click", () => {
  if (lockstepClient) lockstepClient.stop();
  stopLockstep("Left the PS3 match. The browser game remains paused.");
});

window.addEventListener("beforeunload", () => {
  if (lockstepClient) lockstepClient.stop();
  if (emulatorStarted) {
    try { gameManager()?.functions?.saveSaveFiles?.(); } catch {}
  }
  releaseLocalObjectUrls();
});
