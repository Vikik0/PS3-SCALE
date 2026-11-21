import {
  CBN1_TYPES,
  decodeCBN1,
  decodePing,
  decodeRecord,
  decodeSave,
  decodeStart,
  decodeWelcome,
  encodeBye,
  encodeInput,
  encodeJoin,
  encodePong,
  encodeReady
} from "./cbn1.js";

const HISTORY = 2048;
const PER_PACKET = 32;
const RESEND_INTERVAL_MS = 12;
const HOST_TIMEOUT_MS = 6000;
const JOIN_RETRY_MS = 250;
const READY_SAVES = 0x0f;
const READY_START = 0x10;
const READY_ALL = READY_SAVES | READY_START;
const PAD_THERE = 0x01;
const PAD_STICKS = 0x02;

function neutralPad() {
  return new Uint8Array([PAD_THERE | PAD_STICKS, 0, 0, 128, 128, 128, 128]);
}

function makeNeutralRecord(pads) {
  const record = new Uint8Array(pads * 7);
  for (let i = 0; i < pads; i++) record.set(neutralPad(), i * 7);
  return record;
}

function nonce32() {
  const value = new Uint32Array(1);
  crypto.getRandomValues(value);
  return value[0] >>> 0;
}

/**
 * Browser-side CBN1 joiner. The PS3 remains the host; the browser sends
 * the exact protocol-2 packets used by the pinned Crash Bash netcode.
 * The browser runtime advances one synchronized record at a time.
 */
export class CBN1BrowserClient {
  constructor({
    transport,
    hostSessionId,
    pads = 1,
    getLocalPads = () => [neutralPad()],
    onSave = () => {},
    onState = () => {},
    now = () => Date.now(),
    nonce = nonce32()
  }) {
    if (!transport || typeof transport.sendPacket !== "function") {
      throw new TypeError("A connected Crash Bash relay transport is required.");
    }
    if (!/^[0-9a-f]{32}$/i.test(String(hostSessionId || ""))) {
      throw new TypeError("A valid PS3 host session ID is required.");
    }
    if (!Number.isInteger(pads) || pads < 1 || pads > 4) {
      throw new RangeError("Browser player count must be between 1 and 4.");
    }
    this.transport = transport;
    this.hostSessionId = String(hostSessionId).toLowerCase();
    this.localPads = pads;
    this.getLocalPads = getLocalPads;
    this.onSave = onSave;
    this.onState = onState;
    this.now = now;
    this.nonce = nonce >>> 0;

    this.state = "idle";
    this.session = 0;
    this.me = -1;
    this.ready = 0;
    this.delay = 0;
    this.machines = 0;
    this.firstPad = -1;
    this.assignments = [];
    this.localHave = 0;
    this.hostAcked = 0;
    this.have = 0;
    this.next = 0;
    this.hostNext = 0;
    this.localInputs = new Array(HISTORY);
    this.records = new Array(HISTORY);
    this.lastHostHeard = 0;
    this.lastQuiet = false;
    this.gone = 0;
    this.slow = 0;
    this.pendingSaves = new Map();
    this.joinTimer = null;
    this.joinDeadline = 0;
    this.stopped = false;
    this.waiters = new Set();
    this.unsubscribePacket = null;
    this.unsubscribeControl = null;
    this.resolveWelcome = null;
    this.rejectWelcome = null;
    this.welcomePromise = null;
  }

  setState(state, detail = null) {
    this.state = state;
    try { this.onState({ state, detail, client: this }); } catch {}
  }

  async connect() {
    if (this.state !== "idle") throw new Error("CBN1 client has already been started.");
    this.stopped = false;
    this.welcomePromise = new Promise((resolve, reject) => {
      this.resolveWelcome = resolve;
      this.rejectWelcome = reject;
    });
    this.unsubscribePacket = this.transport.on("packet", packet => {
      if (packet?.senderId === this.hostSessionId) {
        try { this.handlePacket(packet.payload); }
        catch (error) { this.fail(error); }
      }
    });
    this.unsubscribeControl = this.transport.on("control", message => {
      if (typeof message === "string" && message.startsWith("ERROR")) {
        this.fail(new Error(message));
      }
    });
    this.transport.on("error", error => {
      if (this.state !== "playing") this.fail(error instanceof Error ? error : new Error("Relay transport error."));
    });
    this.transport.on("close", () => {
      if (!this.stopped && this.state !== "closed") this.fail(new Error("The room relay connection closed."));
    });

    this.setState("connecting");
    try {
      await this.transport.connect();
    } catch (error) {
      this.fail(error);
      throw error;
    }

    this.lastHostHeard = this.now();
    this.joinDeadline = this.now() + 30000;
    this.setState("joining");
    this.sendJoin();
    this.joinTimer = setInterval(() => {
      if (this.state !== "joining") return;
      if (this.now() >= this.joinDeadline) {
        this.fail(new Error("Timed out waiting for the PS3 host to accept the browser player."));
        return;
      }
      this.sendJoin();
    }, JOIN_RETRY_MS);

    return this.welcomePromise;
  }

