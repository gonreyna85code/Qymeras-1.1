# Qymera Dashboard - HTTP API Reference

## Overview

The HTTP API provides a RESTful interface to the Qymera Dashboard, routing all operations through the deterministic Skill API layer. This ensures that the web UI, future LLM adapters, and automation all share the same authoritative control surface.

**Base Path:** `/api/v1/`

**Content-Type:** `application/json`

**Authentication:** None (local network only, future enhancement planned)

## Response Format

All responses use the stable Skill envelope format:

**Success:**
```json
{
  "ok": true,
  "data": <valid JSON value>
}
```

**Error:**
```json
{
  "ok": false,
  "error": {
    "code": "STABLE_ERROR_CODE",
    "message": "Human-readable description"
  }
}
```

The `error.code` is the canonical machine-readable error (from the Skill layer). HTTP status codes map to these codes but the code is always included in the body.

## HTTP Status Code Mapping

| Skill Error Code | HTTP Status | Description |
|-----------------|-------------|-------------|
| SKILL_NOT_FOUND | 404 | Skill does not exist |
| ENTITY_NOT_FOUND | 404 | Device or entity not found |
| RULE_CONFLICT | 409 | Rule ID already exists |
| RULE_INVALID | 400 | Rule fails validation (see Rule Validation below) |
| INVALID_VALUE | 400 | Value out of range or invalid |
| INVALID_INPUT | 400 | Malformed JSON or missing fields |
| INVALID_CAPABILITY | 422 | Entity doesn't support requested action |
| PERMISSION_DENIED | 403 | Caller lacks required permission |
| DEVICE_OFFLINE | 503 | Target device unreachable |
| COMMAND_TIMEOUT | 504 | Command timed out |
| NO_SPACE | 507 | Storage/table full |
| STORAGE_ERROR | 500 | Persistence failure |
| DEPENDENCY_MISSING | 503 | Required runtime dependency missing |

> **Important:** `INVALID_INPUT` (HTTP 400) means the request body failed strict JSON parsing —
> it is *never* silently coerced. For example a relay with `{"value": 1}` returns
> `INVALID_INPUT` because `value` must be the JSON boolean `true`/`false`, and a missing
> `value`/`level` field returns `INVALID_INPUT` (never a silent default).

## Endpoints

### System

#### GET `/api/v1/status`
System health and metrics.

**Response:**
```json
{
  "ok": true,
  "data": {
    "network": "AP",
    "ssid": "Qymera-12345678",
    "ip": "192.168.4.1",
    "free_heap": 21116,
    "uptime_ms": 123456,
    "device_count": 2,
    "entity_count": 5
  }
}
```

`network` is one of `"AP"`, `"APSTA"`, `"STA"` and `ssid` is the SoftAP SSID (empty in pure STA mode).

### Devices

#### GET `/api/v1/devices`
List all registered devices. The `role` and `state` fields are strings, not numbers.

**Response:**
```json
{
  "ok": true,
  "data": [
    {
      "device_id": "dashboard",
      "name": "Qymera-12345678",
      "model": "ESP32",
      "role": "dashboard",
      "state": "operational",
      "location": "",
      "online": true
    }
  ]
}
```

`role` is one of `"dashboard"`, `"remote"`, `"provisioning"`; `state` is one of
`"operational"`, `"offline"`, `"degraded"` (see `device_state_str`).

### Entities

#### GET `/api/v1/entities`
List all entities across all devices.

**Response:**
```json
{
  "ok": true,
  "data": [
    {
      "device_id": "dashboard",
      "entity_id": "temperature",
      "name": "Temperature Sensor",
      "type": "SENSOR_TEMPERATURE",
      "capabilities": ["SENSOR_NUMERIC"],
      "unit": "°C",
      "current": 25.5,
      "desired": 25.5,
      "cmd_status": "STATE_CONFIRMED",
      "reliability": "CONFIRMED"
    }
  ]
}
```

`type` and `capabilities` are strings. Relay entities carry boolean `current`/`desired`;
numeric entities (like dimmer) carry numbers. There is no `last_updated` field in this
response — the UI renders refresh timing from the 10 s autopoll only.

#### GET `/api/v1/entities/:device/:entity`
Get state of a specific entity.

**Response:** Same as single entity object above.

### Control

#### POST `/api/v1/control/relay`
Set relay state.

**Request:**
```json
{
  "device_id": "dashboard",
  "entity_id": "relay0",
  "value": true
}
```

