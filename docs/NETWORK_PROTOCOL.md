# Qymera Dashboard — UDP Network Protocol Specification (v6)

> Part of the Qymera Dashboard architecture contract. Defines the **only** remote-device transport: UDP over Wi-Fi.
> ESP-NOW is **not** an implementation option, fallback, or legacy path. ESP8266 is not supported. No ESP-NOW abstraction exists in this architecture.

---

## 1. Transport Overview

```
Dashboard (hub)  ◄─────  UDP  ─────►  Remote Qymera Device
     │  discovery/telemetry: broadcast (ports A)
     │  commands:            directed unicast (port B)
     │  acks / directed telemetry: unicast (port B)
```

- Three logical channels (two UDP sockets on the Dashboard):
  - **`port_discovery`** — broadcast discovery + periodic telemetry announces.
  - **`port_control`** — directed commands from Dashboard → device; unicast acks and directed telemetry device → Dashboard.
- All datagrams are **v6-framed** (below). v1–v5 packet parsing from the 1.1 tree is removed; a mixed v5 fleet is out of scope for the new product (migration note: upgrade fleet; backward wire compatibility is not preserved).

## 2. Frame and Packet Structure

All multi-byte numeric fields are **little-endian**. Packed, `static_assert`-checked in code.

```
Frame (v6)
┌──────────┬──────────┬────────────┬──────────────┬──────────────┬───────────────┐
│ magic 0xA6│ version 6 │ kind (u8)  │ seq (u32)    │ src_uid (u32)│ len (u16)     │
└──────────┴──────────┴────────────┴──────────────┴──────────────┴───────────────┘
   1           1           1            4              4             2         = 13 B header
then payload (len bytes)
```

`kind`:

| kind | name | payload |
|---|---|---|
| 0x01 | `MSG_DISCOVER` | discovery request (Dashboard → broadcast) |
| 0x02 | `MSG_ANNOUNCE` | device announce (device → broadcast) / dashboard entity announce |
| 0x03 | `MSG_ENTITY_SAMPLE` | telemetry sample(s) (device → dashboard unicast/broadcast) |
| 0x04 | `MSG_ENTITY_STATE` | directed state report / command result |
| 0x05 | `MSG_COMMAND` | control command (Dashboard → device unicast) |
| 0x06 | `MSG_ACK` | acknowledgement (device → Dashboard unicast) |
| 0x07 | `MSG_PING` / `MSG_PONG` | liveness/health probe |
| 0x08 | `MSG_LOG` | structured log page (device → dashboard) |
| 0x09 | `MSG_REGISTER` | explicit registration/identity handshake |
| 0x0A | `MSG_CONFIG_RANGE` | capability/set push (optional, rate-limited) |

Every frame carries `seq` from the *sender*, `src_uid` = sender device identity, and `len` exact payload size. Frame size ≤ 1280 B (IPv4-safe, no fragmentation — retained from the current batching discipline).

### Entity sample payload (0x03)

```
EntitySample {
  entity_id: u32,        // registry-stable entity id (sender-assigned)
  device_id: u32,        // sender identity (== frame src_uid)
  type: u8,              // capability/type enum (extended from dtype set)
  value_f: f32,          // validated numeric (-inf/max-clamped at sender)
  value_b: u8,           // boolean where applicable
  flags: u8,             // reliability, calib-applied, synthetic
  src_ts: u32,           // sender epoch seconds (skew-checked by receiver)
  rssi: i8               // optional
}                         ≈ 18 B; multiple per datagram (batching)
```

## 3. Message Types and Direction

| Message | Direction | Trigger | Reliability model |
|---|---|---|---|
| DISCOVER | Dashboard → bc | Onboarding / periodic | best-effort broadcast |
| REGISTER | Device → Dashboard (unicast) | reply to DISCOVER | acked |
| ANNOUNCE | both → bc | periodic (bounds consent) | best-effort |
| ENTITY_SAMPLE | Device → Dashboard | on sample (rate-limited) / periodic | best-effort minority + periodic guaranteed |
| COMMAND | Dashboard → Device (unicast) | control action | acked, bounded retries |
| ACK | Device → Dashboard | after COMMAND / REGISTER | — |
| PING/PONG | both | health | best-effort |
| LOG | Device → Dashboard | structured log page | best-effort, rate-limited |
| ENTITY_STATE | Device → Dashboard | command result / directed state | acked for command results |

## 4. Discovery & Registration

