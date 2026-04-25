#include "html.h"

namespace html {

// ==================== HTML START ====================
const char HTML_START[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
)rawliteral";

// ==================== STYLES ====================
const char HTML_STYLES[] PROGMEM = R"rawliteral(
  <style>
    body {
      font-family: sans-serif;
      text-align: center;
      margin: 0;
      background: #f4f4f4;
    }
    
    .tabs {
      display: flex;
      justify-content: space-around;
      background: #222;
      color: #fff;
    }
    
    .tab {
      flex: 1;
      padding: 12px;
      cursor: pointer;
      user-select: none;
    }
    
    .tab:hover {
      background: #333;
    }
    
    .tab.active {
      background: #444;
      border-bottom: 3px solid #2ecc71;
    }
    
    .content {
      padding: 15px;
    }
    
    .card {
      background: #fff;
      margin: 10px;
      padding: 15px;
      border-radius: 10px;
      box-shadow: 0 2px 6px rgba(0,0,0,0.2);
      text-align: left;
      border-left: 4px solid #d2d4d5;
    }
    
    .card h3 {
      margin-top: 0;
      text-align: center;
    }
    
    button {
      padding: 6px 12px;
      border: none;
      border-radius: 6px;
      background: #333;
      color: #fff;
      margin: 5px;
      cursor: pointer;
      transition: background 0.2s;
    }
    
    button:hover {
      background: #555;
    }
    
    button:disabled {
      background: #ccc;
      cursor: not-allowed;
    }
    
    .matter-btn {
      padding: 6px 12px;
      border-radius: 6px;
      border: none;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
    }
    
    .matter-btn.on {
      background: #2ecc71;
      color: #000;
    }
    
    .matter-btn.off {
      background: #444;
      color: #bbb;
    }
    
    .modal {
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(0,0,0,0.6);
      display: none;
      align-items: center;
      justify-content: center;
      z-index: 1000;
    }
    
    .modal-content {
      background: #fff;
      padding: 20px;
      border-radius: 10px;
      width: 320px;
      text-align: left;
      max-height: 80vh;
      overflow-y: auto;
    }
    
    .modal input,
    .modal select {
      width: 100%;
      margin-bottom: 10px;
      padding: 6px;
      border-radius: 6px;
      border: 1px solid #ccc;
      box-sizing: border-box;
    }
    
    input[type=range] {
      width: 100%;
    }
    
    table {
      width: 100%;
      border-collapse: collapse;
      background: #fff;
      border-radius: 6px;
      overflow: hidden;
    }
    
    thead {
      background: #333;
      color: #fff;
    }
    
    th, td {
      padding: 10px;
      text-align: left;
      border-bottom: 1px solid #ddd;
    }
    
    tr:hover {
      background: #f9f9f9;
    }
    
    .error {
      color: #c0392b;
      font-weight: bold;
    }
    
    .success {
      color: #27ae60;
      font-weight: bold;
    }
  </style>
</head>
)rawliteral";

// ==================== BODY START ====================
const char HTML_BODY_START[] PROGMEM = R"rawliteral(
<body>
  <h2 style='background:#222;margin:0;padding:12px;text-align:center;color:#eee'>
    🛰️ AntiMatter Satellite
  </h2>
)rawliteral";

// ==================== TABS ====================
const char HTML_TABS[] PROGMEM = R"rawliteral(
  <div class='tabs'>
    <div class='tab' id='t_control'>📱 Devices</div>
    <div class='tab' id='t_auto'>⚙️ Automations</div>
    <div class='tab' id='t_config'>⚡ Settings</div>
    <div class='tab' id='t_wifi'>📡 Network</div>
  </div>
)rawliteral";

// ==================== DEVICES SECTION ====================
const char HTML_DEVICES_SECTION[] PROGMEM = R"rawliteral(
  <div id='control' class='content'>
    <div id='devices_cards'></div>
  </div>
)rawliteral";

// ==================== AUTOMATIONS SECTION ====================
const char HTML_AUTOMATIONS_SECTION[] PROGMEM = R"rawliteral(
  <div id='auto' class='content' style='display:none'>  
    <table>
      <thead>
        <tr>
          <th>ID</th>
          <th>Sensors</th>
          <th>Type</th>
          <th>Logic</th>
          <th>Actuators</th>
          <th>Delay</th>
          <th>Cooldown</th>
          <th>Actions</th>
        </tr>
      </thead>
      <tbody id="auto_table"></tbody>
    </table>
    <div style="margin-top:10px;text-align:left">
      <button onclick="newRule()" style="float:left">+ Add Rule</button>
    </div>
  </div>
)rawliteral";

// ==================== CONFIG SECTION ====================
const char HTML_CONFIG_SECTION[] PROGMEM = R"rawliteral(
  <div id='config' class='content' style='display:none'>
    <div id='cards'></div>
  </div>
)rawliteral";

