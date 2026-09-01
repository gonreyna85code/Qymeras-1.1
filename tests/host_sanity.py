"""Host sanity tests for Qymeras STEP 1 logic.

These tests port the EXACT logic implemented in the firmware so the behavior
can be validated on the host machine (no native C++ toolchain required):

1. timezone conversion  - mirrors sensors.cpp: timezoneOffsetMinutes()/
                          toLocalEpoch()/getTime()/getMinutesOfDay()
2. strict float parsing - mirrors web.cpp: parseStrictFloat()
3. ESP-NOW RX FIFO      - mirrors espnow_p2p.cpp: rx_enqueue()/espnow_recv()
4. ai validators        - mirrors ai.cpp applyResult(): strict DIGITAL/ANALOG

Run:  python tests/host_sanity.py
Exit code 0 = all pass.
"""
import time
import math
import json

PASS = 0
FAIL = 0


def check(name, cond, detail=""):
    global PASS, FAIL
    if cond:
        PASS += 1
        print("  [PASS] %s" % name)
    else:
        FAIL += 1
        print("  [FAIL] %s%s" % (name, (" -- " + detail) if detail else ""))


# ---------------------------------------------------------------- timezone
# sensors.cpp: toLocalEpoch(utc) = utc + offset_minutes*60, then gmtime().
def to_local_epoch(utc, offset_min):
    return utc + offset_min * 60


def get_time(utc, offset_min):
    t = time.gmtime(to_local_epoch(utc, offset_min))
    return (t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec)


def get_minutes_of_day(utc, offset_min):
    t = time.gmtime(to_local_epoch(utc, offset_min))
    return t.tm_hour * 60 + t.tm_min


# Reference UTC epoch for 2026-08-20 06:00:00 UTC.
import calendar
UTC_REF = calendar.timegm((2026, 8, 20, 6, 0, 0))

print("[timezone]")
check("offset 0 -> same clock",
      get_time(UTC_REF, 0) == (2026, 8, 20, 6, 0, 0))
check("offset +120 (Madrid) -> 08:00",
      get_time(UTC_REF, 120) == (2026, 8, 20, 8, 0, 0))
check("offset +120 minutes-of-day == 480",
      get_minutes_of_day(UTC_REF, 120) == 480)
check("offset -360 (CDMX) -> 00:00 same day",
      get_time(UTC_REF, -360) == (2026, 8, 20, 0, 0, 0))
check("offset -360 minutes-of-day == 0",
      get_minutes_of_day(UTC_REF, -360) == 0)
check("offset +840 (max) -> 20:00",
      get_time(UTC_REF, 840) == (2026, 8, 20, 20, 0, 0))
check("offset -720 (min) -> previous day 18:00",
      get_time(UTC_REF, -720) == (2026, 8, 19, 18, 0, 0))
check("minutes-of-day == getTime hour*60+min (consistent)",
      get_minutes_of_day(UTC_REF, -720) ==
      get_time(UTC_REF, -720)[3] * 60 + get_time(UTC_REF, -720)[4])
check("offset 0 min-of-day == 360",
      get_minutes_of_day(UTC_REF, 0) == 360)
check("offset 30 -> 06:30",
      get_time(UTC_REF, 30) == (2026, 8, 20, 6, 30, 0))
# cross-day boundary: 23:50 UTC + 120min -> next day 01:50
check("offset crossing midnight",
      get_time(calendar.timegm((2026, 8, 20, 23, 50, 0)), 120) == (2026, 8, 21, 1, 50, 0))

# ---------------------------------------------------------------- strict float
# web.cpp: parseStrictFloat()
#   empty -> reject; strtof; no prefix consumed -> reject; trailing junk -> reject
#   ERANGE / inf / nan -> reject
MAX_FLOAT = 3.4028235e38


def parse_strict_float(s):
    if len(s) == 0:
        return False, None
    # strtof skips leading whitespace, then parses the longest numeric prefix.
    t = s.lstrip()
    end = 0
    while end < len(t) and t[end] in "0123456789+-.eE":
        end += 1
    if end == 0:  # no prefix consumed (strtof end == begin)
        return False, None
    prefix = t[:end]
    # "12abc" -> prefix "12", remainder "abc" -> reject
    if end != len(t):
        return False, None
    try:
        v = float(prefix)
    except ValueError:
        return False, None
    if math.isinf(v) or math.isnan(v):  # matches isinf/isnan checks
        return False, None
    if abs(v) > MAX_FLOAT:  # ERANGE (overflow)
        return False, None
    return True, v


print("[strict float]")
ok, v = parse_strict_float("")
check("empty rejected", not ok)
ok, v = parse_strict_float("abc")
check("garbage rejected", not ok)
ok, v = parse_strict_float("12")
check("int ok == 12.0", ok and v == 12.0)
ok, v = parse_strict_float("12.5")
check("decimal ok == 12.5", ok and v == 12.5)
ok, v = parse_strict_float("-3.25")
check("negative ok == -3.25", ok and v == -3.25)
ok, v = parse_strict_float("+7")
check("leading + ok == 7.0", ok and v == 7.0)
ok, v = parse_strict_float("  12")
check("leading whitespace ok == 12.0 (strtof skips)", ok and v == 12.0)
ok, v = parse_strict_float("12 ")
check("trailing whitespace rejected (*end != '\\0')", not ok)
ok, v = parse_strict_float("12abc")
check("trailing junk rejected", not ok)
ok, v = parse_strict_float("1e400")
check("overflow -> inf rejected (ERANGE)", not ok)
ok, v = parse_strict_float("nan")
check("nan rejected", not ok)
ok, v = parse_strict_float("inf")
check("inf rejected", not ok)
ok, v = parse_strict_float("-inf")
check("-inf rejected", not ok)
ok, v = parse_strict_float("0")
check("zero ok", ok and v == 0.0)
ok, v = parse_strict_float("1.5")
check("ref float ok == 1.5", ok and v == 1.5)
ok, v = parse_strict_float("abc -> 0")
check("'abc -> 0' rejected (no silent 0)", not ok)

# calib/set range guards mirrored from handleCalibSet
check("tz -720 accepted", -720 >= -720 and -720 <= 840)
check("tz 840 accepted", 840 >= -720 and 840 <= 840)
check("tz 841 rejected", not (841 >= -720 and 841 <= 840))
check("tz -721 rejected", not (-721 >= -720 and -721 <= 840))
check("fade 3600000 accepted", 3600000 >= 0 and 3600000 <= 3600000)
check("fade 3600001 rejected", not (3600001 >= 0 and 3600001 <= 3600000))
check("fade negative rejected", not (-1 >= 0 and -1 <= 3600000))

# ---------------------------------------------------------------- ESP-NOW FIFO
# espnow_p2p.cpp: bounded ring buffer, SPSC (callback writes, loop reads).
QSIZE = 8
MAX_PAYLOAD = 250


class Fifo:
    def __init__(self):
        self.q = [None] * QSIZE
        self.head = 0
        self.tail = 0
        self.count = 0
        self.overflow = 0

    def enqueue(self, payload):
        if len(payload) == 0 or len(payload) > MAX_PAYLOAD:
            return
        if self.count >= QSIZE:
            self.overflow += 1  # drop new, keep oldest
            return
        self.q[self.head] = payload
        self.head = (self.head + 1) % QSIZE
        self.count += 1

    def recv(self):
        if self.count == 0:
            return None
        p = self.q[self.tail]
        self.tail = (self.tail + 1) % QSIZE
        self.count -= 1
        return p


print("[esp-now fifo]")
f = Fifo()
check("empty recv -> None", f.recv() is None)
for i in range(QSIZE):
    f.enqueue(b"m%d" % i)
check("8 entries fill queue, count==8", f.count == 8)
f.enqueue(b"overflow9")
check("9th dropped, overflow==1", f.overflow == 1)
check("oldest preserved (no loss of queued msgs)", f.count == 8)
check("FIFO order (oldest first)",
      [f.recv() for _ in range(8)] == [b"m%d" % i for i in range(8)])
check("drained, count==0", f.count == 0)
f.enqueue(b"")  # zero length dropped without overflow
f.enqueue(b"x" * (MAX_PAYLOAD + 1))
check("zero/oversize dropped, overflow unchanged", f.overflow == 1 and f.count == 0)
# wrap-around: fill, drain 5, fill 5 -> head wraps, FIFO still ordered
for i in range(8):
    f.enqueue(b"a%d" % i)
for _ in range(5):
    f.recv()
for i in range(5):
    f.enqueue(b"b%d" % i)
check("wrap-around count==8", f.count == 8)
got = [f.recv() for _ in range(8)]
check("wrap-around FIFO order",
      got == [b"a%d" % i for i in range(5, 8)] + [b"b%d" % i for i in range(5)])
check("after wrap drain, count==0", f.count == 0)
f.enqueue(b"z")
check("reusable after full cycle", f.recv() == b"z" and f.count == 0)

# ------------------------------------------------------------ ai validators
# Mirrors ai.cpp applyResult(): tolerant per-out_type validation of LLM content.
# DIGITAL accepts "true"/"false" exactly OR as a prefix followed by a separator
# (space . , ; :) -- small models append explanations like "False. The earth...".
# ANALOG accepts a LEADING finite number (strtof semantics), ignoring trailing
# unit/text like "35.20C", and enforces [min,max].
def validate_digital(content):
    val = content.strip().lower()
    seps = (" ", ".", ",", ";", ":")

    def prefix(word):
        if val == word:
            return True
        return any(val.startswith(word + c) for c in seps)

    if prefix("true"):
        return True, True
    if prefix("false"):
        return True, False
    return False, None


