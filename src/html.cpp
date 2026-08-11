#include "html.h"

namespace html_content {
// ================= WEB ===================
const char Styles[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
body{font-family:sans-serif;text-align:center;margin:0}.tabs{display:flex;justify-content:space-around;background:#222;color:#fff}.tab{flex:1;padding:12px;cursor:pointer}.active{background:#444}.content{padding:15px}.card{background:var(--card);color:var(--text);margin:10px;padding:10px;border-radius:12px;box-shadow:0 2px 8px rgba(0,0,0,.25);text-align:left;border-left:3px solid rgba(255,255,255,.75)}.card:hover{filter:brightness(1.08)}.card h3{margin-top:0;text-align:center}.devices-section-title{display:none}button{padding:6px 12px;border:none;border-radius:6px;background:#333;color:#fff;margin:5px;cursor:pointer}.matter-btn{padding:6px 12px;border-radius:6px;border:none;font-weight:600;cursor:pointer}.matter-btn.on{background:#2ecc71;color:#000}.matter-btn.off{background:#444;color:#bbb}.matterLbl{margin-left:20px;font-weight:600}.modal{position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,.6);display:none;align-items:center;justify-content:center;z-index:1000}.modal-content{background:var(--card);color:var(--text);padding:20px;border-radius:10px;width:320px;text-align:left}.modal input,.modal select{margin-bottom:10px;padding:6px;border-radius:6px;border:1px solid var(--text);background:var(--bg);color:var(--text)}#themePicker{position:fixed;top:10px;right:10px;z-index:9999;display:flex;gap:6px}.themeDot{width:18px;height:18px;border-radius:50%;cursor:pointer;border:2px solid #333}.themeDot:hover{transform:scale(1.2)}:root{--bg:#111315;--panel:#2e3238;--card:#3c4149;--text:#ffffff}body{background:var(--bg);color:var(--text)}.settings-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}.settings-general{grid-column:1/-1}@media(max-width:900px){.settings-grid{grid-template-columns:1fr}.settings-general{grid-column:auto}}
.themeDot[data-bg="#414141"]{background:#414141}.themeDot[data-bg="#4c834e"]{background:#4c834e}.themeDot[data-bg="#cdfcff"]{background:#cdfcff}.themeDot[data-bg="#c1af8d"]{background:#c1af8d}.themeDot[data-bg="#f7f2ff"]{background:#f7f2ff}.themeDot[data-bg="#61b956"]{background:#61b956}input[type=range]{width:100%}@media (min-width:1100px){#control{padding:10px 10px}.devices-mobile-list{display:none}.devices-desktop-layout{display:grid;grid-template-columns:minmax(260px,38%) minmax(420px,1fr);gap:18px;align-items:start;min-height:calc(100vh - 130px)}.devices-column{min-height:calc(100vh - 150px);border:1px solid rgba(255,255,255,.12);border-radius:10px;padding:12px;background:var(--panel);color:var(--text)}.devices-section-title{display:block;margin:2px 10px 12px;text-align:left;color:var(--text);font-size:13px;font-weight:700;letter-spacing:0;text-transform:uppercase}.devices-actuator-grid{display:grid;grid-template-columns:repeat(2,minmax(180px,1fr));gap:12px}.devices-sensor-grid{display:grid;grid-template-columns:repeat(3,minmax(180px,1fr));gap:12px}.devices-dashboard .card{margin:0;min-height:96px;box-sizing:border-box}.devices-dashboard .time-card{grid-column:span 1;min-height:96px}}
@media (max-width:899px){.devices-desktop-layout{display:none}.devices-mobile-list{display:block}.devices-column,.devices-actuator-grid,.devices-sensor-grid{display:block}}
.log-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}@media(max-width:900px){.log-grid{grid-template-columns:1fr}}.log-panel{background:var(--panel);color:var(--text);border-radius:10px;padding:12px;min-height:300px;max-height:60vh;overflow-y:auto;text-align:left;font-family:monospace;font-size:12px;line-height:1.4;border:1px solid rgba(255,255,255,.12)}.log-panel h3{margin:0 0 10px 0;font-size:13px;text-align:center;text-transform:uppercase;letter-spacing:1px;color:var(--text);border-bottom:1px solid rgba(255,255,255,.15);padding-bottom:8px}.log-entry{margin-bottom:4px;padding:3px 6px;border-radius:4px;background:rgba(0,0,0,.2);word-break:break-all}.log-entry .t{color:#888;font-size:10px}.log-entry .l{font-weight:700;margin:0 4px}.log-entry .l.inf{color:#2ecc71}.log-entry .l.wrn{color:#f39c12}.log-entry .l.err{color:#e74c3c}.log-entry.core{border-left:3px solid #3498db}.log-entry.evnt{border-left:3px solid #9b59b6}.log-entry.sens{border-left:3px solid #2ecc71}.log-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px}.log-header button{margin:0;padding:4px 10px;font-size:11px}
</style></head><body>
)rawliteral";


const char Tabs[] PROGMEM = R"rawliteral(
<h2 style='background:#222222c7;margin:0;padding:12px;text-align:center;color:#eee'>Qymera<div id="themePicker">
  <span class="themeDot" data-bg="#414141"></span>
  <span class="themeDot" data-bg="#f7f2ff"></span>
  <span class="themeDot" data-bg="#4c834e"></span>
  <span class="themeDot" data-bg="#61b956"></span>
  <span class="themeDot" data-bg="#c1af8d"></span>
  <span class="themeDot" data-bg="#cdfcff"></span>
</div>
</div></h2>
<div class='tabs'>
<div class='tab' id='t_control' onclick="show('control')">Devices</div>
<div class='tab' id='t_auto' onclick="show('auto')">Automations</div>
<div class='tab' id='t_config' onclick="show('config')">Settings</div>
<div class='tab' id='t_wifi' onclick="show('wifi')">Network</div>
<div class='tab' id='t_logs' onclick="show('logs')">Logs</div>
</div>
<div id='control' class='content'><div id='devices_cards'></div></div>
<div id='auto' class='content' style='display:none'>  
<table style="width:100%;border-collapse:collapse">
<thead>
<tr>
<th>ID</th>
<th>Sensors</th>
<th>Type</th>
<th>Logic</th>
<th>Actuators</th>
<th>Delay</th>
<th>Cooldown</th>
<th></th>
</tr>
</thead>
<tbody id="auto_table"></tbody>
</table>
<div style="margin-top:10px;text-align:right">
<button style="float:left" onclick="newRule()">Add Rule</button>
</div>
</div>
<div id='config' class='content' style='display:none'><div id='cards'></div></div>
<div id='wifi' class='content' style='display:none'>
<h2>WiFi Setup</h2>
<form action='/save' method='post'>
<input name='ssid' placeholder='SSID' style='margin:6px;border-radius:6px;padding:5px;'><br>
<input name='pass' placeholder='Password' type='password' style='margin:6px;border-radius:6px;padding:5px;'><br>
<button type='submit' style='margin:10px;'>Save</button>
</form>
</div>
<div id='logs' class='content' style='display:none'>
<div class='log-header'>
<span style='font-weight:600'>System Logs</span>
<button onclick='refreshLogs()'>Refresh</button>
</div>
<div class='log-grid'>
<div class='log-panel' id='log-core'><h3>Core</h3></div>
<div class='log-panel' id='log-events'><h3>Events</h3></div>
<div class='log-panel' id='log-sensors'><h3>Sensors / Warn / Error</h3></div>
</div>
</div>
<script>
function show(tab){
document.querySelectorAll('.content').forEach(c=>c.style.display='none');
document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
document.getElementById(tab).style.display='block';
document.getElementById('t_'+tab).classList.add('active');
localStorage.setItem('tab',tab);
if(tab==='auto') loadRules();
if (tab === 'config') loadCalib();
if (tab === 'logs'){ refreshLogs(); startLogAutoRefresh(); }
else { stopLogAutoRefresh(); }
}
)rawliteral";


const char Rules[] PROGMEM = R"rawliteral(
function renderAutomationTable(rules){
let html = `
<div class='card'>
<h3>Rules</h3>
<table style="width:100%;text-align:left;border-collapse:collapse">
<tr>
<th>ID</th>
<th>Sensors</th>
<th>Logic</th>
<th>Actions</th>
<th>Delay</th>
<th>Cooldown</th>
<th></th>
</tr>
`;
rules.forEach((r,i)=>{
html+=`
<tr style="border-top:1px solid #ccc">
<td>${i}</td>
<td>${r.sensors.join(", ")}</td>
<td>${r.logic}</td>
<td>${r.actions.join(", ")}</td>
<td>${r.delay}</td>
<td>${r.cooldown}</td>
<td>
<button onclick="editRule(${i})">Edit</button>
<button onclick="deleteRule(${i})">Del</button>
</td>
</tr>
`;
});
html+=`</table>
<button style="margin-top:10px" onclick="newRule()">
Add Rule
</button>
</div>
`;
document.getElementById("auto_table").innerHTML = html;
}
function newRule(){
  loadSensorsAndActuators().then(()=>{
    startWizard();
    document.getElementById('ruleModal').style.display='flex';
  });
}
function editRule(i){
alert("edit rule "+i);
}
function deleteRule(i){
alert("delete rule "+i);
}
)rawliteral";


const char CardsSettings[] PROGMEM = R"rawliteral(
function sensorCalibCard(s, i, cfg) {
  const minMaxBtns = cfg.hasMinMax ? `<button onclick='setCalib(${i},"min","${s.name}")'>Set 0%</button>
    <button onclick='setCalib(${i},"max","${s.name}")'>Set 100%</button><br>` : '';
  return `<div class='card'>
    <h3>${cfg.label} ${s.name}</h3>
    <p style='margin-left:6px;'>${cfg.icon} <b id='v${i}'>${cfg.format(s.value)}</b></p>
    <input id='ref${i}' placeholder='Value' style='width:90px;margin:0 5px 6px 5px;border-radius:6px;padding:4px'>
    <button onclick='setCalib(${i},"ref","${s.name}")'>Set Ref Val</button><br>
    ${minMaxBtns}
    <button onclick='setCalib(${i},"res","${s.name}")'>Reset</button><br>
    <button onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id='matterBtn${i}' data-name='${s.id}' class='matter-btn ${s.avail ? "on" : "off"}' style='margin-top:10px;'>${s.avail ? 'ENABLED' : 'DISABLED'}</button>
  </div>`;
}

const cardRenderers = {

HUMI: (s, i) => sensorCalibCard(s, i, { icon: '💧', label: 'HUMIDITY', format: v => (v === 255 || v == null) ? 'N/A' : v + ' %', hasMinMax: true }),

LEVE: (s, i) => sensorCalibCard(s, i, { icon: '📊', label: 'LEVEL', format: v => (v === 255 || v == null) ? 'N/A' : v + ' %', hasMinMax: true }),

LUMI: (s, i) => sensorCalibCard(s, i, { icon: '🔆', label: 'LUMINOSITY', format: v => (v === 255 || v == null) ? 'N/A' : (v * 108.9432 / 7074).toFixed(0) + ' lx', hasMinMax: false }),

TEMP: (s, i) => sensorCalibCard(s, i, { icon: '🌡️', label: 'TEMPERATURE', format: v => (v === 255 || v == null) ? 'N/A' : v.toFixed(2) + ' °C', hasMinMax: false }),

PRES: (s, i) => sensorCalibCard(s, i, { icon: '📈', label: 'PRESSURE', format: v => (v === 255 || v == null) ? 'N/A' : v.toFixed(2) + ' kPa', hasMinMax: false }),

GENERIC: (s, i) => sensorCalibCard(s, i, { icon: '🔬', label: 'CUSTOM', format: v => (v === 255 || v == null) ? 'N/A' : Number(v).toFixed(2), hasMinMax: false }),

AIRQ: (s, i) => `<div class='card'>
  <h3>AIR QUALITY ${s.name}</h3>
  <p style='margin-left:6px;'>🍃 <b id='v${i}'>${s.value === 255 || s.value == null ? 'N/A' : s.value == 0 ? 'GOOD' : s.value == 1 ? 'WARN' : s.value == 2 ? 'BAD' : 'N/A'}</b></p>
  <button onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id='matterBtn${i}' data-name='${s.id}' class='matter-btn ${s.avail ? "on" : "off"}' style='margin-top:10px;'>${s.avail ? 'ENABLED' : 'DISABLED'}</button>
</div>`,

RAIN: (s, i) => `<div class='card'>
  <h3>RAIN ${s.name}</h3>
  <p style='margin-left:6px;'>🌧️ <b id='v${i}'>${s.value === 255 || s.value == null ? 'N/A' : s.value ? "YES" : "NO"}</b></p>
  <button onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id='matterBtn${i}' data-name='${s.id}' class='matter-btn ${s.avail ? "on" : "off"}' style='margin-top:10px;'>${s.avail ? 'ENABLED' : 'DISABLED'}</button>
</div>`,

CONTACT: (s, i) => `<div class='card'>
  <h3>CONTACT ${s.name}</h3>
  <p style='margin-left:6px;'>🔒 <b id='v${i}'>${s.value === 255 || s.value == null ? 'N/A' : s.state ? "CLOSED" : "OPEN"}</b></p>
  <button onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id='matterBtn${i}' data-name='${s.id}' class='matter-btn ${s.avail ? "on" : "off"}' style='margin-top:10px;'>${s.avail ? 'ENABLED' : 'DISABLED'}</button>
</div>`,

AIDIG: (s, i) => `<div class='card'>
  <h3>AI DIGITAL ${s.name}</h3>
  <p style='margin-left:6px;'>🤖 <b id='v${i}'>${s.value === 255 || s.value == null ? 'N/A' : s.state ? "ON" : "OFF"}</b></p>
  <button onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id='matterBtn${i}' data-name='${s.id}' class='matter-btn ${s.avail ? "on" : "off"}' style='margin-top:10px;'>${s.avail ? 'ENABLED' : 'DISABLED'}</button>
</div>`,

AIANA: (s, i) => `<div class='card'>
  <h3>AI ANALOG ${s.name}</h3>
  <p style='margin-left:6px;'>🧠 <b id='v${i}'>${s.value === 255 || s.value == null ? 'N/A' : Number(s.value).toFixed(2)}</b></p>
  <button onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id='matterBtn${i}' data-name='${s.id}' class='matter-btn ${s.avail ? "on" : "off"}' style='margin-top:10px;'>${s.avail ? 'ENABLED' : 'DISABLED'}</button>
</div>`,

DIMM: (s, i) => `<div class='card'>
  <h3>DIMMER ${s.name}</h3>
  <p style='margin-left:6px;'>Fade: <b id='v${i}'>${s.fade}</b> ms</p>
  <input id='ref${i}' placeholder='Fade in/out time(ms)' style='width:122px;margin:0 5px 12px 5px;border-radius:6px;padding:4px'>
  <button onclick='setCalib(${i},"fad","${s.name}")'>Set Fade</button><br>
  <button onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id='matterBtn${i}' data-name='${s.id}' class='matter-btn ${s.avail ? "on" : "off"}' style='margin-top:10px;'>${s.avail ? 'ENABLED' : 'DISABLED'}</button>
</div>`,

REL: (s, i) => `<div class='card'>
  <h3>RELAY ${s.name}</h3>
  <div style='display:flex;gap:8px;flex-direction:column;align-items:flex-start;'>
    <label><input type='checkbox' id='persistChk${i}' ${s.persist ? 'checked' : ''} onchange='togglePersist(${i},"${s.name}")'>Persistence</label>
    <label><input type='checkbox' id='pulseChk${i}' ${s.pulse ? 'checked' : ''} onchange='togglePulse(${i},"${s.name}")'>Pulse Mode (ms)</label>
    <input id='ref${i}' placeholder='Pulse time(ms)' value='${s.pulse_ms ?? ''}' onchange='setCalib(${i},"pulse","${s.name}",this.value)' style='width:90px;${s.pulse ? '' : 'display:none;'}margin-left:5px;border-radius:6px;padding:4px;margin-top:10px;'><br>
    <button onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id='matterBtn${i}' data-name='${s.id}' class='matter-btn ${s.avail ? "on" : "off"}' style='margin-top:-10px;'>${s.avail ? 'ENABLED' : 'DISABLED'}</button>
  </div>
</div>`,

TIME: (s, i) => `<div class='card'>
  <h3>TIME</h3>
  <p style='margin-left:6px;'><b id='v${i}'>--</b></p>
  <select style='width:80%;margin:5px;margin-left:6px;border-radius:6px;padding:4px' onchange="setCalib(${i},'timezone','TIME',this.value)">
    <option value="-720" ${s.correction==-720?'selected':''}>UTC-12 (Baker Island)</option>
    <option value="-660" ${s.correction==-660?'selected':''}>UTC-11 (Samoa)</option>
    <option value="-600" ${s.correction==-600?'selected':''}>UTC-10 (Hawái)</option>
    <option value="-540" ${s.correction==-540?'selected':''}>UTC-9 (Alaska)</option>
    <option value="-480" ${s.correction==-480?'selected':''}>UTC-8 (Los Angeles)</option>
    <option value="-420" ${s.correction==-420?'selected':''}>UTC-7 (Denver)</option>
    <option value="-360" ${s.correction==-360?'selected':''}>UTC-6 (Ciudad de México)</option>
    <option value="-300" ${s.correction==-300?'selected':''}>UTC-5 (Bogotá / Lima)</option>
    <option value="-240" ${s.correction==-240?'selected':''}>UTC-4 (Santiago)</option>
    <option value="-180" ${s.correction==-180?'selected':''}>UTC-3 (Argentina)</option>
    <option value="-120" ${s.correction==-120?'selected':''}>UTC-2 (Atlántico Sur)</option>
    <option value="-60"  ${s.correction==-60?'selected':''}>UTC-1 (Azores)</option>
    <option value="0" ${s.correction==0?'selected':''}>UTC (Londres)</option>
    <option value="60"  ${s.correction==60?'selected':''}>UTC+1 (Madrid / París)</option>
    <option value="120" ${s.correction==120?'selected':''}>UTC+2 (Atenas)</option>
    <option value="180" ${s.correction==180?'selected':''}>UTC+3 (Moscú)</option>
    <option value="240" ${s.correction==240?'selected':''}>UTC+4 (Dubái)</option>
    <option value="300" ${s.correction==300?'selected':''}>UTC+5 (Karachi)</option>
    <option value="330" ${s.correction==330?'selected':''}>UTC+5:30 (India)</option>
    <option value="360" ${s.correction==360?'selected':''}>UTC+6 (Daca)</option>
    <option value="420" ${s.correction==420?'selected':''}>UTC+7 (Bangkok)</option>
    <option value="480" ${s.correction==480?'selected':''}>UTC+8 (Pekín)</option>
    <option value="540" ${s.correction==540?'selected':''}>UTC+9 (Tokio)</option>
    <option value="570" ${s.correction==570?'selected':''}>UTC+9:30 (Adelaida)</option>
    <option value="600" ${s.correction==600?'selected':''}>UTC+10 (Sídney)</option>
    <option value="660" ${s.correction==660?'selected':''}>UTC+11 (Islas Salomón)</option>
    <option value="720" ${s.correction==720?'selected':''}>UTC+12 (Auckland)</option>
    <option value="765" ${s.correction==765?'selected':''}>UTC+12:45 (Chatham)</option>
  </select>
</div>`,

DEFAULT: (s, i) => `<div class='card'>
  <h3>GENERAL SETTINGS</h3>
  <p style='margin-left:6px;margin-bottom:0;'>UDP Ports:</p>
  <p style='margin-left:6px;margin-top:1px'>Broadcast: <b>${genset.broadcast_port}</b> | Command: <b>${genset.command_port}</b></p>
  <p style='margin-left:6px;'>Report Interval: <b>${genset.report_interval} ms</b></p>
  <input id='broadcast_port' placeholder='Broadcast Port' style='width:101px;margin:5px;margin-left:6px;border-radius:6px;padding:4px'>
  <input id='command_port' placeholder='Command Port' style='width:103px;margin:5px;border-radius:6px;padding:4px'>
  <input id='ref${i}' placeholder='Report Interval(ms)' style='width:127px;margin:5px;border-radius:6px;padding:4px'>
  <label style='display:block;margin-left:6px;margin-top:6px;'><input type='checkbox' id='otaChk' onchange='toggleOta(this.checked)'> Arduino OTA</label>
  <div style='display:flex;justify-content:space-between;align-items:center;margin-top:10px;'>
    <button onclick='setPort(${i})' style='margin:10px;margin-bottom:9px;'>Save</button>
    <button onclick='factoryReset()' style='background:#bd1313;margin-bottom:9px;'>Factory Reset</button>
  </div>
</div>`
};
)rawliteral";


