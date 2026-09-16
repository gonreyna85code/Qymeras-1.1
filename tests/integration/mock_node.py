"""Qymera Node mock for Dashboard integration tests.

Implements the authoritative Qymera 1.0.0 firmware API (github.com/
gonreyna85code/Qymera — the wire surface qymera_node_client consumes):
GET /calib (BARE JSON entity array, no envelope), GET /firmware, POST /toggle
(form id=<uid>; flips relay/dimmer state), POST /dimmer (form id=<uid>&value=
0..100). Command success is HTTP 200 text/plain "OK"; errors are conveyed ONLY
by HTTP status (400/401/404/405/429) with a text/plain body — there are no
JSON error codes on the wire. Run directly for manual wiring
(start_node_mock.py) or import in test_contract.py for in-process testing.
"""
from __future__ import print_function
import json
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

# calib `type` ids (firmware sensor/actuator enum, 1..12)
CALIB_TYPE_LUMI = 1
CALIB_TYPE_HUMI = 2
CALIB_TYPE_TEMP = 3
CALIB_TYPE_PRESS = 4
CALIB_TYPE_LEVEL = 5
CALIB_TYPE_AIRQ = 6
CALIB_TYPE_RAIN = 7
CALIB_TYPE_DIMMER = 8
CALIB_TYPE_RELAY = 9
CALIB_TYPE_TIME = 10
CALIB_TYPE_GENERIC = 11
CALIB_TYPE_CONTACT = 12

ACTUATOR_TYPES = (CALIB_TYPE_RELAY, CALIB_TYPE_DIMMER)

CALIB_FIELDS = [
    "id", "index", "device_uid", "name", "value", "correction",
    "avail", "pulse", "state", "pulse_ms", "persist", "fade",
    "type", "local", "age_ms", "ip",
]


def default_firmware(product="nodeA"):
    return {
        "product": product,
        "version": "1.0.0",
        "platform": "esp32",
        "state": "active",
        "latest": "1.0.0",
        "channel": "stable",
        "available": False,
        "progress": 0,
        "error": None,
    }


def default_calib():
    return [
        {
            "id": 1, "index": 1, "device_uid": 0xA1, "name": "Relay One",
            "value": 0.0, "correction": 0.0, "avail": True, "pulse": False,
            "state": False, "pulse_ms": 0, "persist": True, "fade": False,
            "type": CALIB_TYPE_RELAY, "local": True, "age_ms": 0,
            "ip": "127.0.0.1",
        },
        {
            "id": 2, "index": 2, "device_uid": 0xA1, "name": "Dimmer One",
            "value": 30.0, "correction": 0.0, "avail": True, "pulse": False,
            "state": True, "pulse_ms": 0, "persist": True, "fade": False,
            "type": CALIB_TYPE_DIMMER, "local": True, "age_ms": 0,
            "ip": "127.0.0.1",
        },
        {
            "id": 3, "index": 3, "device_uid": 0xA1, "name": "Temp One",
            "value": 24.5, "correction": 0.5, "avail": True, "pulse": False,
            "state": False, "pulse_ms": 0, "persist": False, "fade": False,
            "type": CALIB_TYPE_TEMP, "local": True, "age_ms": 0,
            "ip": "127.0.0.1",
        },
    ]


