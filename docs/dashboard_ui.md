# Qymera Dashboard - UI Reference

## Overview

The Dashboard UI is a single-page application served at `/` that provides a human-operable interface to the Qymera Dashboard system. It consumes the `/api/v1/` REST API and requires no LLM or external dependencies.

## Architecture

```
Browser (HTML/CSS/JS)
    ↓ fetch() calls
REST API (/api/v1/)
    ↓ qymera_skill_execute()
Skill API
    ↓
Deterministic Runtime (Registry, Rule Engine, Control, Storage)
```

The UI is intentionally lightweight:
- **HTML**: ~2KB (embedded in firmware)
- **CSS**: ~3KB (embedded, no external dependencies)
- **JavaScript**: ~8KB (vanilla, no frameworks)
- **Total**: ~13KB served from Flash

## Pages/Tabs

### 1. Dashboard (System Overview)
- Free heap
- Uptime
- IP address
- Device count
- Entity count

### 2. Devices
Grid of device cards showing:
- Device ID, name, model, firmware version
- Role (Dashboard/Remote/Provisioning)
- Online/offline badge
- Entity count, IP, port

### 3. Entities
Grid of entity cards showing:
- Entity name, ID, type, capabilities
- Current vs Desired state (critical distinction!)
- Command status badge (WAITING_ACK, ACKED, STATE_CONFIRMED, etc.)
- Reliability indicator
- **Controls:**
  - Relay → Toggle switch
  - Dimmer → 0-100 slider

### 4. Rules
Grid of rule cards showing:
- Rule name, ID
- Priority, cooldown, max activations/hour
- Trigger summary (entity + operator + threshold)
- Enabled/disabled badge
- Actions: Edit, Enable/Disable, Delete

**Create Rule Form:**
- Name, Rule ID (optional, auto-generated)
- Trigger: entity (device_id + entity_id), operator (GT/LT/GE/LE/EQ), threshold
- Action: entity, action type (SET_BOOL/SET_LEVEL), value
- Cooldown (ms), Priority

**Edit Rule Form:**
- Name, Enabled toggle

### 5. Skills
Skill catalog showing:
- Skill name, version
- Description
- Schema ID
- Permission bits (READ/CONTROL/RULE_READ/RULE_WRITE)

### 6. Logs
Recent log entries (up to 50) with:
- Timestamp
- Source
- Message
- Color-coded by level (ERROR=red, WARNING=amber, INFO=blue)

### 7. System
Same as Dashboard but with Build info.

## Key UI Concepts

### Current vs Desired State
The UI **always shows both** `current` (observed) and `desired` (requested) state for actuators.

```
Current: ON
Desired: OFF
Status: WAITING_ACK
```

This is intentional and critical for remote control semantics. `ACKED` ≠ `STATE_CONFIRMED`.

### Command Status Badges
Color-coded badges show command lifecycle:
- **Amber (pending):** REQUESTED, DISPATCHED, WAITING_ACK, ACKED
- **Green (confirmed):** STATE_CONFIRMED
- **Red (failed):** FAILED, TIMEOUT

### Auto-Refresh
The UI auto-refreshes the current tab every 10 seconds. Manual refresh via tab click.

### Responsive Design
- Desktop: Multi-column grids
- Tablet: 2-column grids
- Mobile: Single column, stacked tabs with horizontal scroll

## JavaScript API

```javascript
// Base API call
async function api(path, opts = {}) {
  const r = await fetch('/api/v1' + path, {
    headers: {'Content-Type': 'application/json'},
    ...opts
  });
  const j = await r.json();
  if (!r.ok) throw new Error(j.error?.message || r.statusText);
  return j;
}

// Tab switching
loadTab('dashboard') // 'dashboard' | 'devices' | 'entities' | 'rules' | 'skills' | 'logs' | 'system'

// Rule operations
toggleRule(ruleId, enabled)  // POST /rules/:id/enable|disable
deleteRule(ruleId)           // DELETE /rules/:id
```

## Adding Custom Entities/Devices

Devices and entities are registered via the firmware sketch using:
```cpp
qymera_register_sensor("device_id", "entity_id", TYPE, "Name", "unit", min, max);
qymera_register_actuator("device_id", "entity_id", TYPE, "Name");
```

They automatically appear in the Dashboard UI after restart.

## Development

The UI is embedded in `src/http/qymera_http_server.c` as the `DASHBOARD_HTML` constant. To modify:

1. Edit the HTML/CSS/JS in the constant
2. Rebuild: `pio run -e esp32_devkit`
3. Flash: `pio run -e esp32_devkit -t upload`

No build step for the UI - it's served directly from Flash.

## Browser Compatibility

- Chrome 80+
- Firefox 75+
- Safari 14+
- Edge 80+

Requires: `fetch`, `async/await`, `Map`, `const/let`, template literals.

## Security

- Served over HTTP (no TLS on ESP32)
- Intended for trusted local networks only
- No authentication (future: API keys, mTLS)
- No sensitive data in UI (passwords, tokens)

## Future Enhancements

- WebSocket/SSE for real-time updates (replace polling)
- Dark mode
- Multi-device dashboard aggregation
- Rule builder wizard
- Entity grouping by location
- Historical charts (requires time-series storage)
- User accounts/permissions