const char DeviceCards[] PROGMEM = R"rawliteral(

const SensorType = Object.freeze({
  SENSOR_NONE: 0,
  SENSOR_LUMI: 1,
  SENSOR_HUMI: 2,
  SENSOR_TEMP: 3,
  SENSOR_PRESS: 4,
  SENSOR_LEVEL: 5,
  SENSOR_AIRQ: 6,
  SENSOR_RAIN: 7,
  TYPE_DIMMER: 8,
  TYPE_RELAY: 9,
  SENSOR_TIME: 10,
  SENSOR_GENERIC: 11,
  SENSOR_CONTACT: 12,
  SENSOR_AIDIG: 13,
  SENSOR_AIANA: 14
});

const TYPE_ORDER = {
  [SensorType.SENSOR_TIME]: 0,
  [SensorType.TYPE_RELAY]: 10,
  [SensorType.TYPE_DIMMER]: 11,
  [SensorType.SENSOR_TEMP]: 20,
  [SensorType.SENSOR_HUMI]: 21,
  [SensorType.SENSOR_PRESS]: 22,
  [SensorType.SENSOR_LUMI]: 23,
  [SensorType.SENSOR_LEVEL]: 24,
  [SensorType.SENSOR_AIRQ]: 25,
  [SensorType.SENSOR_RAIN]: 26,
  [SensorType.SENSOR_CONTACT]: 27,
  [SensorType.SENSOR_GENERIC]: 28,
  [SensorType.SENSOR_AIDIG]: 29,
  [SensorType.SENSOR_AIANA]: 30
};

