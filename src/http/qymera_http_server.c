/**
 * Qymera Dashboard - HTTP Server implementation
 *
 * All control/CRUD operations are routed through qymera_skill_execute().
 * The HTTP layer performs JSON schema validation, then forwards to the
 * Skill layer, which validates and calls the deterministic runtime
 * (Registry, Rule Engine, Control API, Storage). The HTTP layer NEVER
 * calls GPIO, UDP, Registry mutation, Rule Engine mutation, or storage
 * directly.
 */

#include "qymera_http_api.h"
#include "qymera_skill.h"
#include "qymera_core.h"
#include "qymera_hal.h"
#include "qymera_log.h"
#include "qymera_registry.h"
#include "qymera_control.h"
#include <esp_http_server.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define QYMERA_HTTP_BODY_SZ 512

/* =========================
 * Embedded Dashboard UI (HTML + CSS + JS)
 * ========================= */

static const char *DASHBOARD_HTML =
"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Qymera Dashboard</title><style>"
"*{box-sizing:border-box;margin:0;padding:0;font-family:system-ui,sans-serif}"
"body{background:#f5f7fa;color:#1a1d21;min-height:100vh}"
"header{background:#1a1d21;color:#fff;padding:12px 16px;display:flex;justify-content:space-between;align-items:center}"
"nav{display:flex;gap:4px;padding:8px 12px;background:#fff;border-bottom:1px solid #e2e8f0;overflow-x:auto;white-space:nowrap}"
".tab{padding:8px 16px;border:none;background:transparent;color:#475569;font-size:0.85rem;font-weight:500;cursor:pointer;border-bottom:2px solid transparent}"
".tab:hover{color:#1a1d21}.tab.active{color:#2563eb;border-bottom-color:#2563eb}"
"main{padding:16px}.card{background:#fff;border-radius:8px;border:1px solid #e2e8f0;padding:16px;margin-bottom:16px}"
"h2{font-size:0.9rem;font-weight:600;color:#334155;margin-bottom:12px}"
".grid{display:grid;gap:12px}.grid-2{grid-template-columns:repeat(auto-fit,minmax(280px,1fr))}.grid-3{grid-template-columns:repeat(auto-fit,minmax(220px,1fr))}"
".entity{display:flex;justify-content:space-between;align-items:center;padding:10px 12px;background:#f8fafc;border-radius:6px;border:1px solid #e2e8f0}"
".entity-info{display:flex;flex-direction:column;gap:2px}.entity-name{font-weight:500;font-size:0.85rem}.entity-meta{font-size:0.7rem;color:#64748b}"
".btn{padding:6px 12px;border:none;border-radius:4px;font-size:0.75rem;font-weight:500;cursor:pointer}"
".btn-primary{background:#2563eb;color:#fff}.btn-danger{background:#ef4444;color:#fff}.btn-ghost{background:#f1f5f9;color:#475569}"
".toggle{position:relative;width:44px;height:24px;background:#cbd5e1;border:none;border-radius:12px;cursor:pointer}"
".toggle.on{background:#22c55e}.toggle::after{content:\"\";position:absolute;top:2px;left:2px;width:20px;height:20px;background:#fff;border-radius:50%;transition:0.2s}"
".toggle.on::after{left:22px}"
".slider{width:100%;height:6px;-webkit-appearance:none;appearance:none;background:#e2e8f0;border-radius:3px;outline:none}"
".slider::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:18px;height:18px;background:#2563eb;border-radius:50%;cursor:pointer}"
"input,select{width:100%;padding:8px 12px;border:1px solid #cbd5e1;border-radius:4px;background:#fff;color:#1a1d21}"
".log-entry{font-family:monospace;font-size:0.7rem;padding:6px 10px;background:#f8fafc;border-radius:4px;margin-bottom:4px;border-left:3px solid #2563eb}"
".badge{display:inline-block;padding:2px 8px;border-radius:999px;font-size:0.65rem;font-weight:600}"
".badge-online{background:#dcfce7;color:#166534}.badge-offline{background:#fee2e2;color:#991b1b}"
".badge-pending{background:#fef3c7;color:#92400e}.badge-confirmed{background:#dbeafe;color:#1e40af}"
".badge-enabled{background:#dcfce7;color:#166534}.badge-disabled{background:#fee2e2;color:#991b1b}"
"@media(max-width:640px){header h1{font-size:1rem}main{padding:12px}}"
"</style></head><body>"
"<header><h1><span class=\"status-dot\" id=\"statusDot\"></span>Qymera Dashboard</h1><div id=\"sysInfo\"></div></header>"
"<nav id=\"navTabs\">"
"<button class=\"tab active\" data-tab=\"dashboard\">Dashboard</button>"
"<button class=\"tab\" data-tab=\"devices\">Devices</button>"
"<button class=\"tab\" data-tab=\"entities\">Entities</button>"
"<button class=\"tab\" data-tab=\"rules\">Rules</button>"
"<button class=\"tab\" data-tab=\"skills\">Skills</button>"
"<button class=\"tab\" data-tab=\"logs\">Logs</button>"
"<button class=\"tab\" data-tab=\"system\">System</button>"
"</nav>"
"<main id=\"mainContent\"></main>"
"<script>"
"const API='/api/v1';let currentTab='dashboard';"
"async function api(path,opts={}){const r=await fetch(API+path,{headers:{'Content-Type':'application/json'},...opts});const j=await r.json();if(!r.ok)throw new Error(j.error?.message||r.statusText);return j;}"
"document.querySelectorAll('.tab').forEach(b=>b.addEventListener('click',()=>{currentTab=b.dataset.tab;renderTab(currentTab);}));"
"async function renderTab(name){"
"document.querySelectorAll('.tab').forEach(b=>b.classList.toggle('active',b.dataset.tab===name));"
"const main=document.getElementById('mainContent');"
"main.innerHTML='<div class=card><div class=empty>Loading...</div></div>';"
"try{if(name==='dashboard'){"
"const s=await api('/status');"
"main.innerHTML=`<div class=section-header><h2>System Overview</h2></div>"
"<div class=grid grid-3>"
"<div class=card><h2>Free Heap</h2><div style=font-size:1.5rem;font-weight:600>${s.data.free_heap} B</div></div>"
"<div class=card><h2>Uptime</h2><div style=font-size:1.5rem;font-weight:600>${(s.data.uptime_ms/1000).toFixed(1)}s</div></div>"
"<div class=card><h2>IP Address</h2><div style=font-size:1.2rem;font-weight:500>${s.data.ip}</div></div>"
"<div class=card><h2>Devices</h2><div style=font-size:1.5rem;font-weight:600>${s.data.device_count}</div></div>"
"<div class=card><h2>Entities</h2><div style=font-size:1.5rem;font-weight:600>${s.data.entity_count}</div></div>"
"</div>`;"
"document.getElementById('sysInfo').innerHTML='IP: '+s.data.ip+' | Heap: '+s.data.free_heap+' B';"
"document.getElementById('statusDot').className='status-dot ok';"
"}else if(name==='devices'){"
"const d=await api('/devices');"
"if(!d.data||!d.data.length){main.innerHTML='<div class=card><div class=empty>No devices found</div></div>';return;}"
"main.innerHTML=`<div class=section-header><h2>Devices (${d.data.length})</h2></div>"
"<div class=grid grid-2>${d.data.map(dev=>`"
"<div class=card><h2>${dev.name}</h2>"
"<div class=entity><div class=entity-info><span class=entity-name>ID: ${dev.device_id}</span>"
"<span class=entity-meta>Model: ${dev.model} | FW: ${dev.fw_version} | Role: ${['Dashboard','Remote','Provisioning'][dev.role]}</span></div>"
"<span class=\"badge ${dev.online?'badge-online':'badge-offline'}\">${dev.online?'Online':'Offline'}</span></div>"
"<div style=margin-top:8px><span class=entity-meta>Entities: ${dev.entity_count} | IP: ${dev.ip_addr} | Port: ${dev.port}</span></div>"
"</div>`).join('')}</div>`;"
"}else if(name==='entities'){"
"const e=await api('/entities');"
"if(!e.data||!e.data.length){main.innerHTML='<div class=card><div class=empty>No entities found</div></div>';return;}"
"main.innerHTML=`<div class=section-header><h2>Entities (${e.data.length})</h2></div>"
"<div class=grid grid-2>${e.data.map(ent=>{"
"const isRelay=ent.capabilities&&ent.capabilities.includes('ACTUATOR_RELAY');"
"const isDimmer=ent.capabilities&&ent.capabilities.includes('ACTUATOR_DIMMER');"
"const cur=typeof ent.current==='boolean'?(ent.current?'ON':'OFF'):ent.current;"
"const des=typeof ent.desired==='boolean'?(ent.desired?'ON':'OFF'):ent.desired;"
"return `<div class=card><h2>${ent.name} <span class=entity-meta>(${ent.device_id})</span></h2>"
"<div class=entity><div class=entity-info><span class=entity-name>ID: ${ent.entity_id}</span>"
"<span class=entity-meta>Type: ${ent.type} | Caps: ${(ent.capabilities||[]).join(', ')} | Unit: ${ent.unit}</span>"
"<span class=entity-meta>Status: <span class=\"badge badge-pending\">${ent.cmd_status}</span> | Reliability: ${ent.reliability}</span></div>"
"<div style=display:flex;flex-direction:column;gap:4px;min-width:120px>"
"<span class=entity-meta>Current: <strong>${cur}</strong> | Desired: <strong>${des}</strong></span>"
"${isRelay?`<label class=\"toggle ${cur==='ON'?'on':''}\" data-dev=\"${ent.device_id}\" data-ent=\"${ent.entity_id}\"></label>`:''}"
"${isDimmer?`<input type=range class=slider min=0 max=100 value=\"${typeof des==='boolean'?0:des}\" data-dev=\"${ent.device_id}\" data-ent=\"${ent.entity_id}\">`:''}"
"</div></div>"
"<div style=margin-top:8px><span class=entity-meta>Last update: ${new Date(ent.last_updated?.seconds*1000||0).toLocaleTimeString()}</span></div>"
"</div>`;"
"}).join('')}</div>`;"
"main.querySelectorAll('.toggle').forEach(t=>t.addEventListener('click',async()=>{"
"const n=!t.classList.contains('on');t.classList.toggle('on',n);"
"try{await api('/control/relay',{method:'POST',body:JSON.stringify({device_id:t.dataset.dev,entity_id:t.dataset.ent,value:n})});renderTab('entities');}"
"catch(err){t.classList.toggle('on',!n);alert(err.message);}}));"
"main.querySelectorAll('.slider').forEach(s=>s.addEventListener('change',async()=>{"
"try{await api('/control/dimmer',{method:'POST',body:JSON.stringify({device_id:s.dataset.dev,entity_id:s.dataset.ent,level:parseInt(s.value)})});renderTab('entities');}"
"catch(err){alert(err.message);}}));"
"}else if(name==='rules'){"
"const r=await api('/rules');"
"if(!r.data||!r.data.length){main.innerHTML='<div class=card><div class=empty>No rules found</div><button class=btn btn-primary style=margin-top:12px onclick=showCreateRule()>Create Rule</button></div>';return;}"
"main.innerHTML=`<div class=section-header><h2>Rules (${r.data.length})</h2><button class=btn btn-primary onclick=showCreateRule()>Create Rule</button></div>"
"<div class=grid grid-2>${r.data.map(rule=>`"
"<div class=card><h2>${rule.name}</h2>"
"<div class=entity><div class=entity-info><span class=entity-name>ID: ${rule.rule_id}</span>"
"<span class=entity-meta>Priority: ${rule.priority} | Cooldown: ${rule.cooldown_ms/1000}s | Max/hr: ${rule.max_activations_per_hour}</span>"
"<span class=entity-meta>Trigger: ${rule.trigger.entity.device_id}.${rule.trigger.entity.entity_id} ${rule.trigger.operator_} ${rule.trigger.threshold}</span></div>"
"<span class=\"badge ${rule.enabled?'badge-enabled':'badge-disabled'}\">${rule.enabled?'Enabled':'Disabled'}</span></div>"
"<div style=margin-top:8px;display:flex;gap:8px;flex-wrap:wrap>"
"<button class=btn btn-ghost onclick=editRule('${rule.rule_id}')>Edit</button>"
"<button class=btn ${rule.enabled?'btn-danger':'btn-primary'} onclick=toggleRule('${rule.rule_id}',${!rule.enabled})>${rule.enabled?'Disable':'Enable'}</button>"
"<button class=btn btn-danger onclick=deleteRule('${rule.rule_id}')>Delete</button>"
"</div></div>`).join('')}</div>`;"
"}else if(name==='skills'){"
"const s=await api('/skills');"
"main.innerHTML=`<div class=section-header><h2>Skill Catalog (${s.data.length})</h2></div>"
"<div class=grid grid-2>${s.data.map(sk=>`"
"<div class=card><h2>${sk.name}</h2>"
"<div class=entity><div class=entity-info><span class=entity-name>v${sk.version}</span>"
"<span class=entity-meta>${sk.description}</span><span class=entity-meta>Schema: ${sk.schema_id}</span>"
"<span class=entity-meta>Perms: ${(sk.permissions&1?'READ ':'')+(sk.permissions&2?'CONTROL ':'')+(sk.permissions&4?'RULE_READ ':'')+(sk.permissions&8?'RULE_WRITE ':'')}</span></div></div>`"
").join('')}</div>`;"
"}else if(name==='logs'){"
"const l=await api('/logs');"
"if(!l.data||!l.data.length){main.innerHTML='<div class=card><div class=empty>No logs available</div></div>';return;}"
"main.innerHTML=`<div class=section-header><h2>Recent Logs (${l.data.length})</h2></div>"
"<div class=card>${l.data.slice(0,50).map(e=>`"
"<div class=\"log-entry ${e.layer==='ERROR'?'error':e.layer==='WARNING'?'warn':e.layer==='INFO'?'info':''}\">"
"<span style=color:#94a3b8>[${new Date(e.timestamp.seconds*1000).toLocaleTimeString()}]</span> "
"<span style=font-weight:500>${e.source}</span>: ${e.message}</div>`).join('')}</div>`;"
"}else if(name==='system'){"
"const s=await api('/status');"
"main.innerHTML=`<div class=section-header><h2>System Information</h2></div>"
"<div class=grid grid-3>"
"<div class=card><h2>Free Heap</h2><div style=font-size:1.5rem;font-weight:600>${s.data.free_heap} B</div></div>"
"<div class=card><h2>Uptime</h2><div style=font-size:1.5rem;font-weight:600>${(s.data.uptime_ms/1000).toFixed(1)}s</div></div>"
"<div class=card><h2>IP Address</h2><div style=font-size:1.2rem;font-weight:500>${s.data.ip}</div></div>"
"<div class=card><h2>Devices</h2><div style=font-size:1.5rem;font-weight:600>${s.data.device_count}</div></div>"
"<div class=card><h2>Entities</h2><div style=font-size:1.5rem;font-weight:600>${s.data.entity_count}</div></div>"
"<div class=card><h2>Build</h2><div style=font-size:1rem;font-weight:500>ESP32 Dashboard</div></div>"
"</div>`;"
"document.getElementById('sysInfo').innerHTML='IP: '+s.data.ip+' | Heap: '+s.data.free_heap+' B';"
"document.getElementById('statusDot').className='status-dot ok';"
"}"
"}catch(e){main.innerHTML='<div class=card><div class=empty style=color:#ef4444>Error: '+e.message+'</div></div>';}}"
"window.showCreateRule=()=>{"
"const main=document.getElementById('mainContent');"
"main.innerHTML=`<div class=card><h2>Create Rule</h2><form id=ruleForm>"
"<div class=form-group><label>Name</label><input name=name required></div>"
"<div class=form-group><label>Rule ID</label><input name=rule_id placeholder=auto-generated></div>"
"<div class=form-group><label>Trigger Entity</label><input name=trigger[entity][device_id] placeholder=device_id required><input name=trigger[entity][entity_id] placeholder=entity_id required></div>"
"<div class=form-row><div class=form-group><label>Operator</label><select name=trigger[operator_]><option value=GT>>></option><option value=LT><</option></select></div>"
"<div class=form-group><label>Threshold</label><input name=trigger[threshold] type=number step=any required></div></div>"
"<div class=form-group><label>Action Entity</label><input name=actions[0][entity][device_id] placeholder=device_id required><input name=actions[0][entity][entity_id] placeholder=entity_id required></div>"
"<div class=form-row><div class=form-group><label>Action Type</label><select name=actions[0][action]><option value=SET_BOOL>Set Bool</option><option value=SET_LEVEL>Set Level</option></select></div>"
"<div class=form-group><label>Value</label><input name=actions[0][value_u32] type=number></div></div>"
"<div class=form-row><div class=form-group><label>Cooldown (ms)</label><input name=cooldown_ms type=number value=60000></div>"
"<div class=form-group><label>Priority</label><input name=priority type=number value=10></div></div>"
"<button type=submit class=btn btn-primary style=margin-top:12px;width:100%>Create</button>"
"<button type=button class=btn btn-ghost style=margin-top:8px;width:100% onclick=loadTab('rules')>Cancel</button>"
"</form></div>`;"
"document.getElementById('ruleForm').addEventListener('submit',async(e)=>{"
"e.preventDefault();const fd=new FormData(e.target);const body={};"
"for(const[k,v]of fd)body[k]=v;"
"try{await api('/rules',{method:'POST',body:JSON.stringify(body)});loadTab('rules');}"
"catch(err){alert(err.message);}});"
"};"
"window.editRule=id=>{"
"api('/rules/'+id).then(r=>{"
"const main=document.getElementById('mainContent');const rule=r.data;"
"main.innerHTML=`<div class=card><h2>Edit Rule: ${rule.name}</h2><form id=ruleForm>"
"<input type=hidden name=rule_id value=${rule.rule_id}>"
"<div class=form-group><label>Name</label><input name=name value=${rule.name} required></div>"
"<div class=form-group><label>Enabled</label><select name=enabled><option value=true ${rule.enabled?'selected':''}>Yes</option><option value=false ${rule.enabled?'':'selected'}>No</option></select></div>"
"<button type=submit class=btn btn-primary style=margin-top:12px;width:100%>Update</button>"
"<button type=button class=btn btn-ghost style=margin-top:8px;width:100% onclick=loadTab('rules')>Cancel</button>"
"</form></div>`;"
"document.getElementById('ruleForm').addEventListener('submit',async(e)=>{"
"e.preventDefault();const fd=new FormData(e.target);const body={};"
"for(const[k,v]of fd)body[k]=v;"
"try{await api('/rules/'+id,{method:'PUT',body:JSON.stringify(body)});loadTab('rules');}"
"catch(err){alert(err.message);}});"
"};"
"};"
"window.toggleRule=(id,enabled)=>{api('/rules/'+id+(enabled?'enable':'disable'),{method:'POST'}).then(()=>loadTab('rules')).catch(err=>alert(err.message));};"
"window.deleteRule=id=>{if(!confirm('Delete rule '+id+'?'))return;api('/rules/'+id,{method:'DELETE'}).then(()=>loadTab('rules')).catch(err=>alert(err.message));};"
"async function start(){await renderTab('dashboard');setInterval(()=>renderTab(currentTab),10000);}"
"start();"
"</script>"
"</body></html>";



