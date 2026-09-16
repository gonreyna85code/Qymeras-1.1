"""Contract tests against the mock Node (tests/integration/mock_node.py).

Validates the authoritative Qymera 1.0.0 firmware wire surface in
docs/api-contract.yaml against the real mocked endpoints: GET /calib (bare
entity array, no envelope), GET /firmware, POST /toggle (id=<uid>, flips
state), POST /dimmer (id=<uid>&value=0..100) and HTTP-status-only errors with
text/plain bodies. Runs against the in-process mock Node over real HTTP; the
firmware's qymera_node_client consumes exactly this surface (mirrored in
tests/host_sanity.py). Run via run_contract_tests.py or pytest.
"""
from __future__ import print_function
import json
import socket
import urllib.error
import urllib.request

from mock_node import (CALIB_FIELDS, CALIB_TYPE_RELAY, CALIB_TYPE_DIMMER,
                       CALIB_TYPE_TEMP, MockNode)

_FORM_CT = "application/x-www-form-urlencoded"

_CHECKS = []


def _check(name, cond):
    _CHECKS.append((name, bool(cond)))


def _free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def _request(port, path, method="GET", data=None, content_type=None, headers=None):
    url = "http://127.0.0.1:%d%s" % (port, path)
    payload = None
    if isinstance(data, str):
        payload = data.encode("utf-8")
    elif data is not None:
        payload = json.dumps(data).encode("utf-8")
    req = urllib.request.Request(url, data=payload, method=method)
    if content_type:
        req.add_header("Content-Type", content_type)
    for k, v in (headers or {}).items():
        req.add_header(k, v)
    try:
        resp = urllib.request.urlopen(req, timeout=5)
        raw = resp.read().decode("utf-8", "replace")
        try:
            body = json.loads(raw)
        except ValueError:
            body = raw
        return resp.getcode(), body, dict(resp.headers)
    except urllib.error.HTTPError as e:
        raw = e.read().decode("utf-8", "replace")
        try:
            body = json.loads(raw)
        except ValueError:
            body = raw
        return e.code, body, dict(e.headers)
    except urllib.error.URLError:
        return 0, None, {}


def run():
    node = MockNode()
    port = node.serve(_free_port())

    # ---- /calib: bare array, no envelope ----------------------------------
    code, body, headers = _request(port, "/calib")
    _check("calib: HTTP 200", code == 200)
    _check("calib: CORS wildcard", headers.get("Access-Control-Allow-Origin") == "*")
    _check("calib: bare array (no {ok,data} envelope)",
           isinstance(body, list) and len(body) == 3 and not (isinstance(body, dict) and "ok" in body))
    for e in body:
        _check("calib: schema fields present", all(k in e for k in CALIB_FIELDS))
        _check("calib: type is int 1..12", isinstance(e["type"], int) and 1 <= e["type"] <= 12)
        _check("calib: uid + device_uid u32", isinstance(e["id"], int) and isinstance(e["device_uid"], int))
        _check("calib: avail/state are booleans", isinstance(e["avail"], bool) and isinstance(e["state"], bool))
    relay = next(e for e in body if e["type"] == CALIB_TYPE_RELAY)
    temp = next(e for e in body if e["type"] == CALIB_TYPE_TEMP)
    _check("calib: numeric value + correction carried",
           isinstance(temp["value"], (int, float)) and isinstance(temp["correction"], (int, float)))
    _check("calib: owner ip carried", bool(relay["ip"]))

    # ---- /firmware ----------------------------------------------------------
    code, fw, _ = _request(port, "/firmware")
    _check("firmware: HTTP 200", code == 200)
    _check("firmware: product/version/platform",
           all(k in fw for k in ("product", "version", "platform")))

    # ---- /toggle: flips relay state ----------------------------------------
    code, text, _ = _request(port, "/toggle", "POST", "id=1", _FORM_CT)
    _check("toggle: relay -> 200 text/plain OK", code == 200 and text == "OK")
    _, body, _ = _request(port, "/calib")
    relay_on = next(e for e in body if e["type"] == CALIB_TYPE_RELAY)
    _check("toggle: snapshot shows flipped state True", relay_on["state"] is True)
    _request(port, "/toggle", "POST", "id=1", _FORM_CT)
    _, body, _ = _request(port, "/calib")
    relay_off = next(e for e in body if e["type"] == CALIB_TYPE_RELAY)
    _check("toggle: second toggle flips back False", relay_off["state"] is False)

    # toggle errors
    code, body, _ = _request(port, "/toggle", "POST", "id=3", _FORM_CT)  # temp
    _check("toggle: non-actuator type -> 400", code == 400 and isinstance(body, str))
    code, _, _ = _request(port, "/toggle", "POST", "id=", _FORM_CT)
    _check("toggle: missing id -> 400", code == 400)
    code, _, _ = _request(port, "/toggle", "POST", "id=abc", _FORM_CT)
    _check("toggle: non-numeric id -> 400", code == 400)
    code, _, _ = _request(port, "/toggle", "POST", "id=999", _FORM_CT)
    _check("toggle: unknown id -> 404", code == 404)
    code, _, _ = _request(port, "/toggle", "POST", "id=1")  # json content-type
    _check("toggle: form parsed regardless of JSON header", code == 200)

    # ---- /dimmer: sets level 0..100 -----------------------------------------
    code, text, _ = _request(port, "/dimmer", "POST", "id=2&value=80", _FORM_CT)
    _check("dimmer: set level -> 200 text/plain OK", code == 200 and text == "OK")
    _, body, _ = _request(port, "/calib")
    dim = next(e for e in body if e["type"] == CALIB_TYPE_DIMMER)
    _check("dimmer: snapshot shows level 80 / ON", dim["value"] == 80.0 and dim["state"] is True)

    code, _, _ = _request(port, "/dimmer", "POST", "id=2&value=101", _FORM_CT)
    _check("dimmer: out of range 101 -> 400", code == 400)
    code, _, _ = _request(port, "/dimmer", "POST", "id=2", _FORM_CT)
    _check("dimmer: missing value -> 400", code == 400)
    code, _, _ = _request(port, "/dimmer", "POST", "id=1&value=50", _FORM_CT)  # relay
    _check("dimmer: wrong actuator type -> 400", code == 400)
    code, _, _ = _request(port, "/dimmer", "POST", "id=999&value=50", _FORM_CT)
    _check("dimmer: unknown id -> 404", code == 404)

    # ---- unknown path -> 404 -------------------------------------------------
    code, _, _ = _request(port, "/nope")
    _check("unknown path -> 404", code == 404)

    # ---- rate limit applies to protected POSTs only --------------------------
    ok = 0
    for _ in range(6):
        c, _, _ = _request(port, "/calib/set", "POST", "id=1", _FORM_CT)
        if c == 200:
            ok += 1
    code, _, _ = _request(port, "/calib/set", "POST", "id=1", _FORM_CT)
    _check("calib/set: 7th rapid call after burst 6 -> 429", ok == 6 and code == 429)
    code, _, _ = _request(port, "/toggle", "POST", "id=1", _FORM_CT)
    _check("toggle: exempt from rate limit after 429", code == 200)

    node.stop()

    failed = [n for n, ok in _CHECKS if not ok]
    print("contract: %d passed, %d failed (%d total)"
          % (len(_CHECKS) - len(failed), len(failed), len(_CHECKS)))
    for n, ok in _CHECKS:
        print("  [%s] %s" % ("PASS" if ok else "FAIL", n))
    return 1 if failed else 0


def test_contract_surface():
    """pytest entrypoint alias."""
    assert run() == 0


if __name__ == "__main__":
    raise SystemExit(run())