  sendJoin() {
    if (this.stopped || this.state !== "joining") return;
    const packet = encodeJoin(this.nonce, this.localPads);
    this.transport.sendPacket(packet, this.hostSessionId, "join");
  }

  sendReady() {
    if (!this.session || this.me < 0 || this.stopped) return false;
    return this.transport.sendPacket(
      encodeReady(this.session, this.me, this.ready),
      this.hostSessionId,
      "ready"
    );
  }

  handlePacket(packet) {
    const { type } = decodeCBN1(packet);
    switch (type) {
      case CBN1_TYPES.WELCOME:
        this.handleWelcome(packet);
        break;
      case CBN1_TYPES.PING:
        this.handlePing(packet);
        break;
      case CBN1_TYPES.SAVE:
        this.handleSave(packet);
        break;
      case CBN1_TYPES.START:
        this.handleStart(packet);
        break;
      case CBN1_TYPES.RECORD:
        this.handleRecord(packet);
        break;
      case CBN1_TYPES.BYE:
        this.handleBye(packet);
        break;
      default:
        break;
    }
  }

  handleWelcome(packet) {
    if (this.state !== "joining") return;
    const welcome = decodeWelcome(packet, this.nonce);
    if (welcome.refused !== 0) {
      const reasons = {
        3: "The PS3 host's game is full or has already started.",
        4: "The browser and PS3 game builds do not match.",
        5: "The browser and PS3 game data do not match."
      };
      throw new Error(reasons[welcome.refused] || `The PS3 host refused the browser (code ${welcome.refused}).`);
    }
    this.session = welcome.session;
    this.me = welcome.machine;
    this.lastHostHeard = this.now();
    if (this.joinTimer) clearInterval(this.joinTimer);
    this.joinTimer = null;
    this.setState("lobby", { machine: this.me, session: this.session });
    this.resolveWelcome?.(this);
    this.resolveWelcome = null;
    this.rejectWelcome = null;
  }

  handlePing(packet) {
    const ping = decodePing(packet);
    if (!this.session || ping.session !== this.session) return;
    this.lastHostHeard = this.now();
    this.transport.sendPacket(
      encodePong(this.session, this.me, ping.sent),
      this.hostSessionId
    );
    this.setState(this.state, {
      machines: ping.machines,
      me: ping.me,
      pads: Array.from(ping.pads)
    });
  }

  handleSave(packet) {
    const save = decodeSave(packet);
    if (save.session !== this.session || save.slot < 0 || save.slot > 3) return;
    this.lastHostHeard = this.now();
    if (!(this.ready & (1 << save.slot))) {
      // Save import into EmulatorJS memory cards is not implemented yet.
      // Keep the bytes available for a future adapter; minigame/Battle testing can proceed.
      this.pendingSaves.set(save.slot, save.data.slice());
      try { this.onSave(save.slot, save.data.slice()); } catch {}
      this.ready |= 1 << save.slot;
    }
    this.sendReady();
    if (this.state === "lobby") this.setState("lobby", { savesReady: this.ready & READY_SAVES });
  }

  handleStart(packet) {
    const start = decodeStart(packet);
    if (start.session !== this.session) return;
    if (start.me < 1 || start.me >= start.machines) throw new Error("The PS3 host assigned an invalid browser machine slot.");
    const assignment = start.assignments[start.me];
    if (!assignment || assignment.pads !== this.localPads || assignment.first + assignment.pads > 4) {
      throw new Error("The PS3 host assigned an unsupported number of browser controller ports.");
    }

    this.me = start.me;
    this.delay = start.delay;
    this.machines = start.machines;
    this.assignments = start.assignments;
    this.firstPad = assignment.first;
    this.localHave = 0;
    this.hostAcked = 0;
    this.have = 0;
    this.next = 0;
    this.hostNext = 0;
    this.localInputs = new Array(HISTORY);
    this.records = new Array(HISTORY);
    for (let i = 0; i < this.delay; i++) {
      this.localInputs[this.localHave % HISTORY] = makeNeutralRecord(this.localPads);
      this.localHave++;
    }

    this.ready |= READY_START;
    this.lastHostHeard = this.now();
    this.sendReady();
    this.setState(this.ready === READY_ALL ? "playing" : "starting", {
      delay: this.delay,
      machine: this.me,
      firstPad: this.firstPad,
      pads: this.localPads,
      machines: this.assignments
    });
  }

