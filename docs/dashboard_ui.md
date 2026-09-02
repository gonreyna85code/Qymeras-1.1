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
- Network mode + AP SSID (**AP mode → "AP · Qymera-XXXX" badge**, STA → "STA")
- IP address
- Free heap
- Uptime
- Device count
- Entity count
- Online dot that turns red with an OFFLINE label when the API stops responding

### 2. Devices
Grid of device cards showing:
- Device ID, name, model, location
- Local/Remote badge (role `"dashboard"` = LOCAL, otherwise REMOTE)
- Online/offline badge + state (operational/offline/degraded)

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
- Trigger: entity (device_id + entity_id), operator (GT/LT/GE/LE/EQ/NE), threshold; checkbox to skip trigger (conditions-only)
- Action: entity, action type (SET_BOOL/SET_LEVEL/SET_VALUE/TOGGLE), value
- Cooldown (ms), Priority, Max activations/hour

**Edit Rule Form:**
- Name, priority, cooldown, max activations/hour
- The Edit form **round-trips** the rule's existing `trigger` and `actions` unchanged and resends them with the PUT, because updates require a full rule definition (partial updates are rejected with `INVALID_INPUT`/`RULE_INVALID`).
- Enable/disable is done with the dedicated buttons on the Rules tab.

**Failure UX:** no optimistic UI — controls disable while a request is in flight, the card is
re-rendered from the server response, and failures show the API error card. After each
Create/Edit/Enable/Disable/Delete the Rules tab reloads from the API.

### 5. Skills
Skill catalog showing:
- Skill name, version
- Description
- Schema ID
- Permission bits (READ/CONTROL/RULE_READ/RULE_WRITE)

### 6. Logs
Recent log entries (up to 50) from `/api/v1/logs` with:
- Timestamp (`ts` numeric seconds, rendered as local time; falls back to `timestamp.seconds`)
- Source
- Message
- Color-coded by level (`layer` value; ERROR=red, WARNING=amber, INFO=blue)

### 7. System
Same as Dashboard but with Build info.

## Key UI Concepts

### Current vs Desired State
The UI **always shows both** `current` (observed) and `desired` (requested) state for actuators,
and it does not coerce missing values: a dimmer's current/desired render from the JSON `current`/
`desired` fields only, and control requests are built as JSON booleans for relays (`value`) and
numbers for dimmers (`level`).

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
// Base API call: returns j.data on {ok:true}, throws on {ok:false} or invalid JSON
async function api(path, opts = {}) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), 8000);
  try {
    const r = await fetch('/api/v1' + path, {
      headers: {'Content-Type': 'application/json'}, signal: ctrl.signal, ...opts
    });
    const txt = await r.text();
    let j; try { j = JSON.parse(txt); } catch (e) { throw new Error('Invalid JSON response'); }
    if (!j.ok) throw new Error((j.error && j.error.code || '') + ': ' +
                               (j.error && j.error.message || 'request failed'));
    return j.data;
  } finally { clearTimeout(t); }
}

// Tab switching
loadTab('dashboard') // 'dashboard' | 'devices' | 'entities' | 'rules' | 'skills' | 'logs' | 'system'

// Rule operations
toggleRule(ruleId, enabled)  // POST /rules/:id/enable|disable
deleteRule(ruleId)           // DELETE /rules/:id
```

The UI never optimistically mutates state — it disables the control while the request is in
flight, then re-renders the active tab from the server.

## Adding Custom Entities/Devices

Devices and entities are registered via the firmware sketch using:
```cpp
qymera_register_sensor("device_id", "entity_id", TYPE, "Name", "unit", min, max);
qymera_register_actuator("device_id", "entity_id", TYPE, "Name");
```

They automatically appear in the Dashboard UI after restart.

## Development

The UI source lives at `src/http/dashboard.html` and is embedded into firmware as the
`DASHBOARD_HTML` constant in `src/http/qymera_dashboard_html.h`. That header is **generated**
and committed to the repo (so plain `pio run` works without extra steps). To modify the UI:

1. Edit `src/http/dashboard.html`
2. Regenerate the header: `python tools/gen_dashboard_html.py`
3. Rebuild: `pio run -e esp32_devkit`
4. Flash: `pio run -e esp32_devkit -t upload`

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