/* =========================
 * Minimal JSON field extractors
 * ========================= */

static const char *json_find_str(const char *json, const char *key,
                                    char *out, size_t out_sz) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return NULL;
    /* Use a local non-const pointer to iterate */
    char *q = (char *)p + 1; /* skip opening quote */
    size_t i = 0;
    for (; *q && *q != '"' && i < out_sz - 1; q++, i++) {
        if (*q == '\\' && *(q + 1)) {
            q++;
            if (*q == 'n') out[i++] = '\n';
            else if (*q == 't') out[i++] = '\t';
            else if (*q == 'r') out[i++] = '\r';
            else if (*q == '"') out[i++] = '"';
            else if (*q == '\\') out[i++] = '\\';
            else if (*q == '/') out[i++] = '/';
            else if (*q == 'b') out[i++] = '\b';
            else if (*q == 'f') out[i++] = '\f';
            else if (*q == 'u') { q += 4; out[i++] = '?'; }
            else out[i++] = *q;
        } else {
            out[i++] = *q;
        }
    }
    out[i] = '\0';
    return q; /* return pointer after closing quote */
}

static int json_find_int(const char *json, const char *key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ' || *p == ':') p++;
    return (int)atoi(p);
}

static bool json_find_bool(const char *json, const char *key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ' || *p == ':') p++;
    return (strncmp(p, "true", 4) == 0);
}

