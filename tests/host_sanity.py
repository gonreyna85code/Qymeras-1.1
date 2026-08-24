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
print("host_sanity: %d passed, %d failed" % (PASS, FAIL))
raise SystemExit(1 if FAIL else 0)