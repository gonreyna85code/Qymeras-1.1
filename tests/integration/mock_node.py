"""Qymera Node v1 mock for Dashboard integration tests.

Implements the working draft contract in docs/api-contract.yaml (the same
surface qymera_node_client consumes): GET /api/v1/status, GET
/api/v1/entities, POST /api/v1/entities/<entity_id>/command. Always answers
the {ok,data}/{ok,error} envelope; HTTP 200 on command means the request was
understood and answered, not that the actuator physically reached its state.
Run directly for manual wiring (start_node_mock.py) or import in
test_contract.py for in-process contract testing.
"""
from __future__ import print_function
import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

API_PREFIX = "/api/v1"

CANONICAL_ERROR_CODES = [
    "INVALID_REQUEST", "ENTITY_NOT_FOUND", "DEVICE_NOT_FOUND",
    "COMMAND_NOT_SUPPORTED", "INVALID_VALUE", "NOT_AUTHORIZED",
    "DEVICE_OFFLINE", "TIMEOUT", "RATE_LIMITED", "INTERNAL_ERROR",
    "UNSUPPORTED_API_VERSION",
]

ENTITY_TYPES = [
    "sensor.temperature", "sensor.humidity", "sensor.luminosity",
    "sensor.pressure", "sensor.level", "sensor.airq", "sensor.rain",
    "sensor.contact", "sensor.generic", "actuator.relay", "actuator.dimmer",
    "virtual.digital", "virtual.analog", "inference.result", "time",
]

ENTITY_CAPABILITIES = [
    "sensor.numeric", "sensor.digital", "actuator.relay", "actuator.dimmer",
    "actuator.generic", "inference.result", "time.source",
]

DEVICE_CAPABILITIES = ["entities", "commands", "automations", "ota"]


def default_status(node_id="nodeA"):
    return {
        "device_id": node_id,
        "name": "Mock Node A",
        "model": "mock-node",
        "firmware_version": "9.9.9-mock",
        "api_version": "1.0",
        "protocol_version": "1.0",
        "ip": "127.0.0.1",
        "uptime_ms": 12345,
        "free_heap": 200000,
        "online": True,
        "capabilities": list(DEVICE_CAPABILITIES),
    }


def default_entities():
    return [
        {
            "entity_id": "relay1",
            "device_id": "nodeA",
            "name": "Relay One",
            "type": "actuator.relay",
            "capabilities": ["actuator.relay"],
            "unit": None,
            "state": {"value": False, "available": True, "reliability": "live", "timestamp": 1},
            "value": False,
            "available": True,
            "config": {"native_min": 0.0, "native_max": 1.0},
        },
        {
            "entity_id": "dim1",
            "device_id": "nodeA",
            "name": "Dimmer One",
            "type": "actuator.dimmer",
            "capabilities": ["actuator.dimmer"],
            "unit": "%",
            "state": {"value": 30.0, "available": True, "reliability": "live", "timestamp": 2},
            "value": 30.0,
            "available": True,
            "config": {"native_min": 0.0, "native_max": 100.0},
        },
        {
            "entity_id": "temp1",
            "device_id": "nodeA",
            "name": "Temp One",
            "type": "sensor.temperature",
            "capabilities": ["sensor.numeric"],
            "unit": "C",
            "state": {"value": 24.5, "available": True, "reliability": "live", "timestamp": 3},
            "value": 24.5,
            "available": True,
            "config": {"native_min": -40.0, "native_max": 125.0},
        },
    ]


