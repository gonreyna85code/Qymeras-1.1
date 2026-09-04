# Qymera Dashboard — GUI Architecture (Rendering Contract)

> **Scope:** the rendering contract for `src/http/dashboard.html`. This is the
> architectural rulebook for how views render, update, and stay stable. It
> exists so that future GUI changes follow the same pattern instead of
> reintroducing DOM rebuilds or duplicated listeners.
>
> **Source of truth:** `src/http/dashboard.html` (hand-edited).
> **Generated:** `src/http/qymera_dashboard_html.h` — never edit by hand; run
> `python tools/gen_dashboard_html.py` after any change and rebuild.

The ground rule for every future GUI change:

```
MOUNT STRUCTURE ONCE
UPDATE VALUES IN PLACE
RECONCILE COLLECTIONS BY KEY
REFRESH ONLY ACTIVE VIEW
NEVER REBUILD THE WORLD TO CHANGE ONE VALUE
```

---

## 1. View lifecycle

Every tab is a **view** registered in a single `views` object:

```js
views.entities = {
    mount:   function(host) {},              // build DOM once + attach listeners
    refresh: makeRefresh('/entities', 'entities'), // fetch data only
    update:  function(data) {}               // write data into the existing DOM
};
```

The view registry lives in `src/http/dashboard.html`. Tabs listed in
`#navTabs` map 1:1 to view keys (`dashboard`, `devices`, `entities`, `rules`,
`skills`, `logs`, `system`, `network`).

`showView(name)` mounts a view on first access (creating a persistent
`<div id="view-<name>">` host), shows the active host, hides the others, and
triggers one refresh. Hosts persist across tab switches, so a view's DOM is
built once and reused.

## 2. State vs DOM

Application state lives in a single `uiState` object; the DOM reflects it but
is never the source of truth:

```js
var uiState = {
    activeView:   'dashboard',
    formActive:   false,   // a form is open; polling is paused
    editingRuleId: null,    // rule currently being edited (overlay)
    pending:      new Set() // entity keys with an in-flight command
};
```

DOM attributes are used only for **bindings** — `data-field`, `data-action`,
`data-*-key` — and never to store source data. Keeping state out of the DOM is
what makes `update()` idempotent and safe to call on every poll.

## 3. mount / refresh / update

| Method | Owns | Must NOT do |
|--------|------|-------------|
| `mount(host)` | create initial DOM structure; install **persistent** listeners (delegated); capture `this.__f` / `this.__list` / `this.__count` node refs | fetch data; re-run per refresh |
| `refresh(gen)` | fetch exactly one endpoint; resolve with parsed `data` | touch the DOM |
| `update(data)` | write arrived data into existing DOM, in place, only when changed | fetch; rebuild structure; install listeners |

`refresh` is created by `makeRefresh(path, viewName)` which wires the
stale-response guard and `setOnline(true)` automatically:

```js
function makeRefresh(path, viewName) {
    return function (gen) {
        return apiFetch(path).then(function (data) {
            if (gen === views[viewName].gen) {   // stale guard
                setOnline(true);
                views[viewName].update(data);
            }
        });
    };
}
```

## 4. Collection reconciliation

Collections (`devices`, `entities`, `rules`, `skills`, `logs`) are updated in
place via `reconcileCollection(container, list, keyFn, attr, create, apply)`:

```
existing key -> apply(data, node)  (update the existing node)
new key      -> create(data) node, insert at the right position
missing key  -> remove that node
```

The container is **never cleared or recreated**. This preserves focus and any
unpersisted state in the nodes across polls.

## 5. Stable object keys

Each collection node carries a stable key in a dedicated attribute:

| Collection | Key      | Attribute       | `keyFn` |
|-----------|----------|-----------------|---------|
| devices   | `device_id` | `data-device-key` | `x.device_id` |
| entities  | `device_id/entity_id` | `data-entity-key` | `x.device_id + '/' + x.entity_id` |
| rules     | `rule_id` | `data-rule-key`   | `x.rule_id` |
| skills    | `name`    | `data-skill-key`  | `x.name` |
| logs      | `ts:source:msg` | `data-log-key` | `logKey(x)` |

