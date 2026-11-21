import { applyPadStateToEmulator } from "/pad-bridge.js";

const DATA_PATH = "https://cdn.emulatorjs.org/stable/data/";
const gameInput = document.querySelector("#game-files");
const biosInput = document.querySelector("#bios-file");
const startButton = document.querySelector("#start");
const statusLine = document.querySelector("#status");
const gameWrap = document.querySelector("#game-wrap");

let loaderScript = null;
let emulatorStarted = false;
let emulatorLoading = false;
let activeBlobUrls = [];
let loadTimeout = null;

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

async function buildExternalFiles(primary, files) {
  const external = {};
  const selectedByName = new Map();
  const urlByFile = new Map();
  const blobUrls = [];

  try {
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

      if (!references.length) {
        throw new Error("The selected CUE file does not contain any FILE track entries.");
      }

      for (const reference of references) {
        const basename = reference.split(/[\\/]/).pop().toLowerCase();
        const match = selectedByName.get(basename);
        if (!match) {
          throw new Error(`The CUE file references "${reference}", but that file was not selected.`);
        }
        external[reference] = urlByFile.get(match);
      }
    }

    return { external, blobUrls };
  } catch (error) {
    blobUrls.forEach(url => URL.revokeObjectURL(url));
    throw error;
  }
}
function applyPadState(slot, buttons, sticks = [128, 128, 128, 128]) {
  return applyPadStateToEmulator(window.EJS_emulator?.gameManager, slot, buttons, sticks);
}

function exposeBrowserInputBridge() {
  const gameManager = window.EJS_emulator?.gameManager;
  if (!gameManager?.functions?.simulateInput) return false;

  window.CBASH_APPLY_PAD_STATE = applyPadState;
  window.CBASH_GET_EMULATOR_FRAME = () => {
    try {
      return gameManager.functions.getFrameNum();
    } catch {
      return -1;
    }
  };
  window.CBASH_EMULATOR_READY = true;
  return true;
}

function startEmulator(primary, bios, externalFiles, blobUrls) {
  if (loaderScript) loaderScript.remove();
  releaseLocalObjectUrls();
  activeBlobUrls = blobUrls;
  emulatorStarted = false;
  emulatorLoading = true;
  gameWrap.hidden = false;
  document.querySelector("#game").replaceChildren();

  window.CBASH_EMULATOR_READY = false;
  window.EJS_player = "#game";
  window.EJS_gameName = "Crash Bash";
  window.EJS_gameUrl = primary;
  window.EJS_biosUrl = bios || "";
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
    setStatus("Local game started. Browser-to-PS3 input synchronization is the next integration stage.");
    const began = Date.now();
    const expose = () => {
      if (exposeBrowserInputBridge()) {
        releaseLocalObjectUrls();
        return;
      }
      if (Date.now() - began < 10000) {
        setTimeout(expose, 100);
      } else {
        setStatus("Game started, but the browser input bridge did not initialize.", true);
      }
    };
    expose();
  };
  window.EJS_ready = () => {
    // EmulatorJS 4.2.x emits lifecycle events on the emulator instance.
    window.EJS_emulator?.on("exit", () => {
      emulatorStarted = false;
      emulatorLoading = false;
      startButton.disabled = !selectedGame(Array.from(gameInput.files || []));
      releaseLocalObjectUrls();
      window.CBASH_EMULATOR_READY = false;
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
  if (!primary) {
    setStatus("Select a supported PlayStation disc file first.", true);
    return;
  }
  if (primary.size > 900 * 1024 * 1024) {
    setStatus("This disc image is over 900 MB. Use a CHD/PBP image or a desktop browser with more available memory.", true);
    return;
  }

  startButton.disabled = true;
  try {
    window.EJS_emulator?.gameManager?.exit?.();
    releaseLocalObjectUrls();
    const { external, blobUrls } = await buildExternalFiles(primary, files);
    const bios = biosInput.files?.[0] || "";
    startEmulator(primary, bios, external, blobUrls);
  } catch (error) {
    releaseLocalObjectUrls();
    emulatorLoading = false;
    startButton.disabled = !selectedGame(Array.from(gameInput.files || []));
    setStatus(error.message || "Could not prepare the selected disc files.", true);
  }
});

window.addEventListener("beforeunload", () => {
  if (emulatorStarted) {
    try {
      window.EJS_emulator?.gameManager?.saveSaveFiles?.();
    } catch {}
  }
  releaseLocalObjectUrls();
});
