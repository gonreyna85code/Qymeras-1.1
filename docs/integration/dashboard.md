# Dashboard <-> Node integration (v1)

The Dashboard and remote Qymera Nodes talk **only** through the v1
application HTTP API defined in [`docs/api-contract.yaml`](../api-contract.yaml).
There is no raw UDP/ESP-NOW path between the Dashboard and a Node; the legacy
UDP transport (`src/network/udp/`, port macros, ACK wire state machine) was
removed in this rebase.

## Ownership & the single boundary

- `docs/api-contract.yaml` is **authoritative**: it mirrors the Qymera 1.0.0
  firmware surface (`GET /calib` bare entity array, `GET /firmware`,
  `POST /toggle`, `POST /dimmer`, HTTP-status-only errors, no JSON envelope).
- The **single boundary** is `src/network/qymera_node_client.{h,c}`. All
  Node discovery, reconciliation and commands pass through it. If the firmware
  surface evolves, reconcile only inside this module and the integration
  mock/tests it drives.
- `src/control/qymera_control.c` only resolves pending commands against the
  client's `on_remote_state` authoritative snapshots; it never talks wire.

## Layout

| Path | Purpose |
| --- | --- |
| `docs/api-contract.yaml` | Authoritative Node HTTP contract (bare `/calib`, `/firmware`, `/toggle`, `/dimmer`, status-only errors) |
| `src/network/qymera_node_client.{h,c}` | v1 HTTP client: reconcile + command dispatch |
| `src/control/qymera_control.{h,c}` | Pending-command machine (dispatch/accept/confirm/timeout) |
| `tests/integration/mock_node.py` | v1 mock Node (calib/firmware/toggle/dimmer + fault injection) |
| `tests/integration/test_contract.py` | Contract tests over real HTTP |
| `tests/integration/run_dashboard_tests.py` | Runs `tests/host_sanity.py` mirrors |
| `tests/integration/run_contract_tests.py` | Runs the contract suite |
| `tests/integration/start_node_mock.py` | Standalone mock for manual hardware wiring |
| `.github/workflows/ci.yml` | CI: host mirrors + contract + 3 firmware builds |
| `ci/dashboard-integration.yml` | Reference binding for the firmware repo's CI |

## Command lifecycle (happy path)

1. Control API `set_relay`/`set_dimmer` → pending entry `DISPATCHED`.
2. `qymera_node_client_send_command` dispatches by capability:
   - relay → `POST /toggle` with form `id=<uid>` (the Node **flips**, it does
     not set absolutely — the control layer skips the toggle when the observed
     state already equals the desired state).
   - dimmer → `POST /dimmer` with form `id=<uid>&value=<0..100>`.
   - Node HTTP 200 `text/plain "OK"` → status `ACKED` (desired stays
     `PENDING`). Non-200 maps to a canonical error → terminal `FAILED`;
     transport failure → terminal `FAILED`.
3. Node reconcile (`GET /calib`) feeds authoritative state through
   `qymera_control_on_remote_state`. Snapshot matches desired → `CONFIRMED`;
   mismatch → `FAILED`. HTTP 200 alone never confirms an actuator.
4. No snapshot within the deadline → `TIMEOUT` (desired kept, observed kept).

## Reliability model

| Registry value reliability | Meaning |
| --- | --- |
| `CONFIRMED` | Matches authoritative node snapshot |
| `PENDING` | Remote command accepted, awaiting snapshot |
| `STALE` | In snapshot previously, absent from the last poll |
| `OFFLINE` | Owning node reported offline |
| `FAILED` | Command rejected / timed out / transport error |

## Node discovery / configuration

Targets are configured via `GET`/`POST /api/v1/nodes` on the Dashboard
(host, port, optional `poll_interval_ms`; max 4) and persisted as an NVS
blob (`qymera_cfg` / `node_targets_v1`) — a separate small blob so the
network-config struct shape stays stable. The dashboard UI's **Nodes** view
lists targets and marks them online when a matching remote device reports
online.

## Identity & metadata

- Entity identity is the calib `id` uid (base-10 string); device identity is
  the calib `device_uid` (base-10 string). Never IP, never array index.
- Device `ip` comes from the calib owner `ip` field; port = target port;
  `name`/`fw_version`/`model` are refreshed from `GET /firmware`.
- There is **no** `api_version`/`protocol_version` on the wire; the client
  stamps the device with `"1.0"` statically. `UNSUPPORTED_API_VERSION` is
  therefore unused against Nodes today.

## Manual hardware verification

```
python tests/integration/start_node_mock.py 8123
# point the Dashboard's Nodes view at 127.0.0.1:8123 (or the host IP on the
# board's network); observe /calib, /firmware, /toggle and /dimmer.
```

## Governance

- Node schemas/codes change → update the contract YAML **and** the client +
  mock + tests together in one PR (single boundary).
- Do not reintroduce raw transports for Dashboard<->Node traffic.
- The Node remains authoritative for device/entity state; the Dashboard never
  invents remote state.