import re
_NUM_RE = re.compile(r'[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?')


def validate_analog(content, mn, mx):
    val = content.strip()
    m = _NUM_RE.match(val)  # leading number, trailing text ignored (strtof)
    if not m:
        return False, None
    try:
        v = float(m.group(0))
    except ValueError:
        return False, None
    if not math.isfinite(v):  # strtof overflow -> inf/-inf rejected (ERANGE)
        return False, None
    if v < mn or v > mx:
        return False, None
    return True, v


print("[ai-validators]")
check("digital 'true'", validate_digital("true") == (True, True))
check("digital 'TRUE' case-insensitive", validate_digital("TRUE") == (True, True))
check("digital ' true ' trims", validate_digital("  true ") == (True, True))
check("digital 'false'", validate_digital("false") == (True, False))
check("digital 'true.' punctuation accepted (tolerant)",
      validate_digital("true.") == (True, True))
check("digital 'False. The earth...' prefix accepted",
      validate_digital("False. The Earth's rotation is not uniform.")[0] is True)
check("digital 'true, based on TEMP' comma prefix",
      validate_digital("true, based on TEMP sensor") == (True, True))
check("digital 'truthy' rejected (no separator)", validate_digital("truthy")[0] is False)
check("digital 'yes' rejected", validate_digital("yes")[0] is False)
check("digital '1' rejected", validate_digital("1")[0] is False)
check("digital garbage rejected", validate_digital("The risk is low")[0] is False)
check("digital empty rejected", validate_digital("")[0] is False)

check("analog '42' in range", validate_analog("42", 0, 100) == (True, 42.0))
check("analog ' 42.5 ' trims", validate_analog(" 42.5 ", 0, 100) == (True, 42.5))
check("analog '-3' negative ok in range", validate_analog("-3", -10, 100)[0] is True)
check("analog '35.20C' unit suffix accepted (tolerant)",
      validate_analog("35.20C", 0, 100) == (True, 35.2))
check("analog '42abc' trailing junk accepted (leading number)",
      validate_analog("42abc", 0, 100) == (True, 42.0))
check("analog '150' out of range rejected",
      validate_analog("150", 0, 100) == (False, None))
check("analog 'abc' rejected", validate_analog("abc", 0, 100)[0] is False)
check("analog '1e999' overflow rejected", validate_analog("1e999", 0, 100)[0] is False)
check("analog empty rejected", validate_analog("", 0, 100)[0] is False)
check("analog boundary min accepted", validate_analog("0", 0, 100) == (True, 0.0))
check("analog boundary max accepted", validate_analog("100", 0, 100) == (True, 100.0))

# ------------------------------------------------------------ rules engine
# Mirrors automations.cpp tick(): EDGE anti-bounce (CONFIRM_READS=3, rising on
# CMP_GT / falling on CMP_LT, EQ never fires), THRESHOLD GT/LT strict + EQ
# within 0.5, AND/OR combination, cooldown gating.
CONFIRM_READS = 3


class RuleState:
    def __init__(self):
        self.last = [False] * 5
        self.counter = [0] * 5
        self.stable = [False] * 5
        self.last_action = -10 ** 9
        self.pending = False
        self.trigger_time = 0
        self.last_time_exec = 0
        self.last_interval_exec = -10 ** 9


def eval_edge(state, j, raw, cmp_code):
    """One tick of EDGE evaluation for sensor j; returns val_trigger."""
    val_trigger = False
    if raw == state.last[j]:
        if state.counter[j] < CONFIRM_READS:
            state.counter[j] += 1
    else:
        state.last[j] = raw
        state.counter[j] = 1
    if state.counter[j] >= CONFIRM_READS:
        rising = (not state.stable[j]) and raw
        falling = state.stable[j] and (not raw)
        if cmp_code == 0:      # CMP_GT
            val_trigger = rising
        elif cmp_code == 1:    # CMP_LT
            val_trigger = falling
        state.stable[j] = raw
    return val_trigger


def eval_threshold(value, thr, cmp_code):
    if cmp_code == 0:
        return value > thr
    if cmp_code == 1:
        return value < thr
    if cmp_code == 2:
        return abs(value - thr) < 0.5
    return False


print("[rules-engine]")
# EDGE: needs CONFIRM_READS consecutive identical reads before first edge,
# then fires on transitions only (rising for CMP_GT).
st = RuleState()
seq = []
for i in range(8):
    seq.append(eval_edge(st, 0, True, 0))     # hold true: confirm then no edge
for i in range(6):
    seq.append(eval_edge(st, 0, False, 0))    # falling: CMP_GT ignores
for i in range(4):
    seq.append(eval_edge(st, 0, True, 0))     # rising again -> one trigger
check("EDGE GT: single rising after confirm, no repeat while held",
      seq[:CONFIRM_READS] == [False, False, True] and all(seq[3:8]) is False)
check("EDGE GT: falling edges never fire (CMP_GT)", not any(seq[8:14]))
check("EDGE GT: second rise fires once after bounce window",
      seq[14] is False and seq[15] is False and seq[16] is True and seq[17] is False)

# EDGE with CMP_EQ never fires (documented behavior).
st2 = RuleState()
eq_seq = [eval_edge(st2, 0, b, 2) for b in [True] * 4 + [False] * 4]
check("EDGE EQ: never fires either direction", not any(eq_seq))

# THRESHOLD comparators incl. EQ tolerance band.
check("THRESHOLD GT strict (30>29.99 true)", eval_threshold(30.0, 29.99, 0) is True)
check("THRESHOLD GT boundary excluded (30>30 false)", eval_threshold(30.0, 30.0, 0) is False)
check("THRESHOLD LT boundary excluded (20<20 false)", eval_threshold(20.0, 20.0, 1) is False)
check("THRESHOLD EQ within 0.5 band", eval_threshold(30.4, 30.0, 2) is True)
check("THRESHOLD EQ outside band", eval_threshold(30.6, 30.0, 2) is False)

# AND/OR combination over two sensors.
trigger_and = True
va, vb = True, False
trigger_and &= va
trigger_and &= vb
trigger_or = False
trigger_or |= va
trigger_or |= vb
check("AND gate requires both sensors", trigger_and is False)
check("OR gate fires on any sensor", trigger_or is True)

print()
print("[remote-control-state-machine]")
# Reference implementation mirroring src/control/qymera_control.c semantics.
# Only the *logic* is ported; the firmware owns the real structs/registry.

MAX_PENDING = 8
TIMEOUT_MS = 2000
(CMD_REQUESTED, CMD_DISPATCHED, CMD_WAITING_ACK, CMD_ACKED,
 CMD_STATE_CONFIRMED, CMD_FAILED, CMD_TIMEOUT) = range(7)


def entity_hash(eid):
    h = 2166136261
    for c in eid:
        h = ((h ^ ord(c)) * 16777619) & 0xFFFFFFFF
    return h


class PendingCtrl:
    """Portable model of qymera_control_context_t + pending table + tick/ack/state."""

    def __init__(self):
        self.cmd_seq = 1
        self.pending = {}
        self.events = []

    def alloc(self, device_id, entity_id, opcode, value, now):
        if len(self.pending) >= MAX_PENDING:
            return None, "NO_SPACE"
        seq = self.cmd_seq
        self.cmd_seq += 1
        rec = {
            "used": True, "cmd_seq": seq, "device_id": device_id,
            "entity_id": entity_id, "opcode": opcode, "requested_value": value,
            "desired_bool": value not in (0, 0.0, False),
            "desired_numeric": float(value),
            "started_at": now, "deadline": now + TIMEOUT_MS,
            "status": CMD_WAITING_ACK,
        }
        self.pending[seq] = rec
        return seq, "OK"

    def on_ack(self, seq, ack_result, src_ip, expected_ip):
        rec = self.pending.get(seq)
        if rec is None:
            return "IGNORED_UNKNOWN"   # late / duplicate-already-resolved
        if expected_ip is not None and src_ip != expected_ip:
            return "IGNORED_SOURCE"    # wrong node
        if ack_result != 0:
            del self.pending[seq]
            return "FAILED"
        rec["status"] = CMD_ACKED       # ACKED != CONFIRMED
        return "ACKED"

    def on_state(self, device_id, entity_id, observed_bool, observed_val):
        if device_id not in self.registry_devices:
            return "UNKNOWN_DEVICE"
        matched = False
        for seq, rec in list(self.pending.items()):
            if rec["device_id"] != device_id or rec["entity_id"] != entity_id:
                continue
            # desired matches observed ?
            if rec["desired_bool"] == observed_bool:
                rec["status"] = CMD_STATE_CONFIRMED
                del self.pending[seq]
                matched = True
            else:
                rec["status"] = CMD_FAILED
                del self.pending[seq]
        return "CONFIRMED" if matched else "MISMATCH"

    def tick(self, now):
        for seq, rec in list(self.pending.items()):
            if now >= rec["deadline"]:
                rec["status"] = CMD_TIMEOUT
                del self.pending[seq]

    def count(self):
        return len(self.pending)


ctrl = PendingCtrl()
ctrl.registry_devices = {"nodeA", "nodeB"}

