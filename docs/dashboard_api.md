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
| INVALID_VALUE | 400 | Value out of range or invalid |
| INVALID_INPUT | 400 | Malformed JSON or missing fields |
| INVALID_CAPABILITY | 422 | Entity doesn't support requested action |
| PERMISSION_DENIED | 403 | Caller lacks required permission |
| DEVICE_OFFLINE | 503 | Target device unreachable |
| COMMAND_TIMEOUT | 504 | Command timed out |
| NO_SPACE | 507 | Storage/table full |
| STORAGE_ERROR | 500 | Persistence failure |
| DEPENDENCY_MISSING | 503 | Required runtime dependency missing |

## Endpoints

### System

#### GET `/api/v1/status`
System health and metrics.

**Response:**
```json
{
  "ok": true,
  "data": {
    "free_heap": 21116,
    "uptime_ms": 123456,
    "ip": "192.168.1.42",
    "device_count": 2,
    "entity_count": 5
  }
}
```

### Devices

#### GET `/api/v1/devices`
List all registered devices.

**Response:**
```json
{
  "ok": true,
  "data": [
    {
      "device_id": "dashboard",
      "name": "Qymera-12345678",
      "model": "ESP32",
      "fw_version": "1.0.0",
      "role": 0,
      "state": 0,
      "entity_count": 3,
      "ip_addr": "192.168.1.42",
      "port": 13345,
      "online": true
    }
  ]
}
```

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
      "type": 1,
      "capabilities": ["SENSOR_NUMERIC"],
      "unit": "°C",
      "current": 25.5,
      "desired": 25.5,
      "cmd_status": "STATE_CONFIRMED",
      "reliability": "CONFIRMED",
      "last_updated": {"seconds": 123456, "millis": 789}
    }
  ]
}
```

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

**Response:** Same as relay.

### Rules

#### GET `/api/v1/rules`
List all rules.

**Response:**
```json
{
  "ok": true,
  "data": [
    {
      "rule_id": "rule-temp-fan-001",
      "name": "Temperature Fan Control",
      "enabled": true,
      "priority": 10,
      "cooldown_ms": 60000,
      "max_activations_per_hour": 10,
      "trigger": {
        "entity": {"device_id": "dashboard", "entity_id": "temperature"},
        "operator_": "GT",
        "threshold": 30.0
      },
      "actions": [
        {"entity": {"device_id": "dashboard", "entity_id": "fan"}, "action": "SET_BOOL", "value_u32": 1}
      ]
    }
  ]
}
```

#### GET `/api/v1/rules/:id`
Get single rule.

#### POST `/api/v1/rules`
Create a new rule.

**Request:**
```json
{
  "name": "My Rule",
  "rule_id": "my-rule-001",
  "trigger": {
    "entity": {"device_id": "dashboard", "entity_id": "temperature"},
    "operator_": "GT",
    "threshold": 30.0
  },
  "actions": [
    {"entity": {"device_id": "dashboard", "entity_id": "fan"}, "action": "SET_BOOL", "value_u32": 1}
  ],
  "cooldown_ms": 60000,
  "priority": 10,
  "max_activations_per_hour": 10,
  "enabled": true
}
```

#### PUT `/api/v1/rules/:id`
Update a rule. Requires full rule definition.

#### DELETE `/api/v1/rules/:id`
Delete a rule.

#### POST `/api/v1/rules/:id/enable`
Enable a rule.

#### POST `/api/v1/rules/:id/disable`
Disable a rule.

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