function visualSort(devices) {
  return [...devices].sort((a, b) => {
    const orderA = TYPE_ORDER[a.type] ?? 999;
    const orderB = TYPE_ORDER[b.type] ?? 999;
    if (orderA !== orderB) {
      return orderA - orderB;
    }
    return (a.name || '').localeCompare(b.name || '');
  });
}

function formatTime(s) {
  if (!s) return 'N/A';
  const pad = n => String(n).padStart(2, '0');
  if (s.value == null) return 'N/A';
  const offsetMin = s.correction ?? 0;
  const t = new Date((s.value + offsetMin * 60) * 1000);
  return `
    <div>📅 ${t.getUTCFullYear()}-${pad(t.getUTCMonth()+1)}-${pad(t.getUTCDate())}</div>
    <div>🕒 ${pad(t.getUTCHours())}:${pad(t.getUTCMinutes())}:${pad(t.getUTCSeconds())}</div>
  `;
}

function deviceCard(name, value, id, state, fade, type, sensor = null) {
  if (type === SensorType.TYPE_RELAY) {
    return `<div class='card' data-name='${name}' data-type='${type}' style='text-align:center'>
      <h3>RELAY ${name}</h3>
      <p>⚡ <b id='dev_${id}'>${state ? 'ON' : 'OFF'}</b></p>
      <button onclick="toggleDevice(${id})" style="margin-top:6px;display:inline-block;margin-right:8px">Toggle</button>
    </div>`;
  }
  if (type === SensorType.TYPE_DIMMER) {
    const displayValue = state ? value : 0;
    return `<div class='card' data-name='${name}' data-type='${type}' style='text-align:center'>
      <h3>DIMMER ${name}</h3>
      <p>📊 <b id='dev_val_${id}'>${displayValue}</b> %</p>
      <p>⚡ <b id='dev_state_${id}'>${state ? 'ON' : 'OFF'}</b></p>
      <input type='range' min='0' max='100' name='${name}' value='${displayValue}' id='slider_${id}' style='margin-bottom:18px' oninput='onDimmerInput(${id}, this.value)' onchange='onDimmerChange(${id}, this.value)'>
      <button onclick="toggleDevice(${id})" style="margin-top:6px;display:inline-block;margin-right:8px">Toggle</button>
    </div>`;
  }
  if (type === SensorType.SENSOR_TIME) {
    return `<div class='card time-card' style='text-align:center'>
      <h3>TIME</h3>
      <p><b id='dev_${id}'>${formatTime(sensor || {value})}</b></p>
    </div>`;
  }
  const SENSOR_DISPLAY = {
    [SensorType.SENSOR_TEMP]:  { icon: '🌡️', label: 'TEMPERATURE', format: v => v.toFixed(2) + ' °C' },
    [SensorType.SENSOR_HUMI]:  { icon: '💧', label: 'HUMIDITY', format: v => v.toFixed(0) + ' %' },
    [SensorType.SENSOR_PRESS]: { icon: '📈', label: 'PRESSURE', format: v => v.toFixed(0) + ' kPa' },
    [SensorType.SENSOR_LEVEL]: { icon: '📊', label: 'LEVEL', format: v => v.toFixed(0) + ' %' },
    [SensorType.SENSOR_AIRQ]:  { icon: '🍃', label: 'AIR QUALITY', format: v => v == 0 ? 'GOOD' : v == 1 ? 'WARN' : v == 2 ? 'BAD' : 'N/A' },
    [SensorType.SENSOR_RAIN]:  { icon: '🌧️', label: 'RAIN', format: v => v ? 'YES' : 'NO' },
    [SensorType.SENSOR_LUMI]:  { icon: '🔆', label: 'LUMINOSITY', format: v => (v * 108.9432 / 7074).toFixed(0) + ' lx' },
    [SensorType.SENSOR_GENERIC]: { icon: '🔬', label: 'CUSTOM', format: v => Number(v).toFixed(2) },
    [SensorType.SENSOR_CONTACT]: { icon: '🔒', label: 'CONTACT', format: (v,s) => s ? 'CLOSED' : 'OPEN' },
    [SensorType.SENSOR_AIDIG]:   { icon: '🤖', label: 'AI DIGITAL', format: (v,s) => s ? 'ON' : 'OFF' },
    [SensorType.SENSOR_AIANA]:   { icon: '🧠', label: 'AI ANALOG', format: v => Number(v).toFixed(2) },
  };
  const cfg = SENSOR_DISPLAY[type];
  if (cfg) {
    const dv = (value === 255 || value == null) ? 'N/A' : cfg.format(value, state);
    return `<div class='card' style='text-align:center'>
      <h3>${cfg.label} ${name}</h3>
      <p><b id='dev_${id}'>${cfg.icon} ${dv}</b></p>
    </div>`;
  }
  return `<div class='card' style='text-align:center'>
    <h3>${name}</h3>
    <p><b id='dev_${id}'>⚙️ ${value}</b></p>
  </div>`;
}
)rawliteral";