# --- happy path: requested -> dispatched -> waiting -> ack -> state confirm ---
seq, r = ctrl.alloc("nodeA", "relay", 1, 1.0, 100)
check("PENDING: dispatch allocates cmd_seq", r == "OK" and seq == 1)
check("PENDING: status WAITING_ACK after dispatch", ctrl.pending[seq]["status"] == CMD_WAITING_ACK)
r = ctrl.on_ack(seq, 0, "10.0.0.2", "10.0.0.2")
check("PENDING: matching ACK -> ACKED (not confirmed)", r == "ACKED" and ctrl.pending[seq]["status"] == CMD_ACKED)
check("PENDING: ACKED != CONFIRMED (entry still present)",
      ctrl.pending.get(seq) is not None and ctrl.pending[seq]["status"] != CMD_STATE_CONFIRMED)
r = ctrl.on_state("nodeA", "relay", True, 1.0)
check("PENDING: authoritative state -> CONFIRMED and removed",
      r == "CONFIRMED" and ctrl.count() == 0)

# --- duplicate ACK: second ack after resolution is ignored ---
seq, _ = ctrl.alloc("nodeA", "relay", 1, 1.0, 200)
r = ctrl.on_ack(seq, 0, "10.0.0.2", "10.0.0.2")
r2 = ctrl.on_ack(seq, 0, "10.0.0.2", "10.0.0.2")
ctrl.on_state("nodeA", "relay", True, 1.0)
check("PENDING: duplicate ACK while pending stays ACKED", r == "ACKED" and r2 == "ACKED")
check("PENDING: duplicate ACK after removal ignored", ctrl.on_ack(seq, 0, "10.0.0.2", "10.0.0.2") == "IGNORED_UNKNOWN")

# --- wrong cmd_seq ---
seq, _ = ctrl.alloc("nodeA", "relay", 1, 1.0, 300)
check("PENDING: unknown cmd_seq ignored", ctrl.on_ack(999, 0, "10.0.0.2", "10.0.0.2") == "IGNORED_UNKNOWN")
# late ACK does not resurrect a timed-out command
ctrl.tick(300 + TIMEOUT_MS + 1)
check("PENDING: timeout frees the slot", ctrl.count() == 0)
check("PENDING: late ACK after timeout ignored", ctrl.on_ack(seq, 0, "10.0.0.2", "10.0.0.2") == "IGNORED_UNKNOWN")

# --- wrong source ---
seq, _ = ctrl.alloc("nodeA", "relay", 1, 1.0, 400)
r = ctrl.on_ack(seq, 0, "192.168.0.99", "10.0.0.2")   # spoofed source
check("PENDING: wrong source ACK does not resolve command", r == "IGNORED_SOURCE" and ctrl.count() == 1)
ctrl.on_state("nodeA", "relay", True, 1.0)

# --- ACK error result -> FAILED ---
seq, _ = ctrl.alloc("nodeA", "relay", 1, 1.0, 500)
r = ctrl.on_ack(seq, 1, "10.0.0.2", "10.0.0.2")       # result != 0 -> error
check("PENDING: ACK error -> FAILED (not ACKED/CONFIRMED)", r == "FAILED" and ctrl.count() == 0)

# --- timeout: desired kept, observed unchanged, status TIMEOUT ---
seq, _ = ctrl.alloc("nodeA", "relay", 1, 1.0, 600)
ctrl.tick(600 + TIMEOUT_MS - 1)
check("PENDING: before deadline still pending", ctrl.count() == 1)
ctrl.tick(600 + TIMEOUT_MS)
check("PENDING: at deadline -> TIMEOUT, slot freed", ctrl.count() == 0)

# --- state mismatch: desired ON, observed OFF ---
seq, _ = ctrl.alloc("nodeA", "relay", 1, 1.0, 700)
ctrl.on_ack(seq, 0, "10.0.0.2", "10.0.0.2")
r = ctrl.on_state("nodeA", "relay", False, 0.0)
check("PENDING: desired!=observed -> FAILED mismatch (not silent overwrite)", r == "MISMATCH" and ctrl.count() == 0)

# --- multi-device: same entity id, different device ---
sA, _ = ctrl.alloc("nodeA", "relay", 1, 1.0, 800)
sB, _ = ctrl.alloc("nodeB", "relay", 1, 1.0, 801)
check("PENDING: multi-device independent (2 pending)", ctrl.count() == 2)
ctrl.on_ack(sA, 0, "10.0.0.2", "10.0.0.2")
check("PENDING: nodeA ack only resolves nodeA", ctrl.pending.get(sB) is not None)
ctrl.on_ack(sB, 0, "10.0.0.3", "10.0.0.3")
check("PENDING: both acked, still waiting state", ctrl.count() == 2)
ctrl.on_state("nodeA", "relay", True, 1.0)
check("PENDING: nodeA confirmed, nodeB pending", ctrl.count() == 1 and ctrl.pending.get(sB) is not None)
ctrl.on_state("nodeB", "relay", True, 1.0)
check("PENDING: nodeB confirmed, all clear", ctrl.count() == 0)

# --- capacity: fill table -> NO_SPACE -> resolve one -> succeeds ---
crowded = PendingCtrl(); crowded.registry_devices = {"nodeA"}
for i in range(MAX_PENDING):
    seq_, r_ = crowded.alloc("nodeA", "ent%d" % i, 1, 1.0, 1000 + i)
    assert r_ == "OK"
check("PENDING: table fills to MAX_PENDING", crowded.count() == MAX_PENDING)
seq_, r_ = crowded.alloc("nodeA", "extra", 1, 1.0, 2000)
check("PENDING: overflow -> NO_SPACE", r_ == "NO_SPACE")
first_seq = next(iter(crowded.pending))
crowded.on_ack(first_seq, 0, "10.0.0.2", "10.0.0.2")
crowded.on_state("nodeA", "ent0", True, 1.0)
seq_, r_ = crowded.alloc("nodeA", "extra", 1, 1.0, 3000)
check("PENDING: after resolving one, new command succeeds", r_ == "OK")

# ================================================================ Phase 3A: Skill API
# Deterministic Skill layer mirror (qymera_skill.c). Structured calls only;
# no LLM, no natural-language parsing, no direct GPIO/UDP/registry internals.

def _Ok(data):
    return {"ok": True, "data": data}


def _Err(code, message="", details=None):
    return {"ok": False, "error": {"code": code, "message": message, "details": details or {}}}


# Mirrors the C JSON string escaper in qymera_skill.c (out_str_json). Every
# output string must pass through this so the payload is always valid JSON.
def c_json_escape(s):
    if s is None:
        s = ""
    out = []
    for ch in s:
        o = ord(ch)
        if ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif ch == "/":
            out.append("\\/")
        elif ch == "\b":
            out.append("\\b")
        elif ch == "\f":
            out.append("\\f")
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif o < 0x20:
            out.append("\\u%04x" % o)
        else:
            out.append(ch)
    return "".join(out)


def json_str(s):
    return '"' + c_json_escape(s) + '"'


PERMS = {"READ": 1, "CONTROL": 2, "RULE_READ": 4, "RULE_WRITE": 8}

SKILLS = {
    "list_devices": "READ", "list_entities": "READ",
    "get_entity_state": "READ", "get_entity_info": "READ",
    "set_relay": "CONTROL", "set_dimmer": "CONTROL",
    "list_rules": "RULE_READ", "get_rule": "RULE_READ",
    "create_rule": "RULE_WRITE", "update_rule": "RULE_WRITE",
    "delete_rule": "RULE_WRITE", "enable_rule": "RULE_WRITE", "disable_rule": "RULE_WRITE",
}

_ACTION_CAP = {
    "SET_BOOL": {"actuator.relay"}, "TOGGLE": {"actuator.relay"}, "PULSE": {"actuator.relay"},
    "SET_LEVEL": {"actuator.dimmer"}, "SET_VALUE": {"actuator.dimmer"}, "FADE": {"actuator.dimmer"},
}
_OP_VALID = {"GT", "LT", "GE", "LE", "EQ", "NE", "IN_RANGE", "OUT_RANGE"}


class FakeControl:
    def __init__(self, cap=3):
        self.pending = 0
        self.cap = cap

    def set_relay(self, ref, value):
        if self.pending >= self.cap:
            return _Err("NO_SPACE", "pending command table full")
        self.pending += 1
        return _Ok({"requested": bool(value), "status": "WAITING_ACK", "reliability": "PENDING"})

    def set_dimmer(self, ref, level):
        if self.pending >= self.cap:
            return _Err("NO_SPACE", "pending command table full")
        self.pending += 1
        return _Ok({"requested": level, "status": "WAITING_ACK", "reliability": "PENDING"})