/* =========================
 * Build skill input from JSON body
 * ========================= */

static void http_build_input(const char *body, qymera_skill_input_t *in) {
    memset(in, 0, sizeof(qymera_skill_input_t));
    char tmp[64];
    if (json_find_str(body, "device_id", tmp, sizeof(tmp)))
        strncpy(in->device_id, tmp, sizeof(in->device_id) - 1);
    if (json_find_str(body, "entity_id", tmp, sizeof(tmp)))
        strncpy(in->entity_id, tmp, sizeof(in->entity_id) - 1);
    if (json_find_str(body, "name", tmp, sizeof(tmp)))
        strncpy(in->name, tmp, sizeof(in->name) - 1);
    if (json_find_str(body, "rule_id", tmp, sizeof(tmp)))
        strncpy(in->rule_id, tmp, sizeof(in->rule_id) - 1);
    if (json_find_bool(body, "value")) in->value = true;
    in->level = (uint8_t)json_find_int(body, "level");
    if (json_find_bool(body, "enabled")) in->enabled = true;
}

/* =========================
 * Skill dispatch helper
 * ========================= */

static bool http_dispatch(qymera_core_t *core, const char *skill_name,
                            qymera_skill_input_t *input,
                            qymera_skill_output_t *output,
                            uint32_t perm_mask) {
    qymera_skill_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.registry = qymera_core_get_registry(core);
    ctx.rule_engine = qymera_core_get_rule_engine(core);
    ctx.control = qymera_core_get_control(core);
    ctx.storage = qymera_core_get_storage(core);
    ctx.log = qymera_core_get_log(core);
    qymera_err_t err = qymera_skill_execute(&ctx, skill_name, input, output, perm_mask);
    return err == QYMERA_OK;
}

