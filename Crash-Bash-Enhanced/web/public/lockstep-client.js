import {
  CBN1_TYPES, CBN1_BYE_REASONS,
  decodeCBN1, decodeWelcome, decodePing, decodeSave, decodeStart, decodeRecord, decodeBye,
  encodeJoin, encodePong, encodeReady, encodeInput, encodeBye
} from "./cbn1.js";
import { decodePadRecord } from "./pad-bridge.js";

const READY_SAVES = 0x0f;
const READY_START = 0x10;
const READY_ALL = 0x1f;
const MAX_DELAY = 30;

function neutralPad() {
  return new Uint8Array([0x03, 0, 0, 0x80, 0x80, 0x80, 0x80]);
}

function randomNonce() {
  const value = new Uint32Array(1);
  if (globalThis.crypto?.getRandomValues) globalThis.crypto.getRandomValues(value);
  else value[0] = Math.floor(Math.random() * 0x100000000);
  return value[0] >>> 0;
}

function reasonText(reason) {
  return ({
    [CBN1_BYE_REASONS.LEFT]: "The PS3 host left the match.",
    [CBN1_BYE_REASONS.DIFFERS]: "The game copies are no longer synchronized.",
    [CBN1_BYE_REASONS.FULL]: "The PS3 host's match is full or has already started.",
    [CBN1_BYE_REASONS.VERSION]: "The browser and PS3 game builds are incompatible.",
    [CBN1_BYE_REASONS.DATA]: "The host sent save data this browser build cannot use.",
    [CBN1_BYE_REASONS.BEHIND]: "The browser fell too far behind the host. Rejoin for the next match."
  })[reason] || "The PS3 host ended the online session.";
}

export class BrowserLockstepClient {
  constructor({ transport, hostSessionId, capture, applyPadState, advanceFrames, onStatus = () => {}, onMatchStart = () => {}, onStop = () => {}, nonce = randomNonce }) {
    if (!transport) throw new TypeError("A room relay transport is required.");
    if (!/^[0-9a-f]{32}$/i.test(String(hostSessionId || ""))) throw new TypeError("A valid PS3 host session ID is required.");
    if (!capture || typeof capture.snapshot !== "function") throw new TypeError("A browser pad capture is required.");
    if (typeof applyPadState !== "function") throw new TypeError("An EmulatorJS pad-state function is required.");
    if (typeof advanceFrames !== "function") throw new TypeError("An EmulatorJS frame-step function is required.");
    this.transport = transport;
    this.hostSessionId = hostSessionId.toLowerCase();
    this.capture = capture;
    this.applyPadState = applyPadState;
    this.advanceFrames = advanceFrames;
    this.onStatus = onStatus;
    this.onMatchStart = onMatchStart;
    this.onStop = onStop;
    this.nonceFactory = nonce;
    this.phase = "idle";
    this.nonce = 0;
    this.session = 0;
    this.me = -1;
    this.readyMask = 0;
    this.delay = 0;
    this.assignments = [];
    this.inputHistory = [];
    this.inputBase = 0;
    this.meHave = 0;
    this.hostAcked = 0;
    this.have = 0;
    this.hostNext = 0;
    this.next = 0;
    this.records = new Map();
    this.processing = false;
    this.joinTimer = null;
    this.unsubscribers = [];
    this.lastError = "";
  }

  status(message, error = false) { this.onStatus(message, error); }

  send(packet, key) {
    const sent = this.transport.sendPacket(packet, this.hostSessionId, key);
    if (!sent) this.status("The room relay queue is full; waiting for the next input batch.", true);
    return sent;
  }

  async connect() {
    if (this.phase !== "idle") throw new Error("This browser lockstep client has already been used.");
    this.phase = "connecting";
    this.unsubscribers.push(
      this.transport.on("packet", packet => this.handlePacket(packet)),
      this.transport.on("control", message => this.handleControl(message)),
      this.transport.on("close", event => this.handleClose(event)),
      this.transport.on("error", error => this.status(error?.message || "Room relay connection error.", true))
    );
    try {
      await this.transport.connect();
      this.phase = "joining";
      this.nonce = this.nonceFactory() >>> 0;
      this.sendJoin();
      this.joinTimer = setInterval(() => {
        if (this.phase === "joining") this.sendJoin();
      }, 250);
      this.status("Connected to the room relay. Waiting for the PS3 host handshake…");
      return this;
    } catch (error) {
      this.phase = "failed";
      this.cleanup(false);
      throw error;
    }
  }

  sendJoin() {
    this.send(encodeJoin(this.nonce, 1), "cbn1-join");
  }

  handleControl(message) {
    if (typeof message !== "string") return;
    if (message.startsWith("ERROR:")) {
      this.fail(`Room relay rejected the session: ${message.slice(6).replaceAll("_", " ").toLowerCase()}.`);
      return;
    }
    try {
      const control = JSON.parse(message);
      if (control.type === "room_update" && control.room?.status === "running" && this.phase === "lobby") {
        this.status("The PS3 host started the room. Waiting for the CBN1 game-start handshake…");
      }
    } catch { /* STARTED and other plain control messages are informational only. */ }
  }