// ==================== WIFI SECTION ====================
const char HTML_WIFI_SECTION[] PROGMEM = R"rawliteral(
  <div id='wifi' class='content' style='display:none'>
    <div class='card'>
      <h3>📡 WiFi Setup</h3>
      <form action='/save' method='post'>
        <input name='ssid' placeholder='SSID' required>
        <input name='pass' placeholder='Password' type='password' required>
        <button type='submit'>Save WiFi</button>
      </form>
    </div>
  </div>
)rawliteral";

// ==================== BODY END ====================
const char HTML_BODY_END[] PROGMEM = R"rawliteral(
</body>
)rawliteral";

// ==================== JAVASCRIPT CORE ====================
const char JS_CORE[] PROGMEM = R"rawliteral(
<script>

// ============= TAB NAVIGATION =============
function show(tab) {
  document.querySelectorAll('.content').forEach(c => c.style.display = 'none');
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  
  const tabEl = document.getElementById(tab);
  const tabBtn = document.getElementById('t_' + tab);
  
  if(tabEl) tabEl.style.display = 'block';
  if(tabBtn) tabBtn.classList.add('active');
  
  localStorage.setItem('tab', tab);
  
  if(tab === 'auto') loadRules();
}

// ============= INIT ON LOAD =============
document.addEventListener('DOMContentLoaded', () => {
  ['control', 'auto', 'config', 'wifi'].forEach(t => {
    const btn = document.getElementById('t_' + t);
    if(btn) btn.addEventListener('click', () => show(t));
  });
  
  const savedTab = localStorage.getItem('tab') || 'control';
  show(savedTab);
});

)rawliteral";

// ==================== JAVASCRIPT DEVICES ====================
const char JS_DEVICES[] PROGMEM = R"rawliteral(
// ============= DEVICES FUNCTIONS =============

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
    console.error('loadDevices error:', e);
  }
}

function deviceCard(name, value, id, state, fade, type) {
  if (type === SensorType.TYPE_RELAY) {
    return `
      <div class='card' data-name='${name}' data-type='${type}' style='text-align:center'>
        <h3>🔌 RELAY ${name}</h3>
        <p>State: <b id='dev_${id}'>${value ? 'ON' : 'OFF'}</b></p>
        <button onclick="toggleDevice('${name}')">Toggle</button>
      </div>`;
  }

  if (type === SensorType.TYPE_DIMMER) {    
    return `
      <div class='card' data-name='${name}' data-type='${type}' style='text-align:center'>
        <h3>💡 DIMMER ${name}</h3>
        <p>Level: <b id='dev_val_${id}'>${value}</b> %</p>
        <p>State: <b id='dev_state_${id}'>${(value > 0) ? 'ON' : 'OFF'}</b></p>
        <input type='range' min='0' max='100' value='${value}' id='slider_${id}' 
               oninput='onDimmerInput(${id}, this.value)' 
               onchange='onDimmerChange(${id}, this.value, "${name}")'>
        <button onclick="toggleDevice('${name}')">Toggle</button>
      </div>`;
  }

  const displayValue = (val, t) => {
    if (val === 255 || val == null) return 'N/A';
    switch(t) {
      case SensorType.SENSOR_TEMP: return val.toFixed(2) + ' °C';
      case SensorType.SENSOR_HUMI: return val.toFixed(0) + ' %';
      case SensorType.SENSOR_PRESS: return val.toFixed(0) + ' kPa';
      case SensorType.SENSOR_LEVEL: return val.toFixed(0) + ' %';
      case SensorType.SENSOR_LUMI: return (val * 108.9432 / 7074).toFixed(0) + ' lx';
      case SensorType.SENSOR_AIRQ: return (val==0?'GOOD':val==1?'WARN':val==2?'BAD':'N/A');
      case SensorType.SENSOR_RAIN: return val ? 'YES' : 'NO';
      default: return val;
    }
  };

  const typeNames = {
    [SensorType.SENSOR_TEMP]: '🌡️ TEMPERATURE',
    [SensorType.SENSOR_HUMI]: '💧 HUMIDITY',
    [SensorType.SENSOR_PRESS]: '📊 PRESSURE',
    [SensorType.SENSOR_LEVEL]: '📈 LEVEL',
    [SensorType.SENSOR_LUMI]: '☀️ LUMINOSITY',
    [SensorType.SENSOR_AIRQ]: '💨 AIR QUALITY',
    [SensorType.SENSOR_RAIN]: '🌧️ RAIN'
  };

  const typeName = typeNames[type] || 'SENSOR';
  return `
    <div class='card' style='text-align:center'>
      <h3>${typeName} ${name}</h3>
      <p><b id='dev_${id}'>${displayValue(value, type)}</b></p>
    </div>`;
}

function onDimmerInput(id, value) {
  document.getElementById('dev_val_' + id).innerText = value;
}

const dimmerTimeouts = {};

function onDimmerChange(id, value, name) {
  if (dimmerTimeouts[id]) clearTimeout(dimmerTimeouts[id]);
  dimmerTimeouts[id] = setTimeout(() => sendDimmer(name, value), 120);
}