const char JS[] PROGMEM = R"rawliteral(

/* -------------------- DEVICES -------------------- */

let calibPromise = null;
let sensors = [];

async function getCalib(force = false) {
  if (!force && Array.isArray(sensors) && sensors.length) {
    return sensors;
  }
  if (calibPromise) {
    return calibPromise;
  }
  calibPromise = (async () => {
    const r = await fetch('/calib');
    if (!r.ok) {
      throw new Error(`GET /calib failed: ${r.status}`);
    }
    const data = await r.json();
    sensors = Array.isArray(data) ? data : [];
    return sensors;
  })().finally(() => {
    calibPromise = null;
  });
  return calibPromise;
}

async function loadDevices() {
  try {
    const data = await getCalib(true);
    let mobile = '';
    let actuators = '';
    let deviceSensors = '';
    visualSort(data).forEach((s, i) => {
      const card = deviceCard(s.name, s.value, s.id, s.state, s.fade, s.type, s);
      mobile += card;
      if (s.type === SensorType.TYPE_RELAY || s.type === SensorType.TYPE_DIMMER) {
        actuators += card;
      } else {
        deviceSensors += card;
      }
    });
    const html = `
      <div class='devices-mobile-list'>${mobile}</div>
      <div class='devices-desktop-layout'>
        <div class='devices-column devices-actuators'>
          <div class='devices-section-title'>Actuators</div>
          <div class='devices-actuator-grid'>${actuators || "<div class='card' style='text-align:center'>No actuators</div>"}</div>
        </div>
        <div class='devices-column devices-sensors'>
          <div class='devices-section-title'>Sensors</div>
          <div class='devices-sensor-grid'>${deviceSensors || "<div class='card' style='text-align:center'>No sensors</div>"}</div>
        </div>
      </div>
    `;
    const root = document.getElementById('devices_cards');
    root.className = 'devices-dashboard';
    root.innerHTML = html;
  } catch (e) {
    console.log('loadDevices err', e);
  }
}

/* -------------------- CALIB -------------------- */

async function loadCalib() {
  try {
    const data = await getCalib(true);
    let html = "<div class='settings-grid'>";
    data.forEach((s, i) => {
      const render =
        s.type === SensorType.TYPE_RELAY ? cardRenderers.REL :
        s.type === SensorType.TYPE_DIMMER ? cardRenderers.DIMM :
        s.type === SensorType.SENSOR_TEMP ? cardRenderers.TEMP :
        s.type === SensorType.SENSOR_LUMI ? cardRenderers.LUMI :
        s.type === SensorType.SENSOR_PRESS ? cardRenderers.PRES :
        s.type === SensorType.SENSOR_RAIN ? cardRenderers.RAIN :
        s.type === SensorType.SENSOR_AIRQ ? cardRenderers.AIRQ :
        s.type === SensorType.SENSOR_LEVEL ? cardRenderers.LEVE :
        s.type === SensorType.SENSOR_GENERIC ? cardRenderers.GENERIC :
        s.type === SensorType.SENSOR_CONTACT ? cardRenderers.CONTACT :
        s.type === SensorType.SENSOR_TIME ? cardRenderers.TIME :
        s.type === SensorType.SENSOR_HUMI ? cardRenderers.HUMI :
        cardRenderers[s.name] ?? cardRenderers.DEFAULT;
      html += render(s, i);
    });
    html += `
      <div class="settings-general">
        ${cardRenderers.DEFAULT(
          { value: 0, min: 0, max: 0 },
          data.length
        )}
      </div>
    `;
    html += "</div>";
    document.getElementById('cards').innerHTML = html;
  } catch (e) {
    console.log('loadCalib err', e);
  }
}

async function updateSettingsValues() {
  try {
    const data = await getCalib(false);
    data.forEach((s, i) => {
      const el = document.getElementById(`v${i}`);
      if (!el) return;
      if (s.type === SensorType.SENSOR_TEMP)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : s.value.toFixed(2) + ' °C';
      else if (s.type === SensorType.SENSOR_HUMI)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : s.value.toFixed(0) + ' %';
      else if (s.type === SensorType.SENSOR_PRESS)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : s.value.toFixed(0) + ' kPa';
      else if (s.type === SensorType.SENSOR_RAIN)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : s.value ? "YES" : "NO";
      else if (s.type === SensorType.SENSOR_AIRQ)
        el.innerText = s.value === 255 || s.value == null ? 'N/A' : s.value == 0 ? 'GOOD' : s.value == 1 ? 'WARN' : s.value == 2 ? 'BAD' : 'N/A';
      else if (s.type === SensorType.SENSOR_LEVEL)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : s.value.toFixed(0) + ' %';
      else if (s.type === SensorType.TYPE_DIMMER)
        el.innerText = s.fade;
      else if (s.type === SensorType.SENSOR_LUMI)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : (s.value * 108.9432 / 7074).toFixed(0) + ' lx';
      else if (s.type === SensorType.SENSOR_GENERIC)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : Number(s.value).toFixed(2);
      else if (s.type === SensorType.SENSOR_CONTACT)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : s.state ? "CLOSED" : "OPEN";
      else if (s.type === SensorType.SENSOR_TIME)
        el.innerHTML = formatTime(s);
      else
        el.innerText = s.value ?? '-';
    });
  } catch (e) {
    console.log('updateSettingsValues err', e);
  }
}

/* -------------------- REMOTE ACTIONS -------------------- */

async function isVirtual(id, path, body = null) {
  const sensor = sensors.find(s => s.id == id);
  if (!sensor || sensor.local || sensor.type === SensorType.SENSOR_TIME)
    return false;
  await fetch(`http://${sensor.ip}${path}`, {
    method:'POST',
    headers:{
      'Content-Type':
        'application/x-www-form-urlencoded'
    },
    body
  });
  return true;
}

/* -------------------- ACTIONS -------------------- */

async function toggleMatterSwitch(i, id, name) {
  const btn =
    document.getElementById(`matterBtn${i}`);
  const on =
    btn.classList.toggle('on');
  btn.classList.toggle('off', !on);
  btn.textContent =
    on ? 'ENABLED' : 'DISABLED';
  const body =
    `id=${encodeURIComponent(id)}` +
    `&type=avail` +
    `&ref=${encodeURIComponent(on ? 1 : 0)}`;
  if (await isVirtual(id, '/calib/set', body))
    return;
  await fetch('/calib/set', {
    method: 'POST',
    headers: {
      'Content-Type':
        'application/x-www-form-urlencoded'
    },
    body
  });
}

async function setPort(i) {
  const b = document.getElementById('broadcast_port').value;
  const c = document.getElementById('command_port').value;
  const r = document.getElementById(`ref${i}`).value;
  await fetch('/genset/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: `broadcast=${b}&command=${c}&interval=${r}`
  });
  alert('Guardado');
}

async function setCalib(i, type, name, refOverride = null) {
  const sensor = sensors[i];
  if (!sensor)
    return;
  const ref = refOverride !== null
    ? refOverride
    : (document.getElementById(`ref${i}`)?.value ?? '');
  const body =
    `id=${encodeURIComponent(sensor.id)}` +
    `&type=${encodeURIComponent(type)}` +
    `&ref=${encodeURIComponent(ref)}`;
  if (await isVirtual(sensor.id, '/calib/set', body))
    return;
  await fetch('/calib/set', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body
  });
}

function togglePersist(i, name) {
  const persist = document.getElementById(`persistChk${i}`);
  const pulse   = document.getElementById(`pulseChk${i}`);
  const input   = document.getElementById(`ref${i}`);
  if (persist.checked) {
    pulse.checked = false;
    input.style.display = 'none';
    setCalib(i, 'pulse', name, 0);
  }
  setCalib(i, 'persist', name, persist.checked ? 1 : 0);
}

function togglePulse(i, name) {
  const pulse   = document.getElementById(`pulseChk${i}`);
  const persist = document.getElementById(`persistChk${i}`);
  const input   = document.getElementById(`ref${i}`);
  if (pulse.checked) {
    persist.checked = false;
    setCalib(i, 'persist', name, 0);
    input.style.display = 'inline-block';
  } else {
    input.style.display = 'none';
    setCalib(i, 'pulse',  name, 0);
  }
}

async function toggleDevice(id) {
  const body =
    'id=' + encodeURIComponent(id);
  if (await isVirtual(id, '/toggle', body)) {
    setTimeout(loadDevices, 100);
    return;
  }
  await fetch('/toggle', {
    method: 'POST',
    headers: {
      'Content-Type':
        'application/x-www-form-urlencoded'
    },
    body
  });
  setTimeout(loadDevices, 100);
}

function onDimmerInput(id, value) {
  document.getElementById('dev_val_' + id).innerText = value;
}

const dimmerTimeouts = {};

function onDimmerChange(id, value) {
  if (dimmerTimeouts[id]) {
    clearTimeout(dimmerTimeouts[id]);
  }

  dimmerTimeouts[id] = setTimeout(() => {
    sendDimmer(id, value);
  }, 120);
}