/* =========================
 * Response helpers
 * ========================= */

static void http_send_json(httpd_req_t *req, const char *json) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
}

static void http_serialize_ok(qymera_skill_output_t *out, char *buf, size_t sz) {
    if (out->data_len > 0 && out->data_len < QYMERA_SKILL_OUTPUT_SIZE) {
        snprintf(buf, sz, "{\"ok\":true,\"data\":%.*s}",
                 (int)out->data_len, out->data);
    } else {
        snprintf(buf, sz, "{\"ok\":true,\"data\":{}}");
    }
}

static void http_serialize_err(qymera_skill_output_t *out, char *buf, size_t sz) {
    snprintf(buf, sz,
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
             out->error_code, out->message);
}

static void http_skill_to_http(qymera_core_t *core, const char *skill,
                                 qymera_skill_input_t *input,
                                 uint32_t perm, httpd_req_t *req) {
    qymera_skill_output_t out;
    memset(&out, 0, sizeof(out));
    http_dispatch(core, skill, input, &out, perm);
    char buf[QYMERA_SKILL_OUTPUT_SIZE + 128];
    if (out.ok) http_serialize_ok(&out, buf, sizeof(buf));
    else http_serialize_err(&out, buf, sizeof(buf));
    http_send_json(req, buf);
}