`value` **must be a JSON boolean** (`true`/`false`). `1`/`0`/`"true"`/null/absent are rejected — the parser performs a key lookup so `false` is never confused with "field missing".

**Response:**
```json
{
  "ok": true,
  "data": {}
}
```
The entity's `desired` state is updated, `cmd_status` becomes `WAITING_ACK`. Poll `/api/v1/entities/:device/:entity` for status.

#### POST `/api/v1/control/dimmer`
Set dimmer level.

**Request:**
```json
{
  "device_id": "dashboard",
  "entity_id": "dimmer0",
  "level": 75
}
```

`level` must be a JSON number in `0..100`:
- Missing/wrong type/non-numeric → `INVALID_INPUT` (400).
- Out of range (e.g. `101`, or a negative that wraps to `>100` at uint8) → `INVALID_VALUE` (400) from the skill range gate.
- An absent `level` is **never** silently treated as `0`.

**Response:** Same as relay.

### Rules

#### GET `/api/v1/rules`
List all rules. The list is a compact form — it does **not** include trigger/actions.

**Response:**
```json
{
  "ok": true,
  "data": [
    {"rule_id": "rule-temp-fan-001", "name": "Temperature Fan Control", "enabled": true, "revision": 1}
  ]
}
```

#### GET `/api/v1/rules/:id`
Get single rule (full form, see below). Unknown `:id` → `RULE_INVALID` (400).

**Response:**
```json
{
  "ok": true,
  "data": {
    "rule_id": "rule-temp-fan-001",
    "name": "Temperature Fan Control",
    "enabled": true,
    "revision": 1,
    "priority": 10,
    "cooldown_ms": 60000,
    "max_activations_per_hour": 10,
    "created_ts": 1234,
    "updated_ts": 1235,
    "state": {"activation_count": 0, "last_triggered": 0},
    "trigger": [
      {"entity": {"device_id": "dashboard", "entity_id": "temperature"}, "operator": "GT",
       "threshold": 30, "threshold_high": 0, "duration_ms": 0, "negate": false}
    ],
    "conditions": [],
    "actions": [
      {"entity": {"device_id": "dashboard", "entity_id": "fan"}, "action": "SET_BOOL",
       "value": 1, "duration_ms": 0}
    ]
  }
}
```

`trigger` is emitted as an array (0 or 1 element) plus `conditions` as a separate array.

#### POST `/api/v1/rules`
Create a new rule.

**Request:**
```json
{
  "name": "My Rule",
  "rule_id": "my-rule-001",
  "trigger": {
    "entity": {"device_id": "dashboard", "entity_id": "temperature"},
    "operator": "GT",
    "threshold": 30.0
  },
  "actions": [
    {"entity": {"device_id": "dashboard", "entity_id": "fan"}, "action": "SET_BOOL", "value": 1}
  ],
  "cooldown_ms": 60000,
  "priority": 10,
  "max_activations_per_hour": 10,
  "enabled": true
}
```

Parsing contract (see `parse_rule_input`):
- `name` required, `rule_id` optional (auto-generated; duplicates → `RULE_CONFLICT` 409).
- `trigger` may be an object **or** a single-element array (the `GET /rules/:id` shape). All fields are optional at parse time.
- `actions` must be a non-empty array; each entry needs `entity` (object with `device_id`+`entity_id`) and `action` (one of `SET_BOOL`, `SET_LEVEL`, `SET_VALUE`, `TOGGLE`). `value` accepts a JSON number or boolean; `value_u32`/`duration_ms` are numbers.
- Any missing field / wrong type / malformed JSON → `INVALID_INPUT` (400) at the HTTP boundary.

**Rule Validation (engine gate, after parsing):** a rule is **invalid** (`RULE_INVALID`, 400) unless it has **at least one action** and (**a trigger or at least one condition**). Every referenced entity must exist (`ENTITY_NOT_FOUND`, 404) and actions must be compatible with the entity's capabilities. Rules are enabled at creation (`enabled` in the request is informational only).

#### PUT `/api/v1/rules/:id`
Update a rule. The request must carry the **full rule definition** (repeat `trigger` and `actions`, as in POST); partial updates are rejected. If editing triggers/actions is not intended, fetch the rule first and resend its arrays unchanged.

#### DELETE `/api/v1/rules/:id`
Delete a rule.

