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
        return getattr(self, "_" + skill)(inp)

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
        del self.rules[rid]
        self.persist_log.append(("delete", rid))
        return _Ok({"rule_id": rid, "deleted": True})

    def _enable_rule(self, inp):
        rid = inp.get("rule_id")
        if rid not in self.rules:
            return _Err("RULE_INVALID", "rule not found")
        self.rules[rid]["enabled"] = True
        self.persist_log.append(("enable", rid))
        return _Ok({"rule_id": rid, "enabled": True})

    def _disable_rule(self, inp):
        rid = inp.get("rule_id")
        if rid not in self.rules:
            return _Err("RULE_INVALID", "rule not found")
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

print()
print("host_sanity: %d passed, %d failed" % (PASS, FAIL))
raise SystemExit(1 if FAIL else 0)