async function sendDimmer(key, value) {
  try {
    await fetch('/dimmer', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `key=${key}&value=${value}`
    });
  } catch (e) {
    console.error('sendDimmer error:', e);
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
    if (stateEl) stateEl.innerText = stateEl.innerText === 'ON' ? 'OFF' : 'ON';
  }
  if (type === SensorType.TYPE_DIMMER) {
    const stateEl = card.querySelector("[id^='dev_state_']");
    if (stateEl) stateEl.innerText = stateEl.innerText === 'ON' ? 'OFF' : 'ON';
  }
}

setInterval(() => {
  loadDevices();
  updateSettingsValues();
}, 5000);

)rawliteral";

// ==================== JAVASCRIPT CALIBRATION ====================
const char JS_CARD_RENDERERS[] PROGMEM = R"rawliteral(
// ============= CALIBRATION FUNCTIONS =============

async function loadCalib() {
  try {
    const r = await fetch('/calib');
    const sensors = await r.json();
    let html = '';
    sensors.forEach((s, i) => {
      html += renderCalibCard(s, i);
    });
    document.getElementById('cards').innerHTML = html;
  } catch (e) {
    console.error('loadCalib error:', e);
  }
}

function renderCalibCard(s, i) {
  const type = s.type;
  
  if (type === SensorType.TYPE_RELAY) {
    return `
      <div class='card'>
        <h3>🔌 RELAY ${s.name}</h3>
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
        <input id='ref${i}' placeholder='Pulse time(ms)' value='${s.pulse_ms ?? ''}'
          onchange='setCalib(${i},"pulse", "${s.name}", this.value)'
          style='width:100%;${s.pulse ? '' : 'display:none;'}'>
        <button onclick='toggleMatterSwitch(${i}, "${s.id}", "${s.name}")'
          id='matterBtn${i}' class='matter-btn ${s.avail ? "on" : "off"}'>
          ${s.avail ? 'ENABLED' : 'DISABLED'}
        </button>
      </div>`;
  }

  if (type === SensorType.TYPE_DIMMER) {
    return `
      <div class='card'>
        <h3>💡 DIMMER ${s.name}</h3>
        <p>Fade: <b id='v${i}'>${s.fade}</b> ms</p>
        <input id='ref${i}' placeholder='Fade time(ms)' style='width:100%;'>
        <button onclick='setCalib(${i},"fad","${s.name}")'>Set Fade</button>
        <button onclick='toggleMatterSwitch(${i}, "${s.id}","${s.name}")'
          id='matterBtn${i}' class='matter-btn ${s.avail ? "on" : "off"}'>
          ${s.avail ? 'ENABLED' : 'DISABLED'}
        </button>
      </div>`;
  }

  // Sensores de lectura
  const typeNames = {
    [SensorType.SENSOR_TEMP]: '🌡️ TEMPERATURE',
    [SensorType.SENSOR_HUMI]: '💧 HUMIDITY',
    [SensorType.SENSOR_PRESS]: '📊 PRESSURE',
    [SensorType.SENSOR_LEVEL]: '📈 LEVEL',
    [SensorType.SENSOR_LUMI]: '☀️ LUMINOSITY',
    [SensorType.SENSOR_AIRQ]: '💨 AIR QUALITY',
    [SensorType.SENSOR_RAIN]: '🌧️ RAIN'
  };

  return `
    <div class='card'>
      <h3>${typeNames[type] || 'SENSOR'} ${s.name}</h3>
      <p>Value: <b id='v${i}'>${s.value}</b></p>
      <input id='ref${i}' placeholder='Value' style='width:100%;'>
      <button onclick='setCalib(${i},"ref", "${s.name}")'>Set Ref Val</button>
      <button onclick='setCalib(${i},"res", "${s.name}")'>Reset</button>
      <button onclick='toggleMatterSwitch(${i}, "${s.id}","${s.name}")'
        id='matterBtn${i}' class='matter-btn ${s.avail ? "on" : "off"}'>
        ${s.avail ? 'ENABLED' : 'DISABLED'}
      </button>
    </div>`;
}

async function toggleMatterSwitch(i, id, name) {
  const btn = document.getElementById(`matterBtn${i}`);
  const on = btn.classList.toggle('on');
  btn.classList.toggle('off', !on);
  btn.textContent = on ? 'ENABLED' : 'DISABLED';
  
  await fetch('/calib/set', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: `id=${id}&type=avail&name=${name}&ref=${on ? 1 : 0}`
  });
}

async function setCalib(i, type, name, refOverride = null) {
  const ref = refOverride !== null ? refOverride : (document.getElementById(`ref${i}`)?.value ?? '');
  await fetch('/calib/set', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: `id=${i}&type=${type}&name=${name}&ref=${encodeURIComponent(ref)}`
  });
}

function togglePersist(i, name) {
  const persist = document.getElementById(`persistChk${i}`);
  const pulse = document.getElementById(`pulseChk${i}`);
  if (persist.checked) {
    pulse.checked = false;
    setCalib(i, 'pulse', name, 0);
  }
  setCalib(i, 'persist', name, persist.checked ? 1 : 0);
}