/* =========================
 * URI path helpers
 * ========================= */

static const char *path_seg(const char *uri, const char *seg, int n) {
    const char *p = uri;
    for (int i = 0; i < n; i++) {
        p = strstr(p, seg);
        if (!p) return NULL;
        p += strlen(seg);
    }
    return p;
}

static bool extract_rule_id(const char *uri, char *out, size_t sz) {
    const char *p = strstr(uri, "/api/v1/rules/");
    if (!p) return false;
    p += strlen("/api/v1/rules/");
    const char *end = strchr(p, '?');
    if (!end) end = strchr(p, '\0');
    size_t len = end - p;
    if (len >= sz) len = sz - 1;
    strncpy(out, p, len);
    out[len] = '\0';
    return true;
}

static bool extract_entity_path(const char *uri, char *device, size_t dev_sz,
                                  char *entity, size_t ent_sz) {
    const char *p = strstr(uri, "/api/v1/entities/");
    if (!p) return false;
    p += strlen("/api/v1/entities/");
    const char *slash = strchr(p, '/');
    if (!slash) return false;
    size_t dlen = slash - p;
    if (dlen >= dev_sz) dlen = dev_sz - 1;
    strncpy(device, p, dlen);
    device[dlen] = '\0';
    const char *e = slash + 1;
    const char *eq = strchr(e, '?');
    if (!eq) eq = strchr(e, '\0');
    size_t elen = eq - e;
    if (elen >= ent_sz) elen = ent_sz - 1;
    strncpy(entity, e, elen);
    entity[elen] = '\0';
    return true;
}