class MockNodeHandler(BaseHTTPRequestHandler):
    node = None  # set by server thread owner

    # ---- plumbing ---------------------------------------------------------
    def log_message(self, *args):
        pass

    def _send_bytes(self, code, body, content_type):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)

    def _send_json(self, payload, code=200):
        self._send_bytes(code, json.dumps(payload), "application/json")

    def _send_ok(self):
        self._send_bytes(200, "OK", "text/plain")

    def _send_error(self, code, message="Bad Request"):
        self._send_bytes(code, message, "text/plain")

    # ---- routes -----------------------------------------------------------
    def do_GET(self):
        n = self.node
        path = self.path.split("?")[0]
        if path == "/calib":
            return self._send_json(n.calib())
        if path == "/firmware":
            return self._send_json(n.firmware())
        return self._send_error(404, "Not Found")

    def do_POST(self):
        n = self.node
        path = self.path.split("?")[0]
        length = int(self.headers.get("Content-Length", 0) or 0)
        raw = self.rfile.read(length).decode("utf-8", "replace") if length else ""
        form = parse_qs(raw)

        if path == "/toggle":
            return self._toggle(form)
        if path == "/dimmer":
            return self._dimmer(form)
        if path == "/calib/set":
            if not n.rate_limit_ok():
                return self._send_error(429, "Too Many Requests")
            code = n.calib_set(form)
            if code == 400:
                return self._send_error(400, "Bad Request")
            if code == 404:
                return self._send_error(404, "Not Found")
            return self._send_ok()
        return self._send_error(404, "Not Found")

    def _toggle(self, form):
        n = self.node
        raw_id = (form.get("id") or [""])[0]
        if not raw_id or not raw_id.isdigit():
            return self._send_error(400, "Bad Request")
        e = n.find_entity(int(raw_id))
        if not e:
            return self._send_error(404, "Not Found")
        try:
            n.toggle(int(raw_id))
        except ValueError:
            return self._send_error(400, "Bad Request")  # not an actuator
        return self._send_ok()

    def _dimmer(self, form):
        n = self.node
        raw_id = (form.get("id") or [""])[0]
        raw_val = (form.get("value") or [None])[0]
        if not raw_id or not raw_id.isdigit():
            return self._send_error(400, "Bad Request")
        if raw_val is None or not raw_val.isdigit():
            return self._send_error(400, "Bad Request")
        e = n.find_entity(int(raw_id))
        if not e:
            return self._send_error(404, "Not Found")
        try:
            n.dimmer(int(raw_id), int(raw_val))
        except ValueError:
            return self._send_error(400, "Bad Request")  # wrong type / range
        return self._send_ok()


class MockNode(object):
    """Holds the authoritative node model + configurable fault injection.

    NOTE: /toggle toggles (flips) state, exactly like the firmware; it is NOT
    an absolute set. The Dashboard compensates in the control layer by only
    sending a toggle when the observed snapshot differs from the desired
    state, so tests should assert flip semantics here.
    """

    RATE_WINDOW_MS = 2000
    RATE_BURST = 6  # protected POSTs only (calib/set); /toggle+/dimmer exempt

    def __init__(self, calib=None, firmware=None):
        self._calib = [dict(e) for e in (calib if calib is not None else default_calib())]
        self._firmware = dict(default_firmware()) if firmware is None else dict(firmware)
        self.online = True
        self._rate_ts = 0.0
        self._rate_bursts = 0
        self._time_ms = lambda: time.time() * 1000.0

    def calib(self):
        rows = [dict(e) for e in self._calib]
        if not self.online:
            for e in rows:
                e["avail"] = False
        return rows

    def firmware(self):
        return dict(self._firmware)

    def find_entity(self, uid):
        for e in self._calib:
            if e["id"] == uid:
                return e
        return None

    def toggle(self, uid):
        e = self.find_entity(uid)
        if not e:
            raise KeyError(uid)
        if e["type"] not in ACTUATOR_TYPES:
            raise ValueError("invalid actuator type")
        e["state"] = not e["state"]
        e["age_ms"] = 0
        return e["state"]

    def dimmer(self, uid, value):
        e = self.find_entity(uid)
        if not e:
            raise KeyError(uid)
        if e["type"] != CALIB_TYPE_DIMMER:
            raise ValueError("invalid dimmer type")
        if value < 0 or value > 100:
            raise ValueError("dimmer level out of range")
        e["value"] = float(value)
        e["state"] = value > 0
        e["age_ms"] = 0
        return e["value"]

    def calib_set(self, form):
        raw_id = (form.get("id") or [""])[0]
        if not raw_id or not raw_id.isdigit():
            return 400
        if not self.find_entity(int(raw_id)):
            return 404
        return 200

    def rate_limit_ok(self):
        now = self._time_ms()
        if now - self._rate_ts > self.RATE_WINDOW_MS:
            self._rate_bursts = 0
        self._rate_ts = now
        if self._rate_bursts >= self.RATE_BURST:
            return False
        self._rate_bursts += 1
        return True

    # ---- server lifecycle -------------------------------------------------
    def serve(self, port=8123):
        handler = MockNodeHandler
        handler.node = self
        self.httpd = ThreadingHTTPServer(("127.0.0.1", port), handler)
        self.port = port
        t = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        t.start()
        return port

    def stop(self):
        if getattr(self, "httpd", None):
            self.httpd.shutdown()
            self.httpd.server_close()

    def url(self):
        return "http://127.0.0.1:%d" % self.port


def _run_standalone(port):
    node = MockNode()
    port = node.serve(int(port))
    print("mock node listening on %d" % port, flush=True)
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    _run_standalone(sys.argv[1] if len(sys.argv) > 1 else 8123)