function togglePulse(i, name) {
  const pulse = document.getElementById(`pulseChk${i}`);
  const persist = document.getElementById(`persistChk${i}`);
  const input = document.getElementById(`ref${i}`);
  if (pulse.checked) {
    persist.checked = false;
    setCalib(i, 'persist', name, 0);
    input.style.display = 'inline-block';
  } else {
    input.style.display = 'none';
    setCalib(i, 'pulse', name, 0);
  }
}

async function updateSettingsValues() {
  try {
    const r = await fetch('/calib');
    if (!r.ok) return;
    const sensors = await r.json();
    sensors.forEach((s, i) => {
      const el = document.getElementById(`v${i}`);
      if (!el) return;
      const displayValue = (val, t) => {
        if (val === 255 || val == null) return 'N/A';
        switch(t) {
          case SensorType.SENSOR_TEMP: return val.toFixed(2) + ' °C';
          case SensorType.SENSOR_HUMI: return val.toFixed(0) + ' %';
          case SensorType.SENSOR_PRESS: return val.toFixed(0) + ' kPa';
          case SensorType.SENSOR_LEVEL: return val.toFixed(0) + ' %';
          case SensorType.SENSOR_LUMI: return (val * 108.9432 / 7074).toFixed(0) + ' lx';
          case SensorType.SENSOR_AIRQ: return (val==0?'GOOD':val==1?'WARN':val==2?'BAD':'N/A');
          case SensorType.SENSOR_RAIN: return val ? 'YES' : 'NO';
          case SensorType.TYPE_DIMMER: return s.fade + ' ms';
          default: return val;
        }
      };
      el.innerText = displayValue(s.value, s.type);
    });
  } catch (e) {}
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
  alert('✅ Settings saved');
}

function factoryReset() {
  if (!confirm('⚠️ This will delete ALL settings. Are you sure?')) return;
  fetch('/factory', { method: 'POST' })
    .then(() => alert('🔄 Reiniciando...'))
    .catch(() => alert('❌ Error'));
}

)rawliteral";

// ==================== JAVASCRIPT WIZARD CORE ====================
const char JS_WIZARD_CORE[] PROGMEM = R"rawliteral(
// ============= WIZARD STATE =============

let wizard = {
  step: 0,
  data: {
    sensors: [],
    actuators: [],
    type: 0,
    logic: 1,
    delay: 0,
    cooldown: 0,
    interval: 0,
    actions: [],
    levels: [],
    conditions: {},
    time_hour: 0,
    time_minute: 0,
    date_start: '',
    date_end: ''
  }
};

let availableSensors = [];

async function loadSensorsAndActuators() {
  const r = await fetch('/calib');
  availableSensors = await r.json();
}