/* =========================
 * Handlers
 * ========================= */

static esp_err_t h_status_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    uint32_t heap = qymera_system_get_free_heap();
    uint32_t up = qymera_system_get_uptime_ms();
    char ip[16] = {0};
    qymera_wifi_get_ip(ip, sizeof(ip));
    size_t dc = qymera_registry_device_count(qymera_core_get_registry(core));
    size_t ec = qymera_registry_entity_count(qymera_core_get_registry(core));
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"data\":{\"free_heap\":%u,\"uptime_ms\":%u,"
        "\"ip\":\"%s\",\"device_count\":%zu,\"entity_count\":%zu}}",
        heap, up, ip, dc, ec);
    http_send_json(req, buf);
    return ESP_OK;
}

static esp_err_t h_devices_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    http_skill_to_http(core, "list_devices", &in, QYMERA_PERM_READ, req);
    return ESP_OK;
}

static esp_err_t h_entities_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    http_skill_to_http(core, "list_entities", &in, QYMERA_PERM_READ, req);
    return ESP_OK;
}

static esp_err_t h_entity_state_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char device[QYMERA_DEVICE_ID_LEN], entity[QYMERA_ENTITY_ID_LEN];
    if (!extract_entity_path(req->uri, device, sizeof(device),
                              entity, sizeof(entity))) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    strncpy(in.device_id, device, sizeof(in.device_id) - 1);
    strncpy(in.entity_id, entity, sizeof(in.entity_id) - 1);
    http_skill_to_http(core, "get_entity_state", &in, QYMERA_PERM_READ, req);
    return ESP_OK;
}

