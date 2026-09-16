# Dashboard <-> Node integration (v1)

The Dashboard and remote Qymera Nodes talk **only** through the v1
application HTTP API defined in [`docs/api-contract.yaml`](../api-contract.yaml).
There is no raw UDP/ESP-NOW path between the Dashboard and a Node; the legacy
UDP transport (`src/network/udp/`, port macros, ACK wire state machine) was
removed in this rebase.

## Ownership & the single boundary

- The contract file is the Dashboard's working machine-readable spec. The
  **authoritative** copy lives in the Qymera (firmware) repo and is published
  once the Node v1 API lands.
- The **single boundary** is `src/network/qymera_node_client.{h,c}`. All
  Node discovery, reconciliation and commands pass through it. When the
  authoritative firmware spec arrives, reconcile only inside this module and
  the integration mock/tests it drives.
- `src/control/qymera_control.c` only resolves pending commands against the
  client's `on_remote_state` authoritative snapshots; it never talks wire.

## Layout

| Path | Purpose |
| --- | --- |
| `docs/api-contract.yaml` | Working draft, machine-readable (envelopes, schemas, codes) |
| `src/network/qymera_node_client.{h,c}` | v1 HTTP client: reconcile + command dispatch |
| `src/control/qymera_control.{h,c}` | Pending-command machine (dispatch/accept/confirm/timeout) |
| `tests/integration/mock_node.py` | v1 mock Node (status/entities/command) |
| `tests/integration/test_contract.py` | Contract tests over real HTTP |
| `tests/integration/run_dashboard_tests.py` | Runs `tests/host_sanity.py` mirrors |
| `tests/integration/run_contract_tests.py` | Runs the contract suite |
| `tests/integration/start_node_mock.py` | Standalone mock for manual hardware wiring |
| `.github/workflows/ci.yml` | CI: host mirrors + contract + 3 firmware builds |
| `ci/dashboard-integration.yml` | Reference binding for the firmware repo's CI |

## Command lifecycle (happy path)

1. Control API `set_relay`/`set_dimmer` → pending entry `DISPATCHED`.
2. `qymera_node_client_send_command` → `POST /api/v1/entities/<id>/command`.
   - Node accepted (`accepted:true`) → status `ACKED` (desired stays
     `PENDING`).
   - Node rejected (`accepted:false`, canonical `error.code`) → terminal
     `FAILED`; caller receives the mapped error.
   - Transport failure → terminal `FAILED`.
3. Node reconcile (`GET /api/v1/entities`) feeds authoritative state through
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

## Version negotiation

The client reads `api_version` / `protocol_version` / `firmware_version`
from `/status` and stores them on the device. Major `api_version` mismatch
→ node surfaced as incompatible(`UNSUPPORTED_API_VERSION`), never a black
hole. Dashboard identity is `device_id`, never IP or array index.

## Manual hardware verification

```
python tests/integration/start_node_mock.py 8123
# point the Dashboard's Nodes view at 127.0.0.1:8123 (or the host IP on the
# board's network); observe /api/v1/status, /api/v1/entities and commands.
```

## Governance

- Node schemas/codes change → update the contract YAML **and** the client +
  mock + tests together in one PR (single boundary).
- Do not reintroduce raw transports for Dashboard<->Node traffic.
- The Node remains authoritative for device/entity state; the Dashboard never
  invents remote state.