function startWizard(edit = -1) {
  wizard = {
    step: 0,
    data: {
      sensors: [],
      actuators: [],
      type: 0,
      logic: 1,
      delay: 0,
      cooldown: 0,
      interval: 0,
      actions: [],
      levels: [],
      conditions: {},
      time_hour: 0,
      time_minute: 0,
      date_start: '',
      date_end: ''
    }
  };
  
  if (edit >= 0 && window.rules && window.rules[edit]) {
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
      date_start: (rule.year_start && rule.month_start && rule.day_start) 
        ? `${rule.year_start}-${String(rule.month_start).padStart(2,'0')}-${String(rule.day_start).padStart(2,'0')}`
        : '',
      date_end: (rule.year_end && rule.month_end && rule.day_end)
        ? `${rule.year_end}-${String(rule.month_end).padStart(2,'0')}-${String(rule.day_end).padStart(2,'0')}`
        : ''
    };
    
    if (rule.cmp && rule.threshold) {
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

function getRelevantSteps() {
  const steps = [0];
  
  if (wizard.data.type === 0 || wizard.data.type === 1) {
    steps.push(1, 2);
    if (wizard.data.sensors.length > 1) steps.push(3);
  }
  
  if (wizard.data.type === 2) steps.push(3);
  
  steps.push(4, 5);
  
  if (wizard.data.type === 0 || wizard.data.type === 1 || wizard.data.type === 3) {
    steps.push(6);
  }
  
  return steps;
}

)rawliteral";

// ==================== JAVASCRIPT WIZARD STEPS ====================
const char JS_WIZARD_STEPS[] PROGMEM = R"rawliteral(
// ============= WIZARD STEPS RENDERING =============

function showStep(n) {
  const steps = getRelevantSteps();
  if (n >= steps.length) return;

  wizard.step = n;
  const stepNum = steps[n];
  let content = '';

  if (stepNum === 0) {
    content = `<h3>🎯 Rule Type</h3>
      <div style="display:flex;flex-direction:column;gap:12px">
        ${[0,1,2,3].map(t => {
          const cfg = [
            {c:'#27ae60',bg:'#f0fdf4',txt:'🔄 EDGE - State Changes',desc:'Triggers on sensor change'},
            {c:'#2980b9',bg:'#f0f8ff',txt:'📊 THRESHOLD - Value Limits',desc:'Triggers by threshold'},
            {c:'#e67e22',bg:'#fffaf0',txt:'⏰ TIME - Scheduled',desc:'Triggers at fixed time'},
            {c:'#9b59b6',bg:'#faf5ff',txt:'⏱️ INTERVAL - Periodic',desc:'Triggers periodically'}
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
  else if (stepNum === 1) {
    content = `<h3>📊 Select Sensors</h3>
      <select id="sensorList" multiple size="5" style="width:100%"></select>
      <small>Use Ctrl/Cmd + Click to select multiple</small>`;
  }
  else if (stepNum === 2) {
    if (wizard.data.sensors.length === 0) {
      content = `<h3>⚠️ No sensors selected</h3>`;
    } else {
      content = `<h3>⚙️ Conditions</h3>`;
      wizard.data.sensors.forEach(sIdx => {
        const s = availableSensors[sIdx];
        const cond = wizard.data.conditions[sIdx] || {cmp:0,threshold:0};
        const val = getDisplayValue(s.value, s.type);
        content += `
          <div style="border:1px solid #ddd;padding:10px;margin:6px;border-radius:6px">
            <b>${s.name}</b><span style="float:right;color:#555">${val}</span><br>
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
  else if (stepNum === 3) {
    if (wizard.data.type === 2) {
      content = `<h3>⏰ Date & Time Range</h3>
        <div style="margin-bottom:12px">
          <label><strong>From:</strong></label>
          <input id="date_start" type="date" value="${wizard.data.date_start}" style="width:100%;">
        </div>
        <div style="margin-bottom:12px">
          <label><strong>To:</strong></label>
          <input id="date_end" type="date" value="${wizard.data.date_end}" style="width:100%;">
        </div>
        <div style="border-top:1px solid #ddd;padding-top:12px">
          <label><strong>Execution Time:</strong></label>
          <div style="display:flex;gap:8px">
            <input id="time_hour" type="number" min="0" max="23" value="${wizard.data.time_hour}" placeholder="Hours" style="flex:1">
            <input id="time_minute" type="number" min="0" max="59" value="${wizard.data.time_minute}" placeholder="Minutes" style="flex:1">
          </div>
        </div>`;
    } else if (wizard.data.sensors.length > 1) {
      content = `<h3>🔗 Logic</h3>
        <label><input type="radio" name="logic" value="1" ${wizard.data.logic===1?'checked':''}> AND (all must trigger)</label>
        <label><input type="radio" name="logic" value="0" ${wizard.data.logic===0?'checked':''}> OR (any can trigger)</label>`;
    }
  }
  else if (stepNum === 4) {
    content = `<h3>🎛️ Select Actuators</h3>
      <select id="actuatorList" multiple size="5" style="width:100%"></select>`;
  }
  else if (stepNum === 5) {
    if (wizard.data.actuators.length === 0) {
      content = `<h3>⚠️ No actuators selected</h3>`;
    } else {
      content = `<h3>⚡ Actions</h3>`;
      wizard.data.actuators.forEach((aIdx, i) => {
        const a = availableSensors[aIdx];
        const action = wizard.data.actions[i] || 0;
        const level = wizard.data.levels[i] || 0;
        const state = (a.type === 9) ? (a.state ? 'ON' : 'OFF') : (a.type === 8) ? `${a.value}%` : '-';
        content += `
          <div style="border:1px solid #ddd;padding:8px;margin:5px;border-radius:6px">
            <b>${a.name}</b><span style="float:right;color:#555">${state}</span><br>
            <select id="action_${i}">
              <option value="0" ${action===0?'selected':''}>ON</option>
              <option value="1" ${action===1?'selected':''}>OFF</option>
              <option value="2" ${action===2?'selected':''}>TOGGLE</option>
              ${a.type===8?`<option value="3" ${action===3?'selected':''}>LEVEL</option>`:''}
            </select>
            ${a.type===8?`<input id="level_${i}" type="number" min="0" max="100" value="${level}" style="${action===3?'':'display:none;'}"`:''}
          </div>`;
      });
      setTimeout(() => setupActionListeners(), 0);
    }
  }
  else if (stepNum === 6) {
    if (wizard.data.type === 3) {
      content = `<h3>⏱️ Interval</h3>
        <input id="interval" type="number" value="${wizard.data.interval||1000}" placeholder="milliseconds" style="width:100%;"><br>`;
    }
    content += `
      <h3>⏳ Delay</h3>
      <input id="delay" type="number" value="${wizard.data.delay}" placeholder="milliseconds" style="width:100%;"><br>
      <h3>❄️ Cool Down</h3>
      <input id="cooldown" type="number" value="${wizard.data.cooldown}" placeholder="milliseconds" style="width:100%;">`;
  }

  let html = `<div>${content}</div>`;
  html += `<hr>
    <div style="display:flex;justify-content:space-between">
      ${n>0?'<button onclick="prevStep()">← Back</button>':''}
      ${n<steps.length-1?'<button onclick="nextStep()">Next →</button>':'<button onclick="finishWizard()" style="background:#27ae60;">✅ Save Rule</button>'}
    </div>`;

  document.getElementById('wizardContent').innerHTML = html;

  if (stepNum === 1) populateSensors();
  if (stepNum === 4) populateActuators();
}

function getDisplayValue(val, type) {
  if (val === 255 || val == null) return 'N/A';
  switch(type) {
    case SensorType.SENSOR_TEMP: return val.toFixed(2) + ' °C';
    case SensorType.SENSOR_HUMI: return val.toFixed(0) + ' %';
    case SensorType.SENSOR_PRESS: return val.toFixed(0) + ' kPa';
    case SensorType.SENSOR_LEVEL: return val.toFixed(0) + ' %';
    case SensorType.SENSOR_LUMI: return (val * 108.9432 / 7074).toFixed(0) + ' lx';
    case SensorType.SENSOR_AIRQ: return (val==0?'GOOD':val==1?'WARN':val==2?'BAD':'N/A');
    case SensorType.SENSOR_RAIN: return val ? 'YES' : 'NO';
    default: return val;
  }
}

function prevStep() {
  if (wizard.step > 0) showStep(wizard.step - 1);
}

function populateSensors() {
  const sel = document.getElementById('sensorList');
  let filtered = availableSensors;
  
  if (wizard.data.type === 0) {
    filtered = availableSensors.filter((s,i) => [7, 6, 9].includes(s.type));
  } else if (wizard.data.type === 1) {
    filtered = availableSensors.filter((s,i) => [1, 2, 3, 4, 5].includes(s.type));
  }
  
  sel.innerHTML = filtered.map((s,i) => {
    const origIdx = availableSensors.indexOf(s);
    return `<option value="${origIdx}">[${origIdx}] ${s.name}</option>`;
  }).join('');
  
  if (wizard.data.sensors && wizard.data.sensors.length > 0) {
    document.querySelectorAll('#sensorList option').forEach(o => {
      if (wizard.data.sensors.includes(parseInt(o.value))) o.selected = true;
    });
  }
}

function populateActuators() {
  const sel = document.getElementById('actuatorList');
  const actuators = availableSensors.reduce((acc,s,i) => {
    if (s.type === 9 || s.type === 8) acc.push({...s, idx:i});
    return acc;
  }, []);
  
  sel.innerHTML = actuators.map(s => `<option value="${s.idx}">[${s.idx}] ${s.name}</option>`).join('');
  
  if (wizard.data.actuators && wizard.data.actuators.length > 0) {
    document.querySelectorAll('#actuatorList option').forEach(o => {
      if (wizard.data.actuators.includes(parseInt(o.value))) o.selected = true;
    });
  }
}

)rawliteral";

// ==================== JAVASCRIPT WIZARD VALIDATION ====================
const char JS_WIZARD_VALIDATION[] PROGMEM = R"rawliteral(
// ============= VALIDATION =============

function validateStep(stepNum) {
  if (stepNum === 0) {
    const typeRadio = document.querySelector('input[name="type"]:checked');
    if (!typeRadio) {
      alert('⚠️ Please select a rule type');
      return false;
    }
    return true;
  }
  
  if (stepNum === 1) {
    if ((wizard.data.type === 0 || wizard.data.type === 1) && wizard.data.sensors.length === 0) {
      alert('⚠️ This rule type requires at least one sensor');
      return false;
    }
    return true;
  }
  
  if (stepNum === 2) {
    if (wizard.data.type === 0 || wizard.data.type === 1) {
      for (let sIdx of wizard.data.sensors) {
        const threshInput = document.getElementById(`thresh_${sIdx}`);
        if (wizard.data.type === 1 && threshInput) {
          const threshVal = parseInt(threshInput.value);
          if (isNaN(threshVal) || threshVal < -1000 || threshVal > 10000) {
            alert('⚠️ Threshold must be between -1000 and 10000');
            return false;
          }
        }
      }
    }
    return true;
  }
  
  if (stepNum === 3) {
    if (wizard.data.type === 2) {
      const hour = parseInt(document.getElementById('time_hour').value) || 0;
      const min = parseInt(document.getElementById('time_minute').value) || 0;
      if (hour < 0 || hour > 23 || min < 0 || min > 59) {
        alert('⚠️ Time must be between 00:00 and 23:59');
        return false;
      }
      const dateStartEl = document.getElementById('date_start');
      const dateEndEl = document.getElementById('date_end');
      if (dateStartEl.value && dateEndEl.value) {
        const start = new Date(dateStartEl.value);
        const end = new Date(dateEndEl.value);
        if (start > end) {
          alert('⚠️ Start date cannot be after end date');
          return false;
        }
      }
    }
    return true;
  }
  
  if (stepNum === 4) {
    if (wizard.data.actuators.length === 0) {
      alert('⚠️ At least one actuator is required');
      return false;
    }
    return true;
  }
  
  if (stepNum === 5) {
    for (let aPos = 0; aPos < wizard.data.actuators.length; aPos++) {
      const actionSelect = document.getElementById(`action_${aPos}`);
      if (!actionSelect) {
        alert('⚠️ Error loading actions');
        return false;
      }
      const action = parseInt(actionSelect.value);
      const aIdx = wizard.data.actuators[aPos];
      const actuator = availableSensors[aIdx];
      if (action === 3 && actuator.type !== 8) {
        alert('⚠️ LEVEL action only works with dimmers');
        return false;
      }
    }
    return true;
  }
  
  if (stepNum === 6) {
    const delay = parseInt(document.getElementById('delay').value) || 0;
    const cooldown = parseInt(document.getElementById('cooldown').value) || 0;
    if (delay < 0 || delay > 60000 || cooldown < 0 || cooldown > 3600000) {
      alert('⚠️ Check delay and cooldown ranges');
      return false;
    }
    if (wizard.data.type === 3) {
      const interval = parseInt(document.getElementById('interval').value) || 0;
      if (interval < 1000 || interval > 3600000) {
        alert('⚠️ Interval must be between 1000 and 3600000 ms');
        return false;
      }
    }
    return true;
  }
  
  return true;
}

)rawliteral";