static esp_err_t h_relay_post(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char body[QYMERA_HTTP_BODY_SZ];
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) { http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\"}}"); return ESP_OK; }
    body[n] = '\0';
    qymera_skill_input_t in; http_build_input(body, &in);
    http_skill_to_http(core, "set_relay", &in, QYMERA_PERM_CONTROL, req);
    return ESP_OK;
}

static esp_err_t h_dimmer_post(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char body[QYMERA_HTTP_BODY_SZ];
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) { http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\"}}"); return ESP_OK; }
    body[n] = '\0';
    qymera_skill_input_t in; http_build_input(body, &in);
    http_skill_to_http(core, "set_dimmer", &in, QYMERA_PERM_CONTROL, req);
    return ESP_OK;
}

static esp_err_t h_rules_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    http_skill_to_http(core, "list_rules", &in, QYMERA_PERM_RULE_READ, req);
    return ESP_OK;
}

static esp_err_t h_rule_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char rid[QYMERA_RULE_ID_LEN];
    if (!extract_rule_id(req->uri, rid, sizeof(rid))) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    strncpy(in.rule_id, rid, sizeof(in.rule_id) - 1);
    http_skill_to_http(core, "get_rule", &in, QYMERA_PERM_RULE_READ, req);
    return ESP_OK;
}

static esp_err_t h_rules_post(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char body[QYMERA_HTTP_BODY_SZ];
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) { http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\"}}"); return ESP_OK; }
    body[n] = '\0';
    qymera_skill_input_t in; http_build_input(body, &in);
    http_skill_to_http(core, "create_rule", &in, QYMERA_PERM_RULE_WRITE, req);
    return ESP_OK;
}

static esp_err_t h_rules_put(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char rid[QYMERA_RULE_ID_LEN];
    if (!extract_rule_id(req->uri, rid, sizeof(rid))) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    char body[QYMERA_HTTP_BODY_SZ];
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) { http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_INPUT\"}}"); return ESP_OK; }
    body[n] = '\0';
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    strncpy(in.rule_id, rid, sizeof(in.rule_id) - 1);
    http_build_input(body, &in);
    http_skill_to_http(core, "update_rule", &in, QYMERA_PERM_RULE_WRITE, req);
    return ESP_OK;
}

static esp_err_t h_rules_delete(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char rid[QYMERA_RULE_ID_LEN];
    if (!extract_rule_id(req->uri, rid, sizeof(rid))) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    strncpy(in.rule_id, rid, sizeof(in.rule_id) - 1);
    http_skill_to_http(core, "delete_rule", &in, QYMERA_PERM_RULE_WRITE, req);
    return ESP_OK;
}

static bool extract_rule_id_from_uri(const char *uri, const char *suffix,
                                          char *out, size_t sz) {
    const char *p = strstr(uri, "/api/v1/rules/");
    if (!p) return false;
    p += strlen("/api/v1/rules/");
    const char *end = strstr(p, suffix);
    if (!end) end = strchr(p, '\0');
    size_t len = end - p;
    if (len >= sz) len = sz - 1;
    strncpy(out, p, len);
    out[len] = '\0';
    return true;
}