class MockNodeHandler(BaseHTTPRequestHandler):
    node = None  # set by server thread owner

    # ---- plumbing ---------------------------------------------------------
    def log_message(self, *args):
        pass

    def _send(self, code, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_error_env(self, http_code, code, message=""):
        self._send(http_code, {"ok": False,
                               "error": {"code": code, "message": message, "details": None}})

    # ---- routes -----------------------------------------------------------
    def do_GET(self):
        n = self.node
        if self.path.split("?")[0] == API_PREFIX + "/status":
            return self._send(200, {"ok": True, "data": n.status()})
        if self.path.split("?")[0] == API_PREFIX + "/entities":
            return self._send(200, {"ok": True, "data": n.entities()})
        return self._send_error_env(404, "INVALID_REQUEST", "unknown path")

    def do_POST(self):
        n = self.node
        path = self.path.split("?")[0]
        while True:
            if not path.startswith(API_PREFIX + "/entities/"):
                break
            rest = path[len(API_PREFIX + "/entities/"):]
            if not rest or not rest.endswith("/command"):
                break
            entity_id = rest[:-len("/command")]
            if not entity_id or "/" in entity_id:
                break
            length = int(self.headers.get("Content-Length", 0) or 0)
            raw = self.rfile.read(length) if length else b"{}"
            try:
                body = json.loads(raw.decode("utf-8") or "{}")
            except ValueError:
                return self._send_error_env(400, "INVALID_REQUEST", "malformed JSON body")
            return n.command(self, entity_id, body)
        return self._send_error_env(404, "INVALID_REQUEST", "unknown path")


class MockNode(object):
    """Holds the authoritative node model + configurable fault injection."""

    def __init__(self, status=None, entities=None):
        self._status = dict(default_status()) if status is None else dict(status)
        self._entities = [dict(e) for e in (entities if entities is not None else default_entities())]
        self.online = True
        self.reject_command_code = None      # force every command rejected with a code
        self.force_value_out_of_range = False

    def status(self):
        s = dict(self._status)
        s["online"] = self.online
        return s

    def entities(self):
        return [dict(e) for e in self._entities]

    def find_entity(self, entity_id):
        for e in self._entities:
            if e["entity_id"] == entity_id:
                return e
        return None

    def set_entity_value(self, entity_id, value):
        e = self.find_entity(entity_id)
        if not e:
            return False
        e["value"] = value
        e["available"] = True
        e["state"] = {"value": value, "available": True,
                      "reliability": "live", "timestamp": e["state"].get("timestamp", 0) + 1}
        return True

    def command(self, handler, entity_id, body):
        if not self.online:
            return handler._send_error_env(503, "DEVICE_OFFLINE", "node offline")
        if self.reject_command_code:
            code = self.reject_command_code
            self.reject_command_code = None
            return handler._send(200, {
                "ok": True, "accepted": False, "status": "error",
                "target": {"device_id": self._status["device_id"], "entity_id": entity_id},
                "result": None,
                "error": {"code": code, "message": code, "details": None}})
        entity = self.find_entity(entity_id)
        if not entity:
            return handler._send_error_env(404, "ENTITY_NOT_FOUND", "unknown entity: %s" % entity_id)

        action = body.get("action")
        value = body.get("value")
        device_id = body.get("device_id")
        if device_id and device_id != entity["device_id"]:
            return handler._send_error_env(400, "ENTITY_NOT_FOUND",
                                           "device_id mismatch with entity owner")

        if action == "set_relay":
            if not isinstance(value, bool):
                return handler._send_error_env(400, "INVALID_VALUE", "set_relay value must be boolean")
            self.set_entity_value(entity_id, value)
        elif action == "set_dimmer":
            if not isinstance(value, (int, float)):
                return handler._send_error_env(400, "INVALID_VALUE", "set_dimmer value must be numeric")
            if value < 0 or value > 100:
                return handler._send_error_env(400, "INVALID_VALUE", "dimmer level 0..100")
            self.set_entity_value(entity_id, float(value))
        else:
            return handler._send_error_env(400, "COMMAND_NOT_SUPPORTED",
                                           "unknown action: %s" % action)

        return handler._send(200, {
            "ok": True, "accepted": True, "status": "done",
            "target": {"device_id": entity["device_id"], "entity_id": entity_id},
            "result": {"value": self.find_entity(entity_id)["value"]},
            "error": None})

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