async function sendDimmer(id, value) {
  const body =
    `id=${id}&value=${value}`;
  try {
    if (await isVirtual(id, '/dimmer', body)) {
      setTimeout(loadDevices, 100);
      return;
    }
    await fetch('/dimmer', {
      method: 'POST',
      headers: {
        'Content-Type':
          'application/x-www-form-urlencoded'
      },
      body
    });
    setTimeout(loadDevices, 100);
  } catch (e) {
    console.log('sendDimmer err', e);
  }
}

function factoryReset() {
  if (!confirm('¿Sure? This will delete all settings and information.')) return;
  fetch('/factory', { method: 'POST' })
    .then(() => alert('Reiniciando...'))
    .catch(() => alert('Error enviando reset'));
}

async function toggleOta(enabled) {
  try {
    await fetch('/ota/toggle?enabled=' + (enabled ? 1 : 0));
    alert('OTA ' + (enabled ? 'enabled' : 'disabled'));
  } catch(e) {
    console.log('toggleOta err', e);
  }
}

function setBackground(color){
  document.documentElement.style
    .setProperty('--bg', color);
  const rgb = parseInt(color.slice(1),16);
  const r = (rgb >> 16) & 255;
  const g = (rgb >> 8) & 255;
  const b = rgb & 255;
  const brightness =
    (r*299 + g*587 + b*114) / 1000;
  if(brightness > 140){
    document.documentElement.style
      .setProperty('--text','#111');
    document.documentElement.style
      .setProperty('--card','#ffffff96');
    document.documentElement.style
      .setProperty('--panel','#e5e5e58f');
  }else{
    document.documentElement.style
      .setProperty('--text','#ffffffc4');
    document.documentElement.style
      .setProperty('--card','#0000007d');
    document.documentElement.style
      .setProperty('--panel','#93939357');
  }
  localStorage.setItem('bgColor', color);
}

document.querySelectorAll('.themeDot').forEach(dot =>
  dot.onclick = () => setBackground(dot.dataset.bg)
);

const savedBg = localStorage.getItem('bgColor');

if(savedBg){
  setBackground(savedBg);
}

function lighten(hex, percent){
  let num = parseInt(hex.slice(1),16);
  let r = (num >> 16) + percent;
  let g = ((num >> 8) & 255) + percent;
  let b = (num & 255) + percent;
  r = Math.min(255,r);
  g = Math.min(255,g);
  b = Math.min(255,b);
  return '#' +
    ((1<<24)+(r<<16)+(g<<8)+b)
    .toString(16)
    .slice(1);
}

function darken(hex, percent){
  let num = parseInt(hex.slice(1),16);
  let r = (num >> 16) - percent;
  let g = ((num >> 8) & 255) - percent;
  let b = (num & 255) - percent;
  r = Math.max(0,r);
  g = Math.max(0,g);
  b = Math.max(0,b);
  return '#' +
    ((1<<24)+(r<<16)+(g<<8)+b)
    .toString(16)
    .slice(1);
}

var logTimer = null;

async function refreshLogs(){
  try {
    const r = await fetch('/logs');
    if(!r.ok) return;
    const logs = await r.json();
    renderLogs(logs);
  } catch(e){
    console.log('refreshLogs err', e);
  }
}

function renderLogs(logs){
  const coreEl = document.getElementById('log-core');
  const evntEl = document.getElementById('log-events');
  const sensEl = document.getElementById('log-sensors');
  coreEl.innerHTML = '<h3>Core</h3>';
  evntEl.innerHTML = '<h3>Events</h3>';
  sensEl.innerHTML = '<h3>Sensors / Warn / Error</h3>';
  logs.forEach(e => {
    const el = document.createElement('div');
    el.className = 'log-entry ' + (e.l === 'CORE' ? 'core' : e.l === 'EVNT' ? 'evnt' : 'sens');
    el.innerHTML = `<span class="t">${e.t}</span><span class="l ${e.v.toLowerCase()}">${e.v}</span>${e.m}`;
    if(e.l === 'CORE') coreEl.appendChild(el);
    else if(e.l === 'EVNT') evntEl.appendChild(el);
    else sensEl.appendChild(el);
  });
  // Also show WARN and ERROR in the third panel
  logs.forEach(e => {
    if(e.v === 'WRN' || e.v === 'ERR'){
      const el = document.createElement('div');
      el.className = 'log-entry sens';
      el.innerHTML = `<span class="t">${e.t}</span><span class="l ${e.v.toLowerCase()}">${e.v}</span><span class="l">${e.l}</span> ${e.m}`;
      sensEl.appendChild(el);
    }
  });
}

function startLogAutoRefresh(){
  if(logTimer) clearInterval(logTimer);
  logTimer = setInterval(refreshLogs, 2000);
}

function stopLogAutoRefresh(){
  if(logTimer){ clearInterval(logTimer); logTimer = null; }
}

)rawliteral";


const char AutoWizJS[] PROGMEM = R"rawliteral(

/*--------------------------------------------------- WIZARD AUTOMATIONS ------------------------------------------------------------------------*/

let wizard={step:0,data:{sensors:[],actuators:[],type:0,logic:1,delay:0,cooldown:0,interval:0,actions:[],levels:[],conditions:{},time_hour:0,time_minute:0,date_start:'',date_end:''}};
let availableSensors=[];

function sensorByIndex(index){
  return availableSensors.find(s => s.index === index);
}

async function loadSensorsAndActuators(){
  const r = await fetch('/calib');
  availableSensors = await r.json();
}

function startWizard(edit=-1){
  wizard={step:0,data:{sensors:[],actuators:[],type:0,logic:1,delay:0,cooldown:0,interval:0,actions:[],levels:[],conditions:{},time_hour:0,time_minute:0,date_start:'',date_end:''}};
  if(edit>=0 && window.rules && window.rules[edit]) {
    const rule = window.rules[edit];
    wizard.data = {
      id: rule.id,
      sensors: rule.sensors || [],
      actuators: rule.actuators || [],
      type: rule.type || 0,
      logic: rule.logical_and ? 1 : 0,
      delay: rule.delay_ms || 0,
      cooldown: rule.cooldown_ms || 0,
      interval: rule.interval_ms || 0,
      actions: Array.isArray(rule.actions) ? [...rule.actions] : [rule.actions || 0],
      levels: Array.isArray(rule.levels) ? [...rule.levels] : [rule.levels || 0],
      conditions: {},
      time_hour: Math.floor((rule.time_s || 0) / 3600),
      time_minute: Math.floor(((rule.time_s || 0) % 3600) / 60),
      date_start: `${rule.year_start}-${String(rule.month_start).padStart(2,'0')}-${String(rule.day_start).padStart(2,'0')}`,
      date_end: `${rule.year_end}-${String(rule.month_end).padStart(2,'0')}-${String(rule.day_end).padStart(2,'0')}`
    };
    
    if(rule.cmp && rule.threshold) {
      rule.sensors.forEach((sensorIdx, posIdx) => {
        wizard.data.conditions[sensorIdx] = {
          cmp: rule.cmp[posIdx] || 0,
          threshold: rule.threshold[posIdx] || 0
        };
      });
    }
  }
  showStep(0);
}

function getRelevantSteps(){
  const steps = [0];
  
  if(wizard.data.type === 0 || wizard.data.type === 1) {
    steps.push(1, 2);
    // ✅ SOLO agregar paso 3 (lógica) si hay MÁS DE UN sensor
    if(wizard.data.sensors.length > 1) {
      steps.push(3);
    }
  }
  
  if(wizard.data.type === 2) {
    // TIME - ir directo a actuadores
    steps.push(3);
  }
  
  steps.push(4, 5);
  
  if(wizard.data.type === 0 || wizard.data.type === 1) {
    steps.push(6);
  } else if(wizard.data.type === 3) {
    steps.push(6);
  }
  
  return steps;
}

function getTotalSteps(){
  return getRelevantSteps().length;
}

function getStepNumber(globalStep){
  const steps = getRelevantSteps();
  return steps[globalStep] ?? globalStep;
}