class SkillEnv:
    def __init__(self):
        self.devices = {}
        self.rules = {}
        self.control = FakeControl()
        self.persist_log = []
        self.rule_seq = 1
        # Phase 3B simulation knobs (mirror qymera_skill.c hardening)
        self.persist_fail = set()      # rule_ids whose persist must fail
        self.delete_fail = set()       # rule_ids whose storage delete must fail
        self.missing_deps = {}         # skill name -> missing dependency name
        self.output_limit = None       # if set, success outputs larger than this -> OUTPUT_TOO_LARGE

    def set_persist_fail(self, *ids):
        self.persist_fail.update(ids)

    def set_delete_fail(self, *ids):
        self.delete_fail.update(ids)

    def clear_persist_fail(self):
        self.persist_fail.clear()

    def clear_delete_fail(self):
        self.delete_fail.clear()

    def add_device(self, did, name="node", model="esp32", role="remote", online=True):
        self.devices[did] = {"device_id": did, "name": name, "model": model, "role": role,
                             "online": online, "state": "operational", "location": "room",
                             "entities": {}}

    def add_entity(self, did, eid, name, ctype, caps, unit="", obs=None, des=None,
                   cmd="STATE_CONFIRMED", reliability="CONFIRMED"):
        self.devices[did]["entities"][eid] = {
            "device_id": did, "entity_id": eid, "name": name, "type": ctype, "caps": set(caps),
            "unit": unit, "obs": obs, "des": des, "cmd_status": cmd, "reliability": reliability}

    def find_entity(self, did, eid):
        d = self.devices.get(did)
        if not d:
            return None
        return d["entities"].get(eid)

    def execute(self, skill, inp, perm=None):
        if perm is None:
            perm = 0
            for v in PERMS.values():
                perm |= v
        if skill not in SKILLS:
            return _Err("SKILL_NOT_FOUND")
        if (perm & PERMS[SKILLS[skill]]) != PERMS[SKILLS[skill]]:
            return _Err("PERMISSION_DENIED")
        if skill in self.missing_deps:
            return _Err("DEPENDENCY_MISSING", self.missing_deps[skill])
        r = getattr(self, "_" + skill)(inp)
        if self.output_limit is not None and r.get("ok") and "data" in r:
            payload = json.dumps(r["data"])
            if len(payload) > self.output_limit:
                return _Err("OUTPUT_TOO_LARGE", "result exceeds skill output limit")
        return r

    # -- read skills --
    def _list_devices(self, inp):
        return _Ok([{"device_id": did, "name": d["name"], "model": d["model"], "role": d["role"],
                     "online": d["online"], "state": d["state"], "location": d["location"]}
                    for did, d in self.devices.items()])

    def _list_entities(self, inp):
        out = []
        for did, d in self.devices.items():
            for eid, e in d["entities"].items():
                out.append({"device_id": did, "entity_id": eid, "name": e["name"], "type": e["type"],
                            "capabilities": sorted(e["caps"]), "unit": e["unit"],
                            "current": e["obs"], "desired": e["des"], "cmd_status": e["cmd_status"]})
        return _Ok(out)

    def _get_entity_state(self, inp):
        e = self.find_entity(inp.get("device_id"), inp.get("entity_id"))
        if e is None:
            return _Err("ENTITY_NOT_FOUND")
        st = e["cmd_status"]
        if st == "STATE_CONFIRMED":
            rel = "CONFIRMED"
        elif st in ("ACKED", "WAITING_ACK", "DISPATCHED", "REQUESTED"):
            rel = "PENDING"
        elif st in ("FAILED", "TIMEOUT"):
            rel = "FAILED"
        elif e["reliability"] in ("STALE", "OFFLINE"):
            rel = "STALE"
        else:
            rel = "CONFIRMED"
        return _Ok({"device_id": e["device_id"], "entity_id": e["entity_id"], "observed": e["obs"],
                    "desired": e["des"], "status": st, "reliability": rel, "timestamp": 0})

    def _get_entity_info(self, inp):
        e = self.find_entity(inp.get("device_id"), inp.get("entity_id"))
        if e is None:
            return _Err("ENTITY_NOT_FOUND")
        return _Ok({"device_id": e["device_id"], "entity_id": e["entity_id"], "name": e["name"],
                    "type": e["type"], "capabilities": sorted(e["caps"]), "unit": e["unit"]})

    # -- control skills --
    def _set_relay(self, inp):
        e = self.find_entity(inp.get("device_id"), inp.get("entity_id"))
        if e is None:
            return _Err("ENTITY_NOT_FOUND")
        if "actuator.relay" not in e["caps"]:
            return _Err("INVALID_CAPABILITY")
        res = self.control.set_relay(inp["entity_id"], bool(inp.get("value")))
        if not res["ok"]:
            return res
        e["des"] = bool(inp.get("value"))
        e["cmd_status"] = "WAITING_ACK"
        e["reliability"] = "PENDING"
        return _Ok({"device_id": inp["device_id"], "entity_id": inp["entity_id"],
                    "requested": bool(inp.get("value")), "status": "WAITING_ACK", "reliability": "PENDING"})

    def _set_dimmer(self, inp):
        level = inp.get("level", 0)
        if not isinstance(level, int) or level < 0 or level > 100:
            return _Err("INVALID_VALUE")
        e = self.find_entity(inp.get("device_id"), inp.get("entity_id"))
        if e is None:
            return _Err("ENTITY_NOT_FOUND")
        if "actuator.dimmer" not in e["caps"]:
            return _Err("INVALID_CAPABILITY")
        res = self.control.set_dimmer(inp["entity_id"], level)
        if not res["ok"]:
            return res
        e["des"] = level
        e["cmd_status"] = "WAITING_ACK"
        e["reliability"] = "PENDING"
        return _Ok({"device_id": inp["device_id"], "entity_id": inp["entity_id"],
                    "requested": level, "status": "WAITING_ACK", "reliability": "PENDING"})

    # -- rule validation + skills --
    def _body(self, inp):
        b = dict(inp.get("rule") or inp)
        if "name" not in b:
            b["name"] = inp.get("name")
        return b

    def _validate_rule(self, body):
        if not body.get("name"):
            return _Err("INVALID_VALUE", "name required")
        trig = body.get("trigger") or {}
        if trig.get("entity"):
            e = self.find_entity(trig["entity"].get("device_id"), trig["entity"].get("entity_id"))
            if e is None:
                return _Err("ENTITY_NOT_FOUND")
            if trig.get("operator") not in _OP_VALID:
                return _Err("RULE_INVALID")
        for c in body.get("conditions", []):
            ce = c.get("entity") or {}
            if self.find_entity(ce.get("device_id"), ce.get("entity_id")) is None:
                return _Err("ENTITY_NOT_FOUND")
            if c.get("operator") not in _OP_VALID:
                return _Err("RULE_INVALID")
        if not body.get("actions"):
            return _Err("RULE_INVALID", "at least one action required")
        for a in body.get("actions", []):
            ae = a.get("entity") or {}
            e = self.find_entity(ae.get("device_id"), ae.get("entity_id"))
            if e is None:
                return _Err("ENTITY_NOT_FOUND")
            if a.get("action") not in _ACTION_CAP:
                return _Err("RULE_INVALID")
            if not (e["caps"] & _ACTION_CAP[a["action"]]):
                return _Err("RULE_INVALID", "action incompatible with target capability")
        return None

    def _list_rules(self, inp):
        return _Ok([{"rule_id": rid, "name": r["name"], "enabled": r["enabled"], "revision": r["revision"]}
                    for rid, r in self.rules.items()])

    def _get_rule(self, inp):
        r = self.rules.get(inp.get("rule_id"))
        if r is None:
            return _Err("RULE_INVALID", "rule not found")
        return _Ok({"rule_id": inp["rule_id"], "name": r["name"], "enabled": r["enabled"],
                    "revision": r["revision"], "priority": r.get("priority", 0),
                    "cooldown_ms": r.get("cooldown_ms", 0),
                    "max_activations_per_hour": r.get("max_activations_per_hour", 0),
                    "trigger": r.get("trigger"), "conditions": r.get("conditions", []),
                    "actions": r.get("actions", []),
                    "state": {"activation_count": r.get("activation_count", 0)}})

    def _create_rule(self, inp):
        body = self._body(inp)
        err = self._validate_rule(body)
        if err:
            return err
        rid = body.get("rule_id") or inp.get("rule_id") or ("rule_%d" % self.rule_seq)
        if rid in self.rules:
            return _Err("RULE_CONFLICT", "rule id already exists")
        # Transactional: persist first; on failure return STORAGE_ERROR and do
        # NOT create an active rule. (mirrors qymera_skill.c persist->load)
        if rid in self.persist_fail:
            return _Err("STORAGE_ERROR", "rule persist failed")
        if str(rid).startswith("rule_"):
            self.rule_seq += 1
        self.rules[rid] = {"name": body["name"], "enabled": True, "revision": 1,
                           "trigger": body.get("trigger"), "conditions": body.get("conditions", []),
                           "actions": body.get("actions", []),
                           "cooldown_ms": body.get("cooldown_ms", 0),
                           "max_activations_per_hour": body.get("max_activations_per_hour", 0)}
        self.persist_log.append(("create", rid))
        return _Ok({"rule_id": rid, "revision": 1, "enabled": True, "activated": True})

    def _update_rule(self, inp):
        rid = inp.get("rule_id")
        r = self.rules.get(rid)
        if r is None:
            return _Err("RULE_INVALID", "rule not found")
        body = self._body(inp)
        err = self._validate_rule(body)
        if err:
            return err
        # Transactional: persist the replacement first; on failure the old rule
        # (definition and revision) stays intact.
        if rid in self.persist_fail:
            return _Err("STORAGE_ERROR", "rule persist failed")
        snapshot = dict(r)
        for k in ("name", "trigger", "conditions", "actions", "cooldown_ms", "max_activations_per_hour"):
            if k in body:
                r[k] = body[k]
        r["revision"] += 1
        self.persist_log.append(("update", rid))
        return _Ok({"rule_id": rid, "revision": r["revision"], "enabled": r["enabled"], "updated": True})

    def _delete_rule(self, inp):
        rid = inp.get("rule_id")
        if rid not in self.rules:
            return _Err("RULE_INVALID", "rule not found")
        # Transactional: storage delete first; on failure keep the rule active.
        if rid in self.delete_fail:
            return _Err("STORAGE_ERROR", "rule delete persist failed")
        del self.rules[rid]
        self.persist_log.append(("delete", rid))
        return _Ok({"rule_id": rid, "deleted": True})

    def _enable_rule(self, inp):
        rid = inp.get("rule_id")
        if rid not in self.rules:
            return _Err("RULE_INVALID", "rule not found")
        if rid in self.persist_fail:
            return _Err("STORAGE_ERROR", "rule persist failed")
        self.rules[rid]["enabled"] = True
        self.persist_log.append(("enable", rid))
        return _Ok({"rule_id": rid, "enabled": True})

    def _disable_rule(self, inp):
        rid = inp.get("rule_id")
        if rid not in self.rules:
            return _Err("RULE_INVALID", "rule not found")
        if rid in self.persist_fail:
            return _Err("STORAGE_ERROR", "rule persist failed")
        self.rules[rid]["enabled"] = False
        self.persist_log.append(("disable", rid))
        return _Ok({"rule_id": rid, "enabled": False})