These attributes are read-only bindings used only by `reconcileCollection`.
Data values are written with the idempotent helpers below.

## 6. Event delegation

Listeners that target nodes which are created/removed by reconciliation are
attached **once** to the collection container during `mount`, using `closest`
delegation. Listeners are never registered inside `update()` or per-rendered
card.

```js
host.addEventListener('click', function (e) {
    var t = e.target.closest('[data-action="toggle"]');
    if (t) handleToggle(t);
});
```

Recreated nodes (e.g. network scan buttons) are also handled by a single
delegated listener on their container instead of per-button listeners.

## 7. Polling rules

- **Only the active view is polled** (`uiState.activeView`).
- One consistent interval: `setInterval(pollTick, 10000)`.
- A view with `pollable: false` (`network`) is never auto-fetched.
- When `uiState.formActive` is true, polling is paused so form inputs, focus,
  and unsaved text survive.
- **Request dedup:** `view.inflight` — a second refresh is ignored while one is
  in flight (no duplicate traffic).
- **Stale protection:** `view.gen` — a response whose captured `gen` no longer
  matches the current `gen` is discarded (an old response never overwrites a
  newer one).

## 8. Pending / editing state

- **Entity commands:** on relay/dimmer action, add the entity key to
  `uiState.pending`, send the command, then on success
  `runViewRefresh(views.entities)` reconciles only the affected card; on
  failure remove the key and toast. Only the affected control changes — the
  card is not rebuilt and focus is not moved.
- **Dimmer echo:** during `update()`, the slider value is only written when the
  slider is **not** the active element, so a dragging thumb does not jump.
- **Rule editor:** lives in `#overlay`, fully separate from the `rules`
  collection DOM. Opening it sets `uiState.editingRuleId` and
  `uiState.formActive`; closing it clears both and refreshes the collection.
  Polling never touches the overlay.

## 9. Generated dashboard header

- Edit `src/http/dashboard.html` only.
- Regenerate with `python tools/gen_dashboard_html.py`.
- The generated header escapes `"`, `\`, `\n`, `\t` in a way that is reversed
  at serve time, so normal JS with quotes/backslashes is fine; the file uses
  single-quoted JS strings by convention.
- Verify both files are committed together and the firmware builds.

## 10. Adding a new view (correctly)

1. Add a tab button in `#navTabs` with `data-tab="<name>"`.
2. Register `views.<name> = { mount, refresh: makeRefresh(path, '<name>'), update }`.
3. In `mount`, build the host DOM once, capture node refs
   (`this.__f`, `this.__list`, `this.__count`), and install any delegated
   listeners.
4. Use the idempotent helpers (`setText`, `setValue`, `setClass`,
   `setDisabled`, `setAttr`) in `update`; never assign `innerHTML` there.
5. For a collection, use `reconcileCollection` with a stable
   `data-<name>-key`.
6. If the view is user-controlled only, set `pollable: false`.
7. Regenerate the header and rebuild the ESP32 target.

---

### Example: minimal incremental view

```js
views.sensors = {
    mount: function (host) {
        host.innerHTML = '<h2 data-field="count"></h2><div id="sensorList"></div>';
        this.__count = host.querySelector('[data-field="count"]');
        this.__list  = host.querySelector('#sensorList');
    },
    refresh: makeRefresh('/status', 'sensors'),
    update:  function (data) {
        setText(this.__count, data.count);
        reconcileCollection(this.__list, data.sensors,
            function (x) { return x.id; }, 'data-sensor-key',
            function () { return el('div', { 'class': 'card', 'data-sensor-key': '' }); },
            function (item, node) { setText(node, item.name); });
    }
};
```

## Anti-patterns (do not reintroduce)

- `main.innerHTML = ...` as a polling mechanism.
- `renderTab(cur)` as a "safe" post-action refresh.
- Clearing and recreating a collection instead of diffing by key.
- Registering listeners per card / per `update()` call.
- Two simultaneous refreshes of the same view (no dedup).
- A stale response overwriting a newer value (no generation guard).
- Rebuilding a form during a poll (destroying input values / focus).
- Declaring the device OFFLINE from an unrelated stray rejection.