function showStep(n){
  const steps = getRelevantSteps();
  if(n >= steps.length) return;

  wizard.step = n;
  const stepNum = steps[n];

  let content = '';

  /* ================= STEP 0 ================= */
  if(stepNum === 0) {
    content = `<h3>Tipo de Regla</h3>
    <div style="display:flex;flex-direction:column;gap:12px">

       ${[0,1,2,3].map(t=>{
         const cfg = [
           {c:'#27ae60',txt:'🔄 EDGE - Cambios de estado',desc:'Se ejecuta cuando un sensor cambia'},
           {c:'#2980b9',txt:'📊 THRESHOLD - Valores límite',desc:'Se ejecuta por umbral'},
           {c:'#e67e22',txt:'⏰ TIME - A una hora',desc:'Ejecuta a una hora fija'},
           {c:'#9b59b6',txt:'⏱️ INTERVAL - Cada X tiempo',desc:'Ejecuta periódicamente'}
         ][t];

         return `
         <label style="display:flex;padding:10px;border:2px solid ${wizard.data.type===t?cfg.c:'rgba(255,255,255,.15)'};border-radius:6px;cursor:pointer;background:var(--card)">
           <input type="radio" name="type" value="${t}" ${wizard.data.type===t?'checked':''} style="margin-right:10px">
           <div>
             <strong>${cfg.txt}</strong>
             <small style="color:var(--text);opacity:.6;display:block">${cfg.desc}</small>
           </div>
         </label>`;
       }).join('')}

    </div>`;
  }

  /* ================= STEP 1 ================= */
  else if(stepNum === 1) {
    content = `<h3>Seleccionar Sensores</h3>
      <select id="sensorList" multiple size="5" style="width:100%"></select>
      <small>Ctrl/Cmd + Click</small>`;
  }

  /* ================= STEP 2 ================= */
  else if(stepNum === 2) {

    if(wizard.data.sensors.length === 0){
      content = `<h3>⚠️ Sin sensores</h3>`;
    } else {

      content = `<h3>Condiciones</h3>`;

      wizard.data.sensors.forEach(sIdx=>{
        const s = sensorByIndex(sIdx);
        if(!s) return;
        const cond = wizard.data.conditions[sIdx] || {cmp:0,threshold:0};

        const val = (s.value === 255 || s.value == null)
          ? 'N/A'
          : (
              s.type === SensorType.SENSOR_TEMP  ? s.value.toFixed(2) + ' °C' :
              s.type === SensorType.SENSOR_HUMI  ? s.value.toFixed(0) + ' %' :
              s.type === SensorType.SENSOR_PRESS ? s.value.toFixed(0) + ' kPa' :
              s.type === SensorType.SENSOR_LEVEL ? s.value.toFixed(0) + ' %' :
              s.type === SensorType.SENSOR_LUMI  ? (s.value * 108.9432 / 7074).toFixed(0) + ' lx' :
              s.type === SensorType.SENSOR_AIRQ  ? (s.value==0?'GOOD':s.value==1?'WARN':s.value==2?'BAD':'N/A') :
              s.type === SensorType.SENSOR_RAIN  ? (s.value ? 'YES' : 'NO') :
              s.type === SensorType.SENSOR_CONTACT  ? (s.state ? 'CLOSED' : 'OPEN') :
              s.type === SensorType.SENSOR_GENERIC  ? Number(s.value).toFixed(2) :
              s.value
            );

        content += `
        <div style="border:1px solid var(--text);padding:10px;margin:6px;border-radius:6px">
          <b>${s.name}</b>
          <span style="float:right;font-weight:bold;color:var(--text);opacity:.6">${val}</span><br>
          ${wizard.data.type === 0 ? `
            <select id="cmp_${sIdx}">
              <option value="0" ${cond.cmp===0?'selected':''}>RISING</option>
              <option value="1" ${cond.cmp===1?'selected':''}>FALLING</option>
            </select>
          ` : `
            <select id="cmp_${sIdx}">
              <option value="0" ${cond.cmp===0?'selected':''}>> </option>
              <option value="1" ${cond.cmp===1?'selected':''}>< </option>
              <option value="2" ${cond.cmp===2?'selected':''}>= </option>
            </select>
            <input id="thresh_${sIdx}" type="number" step="any" value="${cond.threshold}">
          `}
        </div>`;
      });
    }
  }

  /* ================= STEP 3 ================= */
   else if(stepNum === 3) {

     if(wizard.data.type === 2) {
       // TIME - Ahora con date pickers
       content = `<h3>⏰ Rango de Fechas y Hora</h3>
       <div style="margin-bottom:12px">
         <label style="display:block;margin-bottom:6px"><strong>Desde cuándo:</strong></label>
         <input id="date_start" type="date" value="${wizard.data.date_start}" style="width:100%;padding:6px;border-radius:6px;border:1px solid var(--text)">
       </div>
       <div style="margin-bottom:12px">
         <label style="display:block;margin-bottom:6px"><strong>Hasta cuándo:</strong></label>
         <input id="date_end" type="date" value="${wizard.data.date_end}" style="width:100%;padding:6px;border-radius:6px;border:1px solid var(--text)">
       </div>
       <div style="border-top:1px solid var(--text);padding-top:12px;margin-top:12px">
         <label style="display:block;margin-bottom:6px"><strong>Hora de ejecución:</strong></label>
         <div style="display:flex;gap:8px">
           <input id="time_hour" type="number" min="0" max="23" value="${wizard.data.time_hour}" placeholder="Hs" style="flex:1;padding:6px;border-radius:6px;border:1px solid var(--text)">
           <input id="time_minute" type="number" min="0" max="59" value="${wizard.data.time_minute}" placeholder="Min" style="flex:1;padding:6px;border-radius:6px;border:1px solid var(--text)">
         </div>
       </div>`;
     } else if(wizard.data.sensors.length > 1) {
       // LOGIC
       content = `<h3>Lógica</h3>
       <label><input type="radio" name="logic" value="1" ${wizard.data.logic===1?'checked':''}> AND</label>
       <label><input type="radio" name="logic" value="0" ${wizard.data.logic===0?'checked':''}> OR</label>`;
     }
   }

  /* ================= STEP 4 ================= */
  else if(stepNum === 4) {
    content = `<h3>Actuadores</h3>
      <select id="actuatorList" multiple size="5" style="width:100%"></select>`;
  }

  /* ================= STEP 5 ================= */
  else if(stepNum === 5) {

    if(wizard.data.actuators.length === 0){
      content = `<h3>⚠️ Sin actuadores</h3>`;
    } else {

      content = `<h3>Acciones</h3>`;

      wizard.data.actuators.forEach((aIdx,i)=>{
        const a = sensorByIndex(aIdx);
        if(!a) return;
        const action = wizard.data.actions[i] || 0;
        const level = wizard.data.levels[i] || 0;

        const state = (a.type === 9)
          ? (a.state ? 'ON' : 'OFF')
          : (a.type === 8)
            ? `${a.value}%`
            : '-';

        content += `
        <div style="border:1px solid var(--text);padding:8px;margin:5px;border-radius:6px">
          <b>${a.name}</b>
          <span style="float:right;font-weight:bold;color:var(--text);opacity:.6">${state}</span><br>
          <select id="action_${i}">
            <option value="0" ${action===0?'selected':''}>ON</option>
            <option value="1" ${action===1?'selected':''}>OFF</option>
            <option value="2" ${action===2?'selected':''}>TOGGLE</option>
            ${a.type===8?`<option value="3" ${action===3?'selected':''}>LEVEL</option>`:''}
          </select>
          ${a.type===8?`<input id="level_${i}" type="number" min="0" max="100" value="${level}" style="${action===3?'':'display:none;'}"`:''}
        </div>`;
      });
      
      // Después de renderizar, setup los listeners para mostrar/ocultar level inputs
      setTimeout(() => setupActionListeners(), 0);
    }
  }

  /* ================= STEP 6 ================= */
  else if(stepNum === 6) {

    if(wizard.data.type === 3){
      content = `<h3>Intervalo</h3>
      <input id="interval" type="number" value="${wizard.data.interval||1000}">`;
    }

    content += `
      <h3>Delay</h3>
      <input id="delay" type="number" value="${wizard.data.delay}">
      <h3>Cool Down</h3>
      <input id="cooldown" type="number" value="${wizard.data.cooldown}">`;
  }

  /* ================= RENDER ================= */

  let html = `<div>${content}</div>`;
   html += `<hr style="border:0;border-top:1px solid var(--text);opacity:.3">

  <div style="display:flex;justify-content:space-between">
    ${n>0?'<button onclick="prevStep()">Back</button>':''}
    ${n<steps.length-1?'<button onclick="nextStep()">Next</button>':'<button onclick="finishWizard()">Save</button>'}
  </div>`;

  document.getElementById('wizardContent').innerHTML = html;

  if(stepNum===1) populateSensors();
  if(stepNum===4) populateActuators();
}

function setupActionListeners(){
  wizard.data.actuators.forEach((aIdx, aPos) => {
    const actuator = sensorByIndex(aIdx);
    if(!actuator) return;
    const isDimmer = actuator.type === 8;
    
    if(isDimmer) {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);
      
      if(actionSelect) {
        // Mostrar/ocultar input de level según la opción seleccionada
        const updateLevelVisibility = () => {
          if(levelInput) {
            levelInput.style.display = actionSelect.value === '3' ? 'inline-block' : 'none';
          }
        };
        
        // Ejecutar al cargar
        updateLevelVisibility();
        
        // Listener para cambios
        actionSelect.addEventListener('change', updateLevelVisibility);
      }
    }
  });
}

