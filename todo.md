# Qymera Link - IDF Task List

# Branch: qymera-IDF (Qymera Link - native ESP-IDF + Matter bridge)
# HEAD: c28fd77 (work in progress)

# Priority Order

### P0 - Requirement (done)

- [x] Remove OTA functionality completely (no ota_0/ota_1/otadata; single factory)
- [x] Remove ESP-NOW from runtime (UDP only; TRANSPORT_ESPNOW source-compat only)
- [x] Remove Arduino framework dependency (native ESP-IDF drivers only)
- [x] ESP32 only (no ESP8266 runtime conditionals)
- [x] Partition table single-factory (`partitions_qymera.csv`)
- [x] Full firmware build green (RAM 19.2% / Flash 17.3%)

### P1 - Audited / Resolved

- [x] Build config: `src_dir = main` (legacy root `src/` not compiled)
- [x] Component `REQUIRES` fixed (builtin IDF components only; no esp_ota)
- [x] Remove placeholder components (`espressif__esp_ota`, `espressif__esp_efuse`)
- [x] Remove managed Matter SDK from default build (Matter opt-in)
- [x] `config.h`: `#endif` orphan removed
- [x] `core.*` rewritten to native HAL (no Arduino/OTA fragments)
- [x] `web.cpp`: orphaned OTA block + WiFi/IPAddress leaks removed
- [x] `storage.cpp`: OTA storage funcs removed
- [x] Dead files removed (`espnow_p2p.*`, stale `main.cpp` in component)
- [x] Public API: `Qymera::begin/loop/setInit/setReport/setCommand` (Phase 3)
- [x] Core/HAL boundary vetted; network API through `qhal_*` (Phase 4)
- [x] FreeRTOS model verified (single loop task + recursive mutex) (Phase 8)
- [x] Memory budget verified (no concerning patterns) (Phase 6)
- [x] Storage NVS-backed persistence appropriate (Phase 7)
- [x] UDP-only confirmed (Phase 9)
- [x] HTTP server via qhttpserver HAL suitable (Phase 10)
- [x] Matter isolated/optional (Phase 11)
- [x] Matter bridging reviewed against release/v1.3 API and corrected:
      endpoint creation (`on_off_light/dimmable_light::create(node,&cfg,flag,priv)`),
      node-level attribute callback (`node::create(config, cb, ident_cb)` with
      `PRE_UPDATE` guard), Qymera->Matter state sync via `attribute::update`
      (echo-guarded, no feedback loop), relays->OnOffLight, dimmers->DimmableLight
- [x] Host sanity tests pass (45/45) (Phase 13)
- [x] README-IDF.md written (Phase 14)

### P2 - Remaining

- [ ] Matter ON (Step 8): cannot compile here (needs IDF 5.2.1 + esp_matter ~1.3.0)
- [ ] Verify `idf.py build` in an IDF 5.x shell (authoritative target)
- [ ] Flash to COM3 and verify WiFi/service boot
- [ ] Finalize Matter bridge against FIRST real IDF 5.2.1 build:
      confirm `attribute::update`, `esp_matter::uint8/bool`, `callback_type_t`/
      `PRE_UPDATE` (bridge rewritten against release/v1.3 managed_component_light;
      still unbuilt here)
- [ ] Adopt remaining Arduino-style scalar aliases (`constrain/map/PSTR/dtostrf`)
      in frozen core only if/when core is touched (accepted, documented surface)

### P3 - Future

- [ ] Factory reset must NOT wipe Matter fabric (document interaction; Qymera
      reset = config/WiFi/automations only)
- [ ] Additional hardware/sensor validation
