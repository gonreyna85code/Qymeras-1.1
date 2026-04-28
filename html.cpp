#include "html.h"

namespace html {
// ================= WEB ===================
const char HTML_PAGE_1[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
body{font-family:sans-serif;text-align:center;margin:0;background:#f4f4f4}
.tabs{display:flex;justify-content:space-around;background:#222;color:#fff}
.tab{flex:1;padding:12px;cursor:pointer}
.active{background:#444}
.content{padding:15px}
.card{background:#fff;margin:10px;padding:15px;border-radius:10px;box-shadow:0 2px 6px rgba(0,0,0,0.2);text-align:left;border-left:4px solid #d2d4d5}
.card h3{margin-top:0;text-align:center}
button{padding:6px 12px;border:none;border-radius:6px;background:#333;color:#fff;margin:5px;cursor:pointer}
.matter-btn{padding:6px 12px;border-radius:6px;border:none;font-weight:600;cursor:pointer}
.matter-btn.on{background:#2ecc71;color:#000}
.matter-btn.off{background:#444;color:#bbb}
.matterLbl{margin-left:20px;font-weight:600}
.modal{
position:fixed;
top:0;
left:0;
width:100%;
height:100%;
background:rgba(0,0,0,0.6);
display:none;
align-items:center;
justify-content:center;
z-index:1000;
}
.modal-content{
background:#fff;
padding:20px;
border-radius:10px;
width:320px;
text-align:left;
}
.modal input,
.modal select{
margin-bottom:10px;
padding:6px;
border-radius:6px;
border:1px solid #ccc;
}
input[type=range]{width:100%}
</style></head><body>
<h2 style='background:#222;margin:0;padding:12px;text-align:center;color:#eee'>AntiMatter Satellite</h2>
<div class='tabs'>
<div class='tab' id='t_control'>Devices</div>
<div class='tab' id='t_auto'>Automations</div>
<div class='tab' id='t_config'>Settings</div>
<div class='tab' id='t_wifi'>Network</div>
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
<script>
function show(tab){
document.querySelectorAll('.content').forEach(c=>c.style.display='none');
document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
document.getElementById(tab).style.display='block';
document.getElementById('t_'+tab).classList.add('active');
localStorage.setItem('tab',tab);
if(tab==='auto') loadRules();
}
)rawliteral";

const char HTML_PAGE_2[] PROGMEM = R"rawliteral(
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

const char HTML_PAGE_3[] PROGMEM = R"rawliteral(
const cardRenderers = {

HUMI: (s, i) => `
<div class='card'>
  <h3>HUMIDITY ${s.name}</h3>
  <p style='margin-left:6px;'>
    Moisture:
    <b id='v${i}'>
      ${s.value === 255 || s.value == null ? 'N/A' : s.value + ' %'}
    </b>
  </p>  
  <input id='ref${i}' placeholder='Value' style='width:90px;margin:0 5px 6px 5px;border-radius:6px;padding:4px'>
  <button onclick='setCalib(${i},"ref", "${s.name}")'>Set Ref Val</button><br>
  <button onclick='setCalib(${i},"min", "${s.name}")'>Set 0%</button>
  <button onclick='setCalib(${i},"max", "${s.name}")'>Set 100%</button><br>
  <button onclick='setCalib(${i},"res", "${s.name}")'>Reset</button><br>
  <button
      onclick='toggleMatterSwitch(${i}, "${s.id}", "${s.name}")'
      id='matterBtn${i}'
      data-name='${s.id}'
      class='matter-btn ${s.avail ? "on" : "off"}'
      style='margin-top:10px;'>
      ${s.avail ? 'ENABLED' : 'DISABLED'}
    </button>
</div>`,

LEVE: (s, i) => `
<div class='card'>
  <h3>LEVEL ${s.name}</h3>
  <p style='margin-left:6px;'>
    Level:
    <b id='v${i}'>
      ${s.value === 255 || s.value == null ? 'N/A' : s.value + ' %'}
    </b>
  </p>  
  <input id='ref${i}' placeholder='Value' style='width:90px;margin:0 5px 6px 5px;border-radius:6px;padding:4px'>
  <button onclick='setCalib(${i},"ref","${s.name}")'>Set Ref Val</button><br>
  <button onclick='setCalib(${i},"min","${s.name}")'>Set 0%</button>
  <button onclick='setCalib(${i},"max","${s.name}")'>Set 100%</button><br>
  <button onclick='setCalib(${i},"res","${s.name}")'>Reset</button><br>
  <button
      onclick='toggleMatterSwitch(${i}, "${s.id}","${s.name}")'
      id='matterBtn${i}'
      data-name='${s.id}'
      class='matter-btn ${s.avail ? "on" : "off"}'
      style='margin-top:10px;'>
      ${s.avail ? 'ENABLED' : 'DISABLED'}
    </button>
</div>`,

LUMI: (s, i) => `
<div class='card'>
  <h3>LUMINOSITY ${s.name}</h3>
  <p style='margin-left:6px;'>
    Val:
    <b id='v${i}'>
      ${
        s.value === 255 || s.value == null
          ? 'N/A'
          : (s.value * 108.9432 / 7074).toFixed(0) + ' lx'
      }
    </b>
  </p>  
  <input id='ref${i}' placeholder='Value'
    style='width:90px;margin:0 5px 6px 5px;border-radius:6px;padding:4px'>
  <button onclick='setCalib(${i},"ref", "${s.name}")'>Set Ref Val</button><br>
  <button onclick='setCalib(${i},"res", "${s.name}")'>Reset</button><br>
  <button
      onclick='toggleMatterSwitch(${i}, "${s.id}","${s.name}")'
      id='matterBtn${i}'
      data-name='${s.id}'
      class='matter-btn ${s.avail ? "on" : "off"}'
      style='margin-top:10px;'>
      ${s.avail ? 'ENABLED' : 'DISABLED'}
    </button>
</div>`,

DIMM: (s, i) => `
<div class='card'>
  <h3>DIMMER ${s.name}</h3>
  <p style='margin-left:6px;'>
    Fade: <b id='v${i}'>${s.fade}</b> ms
  </p>
  <input id='ref${i}' placeholder='Fade in/out time(ms)'
    style='width:122px;margin:0 5px 12px 5px;border-radius:6px;padding:4px'>
  <button onclick='setCalib(${i},"fad","${s.name}")'>Set Fade</button><br>
  <button
      onclick='toggleMatterSwitch(${i}, "${s.id}","${s.name}")'
      id='matterBtn${i}'
      data-name='${s.id}'
      class='matter-btn ${s.avail ? "on" : "off"}'
      style='margin-top:10px;'>
      ${s.avail ? 'ENABLED' : 'DISABLED'}
    </button>
</div>`,

TEMP: (s, i) => `
<div class='card'>
  <h3>TEMPERATURE ${s.name}</h3>
  <p style='margin-left:6px;'>
    Val:
    <b id='v${i}'>
      ${s.value === 255 || s.value == null ? 'N/A' : s.value.toFixed(2) + ' °C'}
    </b>
  </p>
  <input id='ref${i}' placeholder='Value'
    style='width:90px;margin:0 5px 6px 5px;border-radius:6px;padding:4px'>
  <button onclick='setCalib(${i},"ref", "${s.name}")'>Set Ref Val</button><br>
  <button onclick='setCalib(${i},"res", "${s.name}")'>Reset</button><br>
  <button
      onclick='toggleMatterSwitch(${i}, "${s.id}","${s.name}")'
      id='matterBtn${i}'
      data-name='${s.id}'
      class='matter-btn ${s.avail ? "on" : "off"}'
      style='margin-top:10px;'>
      ${s.avail ? 'ENABLED' : 'DISABLED'}
    </button>
</div>`,

PRES: (s, i) => `
<div class='card'>
  <h3>PRESSURE ${s.name}</h3>
  <p style='margin-left:6px;'>
    Val:
    <b id='v${i}'>
      ${s.value === 255 || s.value == null ? 'N/A' : s.value.toFixed(2) + ' kPa'}
    </b>
  </p>
  <input id='ref${i}' placeholder='Value'
    style='width:90px;margin:0 5px 6px 5px;border-radius:6px;padding:4px'>
  <button onclick='setCalib(${i},"ref", "${s.name}")'>Set Ref Val</button><br>
  <button onclick='setCalib(${i},"res", "${s.name}")'>Reset</button><br>
  <button
      onclick='toggleMatterSwitch(${i}, "${s.id}","${s.name}")'
      id='matterBtn${i}'
      data-name='${s.id}'
      class='matter-btn ${s.avail ? "on" : "off"}'
      style='margin-top:10px;'>
      ${s.avail ? 'ENABLED' : 'DISABLED'}
    </button>
</div>`,

AIRQ: (s, i) => `
<div class='card'>
  <h3>AIR QUALITY ${s.name}</h3>
  <p style='margin-left:6px;'>
    Val:
    <b id='v${i}'>
      ${s.value === 255 || s.value == null ? 'N/A' : s.value == 0 ? 'GOOD' : s.value == 1 ? 'WARN' : s.value == 2 ? 'BAD' : 'N/A'}
    </b>
  </p>  
    <button
      onclick='toggleMatterSwitch(${i}, "${s.id}","${s.name}")'
      id='matterBtn${i}'
      data-name='${s.id}'
      class='matter-btn ${s.avail ? "on" : "off"}'
      style='margin-top:10px;'>
      ${s.avail ? 'ENABLED' : 'DISABLED'}
    </button>
</div>`,

RAIN: (s, i) => `
<div class='card'>
  <h3>RAIN ${s.name}</h3>
  <p style='margin-left:6px;'>
    Rain:
    <b id='v${i}'>
      ${s.value === 255 || s.value == null ? 'N/A' : s.value ? "YES" : "NO"}
    </b>
  </p>  
    <button
      onclick='toggleMatterSwitch(${i}, "${s.id}","${s.name}")'
      id='matterBtn${i}'
      data-name='${s.id}'
      class='matter-btn ${s.avail ? "on" : "off"}'
      style='margin-top:10px;'>
      ${s.avail ? 'ENABLED' : 'DISABLED'}
    </button>
</div>`,

REL: (s, i) => `
<div class='card'>
  <h3>RELAY ${s.name}</h3>

  <div style='display:flex;gap:8px;flex-direction:column;align-items:flex-start;'>

    <label>
      <input type='checkbox' id='persistChk${i}'
        ${s.persist ? 'checked' : ''}
        onchange='togglePersist(${i}, "${s.name}")'>
      Persistence
    </label>

    <label>
      <input type='checkbox' id='pulseChk${i}'
        ${s.pulse ? 'checked' : ''}
        onchange='togglePulse(${i}, "${s.name}")'>
      Pulse Mode (ms)
    </label>    
    <input id='ref${i}'
      placeholder='Pulse time(ms)'
      value='${s.pulse_ms ?? ''}'
      onchange='setCalib(${i},"pulse", "${s.name}", this.value)'
      style='width:90px;${s.pulse ? '' : 'display:none;'}margin-left:5px;border-radius:6px;padding:4px;margin-top:10px;'><br>
    <button
      onclick='toggleMatterSwitch(${i}, "${s.id}","${s.name}")'
      id='matterBtn${i}'
      data-name='${s.id}'
      class='matter-btn ${s.avail ? "on" : "off"}'
      style='margin-top:-10px;'>
      ${s.avail ? 'ENABLED' : 'DISABLED'}
    </button>
  </div>
</div>`,

DEFAULT: (s, i) => `
<div class='card'>
  <h3>GENERAL SETTINGS</h3>

  <p style='margin-left:6px;margin-bottom:0;'>
    UDP Ports:
  </p>

  <p style='margin-left:6px;margin-top:1px'>
    Broadcast: <b>${genset.broadcast_port}</b> |
    Command: <b>${genset.command_port}</b>
  </p>

  <p style='margin-left:6px;'>
    Report Interval: <b>${genset.report_interval} ms</b>
  </p>

  <input id='broadcast_port' placeholder='Broadcast Port'
    style='width:101px;margin:5px;margin-left:6px;border-radius:6px;padding:4px'>

  <input id='command_port' placeholder='Command Port'
    style='width:103px;margin:5px;border-radius:6px;padding:4px'>

  <input id='ref${i}' placeholder='Report Interval(ms)'
    style='width:127px;margin:5px;border-radius:6px;padding:4px'>

  <div style='display:flex;justify-content:space-between;align-items:center;margin-top:10px;'>
    <button onclick='setPort(${i})'
      style='margin:10px;margin-bottom:9px;'>
      Save
    </button>

    <button onclick='factoryReset()'
      style='background:#bd1313;margin-bottom:9px;'>
      Factory Reset
    </button>
  </div>
</div>`
};
)rawliteral";

const char HTML_PAGE_4[] PROGMEM = R"rawliteral(

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
  TYPE_RELAY: 9
});

function deviceCard(name, value, id, state, fade, type) {

  if (type === SensorType.TYPE_RELAY) {
    return `
<div class='card' data-name='${name}' data-type='${type}' style='text-align:center'>
  <h3>RELAY ${name}</h3>
  <p>
    State:
    <b id='dev_${id}'>${value ? 'ON' : 'OFF'}</b>
  </p>
  <button 
    onclick="toggleDevice('${name}')" 
    style="margin-top:6px; display:inline-block; margin-right:8px">
    Toggle
  </button>
</div>`;
  }

  if (type === SensorType.TYPE_DIMMER) {    
    return `
<div class='card' data-name='${name}' data-type='${type}' style='text-align:center'>
  <h3>DIMMER ${name}</h3>
  <p>
    Level:
    <b id='dev_val_${id}'>${value}</b> %
  </p>
  <p>
    State:
    <b id='dev_state_${id}'>${(value > 0) ? 'ON' : 'OFF'}</b>
  </p>
  <input type='range' min='0' max='100' name='${name}' value='${value}' id='slider_${id}' style='margin-bottom:18px' oninput='onDimmerInput(${id}, this.value)' onchange='onDimmerChange(${id}, this.value, name)'>
  <button 
    onclick="toggleDevice('${name}')" 
    style="margin-top:6px; display:inline-block; margin-right:8px">
    Toggle
  </button>
</div>`;
  }

  if (type === SensorType.SENSOR_TEMP) {
    return `
<div class='card' style='text-align:center'>
  <h3>TEMPERATURE ${name}</h3>
  <p>
    <b id='dev_${id}'>
      ${(value === 255 || value == null) ? 'N/A' : value.toFixed(2) + ' °C'}
    </b>
  </p>
</div>`;
  }

  if (type === SensorType.SENSOR_HUMI) {
    return `
<div class='card' style='text-align:center'>
  <h3>HUMIDITY ${name}</h3>
  <p>
    <b id='dev_${id}'>
      ${(value === 255 || value == null) ? 'N/A' : value.toFixed(0) + ' %'}
    </b>
  </p>
</div>`;
  }

  if (type === SensorType.SENSOR_PRESS) {
    return `
<div class='card' style='text-align:center'>
  <h3>PRESSURE ${name}</h3>
  <p>
    <b id='dev_${id}'>
      ${(value === 255 || value == null) ? 'N/A' : value.toFixed(0) + ' kPa'}
    </b>
  </p>
</div>`;
  }

  if (type === SensorType.SENSOR_LEVEL) {
    return `
<div class='card' style='text-align:center'>
  <h3>LEVEL ${name}</h3>
  <p>
    <b id='dev_${id}'>
      ${(value === 255 || value == null) ? 'N/A' : value.toFixed(0) + ' %'}
    </b>
  </p>
</div>`;
  }

  if (type === SensorType.SENSOR_AIRQ) {
    return `
<div class='card' style='text-align:center'>
  <h3>AIR QUALITY ${name}</h3>
  <p>
    <b id='dev_${id}'>
      ${value === 255 || value == null ? 'N/A' : value == 0 ? 'GOOD' : value == 1 ? 'WARN' : value == 2 ? 'BAD' : 'N/A'}
    </b>
  </p>
</div>`;
  }

  if (type === SensorType.SENSOR_RAIN) {
    return `
<div class='card' style='text-align:center'>
  <h3>RAIN ${name}</h3>
  <p>
    <b id='dev_${id}'>
      ${(value === 255 || value == null) ? 'N/A' : value ? "YES" : "NO"}
    </b>
  </p>
</div>`;
  }

  if (type === SensorType.SENSOR_LUMI) {
    return `
<div class='card' style='text-align:center'>
  <h3>LUMINOSITY ${name}</h3>
  <p>
    <b id='dev_${id}'>
      ${(value === 255 || value == null)
        ? 'N/A'
        : (value * 108.9432 / 7074).toFixed(0) + ' lx'}
    </b>
  </p>
</div>`;
  }

  return `
<div class='card' style='text-align:center'>
  <h3>${name}</h3>
  <p><b id='dev_${id}'>${value}</b></p>
</div>`;
}

/* -------------------- DEVICES -------------------- */

async function loadDevices() {
  try {
    const r = await fetch('/calib');
    if (!r.ok) return;
    const sensors = await r.json();
    let html = '';
    sensors.forEach((s, i) => {
      if (s.avail) html += deviceCard(s.name, s.value, i, s.state, s.fade, s.type);
    });
    document.getElementById('devices_cards').innerHTML = html;
  } catch (e) {
    console.log('loadDevices err', e);
  }
}

/* -------------------- DIMMER -------------------- */

function onDimmerInput(id, value) {
  document.getElementById('dev_val_' + id).innerText = value;
}

const dimmerTimeouts = {};

function onDimmerChange(id, value, name) {
  if (dimmerTimeouts[id]) {
    clearTimeout(dimmerTimeouts[id]);
  }

  dimmerTimeouts[id] = setTimeout(() => {
    sendDimmer(name, value);
  }, 120);
}

async function sendDimmer(key, value) {
  try {
    await fetch('/dimmer', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `key=${key}&value=${value}`
    });
  } catch (e) {
    console.log('sendDimmer err', e);
  }
}

/* -------------------- CALIB -------------------- */

async function loadCalib() {
  const r = await fetch('/calib');
  const sensors = await r.json();
  let html = '';
  sensors.forEach((s, i) => {
    const render =
      s.type === SensorType.TYPE_RELAY  ? cardRenderers.REL :
      s.type === SensorType.TYPE_DIMMER  ? cardRenderers.DIMM :
      s.type === SensorType.SENSOR_TEMP  ? cardRenderers.TEMP :
      s.type === SensorType.SENSOR_LUMI  ? cardRenderers.LUMI :
      s.type === SensorType.SENSOR_PRESS  ? cardRenderers.PRES :
      s.type === SensorType.SENSOR_RAIN  ? cardRenderers.RAIN :
      s.type === SensorType.SENSOR_AIRQ  ? cardRenderers.AIRQ :
      s.type === SensorType.SENSOR_LEVEL  ? cardRenderers.LEVE :
      s.type === SensorType.SENSOR_HUMI ? cardRenderers.HUMI :
      cardRenderers[s.name] ?? cardRenderers.DEFAULT;
    html += render(s, i);
  });
  html += cardRenderers.DEFAULT(
    { value: 0, min: 0, max: 0 },
    sensors.length
  );
  document.getElementById('cards').innerHTML = html;
}

async function updateSettingsValues() {
  try {
    const r = await fetch('/calib');
    if (!r.ok) return;
    const sensors = await r.json();
    sensors.forEach((s, i) => {
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
      else
        el.innerText = s.value ?? '-';
    });
  } catch (e) {}
}

/* -------------------- ACTIONS -------------------- */

async function toggleMatterSwitch(i, id, name) {
  const btn = document.getElementById(`matterBtn${i}`);
  const on = btn.classList.toggle('on');

  btn.classList.toggle('off', !on);
  btn.textContent = on ? 'ENABLED' : 'DISABLED';
  
  await fetch('/calib/set', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: `id=${id}&type=${'avail'}&name=${name}&ref=${encodeURIComponent(on ? 1 : 0)}`
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
  const ref = refOverride !== null
    ? refOverride
    : (document.getElementById(`ref${i}`)?.value ?? '');

  await fetch('/calib/set', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: `id=${i}&type=${type}&name=${name}&ref=${encodeURIComponent(ref)}`
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

function toggleDevice(name) {
  fetch('/toggle', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'key=' + encodeURIComponent(name)
  });
  const card = document.querySelector(`.card[data-name="${name}"]`);
  if (!card) return;
  const type = parseInt(card.dataset.type);
  if (type === SensorType.TYPE_RELAY) {
    const stateEl = card.querySelector('b');
    if (!stateEl) return;
    const current = stateEl.innerText === 'ON';
    stateEl.innerText = current ? 'OFF' : 'ON';  }
  if (type === SensorType.TYPE_DIMMER) {
    const stateEl = card.querySelector("[id^='dev_state_']");
    if (!stateEl) return;
    const current = stateEl.innerText === 'ON';
    stateEl.innerText = current ? 'OFF' : 'ON';
  }
}

function factoryReset() {
  if (!confirm('¿Sure? This will delete all settings and information.')) return;

  fetch('/factory', { method: 'POST' })
    .then(() => alert('Reiniciando...'))
    .catch(() => alert('Error enviando reset'));
}

/*--------------------------------------------------- WIZARD AUTOMATIONS ------------------------------------------------------------------------*/

let wizard={step:0,data:{sensors:[],actuators:[],type:0,logic:1,delay:0,cooldown:0,interval:0,actions:[],levels:[],conditions:{},time_hour:0,time_minute:0,date_start:'',date_end:''}};
let availableSensors=[];

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
          {c:'#27ae60',bg:'#f0fdf4',txt:'🔄 EDGE - Cambios de estado',desc:'Se ejecuta cuando un sensor cambia'},
          {c:'#2980b9',bg:'#f0f8ff',txt:'📊 THRESHOLD - Valores límite',desc:'Se ejecuta por umbral'},
          {c:'#e67e22',bg:'#fffaf0',txt:'⏰ TIME - A una hora',desc:'Ejecuta a una hora fija'},
          {c:'#9b59b6',bg:'#faf5ff',txt:'⏱️ INTERVAL - Cada X tiempo',desc:'Ejecuta periódicamente'}
        ][t];

        return `
        <label style="display:flex;padding:10px;border:2px solid ${wizard.data.type===t?cfg.c:'#ddd'};border-radius:6px;cursor:pointer;background:${wizard.data.type===t?cfg.bg:'#fff'}">
          <input type="radio" name="type" value="${t}" ${wizard.data.type===t?'checked':''} style="margin-right:10px">
          <div>
            <strong>${cfg.txt}</strong>
            <small style="color:#666;display:block">${cfg.desc}</small>
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
        const s = availableSensors[sIdx];
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
              s.value
            );

        content += `
        <div style="border:1px solid #ddd;padding:10px;margin:6px;border-radius:6px">
          <b>${s.name}</b>
          <span style="float:right;font-weight:bold;color:#555">${val}</span><br>
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
            <input id="thresh_${sIdx}" type="number" value="${cond.threshold}">
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
         <input id="date_start" type="date" value="${wizard.data.date_start}" style="width:100%;padding:6px;border-radius:6px;border:1px solid #ccc">
       </div>
       <div style="margin-bottom:12px">
         <label style="display:block;margin-bottom:6px"><strong>Hasta cuándo:</strong></label>
         <input id="date_end" type="date" value="${wizard.data.date_end}" style="width:100%;padding:6px;border-radius:6px;border:1px solid #ccc">
       </div>
       <div style="border-top:1px solid #ddd;padding-top:12px;margin-top:12px">
         <label style="display:block;margin-bottom:6px"><strong>Hora de ejecución:</strong></label>
         <div style="display:flex;gap:8px">
           <input id="time_hour" type="number" min="0" max="23" value="${wizard.data.time_hour}" placeholder="Hs" style="flex:1;padding:6px;border-radius:6px;border:1px solid #ccc">
           <input id="time_minute" type="number" min="0" max="59" value="${wizard.data.time_minute}" placeholder="Min" style="flex:1;padding:6px;border-radius:6px;border:1px solid #ccc">
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
        const a = availableSensors[aIdx];
        const action = wizard.data.actions[i] || 0;
        const level = wizard.data.levels[i] || 0;

        const state = (a.type === 9)
          ? (a.state ? 'ON' : 'OFF')
          : (a.type === 8)
            ? `${a.value}%`
            : '-';

        content += `
        <div style="border:1px solid #ddd;padding:8px;margin:5px;border-radius:6px">
          <b>${a.name}</b>
          <span style="float:right;font-weight:bold;color:#555">${state}</span><br>
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
  html += `<hr>
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
    const actuator = availableSensors[aIdx];
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
          const threshVal = parseInt(threshInput.value);
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
      const actuator = availableSensors[aIdx];
      
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
    filtered = availableSensors.filter((s,i) => [7, 6, 9].includes(s.type));
  } else if(wizard.data.type === 1) {
    filtered = availableSensors.filter((s,i) => [1, 2, 3, 4, 5].includes(s.type));
  }
  
  sel.innerHTML = filtered.map((s,i)=>{
    const origIdx = availableSensors.indexOf(s);
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
    if(s.type===9 || s.type===8) acc.push({...s, idx:i});
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
    const actuator = availableSensors[aIdx];
    
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
  
  // ✅ DEBUG
  console.log('=== VALIDATION PASSED ===');
  console.log('Sending data:');
  console.log('type:', wizard.data.type);
  console.log('sensors:', wizard.data.sensors);
  console.log('actuators:', wizard.data.actuators);
  console.log('actions:', wizard.data.actions);
  console.log('levels:', wizard.data.levels);
  console.log('time_s:', time_s);
  console.log('year_start:', year_start, 'month_start:', month_start, 'day_start:', day_start);
  console.log('year_end:', year_end, 'month_end:', month_end, 'day_end:', day_end);
  console.log('delay:', wizard.data.delay, 'cooldown:', wizard.data.cooldown);
  console.log('interval:', wizard.data.interval);
  
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

loadCalib();
loadRules();
loadDevices();
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