// ==================== JAVASCRIPT WIZARD HANDLERS ====================
const char JS_WIZARD_HANDLERS[] PROGMEM = R"rawliteral(
// ============= WIZARD HANDLERS =============

function setupActionListeners() {
  wizard.data.actuators.forEach((aIdx, aPos) => {
    const actuator = availableSensors[aIdx];
    if (actuator.type === 8) {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);
      if (actionSelect && levelInput) {
        const updateVisibility = () => {
          levelInput.style.display = actionSelect.value === '3' ? 'inline-block' : 'none';
        };
        updateVisibility();
        actionSelect.addEventListener('change', updateVisibility);
      }
    }
  });
}

function nextStep() {
  const steps = getRelevantSteps();
  const stepNum = steps[wizard.step];

  if (stepNum === 1) {
    wizard.data.sensors = [...document.querySelectorAll('#sensorList option:checked')].map(o => parseInt(o.value));
  }
  if (stepNum === 4) {
    wizard.data.actuators = [...document.querySelectorAll('#actuatorList option:checked')].map(o => parseInt(o.value));
  }

  if (!validateStep(stepNum)) return;

  if (stepNum === 0) {
    const typeRadio = document.querySelector('input[name="type"]:checked');
    if (typeRadio) wizard.data.type = parseInt(typeRadio.value);
  }
  else if (stepNum === 2) {
    wizard.data.sensors.forEach(sIdx => {
      const cmpSelect = document.getElementById(`cmp_${sIdx}`);
      const threshInput = document.getElementById(`thresh_${sIdx}`);
      if (cmpSelect) {
        wizard.data.conditions[sIdx] = {
          cmp: parseInt(cmpSelect.value),
          threshold: threshInput ? parseFloat(threshInput.value) || 0 : 0
        };
      }
    });
  }
  else if (stepNum === 3) {
    if (wizard.data.type === 2) {
      wizard.data.date_start = document.getElementById('date_start').value || '';
      wizard.data.date_end = document.getElementById('date_end').value || '';
      wizard.data.time_hour = parseInt(document.getElementById('time_hour').value) || 0;
      wizard.data.time_minute = parseInt(document.getElementById('time_minute').value) || 0;
    } else if (wizard.data.sensors.length > 1) {
      const logicRadio = document.querySelector('input[name="logic"]:checked');
      if (logicRadio) wizard.data.logic = parseInt(logicRadio.value);
    }
  }
  else if (stepNum === 5) {
    wizard.data.actions = [];
    wizard.data.levels = [];
    wizard.data.actuators.forEach((aIdx, aPos) => {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);
      const action = actionSelect ? parseInt(actionSelect.value) : 0;
      let level = 0;
      if (levelInput && levelInput.style.display !== 'none') {
        level = levelInput.value && levelInput.value.trim() !== '' ? parseInt(levelInput.value) : 0;
      }
      wizard.data.actions.push(action);
      wizard.data.levels.push(level);
    });
  }
  else if (stepNum === 6) {
    wizard.data.delay = parseInt(document.getElementById('delay').value) || 0;
    wizard.data.cooldown = parseInt(document.getElementById('cooldown').value) || 0;
    if (wizard.data.type === 3) {
      wizard.data.interval = parseInt(document.getElementById('interval').value) || 1000;
    }
  }

  showStep(wizard.step + 1);
}