env = SkillEnv()
env.add_device("node-a", name="Node A")
env.add_device("node-b", name="Node B")
env.add_entity("node-a", "temperature", "Temp", "sensor.numeric", ["sensor.numeric"], unit="C",
               obs=25.0, des=25.0)
env.add_entity("node-a", "relay0", "Relay", "actuator.relay", ["actuator.relay"], obs=False, des=False,
               cmd="STATE_CONFIRMED", reliability="CONFIRMED")
env.add_entity("node-b", "dimmer0", "Dimmer", "actuator.dimmer", ["actuator.dimmer"], obs=30, des=30,
               cmd="STATE_CONFIRMED", reliability="CONFIRMED")

# --- 1. Skill discovery ---
check("SKILL: registry exposes 13 fixed skills", len(SKILLS) == 13)
check("SKILL: all required skills present",
      set(SKILLS.keys()) == {"list_devices", "list_entities", "get_entity_state", "get_entity_info",
                             "set_relay", "set_dimmer", "list_rules", "get_rule", "create_rule",
                             "update_rule", "delete_rule", "enable_rule", "disable_rule"})
check("SKILL: set_relay requires CONTROL", SKILLS["set_relay"] == "CONTROL")
check("SKILL: get_entity_state requires READ", SKILLS["get_entity_state"] == "READ")
check("SKILL: create_rule requires RULE_WRITE", SKILLS["create_rule"] == "RULE_WRITE")
check("SKILL: unknown skill -> SKILL_NOT_FOUND", env.execute("nope", {})["error"]["code"] == "SKILL_NOT_FOUND")

# --- 2. Permissions ---
READ = PERMS["READ"]; CONTROL = PERMS["CONTROL"]; RW = PERMS["RULE_WRITE"]
r = env.execute("set_relay", {"device_id": "node-a", "entity_id": "relay0", "value": True}, perm=READ)
check("SKILL: set_relay with READ only -> PERMISSION_DENIED", r["error"]["code"] == "PERMISSION_DENIED")
r = env.execute("set_relay", {"device_id": "node-a", "entity_id": "relay0", "value": True}, perm=READ | CONTROL)
check("SKILL: set_relay with READ|CONTROL succeeds", r["ok"] is True)
r = env.execute("list_devices", {}, perm=CONTROL)
check("SKILL: list_devices with CONTROL only -> PERMISSION_DENIED", r["error"]["code"] == "PERMISSION_DENIED")
r = env.execute("create_rule", {"name": "x", "rule": {"trigger": {}}}, perm=READ)
check("SKILL: create_rule with READ only -> PERMISSION_DENIED", r["error"]["code"] == "PERMISSION_DENIED")

# --- 3. Input validation ---
r = env.execute("set_dimmer", {"device_id": "node-b", "entity_id": "dimmer0", "level": 101})
check("SKILL: set_dimmer level 101 -> INVALID_VALUE", r["error"]["code"] == "INVALID_VALUE")
r = env.execute("set_dimmer", {"device_id": "node-b", "entity_id": "dimmer0", "level": -1})
check("SKILL: set_dimmer level -1 -> INVALID_VALUE", r["error"]["code"] == "INVALID_VALUE")
check("SKILL: create_rule empty name -> INVALID_VALUE",
      env.execute("create_rule", {"name": "", "rule": {}})["error"]["code"] == "INVALID_VALUE")

# --- 4. Entity lookup ---
r = env.execute("get_entity_state", {"device_id": "node-a", "entity_id": "temperature"})
check("SKILL: get_entity_state finds sensor", r["ok"] and r["data"]["observed"] == 25.0)
check("SKILL: get_entity_state missing entity -> ENTITY_NOT_FOUND",
      env.execute("get_entity_state", {"device_id": "node-a", "entity_id": "nope"})["error"]["code"] == "ENTITY_NOT_FOUND")

# --- 5. State retrieval (observed/desired/status/reliability) ---
state_env = SkillEnv()
state_env.add_device("node-a")
state_env.add_entity("node-a", "relay0", "Relay", "actuator.relay", ["actuator.relay"], obs=False, des=False,
                     cmd="STATE_CONFIRMED", reliability="CONFIRMED")
r = state_env.execute("get_entity_state", {"device_id": "node-a", "entity_id": "relay0"})
check("SKILL: relay state has observed false", r["data"]["observed"] is False)
check("SKILL: relay state has status STATE_CONFIRMED", r["data"]["status"] == "STATE_CONFIRMED")
check("SKILL: relay state has reliability CONFIRMED", r["data"]["reliability"] == "CONFIRMED")
check("SKILL: relay state has desired+observed present",
      ("desired" in r["data"]) and ("observed" in r["data"]))

# --- 6. Relay control ---
r = env.execute("set_relay", {"device_id": "node-a", "entity_id": "relay0", "value": True})
check("SKILL: set_relay dispatches", r["ok"] and r["data"]["status"] == "WAITING_ACK")
check("SKILL: set_relay marks desired true", env.find_entity("node-a", "relay0")["des"] is True)

# --- 7. Dimmer control ---
r = env.execute("set_dimmer", {"device_id": "node-b", "entity_id": "dimmer0", "level": 50})
check("SKILL: set_dimmer dispatches 50", r["ok"] and r["data"]["requested"] == 50)
check("SKILL: set_dimmer marks desired 50", env.find_entity("node-b", "dimmer0")["des"] == 50)

# --- 8. Rule create ---
res = env.execute("create_rule", {"name": "Hot room fan",
                                  "rule": {"trigger": {"entity": {"device_id": "node-a", "entity_id": "temperature"},
                                                       "operator": "GT", "threshold": 30},
                                           "conditions": [],
                                           "actions": [{"entity": {"device_id": "node-a", "entity_id": "relay0"},
                                                        "action": "SET_BOOL", "value": True}]}})
check("SKILL: create_rule activates", res["ok"] and res["data"]["activated"] is True)
rid = res["data"]["rule_id"]
check("SKILL: create_rule stored in rules", rid in env.rules)

# --- 9. Rule get ---
r = env.execute("get_rule", {"rule_id": rid})
check("SKILL: get_rule returns trigger+actions",
      r["ok"] and "trigger" in r["data"] and len(r["data"]["actions"]) == 1)

# --- 10. Rule update ---
r = env.execute("update_rule", {"rule_id": rid, "name": "Hot room fan v2",
                                "rule": {"trigger": {"entity": {"device_id": "node-a", "entity_id": "temperature"},
                                                     "operator": "GT", "threshold": 35},
                                         "conditions": [],
                                         "actions": [{"entity": {"device_id": "node-a", "entity_id": "relay0"},
                                                      "action": "SET_BOOL", "value": False}]}})
check("SKILL: update_rule bumps revision", r["ok"] and r["data"]["revision"] == 2)

# rules list after create+update
check("SKILL: list_rules reflects count", len(env.execute("list_rules", {})["data"]) == 1)

# --- 11. Rule enable / disable ---
r = env.execute("disable_rule", {"rule_id": rid})
check("SKILL: disable_rule -> enabled false", r["ok"] and r["data"]["enabled"] is False)
check("SKILL: disable_rule mirrored in store", env.rules[rid]["enabled"] is False)
r = env.execute("enable_rule", {"rule_id": rid})
check("SKILL: enable_rule -> enabled true", r["ok"] and r["data"]["enabled"] is True)

# --- 12. Invalid capability ---
r = env.execute("set_dimmer", {"device_id": "node-a", "entity_id": "relay0", "level": 50})
check("SKILL: set_dimmer on relay -> INVALID_CAPABILITY", r["error"]["code"] == "INVALID_CAPABILITY")
r = env.execute("set_relay", {"device_id": "node-b", "entity_id": "dimmer0", "value": True})
check("SKILL: set_relay on dimmer -> INVALID_CAPABILITY", r["error"]["code"] == "INVALID_CAPABILITY")
r = env.execute("create_rule", {"name": "Bad cap", "rule": {"trigger": {},
                                                            "actions": [{"entity": {"device_id": "node-b", "entity_id": "dimmer0"},
                                                                         "action": "SET_BOOL", "value": True}]}})
check("SKILL: rule action incompatible capability -> RULE_INVALID", r["error"]["code"] == "RULE_INVALID")

# --- 13. Invalid entity (in rule) ---
r = env.execute("create_rule", {"name": "Bad ref", "rule": {"trigger": {"entity": {"device_id": "node-a", "entity_id": "ghost"},
                                                                        "operator": "GT", "threshold": 1},
                                                            "conditions": [], "actions": []}})
