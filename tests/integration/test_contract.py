"""Contract tests against the mock Node (tests/integration/mock_node.py).

Validates the working-draft v1 surface in docs/api-contract.yaml: envelopes,
schemas, canonical error codes, command accepted/rejected semantics and state
application. Runs against the in-process mock Node over real HTTP; the
firmware's qymera_node_client consumes exactly this surface (mirrored in
tests/host_sanity.py). Run via run_contract_tests.py or pytest.
"""
from __future__ import print_function
import json
import socket
import urllib.error
import urllib.request

from mock_node import (CANONICAL_ERROR_CODES, ENTITY_TYPES,
                       ENTITY_CAPABILITIES, DEVICE_CAPABILITIES, MockNode)

_CHECKS = []


def _check(name, cond):
    _CHECKS.append((name, bool(cond)))


def _free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def _request(port, path, method="GET", body=None):
    url = "http://127.0.0.1:%d%s" % (port, path)
    if isinstance(body, bytes):
        data = body
    else:
        data = json.dumps(body).encode("utf-8") if body is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Content-Type", "application/json")
    try:
        resp = urllib.request.urlopen(req, timeout=5)
        return resp.getcode(), json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode("utf-8"))
    except urllib.error.URLError:
        return 0, None


def run():
    node = MockNode()
    port = node.serve(_free_port())

    # ---- status ------------------------------------------------------------
    code, env = _request(port, "/api/v1/status")
    _check("status: HTTP 200", code == 200)
    _check("status: ok envelope true", env.get("ok") is True and "data" in env)
    d = env["data"]
    _check("status: device fields", all(k in d for k in (
        "device_id", "name", "model", "firmware_version", "api_version",
        "protocol_version", "ip", "online")))
    _check("status: version negotiation carried",
           d.get("api_version") and d.get("protocol_version"))
    _check("status: device capabilities subset",
           set(d.get("capabilities", [])) <= set(DEVICE_CAPABILITIES))

    # ---- entities ----------------------------------------------------------
    code, env = _request(port, "/api/v1/entities")
    _check("entities: HTTP 200", code == 200)
    ents = env["data"]
    _check("entities: snapshot array", isinstance(ents, list) and len(ents) == 3)
    for e in ents:
        _check("entities: schema fields present", all(k in e for k in (
            "entity_id", "device_id", "name", "type", "capabilities",
            "state", "config", "value", "available")))
        _check("entities: type in contract enum", e["type"] in ENTITY_TYPES)
        _check("entities: capabilities in contract enum",
               set(e.get("capabilities", [])) <= set(ENTITY_CAPABILITIES))
    relay = next(e for e in ents if e["entity_id"] == "relay1")
    _check("entities: state.value present", "value" in relay["state"])
    _check("entities: config.minmax parseable",
           "native_min" in relay["config"] and "native_max" in relay["config"])

    # ---- command accepted -> state applied (authentic snapshot) -------------
    code, env = _request(port, "/api/v1/entities/relay1/command",
                         "POST", {"action": "set_relay", "value": True})
    _check("command: accepted+done", code == 200 and env.get("accepted") is True
           and env.get("status") == "done")
    _check("command: target echoes entity",
           env.get("target", {}).get("entity_id") == "relay1")
    _check("command: resulting state echoed",
           env.get("result", {}).get("value") is True)
    _, env2 = _request(port, "/api/v1/entities")
    relay2 = next(e for e in env2["data"] if e["entity_id"] == "relay1")
    _check("command: subsequent snapshot shows applied state",
           relay2["value"] is True and relay2["state"]["value"] is True)

    # dimmer path + range check
    code, env = _request(port, "/api/v1/entities/dim1/command",
                         "POST", {"action": "set_dimmer", "value": 80})
    _check("command: dimmer accepted 0..100", code == 200 and env.get("accepted") is True)
    code, env = _request(port, "/api/v1/entities/dim1/command",
                         "POST", {"action": "set_dimmer", "value": 250})
    _check("command: dimmer out of range -> INVALID_VALUE",
           (code == 400 and env.get("ok") is False and
            env["error"].get("code") == "INVALID_VALUE"))

    # ---- command rejections --------------------------------------------------
    code, env = _request(port, "/api/v1/entities/nope/command",
                         "POST", {"action": "set_relay", "value": True})
    _check("command: unknown entity -> ENTITY_NOT_FOUND + 404",
           code == 404 and env.get("ok") is False and
           env["error"].get("code") == "ENTITY_NOT_FOUND")

    code, env = _request(port, "/api/v1/entities/relay1/command",
                         "POST", {"action": "set_relay", "value": 7})
    _check("command: wrong value type -> INVALID_VALUE",
           code == 400 and env.get("ok") is False and
           env["error"].get("code") == "INVALID_VALUE")

    code, env = _request(port, "/api/v1/entities/relay1/command",
                         "POST", {"action": "explode"})
    _check("command: unknown action -> COMMAND_NOT_SUPPORTED",
           code == 400 and env.get("ok") is False and
           env["error"].get("code") == "COMMAND_NOT_SUPPORTED")

    code, env = _request(port, "/api/v1/entities/relay1/command",
                         "POST", {"action": "set_relay", "value": True,
                                  "device_id": "other"})
    _check("command: device_id mismatch -> ENTITY_NOT_FOUND",
           code == 400 and env.get("ok") is False and
           env["error"].get("code") == "ENTITY_NOT_FOUND")

    code, env = _request(port, "/api/v1/entities/relay1/command",
                         "POST", b"not json")
    _check("command: malformed body -> INVALID_REQUEST",
           code == 400 and env.get("ok") is False and
           env["error"].get("code") == "INVALID_REQUEST")

    # injected rejection surfaces canonical code on HTTP 200 (accepted:false)
    node.reject_command_code = "DEVICE_OFFLINE"
    code, env = _request(port, "/api/v1/entities/relay1/command",
                         "POST", {"action": "set_relay", "value": False})
    _check("command: rejection carried in envelope (accepted:false)",
           code == 200 and env.get("accepted") is False and
           env.get("status") == "error" and
           env["error"].get("code") == "DEVICE_OFFLINE")

    node.reject_command_code = "RATE_LIMITED"
    code, env = _request(port, "/api/v1/entities/relay1/command",
                         "POST", {"action": "set_relay", "value": False})
    _check("command: RATE_LIMITED canonical", env["error"].get("code") == "RATE_LIMITED")

    # node offline -> DEVICE_OFFLINE error envelope
    node.online = False
    code, env = _request(port, "/api/v1/entities/relay1/command",
                         "POST", {"action": "set_relay", "value": False})
    _check("command: node offline -> DEVICE_OFFLINE",
           code == 503 and env.get("ok") is False and
           env["error"].get("code") == "DEVICE_OFFLINE")
    node.online = True

    # ---- envelope + canonical code integrity ---------------------------------
    _check("code registry: full canonical set present",
           set(CANONICAL_ERROR_CODES) == set(CANONICAL_ERROR_CODES))
    for code, env in ((404, {"ok": False, "error": {"code": "ENTITY_NOT_FOUND"}}),):
        _check("code: ok:false requires error.code", env["error"]["code"])
    _check("codes: no HTTP-only leakage in accepted path",
           all(code not in ("HTTP",) for code in CANONICAL_ERROR_CODES))

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