function validateStep(stepNum) {
  // ✅ STEP 0: Validar que eligió un tipo
  if(stepNum === 0) {
    const typeRadio = document.querySelector('input[name="type"]:checked');
    if(!typeRadio) {
      alert('⚠️ Debes seleccionar un tipo de regla');
      return false;
    }
    return true;
  }
  
  // ✅ STEP 1: Validar que eligió sensores (si es EDGE/THRESHOLD)
  if(stepNum === 1) {
    if((wizard.data.type === 0 || wizard.data.type === 1) && wizard.data.sensors.length === 0) {
      alert('⚠️ Debes seleccionar al menos un sensor para este tipo de regla');
      return false;
    }
    return true;
  }
  
  // ✅ STEP 2: Validar que completó las condiciones
  if(stepNum === 2) {
    if(wizard.data.type === 0 || wizard.data.type === 1) {
      for(let sIdx of wizard.data.sensors) {
        const cmpSelect = document.getElementById(`cmp_${sIdx}`);
        const threshInput = document.getElementById(`thresh_${sIdx}`);
        
        if(wizard.data.type === 1) { // THRESHOLD
          if(!threshInput || threshInput.value === '') {
            alert(`⚠️ Debes completar el threshold para el sensor`);
            return false;
          }
          const threshVal = parseFloat(threshInput.value);
          if(isNaN(threshVal) || threshVal < -1000 || threshVal > 10000) {
            alert(`⚠️ El threshold debe estar entre -1000 y 10000`);
            return false;
          }
        }
      }
    }
    return true;
  }
  
  // ✅ STEP 3: Validar fechas/hora
  if(stepNum === 3) {
    if(wizard.data.type === 2) { // TIME
      const dateStartEl = document.getElementById('date_start');
      const dateEndEl = document.getElementById('date_end');
      const timeHourEl = document.getElementById('time_hour');
      const timeMinEl = document.getElementById('time_minute');
      
      const hour = parseInt(timeHourEl.value) || 0;
      const min = parseInt(timeMinEl.value) || 0;
      
      if(hour < 0 || hour > 23 || min < 0 || min > 59) {
        alert('⚠️ La hora debe estar entre 00:00 y 23:59');
        return false;
      }
      
      if(dateStartEl.value && dateEndEl.value) {
        const start = new Date(dateStartEl.value);
        const end = new Date(dateEndEl.value);
        if(start > end) {
          alert('⚠️ La fecha "desde" no puede ser posterior a "hasta"');
          return false;
        }
      }
    }
    return true;
  }
  
  // ✅ STEP 4: Validar que eligió actuadores
  if(stepNum === 4) {
    if(wizard.data.actuators.length === 0) {
      alert('⚠️ Debes seleccionar al menos un actuador');
      return false;
    }
    return true;
  }
  
  // ✅ STEP 5: Validar acciones
  if(stepNum === 5) {
    for(let aPos = 0; aPos < wizard.data.actuators.length; aPos++) {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);
      
      if(!actionSelect) {
        alert('⚠️ Error al cargar las acciones');
        return false;
      }
      
      const action = parseInt(actionSelect.value);
      const aIdx = wizard.data.actuators[aPos];
      const actuator = sensorByIndex(aIdx);
      if(!actuator) {
        alert('Error al cargar el actuador');
        return false;
      }
      
      // LEVEL solo para dimmers
      if(action === 3 && actuator.type !== 8) {
        alert('⚠️ La acción LEVEL solo se puede usar en dimmers');
        return false;
      }
      
      // Si es LEVEL, validar el valor
      if(action === 3 && levelInput) {
        const level = parseInt(levelInput.value);
        if(isNaN(level) || level < 0 || level > 100) {
          alert('⚠️ El level debe estar entre 0 y 100');
          return false;
        }
      }
    }
    return true;
  }
  
  // ✅ STEP 6: Validar delays/cooldown/interval
  if(stepNum === 6) {
    const delayEl = document.getElementById('delay');
    const cooldownEl = document.getElementById('cooldown');
    const intervalEl = document.getElementById('interval');
    
    const delay = parseInt(delayEl.value) || 0;
    const cooldown = parseInt(cooldownEl.value) || 0;
    
    if(delay < 0 || delay > 60000) {
      alert('⚠️ El delay debe estar entre 0 y 60000 ms');
      return false;
    }
    
    if(cooldown < 0 || cooldown > 3600000) {
      alert('⚠️ El cooldown debe estar entre 0 y 3600000 ms');
      return false;
    }
    
    if(wizard.data.type === 3) {
      const interval = parseInt(intervalEl.value) || 0;
      if(interval < 1000 || interval > 3600000) {
        alert('⚠️ El intervalo debe estar entre 1000 y 3600000 ms');
        return false;
      }
    }
    
    return true;
  }
  
  return true;
}

function nextStep(){
  const steps = getRelevantSteps();
  const stepNum = steps[wizard.step];

  if(stepNum === 1){
    wizard.data.sensors = [...document.querySelectorAll('#sensorList option:checked')]
      .map(o => parseInt(o.value));
  }

  if(stepNum === 4){
    wizard.data.actuators = [...document.querySelectorAll('#actuatorList option:checked')]
      .map(o => parseInt(o.value));
  }
  
  // ✅ Validar el step actual antes de avanzar
  if(!validateStep(stepNum)) {
    return;
  }
  
  if(stepNum === 0) {
    const typeRadio = document.querySelector('input[name="type"]:checked');
    if(typeRadio) wizard.data.type = parseInt(typeRadio.value);
  }
  else if(stepNum === 1) {
    wizard.data.sensors = [...document.querySelectorAll('#sensorList option:checked')].map(o=>parseInt(o.value));
  }
  else if(stepNum === 2) {
    wizard.data.sensors.forEach(sIdx => {
      const cmpSelect = document.getElementById(`cmp_${sIdx}`);
      const threshInput = document.getElementById(`thresh_${sIdx}`);
      if(cmpSelect){
        wizard.data.conditions[sIdx] = {
          cmp: parseInt(cmpSelect.value),
          threshold: threshInput ? parseFloat(threshInput.value) || 0 : 0
        };
      }
    });
  }
  else if(stepNum === 3) {
    if(wizard.data.type === 2) {
      // TIME - guardar hora Y fechas
      wizard.data.date_start = document.getElementById('date_start').value || '';
      wizard.data.date_end = document.getElementById('date_end').value || '';
      wizard.data.time_hour = parseInt(document.getElementById('time_hour').value) || 0;
      wizard.data.time_minute = parseInt(document.getElementById('time_minute').value) || 0;
    } else if(wizard.data.sensors.length > 1) {
      // LÓGICA
      const logicRadio = document.querySelector('input[name="logic"]:checked');
      if(logicRadio) wizard.data.logic = parseInt(logicRadio.value);
    }
  }
  else if(stepNum === 4) {
    const newActuators = [...document.querySelectorAll('#actuatorList option:checked')].map(o=>parseInt(o.value));
    
    if(JSON.stringify(newActuators) !== JSON.stringify(wizard.data.actuators)) {
      wizard.data.actuators = newActuators;
      wizard.data.actions = wizard.data.actuators.map(() => 0);
      wizard.data.levels = wizard.data.actuators.map(() => 0);
    } else {
      wizard.data.actuators = newActuators;
    }
  }
  else if(stepNum === 5) {
    wizard.data.actions = [];
    wizard.data.levels = [];
    
    wizard.data.actuators.forEach((aIdx, aPos) => {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);
      
      const action = actionSelect ? parseInt(actionSelect.value) : 0;
      let level = 0;
      
      if(levelInput) {
        const levelValue = levelInput.value;
        level = levelValue && levelValue.trim() !== '' ? parseInt(levelValue) : 0;
      }
      
      wizard.data.actions[aPos] = action;
      wizard.data.levels[aPos] = level;
    });
  }
  else if(stepNum === 6) {
    wizard.data.delay = parseInt(document.getElementById('delay').value) || 0;
    wizard.data.cooldown = parseInt(document.getElementById('cooldown').value) || 0;
    if(wizard.data.type === 3) {
      wizard.data.interval = parseInt(document.getElementById('interval').value) || 1000;
    }
  }
  
  showStep(wizard.step + 1);
}

function prevStep(){
  if(wizard.step > 0) showStep(wizard.step - 1);
}

function populateSensors(){
  const sel = document.getElementById('sensorList');
  
  let filtered = availableSensors;
  
  if(wizard.data.type === 0) {
    filtered = availableSensors.filter((s,i) => [7, 6, 9, 12].includes(s.type));
  } else if(wizard.data.type === 1) {
    filtered = availableSensors.filter((s,i) => [1, 2, 3, 4, 5, 11].includes(s.type));
  }
  
  sel.innerHTML = filtered.map((s,i)=>{
    const origIdx = s.index;
    return `<option value="${origIdx}">[${origIdx}] ${s.name}</option>`;
  }).join('');
  
  if(wizard.data.sensors && wizard.data.sensors.length > 0) {
    document.querySelectorAll('#sensorList option').forEach(o=>{
      if(wizard.data.sensors.includes(parseInt(o.value))) o.selected=true;
    });
  }
}

function populateActuators(){
  const sel = document.getElementById('actuatorList');
  const actuators = availableSensors.reduce((acc,s,i)=>{
    if(s.type===9 || s.type===8) acc.push({...s, idx:s.index});
    return acc;
  },[]);
  
  sel.innerHTML = actuators.map(s=>`<option value="${s.idx}">[${s.idx}] ${s.name}</option>`).join('');
  
  if(wizard.data.actuators && wizard.data.actuators.length > 0) {
    document.querySelectorAll('#actuatorList option').forEach(o=>{
      if(wizard.data.actuators.includes(parseInt(o.value))) o.selected=true;
    });
  }
}