  handlePacket(envelope) {
    if (!envelope || envelope.senderId?.toLowerCase() !== this.hostSessionId) return;
    let packet;
    try { packet = decodeCBN1(envelope.payload); }
    catch (error) { this.status(error.message, true); return; }
    try {
      switch (packet.type) {
        case CBN1_TYPES.WELCOME: this.handleWelcome(envelope.payload); break;
        case CBN1_TYPES.PING: this.handlePing(envelope.payload); break;
        case CBN1_TYPES.SAVE: this.handleSave(envelope.payload); break;
        case CBN1_TYPES.START: this.handleStart(envelope.payload); break;
        case CBN1_TYPES.RECORD: this.handleRecord(envelope.payload); break;
        case CBN1_TYPES.BYE: this.handleBye(envelope.payload); break;
        default: break;
      }
    } catch (error) {
      this.fail(error.message || "The PS3 host sent an invalid lockstep packet.");
    }
  }

  handleWelcome(packet) {
    if (this.phase !== "joining" && this.phase !== "lobby") return;
    const welcome = decodeWelcome(packet, this.nonce);
    if (welcome.refused) {
      const message = welcome.refused === CBN1_BYE_REASONS.VERSION
        ? "The PS3 host is running an incompatible game build."
        : welcome.refused === CBN1_BYE_REASONS.DATA
          ? "The game data does not match the PS3 host."
          : "The PS3 host cannot accept another player right now.";
      this.fail(message, false);
      return;
    }
    if (welcome.machine < 1 || welcome.machine > 3) throw new Error("The host assigned an invalid machine number.");
    this.session = welcome.session;
    this.me = welcome.machine;
    this.phase = "lobby";
    if (this.joinTimer) clearInterval(this.joinTimer);
    this.joinTimer = null;
    this.status(`Joined the PS3 host as machine ${this.me + 1}. Waiting for lobby pings and save-slot checks…`);
  }

  handlePing(packet) {
    if (!this.session || (this.phase !== "lobby" && this.phase !== "starting" && this.phase !== "playing")) return;
    const ping = decodePing(packet);
    if (ping.session !== this.session) return;
    if (ping.me !== this.me || ping.machines < 2 || ping.machines > 4 || ping.pads.length !== ping.machines) {
      throw new Error("The PS3 host sent an inconsistent lobby roster.");
    }
    if (ping.pads[this.me] !== 1) throw new Error("The browser room is configured for one controller per browser player.");
    this.send(encodePong(this.session, this.me, ping.sent), "cbn1-pong");
  }

  handleSave(packet) {
    if (!this.session || this.phase !== "lobby" && this.phase !== "starting") return;
    const save = decodeSave(packet);
    if (save.session !== this.session) return;
    if (save.slot < 0 || save.slot > 3) throw new Error("The host sent an invalid save-slot number.");
    if (save.data.byteLength !== 0) {
      this.send(encodeBye(this.session, this.me, CBN1_BYE_REASONS.DATA, 0), "cbn1-bye");
      this.fail("This first browser milestone supports empty PS3 game-save slots only. The host must use a fresh Battle-mode save profile.", false);
      return;
    }
    this.readyMask |= 1 << save.slot;
    this.send(encodeReady(this.session, this.me, this.readyMask), "cbn1-ready");
    this.status(`PS3 save slot ${save.slot + 1}/4 checked. ${this.readyMask & READY_SAVES ? "" : ""}Waiting for the host to prepare the match…`);
  }

  handleStart(packet) {
    if (!this.session || (this.phase !== "lobby" && this.phase !== "starting")) return;
    const start = decodeStart(packet);
    if (start.session !== this.session) return;
    if (start.me !== this.me || start.machines < 2 || start.machines > 4 || start.assignments.length !== start.machines) {
      throw new Error("The PS3 host sent an invalid start roster.");
    }
    if ((this.readyMask & READY_SAVES) !== READY_SAVES) throw new Error("The PS3 host started before all four empty save slots were acknowledged.");
    if (!Number.isInteger(start.delay) || start.delay < 1 || start.delay > MAX_DELAY) throw new Error("The PS3 host selected an unsupported input delay.");
    const occupied = new Set();
    for (const assignment of start.assignments) {
      if (assignment.pads < 1 || assignment.pads > 4 || assignment.first < 0 || assignment.first + assignment.pads > 4) {
        throw new Error("The PS3 host assigned an invalid controller range.");
      }
      for (let slot = assignment.first; slot < assignment.first + assignment.pads; slot++) {
        if (occupied.has(slot)) throw new Error("The PS3 host assigned the same controller slot twice.");
        occupied.add(slot);
      }
    }
    const mine = start.assignments[this.me];
    if (!mine || mine.pads !== 1) throw new Error("Each browser participant must use exactly one controller.");
    this.delay = start.delay;
    this.assignments = start.assignments;
    this.readyMask |= READY_START;
    this.inputHistory = Array.from({ length: this.delay }, neutralPad);
    this.inputBase = 0;
    this.meHave = this.delay;
    this.hostAcked = 0;
    this.have = 0;
    this.hostNext = 0;
    this.next = 0;
    this.records.clear();
    this.phase = "playing";
    this.send(encodeReady(this.session, this.me, this.readyMask | READY_ALL), "cbn1-ready");
    this.sendInputs();
    this.onMatchStart({ me: this.me, delay: this.delay, assignments: this.assignments });
    this.status(`Game handshake complete. Input delay: ${this.delay} steps. Waiting for the first synchronized frame…`);
  }