async function finishWizard() {
  const steps = getRelevantSteps();
  const stepNum = steps[wizard.step];

  if (wizard.data.type === 2 && stepNum === 3) {
    wizard.data.date_start = document.getElementById('date_start').value || '';
    wizard.data.date_end = document.getElementById('date_end').value || '';
    wizard.data.time_hour = parseInt(document.getElementById('time_hour').value) || 0;
    wizard.data.time_minute = parseInt(document.getElementById('time_minute').value) || 0;
  }
  else if (stepNum === 5) {
    wizard.data.actions = [];
    wizard.data.levels = [];
    wizard.data.actuators.forEach((aIdx, aPos) => {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);
      const action = actionSelect ? parseInt(actionSelect.value) : 0;
      let level = 0;
      if (levelInput && levelInput.style.display !== 'none') {
        level = levelInput.value && levelInput.value.trim() !== '' ? parseInt(levelInput.value) : 0;
      }
      wizard.data.actions.push(action);
      wizard.data.levels.push(level);
    });
  }
  else if (stepNum === 6) {
    wizard.data.delay = parseInt(document.getElementById('delay').value) || 0;
    wizard.data.cooldown = parseInt(document.getElementById('cooldown').value) || 0;
    if (wizard.data.type === 3) {
      wizard.data.interval = parseInt(document.getElementById('interval').value) || 1000;
    }
  }

  if (wizard.data.actuators.length === 0) {
    alert('⚠️ At least one actuator is required');
    return;
  }

  if ((wizard.data.type === 0 || wizard.data.type === 1) && wizard.data.sensors.length === 0) {
    alert('⚠️ This rule type requires at least one sensor');
    return;
  }

  let cmps = [], thresholds = [];
  wizard.data.sensors.forEach(sIdx => {
    const cond = wizard.data.conditions[sIdx] || {cmp: 0, threshold: 0};
    cmps.push(cond.cmp);
    thresholds.push(cond.threshold);
  });

  while (wizard.data.levels.length < wizard.data.actuators.length) {
    wizard.data.levels.push(0);
  }

  let time_s = wizard.data.type === 2 ? wizard.data.time_hour * 3600 + wizard.data.time_minute * 60 : 0;

  let year_start = 0, month_start = 0, day_start = 0;
  let year_end = 0, month_end = 0, day_end = 0;

  if (wizard.data.type === 2) {
    if (wizard.data.date_start) {
      const [y, m, d] = wizard.data.date_start.split('-');
      year_start = parseInt(y);
      month_start = parseInt(m);
      day_start = parseInt(d);
    }
    if (wizard.data.date_end) {
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
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: params.toString()
    });

    if (res.ok) {
      alert('✅ Rule saved successfully');
      closeRule();
      loadRules();
    } else {
      const errMsg = await res.text();
      alert(`❌ Error: ${errMsg}`);
    }
  } catch (e) {
    alert(`❌ Connection error: ${e.message}`);
  }
}

