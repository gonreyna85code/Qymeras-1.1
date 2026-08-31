# Qymera Dashboard — Product Vision

> The product-level story. For the exact technical contracts see the companion documents.

---

## 1. Product Vision

**Qymera Dashboard** is a local-first, deterministic automation runtime and AI gateway for ESP32-class hardware: a box on your network that listens to natural language, turns it into *validated, durable automation*, and then executes it forever with **no internet, no cloud, and no LLM required** — while still answering questions about the past from its own bounded history.

It is the successor direction of Qymera 1.1 ("a node that reads sensors and fires simple rules"). Qymera Dashboard is the *operations brain*: hundreds of rules, many remote devices, structured events, history, analytics, structured logs, and a full agent programming interface.

```
AI is the author.      Qymera is the runtime.
AI can create intelligence.  Qymera guarantees execution.
AI is optional.        Automation is not.
Internet is optional.  Local control is not.
Matter is optional.    Qymera core is not dependent on it.
UDP is the remote-device transport.
Memory goes to rules + state + history + events + analytics + context.
```

## 2. Architecture Philosophy

- **Lightweight runtime.** The product refuses to spend scarce SRAM on a heavy framework. Memory is invested in rule storage, event queues, ring buffers, sensor history, analytics, logs, device registry, AI context, and automation state.
- **Bounded by design.** Every structure that could grow is a ring or a fixed-capacity structure. No unbounded growth; predictable worst cases; counters on drops.
- **Event-driven, deterministic.** Things happen because events/timers wake the rule engine — not because everything is re-scanned every loop.
- **One control path.** Users, UI, rules, agents, and remote commands all actuate through the same validated control API. Nothing (not even an LLM) writes GPIO directly.
- **ESP32-only, UDP-only.** ESP8266 and ESP-NOW are removed; the platform and the transport are deliberately single and simple.

## 3. Local-First Principle

- All automation artifacts are compiled and stored on-device. Once a rule is active, execution does not depend on any LLM, provider, browser, or internet link.
- If the network or AI provider disappears: automations keep running, telemetry keeps recording, the dashboard keeps serving, and offline devices are reported deterministically.
- The dashboard operates correctly in **NO AI** mode from the factory default.

## 4. The AI Agent's Role

The agent is a **programmer**, not a puppeteer:

- It discovers devices and capabilities through the Skill.
- It authors structured rules from natural language (plus targeted context).
- It validates nothing by itself — the Skill's validation pipeline is authoritative and returns structured reports it must act on.
- It can ask the system questions (history, analytics, events) via the same Skill.
- Everything it does is auditable in the `AI`/`AUTOMATION` log layers.

The agent never executes GPIO; it produces *artifacts* (rules) and *requests* (control calls) that the runtime validates and executes. This is the "AI designs, Qymera executes" guarantee that makes AI optional and automation reliable.

## 5. Deterministic Runtime

- Compiled rule VM: stable IDs, event-driven dispatch, timer wheel, sustained-window state machines, cooldowns/rate limits, historical conditions.
- Deterministic fallback for inference conditions (`fail_open`/`fail_closed`/`last_known`).
- Rules carry versions/revisions; boot recompiles from persisted canonical form; failed compiles are quarantined with a structured error, never silently dropped.

## 6. External Inference

Supported modes:

```
NO AI      deterministic core only                (factory default)
LOCAL AI   on-LAN OpenAI-compatible endpoint
REMOTE AI  cloud provider (HTTPS)
HYBRID     local preferred, remote fallback
```

Inference is an **optional information provider** feeding *inference-result entities* consumed by rule conditions with explicit failure policy. It never gates deterministic automations. Providers are time-bounded, cached, rate-limited, and audited. Example loop:

```
Motion detected → external inference ("is a person present?") → Boolean result
→ local rule engine → Light ON
```

If inference is unavailable, the same rule degrades per its declared fallback and the operator can see why in the AI/EVENT logs.

## 7. Example User Workflows

### Workflow A — "Make my greenhouse autonomous."
1. User connects Dashboard + greenhouse nodes (UDP discovery, opt-in).
2. User says to the Dashboards's AI tab: *"Keep the greenhouse between 22 and 26 °C. Use the fan when temperature rises. Don't run it at night. If the temperature trend becomes abnormal, ask the external AI to analyze it."*
3. Agent discovers entities (`temperature`, `fan`), authors two rules (thermostat + anomaly-watch), both validated/compiled/persisted/activated; audit trail recorded.
4. Days later with **no internet**: the thermostat rule runs locally; the anomaly rule runs locally and only *conditionally* consults inference; on provider down it falls back per policy.
5. The user checks the Analytics/Events tabs and sees "how many times the fan ran today", "temperature over 6 h", and the AI activity ledger.

### Workflow B — Diagnostics without a laptop.
1. Device health dips; user opens the dashboard Overview (or asks the agent).
2. Context tools return uptime, heap, transport metrics, offline devices, recent logs.
3. `test_rule` lets the user/agent dry-run a candidate rule before enabling it.

### Workflow C — Remote device on the LAN.
1. A new sensor node announces itself; Dashboard registers it (consent) and lists its entities.
2. The agent adds a rule using one of its capabilities; the rule's actions route by capability to the remote device over UDP.
3. The node goes offline → rule conditions see `entity offline` deterministically; reconnects → `device.online` recovers state.

## 8. Relationship with Matter

- Matter is an **external/optional adapter** on its own branch/component.
- Qymera core exposes only seams: entity access, control, event subscription, registry, and config — with no Matter types in any core interface.
- A Matter adapter lets Matter controllers observe/control Dashboard entities; the reverse independence holds: **Dashboard works with zero Matter present.**

## 9. Relationship with Remote Qymera Devices

- UDP is the only transport. Dashboard is the hub: discovery, registration, telemetry ingestion, command routing, offline management.
- Devices are first-class registry members with stable identity beyond IP; the Dashboard scales to large fleets because device/entity capacity no longer sits inside a fixed 64-slot sensor table.

## 10. The Product Promise

| If… | Then… |
|---|---|
| The internet dies | automations run, UI works, history records |
| The AI provider dies | deterministic automations unaffected; inference rules degrade per policy; audit shows why |
| Matter is absent | nothing in the core changes |
| The browser is closed | the runtime runs unattended |
| Network saturates | bounded, counted, priority-biased drops; loop never starves |
| A rule is wrong | validation catches it at authoring; dry-run previews it; revision history audits it |

---

*Companion specs: [QYMERA_DASHBOARD_ARCHITECTURE.md](./QYMERA_DASHBOARD_ARCHITECTURE.md), [AUTOMATION_ENGINE.md](./AUTOMATION_ENGINE.md), [AI_AGENT_SKILL.md](./AI_AGENT_SKILL.md), [DATA_MODEL.md](./DATA_MODEL.md), [STORAGE_AND_MEMORY.md](./STORAGE_AND_MEMORY.md), [NETWORK_PROTOCOL.md](./NETWORK_PROTOCOL.md), [MIGRATION_PLAN.md](./MIGRATION_PLAN.md).*