  handleRecord(packet) {
    if (!this.session || this.phase !== "playing") return;
    const record = decodeRecord(packet);
    if (record.session !== this.session) return;
    if (record.acked > this.hostAcked && record.acked <= this.meHave) {
      this.hostAcked = record.acked;
      this.pruneAcknowledgedInputs();
    }
    if (record.hostNext > this.hostNext) this.hostNext = record.hostNext;
    for (let i = 0; i < record.records.length; i++) {
      const step = record.first + i;
      if (step === this.have) {
        this.records.set(step, record.records[i]);
        this.have++;
      }
    }
    if (record.quiet) this.status("The PS3 host is temporarily playing without waiting for this browser. Reconnecting inputs…", true);
    else if (record.slow) this.status("Network delay detected. The match is waiting for synchronized inputs.", true);
    this.sendInputs();
    this.processNextStep();
  }

  pruneAcknowledgedInputs() {
    while (this.inputBase < this.hostAcked && this.inputHistory.length) {
      this.inputHistory.shift();
      this.inputBase++;
    }
  }

  sendInputs() {
    if (!this.session || this.phase !== "playing") return;
    const start = Math.max(this.inputBase, this.hostAcked);
    const offset = start - this.inputBase;
    const count = Math.max(0, Math.min(32, this.meHave - start));
    const records = this.inputHistory.slice(offset, offset + count);
    const packet = encodeInput({
      session: this.session, me: this.me, hostHave: this.have, hostAcked: this.hostAcked,
      records, printStep: -1, printValue: 0
    });
    this.send(packet, "cbn1-input");
  }

  async processNextStep() {
    if (this.phase !== "playing" || this.processing || !this.records.has(this.next)) return;
    this.processing = true;
    const step = this.next;
    const authoritative = this.records.get(step);
    this.records.delete(step);
    try {
      const local = this.capture.snapshot();
      this.inputHistory.push(local);
      this.meHave++;
      this.sendInputs();
      for (let slot = 0; slot < 4; slot++) {
        const pad = decodePadRecord(authoritative, slot * 7);
        this.applyPadState(slot, pad.connected ? pad.buttons : 0, pad.sticks);
      }
      await this.advanceFrames(2);
      if (this.phase !== "playing") return;
      this.next = step + 1;
    } catch (error) {
      this.fail(error.message || "The browser emulator could not advance the synchronized frame.");
    } finally {
      this.processing = false;
      if (this.phase === "playing") this.processNextStep();
    }
  }

  handleBye(packet) {
    const bye = decodeBye(packet);
    if (this.session && bye.session !== this.session) return;
    this.fail(reasonText(bye.reason), false);
  }

  handleClose(event) {
    if (["closed", "failed", "stopped"].includes(this.phase)) return;
    this.phase = "disconnected";
    this.clearTimers();
    this.onStop({ reason: "relay-closed", event });
    this.status("The room relay disconnected. The browser game is paused; rejoin before starting another match.", true);
  }

  fail(message, sendBye = true) {
    if (["failed", "stopped", "closed"].includes(this.phase)) return;
    this.lastError = message;
    if (sendBye && this.session) {
      try { this.send(encodeBye(this.session, this.me < 0 ? 0 : this.me, CBN1_BYE_REASONS.LEFT, this.next), "cbn1-bye"); } catch {}
    }
    this.phase = "failed";
    this.cleanup(true);
    this.onStop({ reason: "failed", message });
    this.status(message, true);
  }

  clearTimers() {
    if (this.joinTimer) clearInterval(this.joinTimer);
    this.joinTimer = null;
  }

  cleanup(closeTransport) {
    this.clearTimers();
    for (const unsubscribe of this.unsubscribers.splice(0)) {
      try { unsubscribe(); } catch {}
    }
    if (closeTransport) this.transport.close(1000, "Lockstep session ended");
  }

  stop() {
    if (["stopped", "closed"].includes(this.phase)) return;
    if (this.session && this.me >= 0 && this.phase === "playing") {
      this.send(encodeBye(this.session, this.me, CBN1_BYE_REASONS.LEFT, this.next), "cbn1-bye");
    }
    this.phase = "stopped";
    this.cleanup(true);
    this.onStop({ reason: "user-left" });
  }
}

export const CBN1_READY_FLAGS = Object.freeze({ SAVES: READY_SAVES, START: READY_START, ALL: READY_ALL });