  handleRecord(packet) {
    const record = decodeRecord(packet);
    if (record.session !== this.session || this.state !== "playing") return;
    if (record.first < 0) throw new Error("The PS3 host sent an invalid CBN1 record index.");

    this.lastHostHeard = this.now();
    if (record.hostNext >= this.hostNext) {
      this.lastQuiet = record.quiet;
      this.gone = record.gone;
      this.slow = record.slow;
    }
    if (record.acked > this.hostAcked && record.acked <= this.localHave) {
      this.hostAcked = record.acked;
    }
    if (record.hostNext > this.hostNext) this.hostNext = record.hostNext;

    for (let i = 0; i < record.records.length; i++) {
      const step = record.first + i;
      if (step === this.have && this.have < this.next + HISTORY / 2) {
        this.records[this.have % HISTORY] = record.records[i].slice();
        this.have++;
      }
    }
    this.notifyWaiters();
  }

  handleBye(packet) {
    const body = decodeCBN1(packet).body;
    if (body.byteLength < 10 || new DataView(body.buffer, body.byteOffset, body.byteLength).getUint32(0, true) !== this.session) return;
    const reason = body[5];
    const step = new DataView(body.buffer, body.byteOffset, body.byteLength).getInt32(6, true);
    const labels = {
      2: `The host detected a game-state mismatch at step ${step}.`,
      6: "The browser fell too far behind the other players."
    };
    this.fail(new Error(labels[reason] || `The PS3 host ended the match (reason ${reason}).`));
  }

  sendInputs() {
    if (this.state !== "playing" || !this.session || this.stopped) return false;
    const available = Math.max(0, this.localHave - this.hostAcked);
    const count = Math.min(available, PER_PACKET);
    const records = [];
    for (let i = 0; i < count; i++) {
      const record = this.localInputs[(this.hostAcked + i) % HISTORY];
      if (!record) break;
      records.push(record);
    }
    return this.transport.sendPacket(
      encodeInput({
        session: this.session,
        me: this.me,
        hostHave: this.have,
        hostAcked: this.hostAcked,
        records,
        printStep: -1,
        printValue: 0
      }),
      this.hostSessionId,
      "input"
    );
  }

  notifyWaiters() {
    for (const wake of this.waiters) wake();
    this.waiters.clear();
  }

  waitBriefly(ms) {
    return new Promise(resolve => {
      let done = false;
      const finish = () => {
        if (done) return;
        done = true;
        clearTimeout(timer);
        this.waiters.delete(finish);
        resolve();
      };
      const timer = setTimeout(finish, ms);
      this.waiters.add(finish);
    });
  }

  async step() {
    if (this.state !== "playing" || this.stopped) throw new Error("The browser CBN1 client is not in a running match.");
    const pads = this.getLocalPads() || [];
    const local = makeNeutralRecord(this.localPads);
    for (let i = 0; i < this.localPads; i++) {
      const pad = pads[i];
      if (pad instanceof Uint8Array && pad.byteLength === 7) local.set(pad, i * 7);
    }
    this.localInputs[this.localHave % HISTORY] = local;
    this.localHave++;

    let lastSent = Number.NEGATIVE_INFINITY;
    const began = this.now();
    while (this.have <= this.next) {
      if (this.stopped) throw new Error("The browser netplay session was closed.");
      if (this.now() - this.lastHostHeard > HOST_TIMEOUT_MS) {
        throw new Error("The PS3 host stopped sending synchronized game records.");
      }
      if (this.now() - began > HOST_TIMEOUT_MS * 2) {
        throw new Error("Timed out waiting for the PS3 host's next game frame.");
      }
      if (this.now() - lastSent >= RESEND_INTERVAL_MS) {
        this.sendInputs();
        lastSent = this.now();
      }
      await this.waitBriefly(RESEND_INTERVAL_MS);
    }

    const record = this.records[this.next % HISTORY];
    if (!(record instanceof Uint8Array) || record.byteLength !== 28) {
      throw new Error("The PS3 host's synchronized input record is missing.");
    }
    const step = this.next;
    this.next++;
    return { step, record: record.slice(), delay: this.delay, firstPad: this.firstPad };
  }

  fail(error) {
    if (this.stopped) return;
    this.stopped = true;
    if (this.joinTimer) clearInterval(this.joinTimer);
    this.joinTimer = null;
    this.setState("failed", error);
    this.notifyWaiters();
    this.rejectWelcome?.(error);
    this.resolveWelcome = null;
    this.rejectWelcome = null;
    this.unsubscribePacket?.();
    this.unsubscribeControl?.();
    this.transport.close(1008, "CBN1 session failed");
  }

  close(reason = 1) {
    if (this.stopped) return;
    if (this.session && this.me >= 0) {
      this.transport.sendPacket(
        encodeBye(this.session, this.me, reason, this.next),
        this.hostSessionId
      );
    }
    this.stopped = true;
    if (this.joinTimer) clearInterval(this.joinTimer);
    this.joinTimer = null;
    this.notifyWaiters();
    this.unsubscribePacket?.();
    this.unsubscribeControl?.();
    this.transport.close(1000, "Browser netplay closed");
    this.setState("closed");
  }
}