1. Dashboard sends `DISCOVER` on `port_discovery` broadcast periodically (backoff-bounded).
2. Remote device replies `REGISTER` (unicast) carrying identity (`chip_uid`, model, fw version, capability bitmask, entity roster hash).
3. Dashboard validates identity, optionally requires provisioning consent, assigns a stable `device_id` (mapped from `chip_uid`), stores `net_id` (IP + last-seen) in the Device Registry.
4. Device sends `ANNOUNCE` on roster/hash changes (add/remove entity, capability change). Dashboard diff-updates the registry (the "500 rules / hundreds of devices" case: only changed bits travel).
5. **Offline**: `last_seen` maintained from any inbound frame (`ENTITY_SAMPLE`, `PING`, `ANNOUNCE`, `ACK`). Missing > `TIME_OUT` (default 3 × announce interval) → `online:false`, `device.offline` event; entity `reliability:"offline"`. Reappearance → `device.online` event and rule `device_state` conditions re-evaluate.

## 5. Commands, Ack, Sequence, Duplicates, Retries

- `COMMAND` (Dashboard → device): payload carries target `entity_id`, opcode (`set_relay`/`set_dimmer`/`set_value`/`pulse`/`fade`), value, and the command `seq`.
- `ACK`: device returns `MSG_ACK` echoing `seq` + result code (`ok` / `unknown_entity` / `capability` / `out_of_range` / `busy`). Results drive `actuator.changed` + logs.
- **Sequence numbers**: per (sender, direction) monotonic counter; receiver keeps a small **duplicate window** (e.g. last 16 seqs per sender) to drop replayed/duplicated frames (UDP can duplicate; ACK loss must not double-execute a command).
- **Retries**: COMMAND retried up to `N` (e.g. 3) with backoff; no ACK within budget → `command.failed/timeout` event; control API returns failure to the caller. Idempotent commands (set same value) are safe under retry.
- **Rate limiting / congestion**: per-device outbound budget and per-tick RX drain budget (pattern loaned from today's `MAX_RX_PACKETS_PER_TICK` but now budget/priority-based). A misbehaving sender is throttled (drop + counter), never blocks the loop.

## 6. Telemetry Flow

- Devices send `ENTITY_SAMPLE` on *change* (significant delta / state flip) **rate-limited**; full roster `ANNOUNCE`s every announce interval (guaranteed baseline liveness and resync).
- Dashboard ingests via the UDP stack → publishes `sample` events → consumers (registry/telemetry/rules/analytics) — no direct registry writes by the transport layer.
- Sender timestamps (`src_ts`) are **skew-checked** (|skew| > threshold → packet marked `unclocked`; calendar conditions excluded per DATA_MODEL). Receiver does not overwrite sender ts blindly.

## 7. Device Identity & Addressing

- Identity = `chip_uid` (immutable, provisioned) + `device_id` (registry stable alias) + `net_id` (IP, volatile).
- UDP addressing: unicast via the device's source IP (from its last frame on `port_control`) or explicit `net_id.addr`; broadcast only for discovery/announce.
- `src_uid` in every frame prevents spoofing *identity* across IP changes (DHCP churn is handled: registry tracks device by `src_uid`, not by IP).
- Versioning: frame `version=6`; capability bitmask is versioned with a schema rev.
- Security: local-LAN trust model; provisioning consent required by default; optional shared pre-shared-key HMAC tag on frames (v6.1 optional field) documented, not in v1 core.

## 8. Retry / Duplicate / Ordering Summary Table

| Concern | Policy |
|---|---|
| Frame loss | sender retries (commands), periodic announce (telemetry) |
| Duplicate frames | per-sender seq window, drop |
| Out-of-order | commands are idempotent; telemetry is state-of-last-known (ordering not critical) |
| Replay | seq window + optional PSK HMAC (later) |
| Oversized | len ≤ 1280; oversized → drop + counter |
| Malformed | strict parse, `static_assert` sizes, drop + counter (pattern from current `mesh.cpp`) |

## 9. Offline Behavior

- Offline device: registry retains identity + last known entity values (`reliability:"offline"`) so rules touching its state evaluate deterministically to `offline`; actuation to an offline device returns `command.failed` (no silent queue-and-lose).
- Entities are never deleted on offline; they go `offline`; stale *unknown* devices are reaped only after an explicit deprovisioning timeout (configurable) — a deliberate change from the current slot-reclaim approach, because registry capacity no longer depends on a fixed sensor table.

## 10. Scalability Targets

- Devices: 50–200+ per Dashboard (registry + rate budgets).
- Entities per device: up to the per-device roster budget; Dashboard total entity capacity bounded by Registry capacity (see Storage doc).
- Sample rate: aggregate telemetry budgeted (e.g. ≤ 100 samples/s sustained burst-off; burst handling via drop + counter + high-priority bias).
- Broadcast cost bounded by announce-interval backoff and entity-diff batching.

## 11. No ESP-NOW

- No `espnow_p2p.*`, no `TRANSPORT_ESPNOW`, no fallback logic, no peer-MAC table, no AP-mode-only networking special case.
- UDP is the sole remote transport. On Wi-Fi lacking AP/STA, the Dashboard surfaces it as a health condition, not a second transport.