static esp_err_t h_rule_enable(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char rid[QYMERA_RULE_ID_LEN];
    if (!extract_rule_id_from_uri(req->uri, "/enable", rid, sizeof(rid))) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    strncpy(in.rule_id, rid, sizeof(in.rule_id) - 1);
    in.enabled = true;
    http_skill_to_http(core, "enable_rule", &in, QYMERA_PERM_RULE_WRITE, req);
    return ESP_OK;
}

static esp_err_t h_rule_disable(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    char rid[QYMERA_RULE_ID_LEN];
    if (!extract_rule_id_from_uri(req->uri, "/disable", rid, sizeof(rid))) {
        http_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MALFORMED_URI\"}}");
        return ESP_OK;
    }
    qymera_skill_input_t in; memset(&in, 0, sizeof(in));
    strncpy(in.rule_id, rid, sizeof(in.rule_id) - 1);
    in.enabled = false;
    http_skill_to_http(core, "disable_rule", &in, QYMERA_PERM_RULE_WRITE, req);
    return ESP_OK;
}

static esp_err_t h_skills_get(httpd_req_t *req) {
    qymera_core_t *core = (qymera_core_t *)req->user_ctx;
    size_t count = qymera_skill_registry_count();
    char buf[2048];
    char *p = buf;
    size_t remaining = sizeof(buf);
    int n = snprintf(p, remaining, "{\"ok\":true,\"data\":[");
    p += n; remaining -= n;
    for (size_t i = 0; i < count; i++) {
        const qymera_skill_entry_t *entry = NULL;
        qymera_skill_id_t id = qymera_skill_registry_get(i, &entry);
        if (!entry) continue;
        if (i > 0) { n = snprintf(p, remaining, ","); p += n; remaining -= n; }
        n = snprintf(p, remaining,
            "{\"name\":\"%s\",\"version\":\"%s\",\"description\":\"%s\","
            "\"schema_id\":\"%s\",\"permissions\":%u}",
            entry->meta.name, entry->meta.version, entry->meta.description,
            entry->meta.schema_id, entry->meta.permissions);
        p += n; remaining -= n;
    }
    n = snprintf(p, remaining, "]}");
    http_send_json(req, buf);
    return ESP_OK;
}

static esp_err_t h_root_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, DASHBOARD_HTML, strlen(DASHBOARD_HTML));
    return ESP_OK;
}

/* =========================
 * Route table
 * ========================= */

static const httpd_uri_t routes[] = {
    { .uri = "/", .method = HTTP_GET, .handler = h_root_get },
    { .uri = "/api/v1/status", .method = HTTP_GET, .handler = h_status_get },
    { .uri = "/api/v1/devices", .method = HTTP_GET, .handler = h_devices_get },
    { .uri = "/api/v1/entities", .method = HTTP_GET, .handler = h_entities_get },
    { .uri = "/api/v1/entities/", .method = HTTP_GET, .handler = h_entity_state_get },
    { .uri = "/api/v1/control/relay", .method = HTTP_POST, .handler = h_relay_post },
    { .uri = "/api/v1/control/dimmer", .method = HTTP_POST, .handler = h_dimmer_post },
    { .uri = "/api/v1/rules", .method = HTTP_GET, .handler = h_rules_get },
    { .uri = "/api/v1/rules/", .method = HTTP_GET, .handler = h_rule_get },
    { .uri = "/api/v1/rules", .method = HTTP_POST, .handler = h_rules_post },
    { .uri = "/api/v1/rules/", .method = HTTP_PUT, .handler = h_rules_put },
    { .uri = "/api/v1/rules/", .method = HTTP_DELETE, .handler = h_rules_delete },
    { .uri = "/api/v1/rules/", .method = HTTP_POST, .handler = h_rule_enable },
    { .uri = "/api/v1/rules/", .method = HTTP_POST, .handler = h_rule_disable },
    { .uri = "/api/v1/skills", .method = HTTP_GET, .handler = h_skills_get },
};

/* =========================
 * Init
 * ========================= */

qymera_err_t qymera_http_api_init(qymera_core_t *core) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    httpd_handle_t handle = NULL;
    if (httpd_start(&handle, &config) != ESP_OK) return QYMERA_ERR_INVALID_STATE;
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_uri_t uri = routes[i];
        uri.user_ctx = core;
        if (httpd_register_uri_handler(handle, &uri) != ESP_OK) {
            httpd_stop(handle);
            return QYMERA_ERR_INVALID_STATE;
        }
    }
    return QYMERA_OK;
}