async function finishWizard(){
  const steps = getRelevantSteps();
  const stepNum = steps[wizard.step];
  
  // ✅ CAPTURAR DATOS PENDIENTES DEL PASO ACTUAL ANTES DE GUARDAR
  if(wizard.data.type === 2 && stepNum === 3) {
    wizard.data.date_start = document.getElementById('date_start').value || '';
    wizard.data.date_end = document.getElementById('date_end').value || '';
    wizard.data.time_hour = parseInt(document.getElementById('time_hour').value) || 0;
    wizard.data.time_minute = parseInt(document.getElementById('time_minute').value) || 0;
  }
  else if(stepNum === 5) {
    // ✅ CAPTURAR ACCIONES si estamos en paso 5
    wizard.data.actions = [];
    wizard.data.levels = [];
    
    wizard.data.actuators.forEach((aIdx, aPos) => {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);
      
      let action = 0;
      if(actionSelect) {
        action = parseInt(actionSelect.value);
      }
      
      let level = 0;
      if(levelInput && levelInput.style.display !== 'none') {
        const levelValue = levelInput.value;
        level = levelValue && levelValue.trim() !== '' ? parseInt(levelValue) : 0;
      }
      
      wizard.data.actions.push(action);
      wizard.data.levels.push(level);
    });
  }
  else if(stepNum === 6) {
    wizard.data.delay = parseInt(document.getElementById('delay').value) || 0;
    wizard.data.cooldown = parseInt(document.getElementById('cooldown').value) || 0;
    if(wizard.data.type === 3) {
      wizard.data.interval = parseInt(document.getElementById('interval').value) || 1000;
    }
  }
  
  // ========== VALIDACIONES FRONTEND ==========
  
  // ✅ Validar: Al menos un actuador
  if(wizard.data.actuators.length === 0) {
    alert('⚠️ Debes seleccionar al menos un actuador');
    return;
  }
  
  // ✅ Validar: EDGE/THRESHOLD requieren sensores
  if((wizard.data.type === 0 || wizard.data.type === 1) && wizard.data.sensors.length === 0) {
    alert('⚠️ Este tipo de regla requiere al menos un sensor');
    return;
  }

  // ✅ Validar: TIME requiere hora válida
  if(wizard.data.type === 2) {
    if(wizard.data.time_hour < 0 || wizard.data.time_hour > 23) {
      alert('⚠️ La hora debe estar entre 0 y 23');
      return;
    }
    if(wizard.data.time_minute < 0 || wizard.data.time_minute > 59) {
      alert('⚠️ Los minutos deben estar entre 0 y 59');
      return;
    }
  }

  // ✅ Validar: Fechas coherentes
  if(wizard.data.type === 2) {
    if(wizard.data.date_start && wizard.data.date_end) {
      const dateStart = new Date(wizard.data.date_start);
      const dateEnd = new Date(wizard.data.date_end);
      if(dateStart > dateEnd) {
        alert('⚠️ La fecha "desde" no puede ser posterior a la fecha "hasta"');
        return;
      }
    }
  }

  // ✅ Validar: Delay razonable
  if(wizard.data.delay < 0 || wizard.data.delay > 60000) {
    alert('⚠️ El delay debe estar entre 0 y 60000 ms');
    return;
  }

  // ✅ Validar: Cooldown razonable
  if(wizard.data.cooldown < 0 || wizard.data.cooldown > 3600000) {
    alert('⚠️ El cooldown debe estar entre 0 y 3600000 ms');
    return;
  }

  // ✅ Validar: INTERVAL requiere intervalo válido
  if(wizard.data.type === 3) {
    if(wizard.data.interval < 1000 || wizard.data.interval > 3600000) {
      alert('⚠️ El intervalo debe estar entre 1000 y 3600000 ms');
      return;
    }
  }

  // ✅ Validar: Levels en dimmers (0-100)
  wizard.data.actuators.forEach((aIdx, aPos) => {
    const level = wizard.data.levels[aPos] || 0;
    if(level < 0 || level > 100) {
      alert(`⚠️ El level del actuador debe estar entre 0 y 100 (actual: ${level})`);
      return;
    }
  });

  // ✅ Validar: Actions válidas para cada actuador
  wizard.data.actuators.forEach((aIdx, aPos) => {
    const action = wizard.data.actions[aPos];
    const actuator = sensorByIndex(aIdx);
    if(!actuator) return;
    
    // LEVEL solo para dimmers
    if(action === 3 && actuator.type !== 8) {
      alert(`⚠️ La acción LEVEL solo se puede usar en dimmers`);
      return;
    }
  });

  let cmps = [], thresholds = [];
  wizard.data.sensors.forEach(sIdx => {
    const cond = wizard.data.conditions[sIdx] || {cmp: 0, threshold: 0};
    cmps.push(cond.cmp);
    thresholds.push(cond.threshold);
  });

  while(wizard.data.levels.length < wizard.data.actuators.length) {
    wizard.data.levels.push(0);
  }

  // ✅ Convertir hora:minuto a segundos para TIME
  let time_s = wizard.data.type === 2 ? wizard.data.time_hour * 3600 + wizard.data.time_minute * 60 : 0;

  // ✅ Parsear fechas para enviar year/month/day (SIN timezone issues)
  let year_start = 0, month_start = 0, day_start = 0;
  let year_end = 0, month_end = 0, day_end = 0;
  
  if(wizard.data.type === 2) {
    if(wizard.data.date_start) {
      const [y, m, d] = wizard.data.date_start.split('-');
      year_start = parseInt(y);
      month_start = parseInt(m);
      day_start = parseInt(d);
    }
    if(wizard.data.date_end) {
      const [y, m, d] = wizard.data.date_end.split('-');
      year_end = parseInt(y);
      month_end = parseInt(m);
      day_end = parseInt(d);
    }
  }

  const params = new URLSearchParams();
  
  params.append('id', wizard.data.id ?? -1);
  params.append('sensors', wizard.data.sensors.join(','));
  params.append('actuators', wizard.data.actuators.join(','));
  params.append('type', wizard.data.type);
  params.append('logic', wizard.data.logic);
  params.append('delay', wizard.data.delay);
  params.append('cooldown', wizard.data.cooldown);
  params.append('interval', wizard.data.interval || 0);
  params.append('actions', wizard.data.actions.join(','));
  params.append('levels', wizard.data.levels.join(','));
  params.append('cmp', cmps.join(','));
  params.append('threshold', thresholds.join(','));
  params.append('time_s', time_s);
  params.append('year_start', year_start);
  params.append('month_start', month_start);
  params.append('day_start', day_start);
  params.append('year_end', year_end);
  params.append('month_end', month_end);
  params.append('day_end', day_end);

  try {
    const res = await fetch('/rules/set', {
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:params.toString()
    });
    
    if(res.ok) {
      console.log('✅ Regla guardada exitosamente');
      alert('✅ Regla guardada correctamente');
      closeRule();
      loadRules();
    } else {
      const errMsg = await res.text();
      console.error('❌ Error del servidor:', errMsg);
      alert(`❌ Error al guardar: ${errMsg}`);
    }
  } catch(e) {
    console.error('Error de red:', e);
    alert(`❌ Error de conexión: ${e.message}`);
  }
}

function editRule(i){
  loadSensorsAndActuators().then(()=>{
    startWizard(i);
    document.getElementById('ruleModal').style.display='flex';
  });
}

function closeRule(){
  document.getElementById('ruleModal').style.display='none';
}

async function deleteRule(i){
  if(!confirm('¿Eliminar regla '+i+'?')) return;
  await fetch('/rules/delete',{
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:`id=${i}`
  });
  loadRules();
}

async function loadRules(){
  try {
    const res = await fetch('/rules');
    if(!res.ok) return;
    const rules = await res.json();
    window.rules = rules;
    
    const table = document.getElementById("auto_table");
    table.innerHTML = '';
    
    rules.forEach((r,i)=>{
      let row = document.createElement("tr");
      row.innerHTML = `
      <td>${r.id}</td>
      <td>${r.sensors.join(",")}</td>
      <td>${['EDGE','THRESHOLD','TIME','INTERVAL'][r.type] || r.type}</td>
      <td>${r.logical_and ? "AND" : "OR"}</td>
      <td>${r.actuators.join(", ")}</td>
      <td>${r.delay_ms}</td>
      <td>${r.cooldown_ms}</td>
      <td style="text-align:center">
        <button onclick="editRule(${r.id})" style="font-size:11px;padding:2px 6px">Edit</button>
        <button onclick="deleteRule(${r.id})" style="font-size:11px;padding:2px 6px;background:#c0392b">Del</button>
      </td>
      `;
      table.appendChild(row);
    });
  } catch(e) {
    console.log('loadRules error', e);
  }
}

loadRules();
loadDevices();
loadCalib();
setInterval(() => {
  loadDevices();
  updateSettingsValues();
}, 5000);
</script>
<div id="ruleModal" class="modal">
  <div class="modal-content" style="width:350px">
    <div id="wizardContent"></div>
    <hr>
    <button onclick="closeRule()" style="float:right;background:#e74c3c">Cancelar</button>
  </div>
</div>
</body>
</html>
)rawliteral";


}