function newRule() {
  loadSensorsAndActuators().then(() => {
    startWizard(-1);
    document.getElementById('ruleModal').style.display = 'flex';
  });
}

function editRule(i) {
  loadSensorsAndActuators().then(() => {
    startWizard(i);
    document.getElementById('ruleModal').style.display = 'flex';
  });
}

function closeRule() {
  document.getElementById('ruleModal').style.display = 'none';
}

async function deleteRule(i) {
  if (!confirm(`⚠️ Delete rule ${i}?`)) return;
  try {
    const res = await fetch('/rules/delete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `id=${i}`
    });
    if (res.ok) {
      await new Promise(resolve => setTimeout(resolve, 200));
      loadRules();
    } else {
      alert('❌ Error deleting rule');
    }
  } catch (e) {
    alert(`❌ Error: ${e.message}`);
  }
}

async function loadRules() {
  try {
    const res = await fetch('/rules');
    if (!res.ok) return;
    const rules = await res.json();
    window.rules = rules;

    const table = document.getElementById("auto_table");
    table.innerHTML = '';

    rules.forEach((r, i) => {
      let row = document.createElement("tr");
      row.innerHTML = `
        <td><b>${r.id}</b></td>
        <td>${r.sensors.join(",")}</td>
        <td>${['EDGE','THRESHOLD','TIME','INTERVAL'][r.type] || r.type}</td>
        <td>${r.logical_and ? "AND" : "OR"}</td>
        <td>${r.actuators.join(", ")}</td>
        <td>${r.delay_ms}</td>
        <td>${r.cooldown_ms}</td>
        <td style="text-align:center;white-space:nowrap">
          <button onclick="editRule(${r.id})" style="font-size:11px;padding:2px 6px">✏️ Edit</button>
          <button onclick="deleteRule(${r.id})" style="font-size:11px;padding:2px 6px;background:#c0392b">🗑️ Del</button>
        </td>`;
      table.appendChild(row);
    });
  } catch (e) {
    console.error('loadRules error:', e);
  }
}

)rawliteral";

// ==================== JAVASCRIPT INIT ====================
const char JS_INIT[] PROGMEM = R"rawliteral(
// ============= INITIALIZATION =============

loadCalib();
loadRules();
loadDevices();

</script>

<div id="ruleModal" class="modal">
  <div class="modal-content">
    <h2 style="margin-top:0">Automation Rule Wizard</h2>
    <div id="wizardContent"></div>
    <hr>
    <button onclick="closeRule()" style="float:right;background:#e74c3c">❌ Cancel</button>
  </div>
</div>
)rawliteral";

// ==================== HTML END ====================
const char HTML_END[] PROGMEM = R"rawliteral(
</body>
</html>
)rawliteral";

}  // namespace html