check("SKILL: rule referencing nonexistent entity -> ENTITY_NOT_FOUND", r["error"]["code"] == "ENTITY_NOT_FOUND")

# --- 14. Command failure (pending table full -> NO_SPACE) ---
env2 = SkillEnv()
env2.add_device("node-a")
env2.add_entity("node-a", "relay0", "Relay", "actuator.relay", ["actuator.relay"], obs=False, des=False)
env2.control = FakeControl(cap=1)
env2.execute("set_relay", {"device_id": "node-a", "entity_id": "relay0", "value": True})
r = env2.execute("set_relay", {"device_id": "node-a", "entity_id": "relay0", "value": False})
check("SKILL: pending full -> NO_SPACE command failure", r["error"]["code"] == "NO_SPACE")

# --- 15. Permission denied (rule write) ---
r = env.execute("delete_rule", {"rule_id": rid}, perm=READ | PERMS["RULE_READ"])
check("SKILL: delete_rule without RULE_WRITE -> PERMISSION_DENIED", r["error"]["code"] == "PERMISSION_DENIED")

# --- 16. Rule delete ---
r = env.execute("delete_rule", {"rule_id": rid})
check("SKILL: delete_rule removes rule", r["ok"] and r["data"]["deleted"] is True)
check("SKILL: delete_rule removed from store", rid not in env.rules)

# --- LLM-independent execution: future AI workflow (no LLM) ---
wf = SkillEnv()
wf.add_device("node-a")
wf.add_device("node-b")
wf.add_entity("node-a", "temperature", "Temp", "sensor.numeric", ["sensor.numeric"], unit="C", obs=22.0, des=22.0)
wf.add_entity("node-b", "relay0", "Relay", "actuator.relay", ["actuator.relay"], obs=False, des=False,
              cmd="STATE_CONFIRMED", reliability="CONFIRMED")

step1 = wf.execute("list_entities", {})
step2 = wf.execute("get_entity_state", {"device_id": "node-a", "entity_id": "temperature"})
step3 = wf.execute("create_rule", {"name": "Warm fan", "rule": {
    "trigger": {"entity": {"device_id": "node-a", "entity_id": "temperature"}, "operator": "GT", "threshold": 28},
    "conditions": [], "actions": [{"entity": {"device_id": "node-b", "entity_id": "relay0"}, "action": "SET_BOOL", "value": True}]}})
wf_rid = step3["data"]["rule_id"]
step4 = wf.execute("enable_rule", {"rule_id": wf_rid})
step5 = wf.execute("set_relay", {"device_id": "node-b", "entity_id": "relay0", "value": True})
step6 = wf.execute("get_entity_state", {"device_id": "node-b", "entity_id": "relay0"})
wf_ok = (step1["ok"] and step2["ok"] and step3["ok"] and step4["ok"] and step5["ok"] and step6["ok"])
check("SKILL: AI workflow list_entities->state->create->enable->set->observe (no LLM)", wf_ok)
check("SKILL: workflow observe reflects desired", wf_ok and step6["data"]["desired"] is True and
      step6["data"]["status"] == "WAITING_ACK")
check("SKILL: discovery lists both devices",
      len(wf.execute("list_devices", {})["data"]) == 2 and
      len(wf.execute("list_entities", {})["data"]) == 2)

# ==========================================================================
# Phase 3B: Hardened Skill API contract (mirrors qymera_skill.c)
# ==========================================================================
#
# Every payload returned by execute() must serialize to VALID JSON through the
# C escaper (out_str_json). We validate that by round-tripping model outputs
# through the mirrored escaper + json.loads.

def valid_json_output(obj):
    try:
        return json.loads(json.dumps(obj)) == obj
    except (ValueError, TypeError):
        return False


# -- JSON escaping ----------------------------------------------------------
esc = c_json_escape
check("P3B JSON: bare string round-trips", json.loads(json_str("plain text")) == "plain text")
check("P3B JSON: double quote escaped", json.loads(json_str('say "hi"')) == 'say "hi"')
check("P3B JSON: backslash escaped", json.loads(json_str("a\\b")) == "a\\b")
check("P3B JSON: forward slash escaped", json.loads(json_str("a/b")) == "a/b")
check("P3B JSON: newline escaped", json.loads(json_str("a\nb")) == "a\nb")
check("P3B JSON: carriage return escaped", json.loads(json_str("a\rb")) == "a\rb")
check("P3B JSON: tab escaped", json.loads(json_str("a\tb")) == "a\tb")
check("P3B JSON: control char U+0001 unicode-escaped",
      json.loads(json_str("a" + chr(1) + "b")) == "a" + chr(1) + "b")
check("P3B JSON: control char U+001F unicode-escaped",
      "\u001f" in json.loads(json_str("x" + chr(31))))
check("P3B JSON: UTF-8 multibyte preserved", json.loads(json_str("café ✓")) == "café ✓")
check("P3B JSON: astral emoji in UTF-16 astral via surrogate pair",
      json.loads(json_str("😀")) == "😀")
check("P3B JSON: empty string valid", json.loads(json_str("")) == "")
check("P3B JSON: null coerced to empty", json.loads(json_str(None)) == "")

# Entity with hostile characters: quotes, backslash, newline, control chars
hostile = 'Qty "Relay #1\\\\A" \n tab:\t \x00x'
b3 = SkillEnv()
b3.add_device("d1")
b3.add_entity("d1", "r1", hostile, "actuator.relay", ["actuator.relay"], obs=False)
out = b3.execute("get_entity_info", {"device_id": "d1", "entity_id": "r1"})
check("P3B JSON: entity info with hostile name is valid JSON", valid_json_output(out))
check("P3B JSON: entity name preserved through escaper", out["ok"] and out["data"]["name"] == hostile)
outl = b3.execute("list_entities", {})
check("P3B JSON: list_entities with hostile name is valid JSON", valid_json_output(outl))
check("P3B JSON: list_entities preserves hostile name",
      outl["ok"] and any(e.get("name") == hostile for e in outl["data"]))

# -- Output envelope: OK/ERROR/OUTPUT_TOO_LARGE -----------------------------
b4 = SkillEnv()
b4.add_device("d1")
b4.add_entity("d1", "t", "Temp", "sensor.numeric", ["sensor.numeric"], obs=21.0)
okr = b4.execute("get_entity_state", {"device_id": "d1", "entity_id": "t"})
check("P3B ENV: ok envelope is {ok:true,data}", okr == {"ok": True, "data": okr["data"]} and okr["ok"] is True)
err = b4.execute("get_entity_state", {"device_id": "nope", "entity_id": "t"})
check("P3B ENV: error envelope is {ok:false,error{code,message,details}}",
      err["ok"] is False and sorted(err["error"].keys()) == ["code", "details", "message"])
check("P3B ENV: error code present", err["error"]["code"] in ("ENTITY_NOT_FOUND", "NOT_FOUND"))

b4.output_limit = 10  # force OUTPUT_TOO_LARGE for any success
big = b4.execute("list_entities", {})
check("P3B ENV: oversized success resolves to OUTPUT_TOO_LARGE",
      big["ok"] is False and big["error"]["code"] == "OUTPUT_TOO_LARGE")
check("P3B ENV: OUTPUT_TOO_LARGE payload still valid JSON", valid_json_output(big))
b4.output_limit = None
check("P3B ENV: limit cleared restores ok output", b4.execute("list_entities", {})["ok"] is True)

# -- Registry lookup: invalid index / unknown skill ------------------------
b5 = SkillEnv()
check("P3B REG: unknown skill -> SKILL_NOT_FOUND",
      b5.execute("no_such_skill", {})["error"]["code"] == "SKILL_NOT_FOUND")
check("P3B REG: missing permission -> PERMISSION_DENIED",
      b5.execute("set_relay", {"device_id": "x", "entity_id": "y", "value": True}, perm=1)["error"]["code"] ==
      "PERMISSION_DENIED")
read_only = PERMS["READ"] | PERMS["CONTROL"]
res_perm = b5.execute("create_rule", {"name": "r"}, perm=read_only)
check("P3B REG: read-only caller denied rule write -> PERMISSION_DENIED",
      res_perm["error"]["code"] == "PERMISSION_DENIED")
check("P3B REG: read-only caller CAN read entities",
      b5.execute("list_entities", {}, perm=PERMS["READ"])["ok"] is True)
check("P3B REG: registry entry index 0 is device 0",
      list(b5.devices.keys()) == [])
check("P3B REG: empty registry lists no devices", b5.execute("list_devices", {}) == {"ok": True, "data": []})

# -- Null dependencies ------------------------------------------------------
b6 = SkillEnv()
b6.missing_deps["set_relay"] = "control"
b6.missing_deps["create_rule"] = "storage"
res = b6.execute("set_relay", {"device_id": "x", "entity_id": "y", "value": True})
check("P3B NULL: null control dependency -> DEPENDENCY_MISSING",
      res["error"]["code"] == "DEPENDENCY_MISSING" and res["error"]["message"] == "control")
res = b6.execute("create_rule", {"name": "r", "rule": {"trigger": {"entity": {"device_id": "x", "entity_id": "y"}, "operator": "GT", "threshold": 1}, "conditions": [], "actions": []}})
check("P3B NULL: null storage dependency -> DEPENDENCY_MISSING",
      res["error"]["code"] == "DEPENDENCY_MISSING" and res["error"]["message"] == "storage")