#### POST `/api/v1/rules/:id/enable`
Enable a rule.

#### POST `/api/v1/rules/:id/disable`
Disable a rule.

### Logs

#### GET `/api/v1/logs`
Most recent log entries (up to 50) from the ring buffer.

**Response:**
```json
{
  "ok": true,
  "data": [
    {"seq": 42, "ts": 123456, "layer": "INFO", "source": "main", "msg": "Dashboard HTTP API started on port 80"}
  ]
}
```

### Skills

#### GET `/api/v1/skills`
List all available skills with metadata.

**Response:**
```json
{
  "ok": true,
  "data": [
    {
      "name": "list_devices",
      "version": "1.0",
      "description": "List all registered devices",
      "schema_id": "list_devices.v1",
      "permissions": 1
    }
  ]
}
```

## Permission Model

Skills require explicit permission masks:

| Permission | Bit | Skills |
|------------|-----|--------|
| READ | 1 | list_devices, list_entities, get_entity_state, list_rules, get_rule |
| CONTROL | 2 | set_relay, set_dimmer |
| RULE_READ | 4 | list_rules, get_rule |
| RULE_WRITE | 8 | create_rule, update_rule, delete_rule, enable_rule, disable_rule |

The HTTP API automatically applies the correct mask per endpoint.

## Command Status Semantics

For relay/dimmer entities, the `cmd_status` field tracks remote command lifecycle:

| Status | Meaning |
|--------|---------|
| REQUESTED | Command created, not dispatched |
| DISPATCHED | Accepted by control, not yet sent |
| WAITING_ACK | Sent over UDP, awaiting remote ACK |
| ACKED | Remote acknowledged, awaiting state confirm |
| STATE_CONFIRMED | Remote state matches desired |
| FAILED | Remote error or invalid response |
| TIMEOUT | No ACK/state within deadline |

**Critical:** `ACKED` ≠ `STATE_CONFIRMED`. The UI must show both desired and observed state.

## Versioning

All endpoints are under `/api/v1/`. Future versions will use `/api/v2/` etc. No unversioned endpoints.

## Error Handling

Client code should:
1. Check HTTP status
2. Parse JSON response
3. Check `ok` field
4. On error, use `error.code` for programmatic handling, `error.message` for display
5. Never rely solely on HTTP status - always check the error code

The server sets an explicit HTTP status per error (see the mapping table) and the body always
carries the envelope, so callers can branch on either — but `error.code` is canonical.

## Routing

Routes are registered with wildcard matching (`httpd_uri_match_wildcard`), so each
(template, method) appears exactly once:

| Method | URI template | Handler |
|--------|--------------|---------|
| GET | `/api/v1/status` | status |
| GET | `/api/v1/devices` | devices |
| GET | `/api/v1/entities` | entities |
| GET | `/api/v1/entities/*` | entity state |
| POST | `/api/v1/control/relay` | dispatch control (relay) |
| POST | `/api/v1/control/dimmer` | dispatch control (dimmer) |
| GET | `/api/v1/rules` | list rules |
| POST | `/api/v1/rules` | create rule |
| GET | `/api/v1/rules/*` | get rule |
| PUT | `/api/v1/rules/*` | update rule |
| DELETE | `/api/v1/rules/*` | delete rule |
| POST | `/api/v1/rules/*` | enable/disable (parses trailing `/enable` or `/disable`) |
| GET | `/api/v1/skills`, `/api/v1/logs` | catalog / recent logs |
| GET | `/` | embedded dashboard (HTML) |

A wildcard template never swallows its exact base URI (e.g. `GET /api/v1/entities` is the
list, `GET /api/v1/entities/*` is entity state). Duplicate registration of a (template,
method) pair is rejected by the HTTP server (`ESP_ERR_HTTPD_HANDLER_EXISTS`) and aborts the
server with `QYMERA_ERR_INVALID_STATE`.

## Rate Limiting

No explicit rate limiting. The underlying Skill API enforces:
- Max 8 tool calls per turn (for LLM adapter)
- Command table limited to 8 concurrent commands
- Rule table limited to 500 rules

## Future LLM Compatibility

The API is designed so that a future LLM adapter can use the same endpoints:

```
Browser → HTTP API → Skill API → Runtime
LLM     → Skill API → Runtime
```

Both converge on `qymera_skill_execute()`. The Skill catalog (`/api/v1/skills`) provides the tool schema for LLM function calling.