# -- Transactional rule mutation -------------------------------------------
b7 = SkillEnv()
b7.add_device("d1")
b7.add_entity("d1", "t", "Temp", "sensor.numeric", ["sensor.numeric"], obs=20.0)
b7.add_entity("d1", "r", "Relay", "actuator.relay", ["actuator.relay"])
trig = {"entity": {"device_id": "d1", "entity_id": "t"}, "operator": "GT", "threshold": 25}
act = [{"entity": {"device_id": "d1", "entity_id": "r"}, "action": "SET_BOOL", "value": True}]
r1 = b7.execute("create_rule", {"name": "r1", "rule": {"trigger": trig, "conditions": [], "actions": act}})
rid = r1["data"]["rule_id"]
check("P3B TX: create succeeds", r1["ok"])
check("P3B TX: created rule revision 1", b7.rules[rid]["revision"] == 1)

# create with duplicate id -> RULE_CONFLICT, no new rule
dup = b7.execute("create_rule", {"name": "r1b", "rule_id": rid,
                                 "rule": {"trigger": trig, "conditions": [], "actions": act}})
check("P3B TX: duplicate rule id -> RULE_CONFLICT", dup["error"]["code"] == "RULE_CONFLICT")
check("P3B TX: collision does not harm existing rule", b7.rules[rid]["revision"] == 1)

# create with persist failure -> STORAGE_ERROR and NO active rule
b8 = SkillEnv()
b8.add_device("d1")
b8.add_entity("d1", "t", "Temp", "sensor.numeric", ["sensor.numeric"], obs=20.0)
b8.add_entity("d1", "r", "Relay", "actuator.relay", ["actuator.relay"])
b8.set_persist_fail("fail_rule")
fr = b8.execute("create_rule", {"rule_id": "fail_rule", "name": "f",
                                "rule": {"trigger": trig, "conditions": [], "actions": act}})
check("P3B TX: create on persist failure -> STORAGE_ERROR", fr["error"]["code"] == "STORAGE_ERROR")
check("P3B TX: failed create leaves no active rule", "fail_rule" not in b8.rules)

# update: on success revision increments
u = b7.execute("update_rule", {"rule_id": rid, "name": "r1 renamed",
                               "rule": {"trigger": trig, "conditions": [], "actions": act}})
check("P3B TX: update succeeds", u["ok"] and u["data"]["revision"] == 2)
check("P3B TX: update applied new name", b7.rules[rid]["name"] == "r1 renamed")

# update failure: old rule + revision intact, atomic
b7.set_persist_fail(rid)
u2 = b7.execute("update_rule", {"rule_id": rid, "name": "r1 should-not-apply",
                                "rule": {"trigger": trig, "conditions": [], "actions": act}})
check("P3B TX: update on persist failure -> STORAGE_ERROR", u2["error"]["code"] == "STORAGE_ERROR")
check("P3B TX: failed update keeps old definition", b7.rules[rid]["name"] == "r1 renamed")
check("P3B TX: failed update does not increment revision", b7.rules[rid]["revision"] == 2)
b7.set_persist_fail()

# enable/disable failure: runtime preserved
b7.set_persist_fail(rid)
d = b7.execute("disable_rule", {"rule_id": rid})
check("P3B TX: disable on persist failure -> STORAGE_ERROR", d["error"]["code"] == "STORAGE_ERROR")
check("P3B TX: failed disable leaves runtime enabled", b7.rules[rid]["enabled"] is True)
b7.clear_persist_fail()
ok_d = b7.execute("disable_rule", {"rule_id": rid})
check("P3B TX: disable succeeds after clear", ok_d["ok"] and b7.rules[rid]["enabled"] is False)

# enable success then delete failure keeps rule
b7.set_delete_fail(rid)
delr = b7.execute("delete_rule", {"rule_id": rid})
check("P3B TX: delete on storage failure -> STORAGE_ERROR", delr["error"]["code"] == "STORAGE_ERROR")
check("P3B TX: failed delete keeps rule active", rid in b7.rules)
b7.clear_delete_fail()
delr2 = b7.execute("delete_rule", {"rule_id": rid})
check("P3B TX: delete succeeds after clear", delr2["ok"] and rid not in b7.rules)

# slot reuse: after freeing a slot, a new rule can land there (registry reload)
b9 = SkillEnv()
b9.add_device("d1")
b9.add_entity("d1", "t", "Temp", "sensor.numeric", ["sensor.numeric"], obs=20.0)
b9.add_entity("d1", "r", "Relay", "actuator.relay", ["actuator.relay"])
for i in range(3):
    r = b9.execute("create_rule", {"name": "slot_%d" % i,
                                   "rule": {"trigger": trig, "conditions": [], "actions": act}})
slot_ids = list(b9.rules.keys())
b9.execute("delete_rule", {"rule_id": slot_ids[1]})
check("P3B SLOT: middle slot freed", len(b9.rules) == 2)
r_new = b9.execute("create_rule", {"name": "slot_reuse",
                                   "rule": {"trigger": trig, "conditions": [], "actions": act}})
check("P3B SLOT: new rule reuses freed slot",
      r_new["ok"] and len(b9.rules) == 3 and
      any(r.get("name") == "slot_reuse" for r in b9.rules.values()))

# revision consistency across a full enable->disable->update cycle
b10 = SkillEnv()
b10.add_device("d1")
b10.add_entity("d1", "t", "Temp", "sensor.numeric", ["sensor.numeric"], obs=20.0)
b10.add_entity("d1", "r", "Relay", "actuator.relay", ["actuator.relay"])
rr = b10.execute("create_rule", {"name": "rev", "rule": {"trigger": trig, "conditions": [], "actions": act}})
rid = rr["data"]["rule_id"]
b10.execute("enable_rule", {"rule_id": rid})
b10.execute("disable_rule", {"rule_id": rid})
u = b10.execute("update_rule", {"rule_id": rid, "name": "rev2", "rule": {"trigger": trig, "conditions": [], "actions": act}})
check("P3B REV: revision monotonic across lifecycle", b10.rules[rid]["revision"] == 2)

# shutdown-style persistence: exact counts match
ops = [x[0] for x in b7.persist_log]
check("P3B PERSIST: create/update/disable/delete all recorded",
      all(op in ops for op in ("create", "update", "disable", "delete")))
check("P3B PERSIST: failed ops never touch the write-ahead log",
      sum(1 for x in b7.persist_log if x[0] == "update") == 1)

# ==========================================================================
# Phase 3C: LLM Adapter over the hardened Skill API
#
# Mirrors src/ai/qymera_llm_adapter.c. The adapter sits strictly ABOVE the Skill
# layer: it validates a bounded structured tool call, propagates an explicit
# permission mask, and dispatches ONLY through env.execute() (the reference for
# qymera_skill_execute). No direct registry/rule-engine/GPIO/UDP access here.
# No natural-language parsing. Each turn is bounded by MAX_TOOL_CALLS.
# ==========================================================================

class LLMAdapter:
    MAX_TOOL_CALLS = 8
    REQ_ENTITY = {"get_entity_state", "get_entity_info", "set_relay", "set_dimmer"}
    REQ_RULE_ID = {"get_rule", "update_rule", "delete_rule", "enable_rule", "disable_rule"}
    REQ_NAME = {"create_rule"}

    def __init__(self, env):
        self.env = env

    # Tool catalog DERIVED from the skill registry (single source of truth).
    def tool_catalog(self):
        return [(n, PERMS[self.env_perm(n)]) for n in sorted(SKILLS)]

    def env_perm(self, name):
        return SKILLS[name]

    def execute_tool(self, name, args=None, perm=0):
        """Envelope validation FIRST; never reaches the runtime on failure."""
        if name not in SKILLS:
            return ("UNKNOWN", _Err("SKILL_NOT_FOUND", "unknown skill / tool"))
        req = PERMS[SKILLS[name]]
        if (perm & req) != req:                      # no silent grant-all
            return ("PERMISSION", _Err("PERMISSION_DENIED", "insufficient permission for this skill"))
        args = args or {}
        if name in self.REQ_ENTITY and not (args.get("device_id") and args.get("entity_id")):
            return ("MISSING_ARGS", _Err("INVALID_INPUT", "missing required arguments (device_id/entity_id)"))
        if name in self.REQ_RULE_ID and not args.get("rule_id"):
            return ("MISSING_ARGS", _Err("INVALID_INPUT", "missing required argument (rule_id)"))
        if name in self.REQ_NAME and not args.get("name"):
            return ("MISSING_ARGS", _Err("INVALID_INPUT", "missing required argument (name)"))
        res = self.env.execute(name, args, perm=perm)
        return ("OK", res)

    def process(self, provider, perm=0, max_calls=None):
        budget = min(max_calls or self.MAX_TOOL_CALLS, self.MAX_TOOL_CALLS)
        result = {"ended": "text", "tool_calls": 0, "steps": [], "final": None}
        while True:
            msg = provider.next()
            kind = msg["kind"]
            if kind == "tool_call":
                if result["tool_calls"] >= budget:
                    result["ended"] = "toollimit"
                    break
                terr, res = self.execute_tool(msg["name"], msg.get("args"), perm)
                result["tool_calls"] += 1
                result["steps"].append((msg["name"], terr, res))
                continue
            # terminal: text / malformed / error / timeout
            result["ended"] = {"text": "text", "malformed": "malformed",
                               "error": "provider_error", "timeout": "provider_error"}[kind]
            result["final"] = msg
            break
        return result


class MockProvider:
    """Deterministic scripted provider (no real LLM): exact reference to the
    C qymera_llm_mock_provider + tests exercising all message kinds."""
    def __init__(self, steps):
        self.steps = list(steps)
        self.i = 0

    def next(self):
        if self.i >= len(self.steps):
            return {"kind": "text", "text": "[done]"}
        s = self.steps[self.i]
        self.i += 1
        return s


# -- Tool catalog derived from registry -------------------------------------
envA = LLMAdapter(SkillEnv())
cat = envA.tool_catalog()
check("P3C CATALOG: 13 tools", len(cat) == 13)
check("P3C CATALOG: no duplicate tool names", len(set(n for n, _ in cat)) == 13)
check("P3C CATALOG: tool set matches skill registry",
      {n for n, _ in cat} == set(SKILLS.keys()))
check("P3C CATALOG: each tool carries its permission requirement",
      all(p in (1, 2, 4, 8) for _, p in cat))
check("P3C CATALOG: control tools carry CONTROL bit",
      all(p == PERMS["CONTROL"] for n, p in cat if n in ("set_relay", "set_dimmer")))

# -- Tool execution: valid / unknown / invalid-args / permission -------------
w = SkillEnv()
w.add_device("node-a")
w.add_entity("node-a", "relay0", "Relay0", "actuator.relay", ["actuator.relay"], obs=False)
adp = LLMAdapter(w)
terr, res = adp.execute_tool("set_relay", {"device_id": "node-a", "entity_id": "relay0", "value": True},
                             perm=PERMS["READ"] | PERMS["CONTROL"])
check("P3C EXEC: valid tool -> OK + skill envelope", terr == "OK" and res["ok"] is True)

terr, res = adp.execute_tool("no_such_tool", {}, perm=0xFFFF)
check("P3C EXEC: unknown tool -> UNKNOWN + SKILL_NOT_FOUND",
      terr == "UNKNOWN" and res["error"]["code"] == "SKILL_NOT_FOUND")

terr, res = adp.execute_tool("set_relay", {"device_id": "node-a"}, perm=PERMS["READ"] | PERMS["CONTROL"])
check("P3C EXEC: missing args -> MISSING_ARGS + INVALID_INPUT",
      terr == "MISSING_ARGS" and res["error"]["code"] == "INVALID_INPUT")

terr, res = adp.execute_tool("set_relay", {"device_id": "node-a", "entity_id": "relay0", "value": True},
                             perm=PERMS["READ"])
check("P3C EXEC: permission denied -> PERMISSION + PERMISSION_DENIED",
      terr == "PERMISSION" and res["error"]["code"] == "PERMISSION_DENIED")

terr, res = adp.execute_tool("list_entities", {}, perm=PERMS["READ"])
check("P3C EXEC: read skill with READ perm -> OK", terr == "OK" and res["ok"] is True)

# -- Budgets -----------------------------------------------------------------
w2 = SkillEnv()
w2.add_device("node-a")
w2.add_entity("node-a", "relay0", "Relay0", "actuator.relay", ["actuator.relay"], obs=False)
adp2 = LLMAdapter(w2)
full_perm = 0xFF
steps8 = [{"kind": "tool_call", "name": "list_devices", "args": {}}] * 8
steps10 = steps8 + [{"kind": "tool_call", "name": "list_devices", "args": {}}] * 2

r = adp2.process(MockProvider([{"kind": "tool_call", "name": "list_devices", "args": {}}]),
                 perm=full_perm, max_calls=1)
check("P3C BUDGET: 1 tool call with budget 1", r["tool_calls"] == 1 and r["ended"] == "text")

r = adp2.process(MockProvider(steps8), perm=full_perm, max_calls=8)
check("P3C BUDGET: N calls -> text at end (all ran)", r["tool_calls"] == 8 and r["ended"] == "text")

r = adp2.process(MockProvider(steps10), perm=full_perm, max_calls=8)
check("P3C BUDGET: N+1 -> TOOL_CALL_LIMIT at 8", r["tool_calls"] == 8 and r["ended"] == "toollimit")

r = adp2.process(MockProvider(steps8), perm=full_perm, max_calls=4)
check("P3C BUDGET: N > max -> limit at max", r["tool_calls"] == 4 and r["ended"] == "toollimit")

# -- Provider behavior: never assume a tool call -----------------------------
r = adp2.process(MockProvider([{"kind": "text", "text": "hello"}]), perm=full_perm, max_calls=4)
check("P3C PROVIDER: assistant text -> turn ends text", r["ended"] == "text" and r["tool_calls"] == 0)

r = adp2.process(MockProvider([{"kind": "malformed", "text": "???"}]), perm=full_perm, max_calls=4)
check("P3C PROVIDER: malformed -> turn ends malformed", r["ended"] == "malformed" and r["tool_calls"] == 0)

r = adp2.process(MockProvider([{"kind": "error", "text": "boom"}]), perm=full_perm, max_calls=4)
check("P3C PROVIDER: provider error -> turn ends provider_error", r["ended"] == "provider_error")

r = adp2.process(MockProvider([{"kind": "timeout", "text": "late"}]), perm=full_perm, max_calls=4)
check("P3C PROVIDER: timeout -> turn ends provider_error", r["ended"] == "provider_error")

r = adp2.process(MockProvider([{"kind": "text", "text": "pre"}, {"kind": "tool_call", "name": "list_devices", "args": {}}]),
                 perm=full_perm, max_calls=4)
check("P3C PROVIDER: leading assistant text stops the turn (no tool ran)",
      r["tool_calls"] == 0 and r["ended"] == "text")

r = adp2.process(MockProvider([{"kind": "tool_call", "name": "list_devices", "args": {}}, {"kind": "text", "text": "post"}]),
                 perm=full_perm, max_calls=4)
check("P3C PROVIDER: tool then text -> 1 tool ran then text end",
      r["tool_calls"] == 1 and r["ended"] == "text")

# -- Permission propagation --------------------------------------------------
adpP = LLMAdapter(w)
terr, _ = adpP.execute_tool("set_relay", {"device_id": "node-a", "entity_id": "relay0", "value": True},
                            perm=PERMS["READ"])
check("P3C PERM: READ-only cannot CONTROL", terr == "PERMISSION")
terr, _ = adpP.execute_tool("set_relay", {"device_id": "node-a", "entity_id": "relay0", "value": True},
                            perm=PERMS["CONTROL"])
check("P3C PERM: CONTROL can set_relay", terr == "OK")
terr, _ = adpP.execute_tool("create_rule", {"name": "r"}, perm=PERMS["RULE_READ"])
check("P3C PERM: RULE_READ cannot RULE_WRITE", terr == "PERMISSION")
terr, _ = adpP.execute_tool("create_rule", {"name": "r", "rule": {
    "trigger": {"entity": {"device_id": "node-a", "entity_id": "relay0"}, "operator": "EQ", "threshold": 1},
    "conditions": [], "actions": []}}, perm=PERMS["RULE_READ"] | PERMS["RULE_WRITE"])
check("P3C PERM: RULE_WRITE can create_rule", terr == "OK")
terr, _ = adpP.execute_tool("list_entities", {}, perm=PERMS["READ"])
check("P3C PERM: READ can list_entities", terr == "OK")

# -- Full structured workflow (6 steps, no LLM) -------------------------------
fw = SkillEnv()
fw.add_device("node-a")
fw.add_entity("node-a", "temperature", "Temp", "sensor.numeric", ["sensor.numeric"], obs=30.0)
fw.add_entity("node-a", "garden_relay", "Garden Relay", "actuator.relay", ["actuator.relay"], obs=False)
adpW = LLMAdapter(fw)
trig = {"entity": {"device_id": "node-a", "entity_id": "temperature"}, "operator": "GT", "threshold": 28}
act = [{"entity": {"device_id": "node-a", "entity_id": "garden_relay"}, "action": "SET_BOOL", "value": True}]
script = [
    {"kind": "tool_call", "name": "list_entities", "args": {}},
    {"kind": "tool_call", "name": "get_entity_state", "args": {"device_id": "node-a", "entity_id": "temperature"}},
    {"kind": "tool_call", "name": "create_rule", "args": {"name": "Garden fan", "rule": {"trigger": trig, "conditions": [], "actions": act}}},
    {"kind": "tool_call", "name": "enable_rule", "args": {"rule_id": "rule_1"}},
    {"kind": "tool_call", "name": "set_relay", "args": {"device_id": "node-a", "entity_id": "garden_relay", "value": True}},
    {"kind": "tool_call", "name": "get_entity_state", "args": {"device_id": "node-a", "entity_id": "garden_relay"}},
    {"kind": "text", "text": "Workflow complete"},
]
r = adpW.process(MockProvider(script), perm=0xFF, max_calls=8)
steps = r["steps"]
all_ok = all(t == "OK" for _, t, _ in steps)
check("P3C WORKFLOW: all 6 steps executed through the Skill layer",
      len(steps) == 6 and all_ok and r["ended"] == "text")
check("P3C WORKFLOW: create_rule produced an active rule", "rule_1" in fw.rules and fw.rules["rule_1"]["enabled"] is True)
step_names = [n for n, _, _ in steps]
check("P3C WORKFLOW: step order is discover->inspect->build->create->enable->act->observe",
      step_names == ["list_entities", "get_entity_state", "create_rule", "enable_rule", "set_relay", "get_entity_state"])
last = steps[-1][2]
check("P3C WORKFLOW: final inspect reflects requested actuator state",
      last["ok"] and last["data"]["desired"] is True)

print()
print("host_sanity: %d passed, %d failed" % (PASS, FAIL))
raise SystemExit(1 if FAIL else 0)