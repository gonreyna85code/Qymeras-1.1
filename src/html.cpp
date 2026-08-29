#include "html.h"

namespace html_content {
// ================= WEB ===================
const char Styles[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<meta name='color-scheme' content='dark light'>
<title>Qymeras 1.1</title>
<style>
:root{
--bg:#0d1117;--bg-2:#131922;--surface:#161d29;--surface-2:#1c2533;--surface-hover:#232f41;
--border:#2a3444;--text:#e7edf5;--text-muted:#94a3b8;
--accent:#3b82f6;--accent-hover:#60a5fa;--on-accent:#ffffff;
--success:#22c55e;--warning:#f59e0b;--danger:#ef4444;--info:#38bdf8;
--radius-sm:8px;--radius-md:12px;--radius-lg:18px;
--shadow-mild:0 1px 2px rgba(0,0,0,.22);--shadow-lg:0 16px 40px rgba(0,0,0,.40);
--font:system-ui,-apple-system,"Segoe UI",Roboto,Inter,"Helvetica Neue",Arial,sans-serif;
--mono:ui-monospace,SFMono-Regular,Menlo,Consolas,"Liberation Mono",monospace;
--card:var(--surface);--panel:var(--surface-2);
}
[data-theme="light"]{
--bg:#f3f5f9;--bg-2:#eef1f6;--surface:#ffffff;--surface-2:#f4f6fa;--surface-hover:#e9edf4;
--border:#d8dfe9;--text:#1a2433;--text-muted:#5c6a7d;
--accent:#2456c8;--accent-hover:#3b6ae0;--on-accent:#ffffff;
--success:#16a34a;--warning:#b45309;--danger:#dc2626;--info:#0ea5e9;
--shadow-lg:0 16px 40px rgba(15,23,42,.18);color-scheme:light;
}
[data-theme="forest"]{
--bg:#0d1610;--bg-2:#121f16;--surface:#16241b;--surface-2:#1c2d21;--surface-hover:#243829;
--border:#2a4431;--text:#e3efe6;--text-muted:#8fa895;
--accent:#2fbf71;--accent-hover:#4cd78c;--on-accent:#06230f;
--success:#4ade80;--warning:#fbbf24;--danger:#f87171;--info:#38bdf8;
}
[data-theme="sand"]{
--bg:#191410;--bg-2:#201a13;--surface:#262015;--surface-2:#2f291c;--surface-hover:#3a3324;
--border:#4a402b;--text:#f1ead9;--text-muted:#b4a887;
--accent:#dc9b4f;--accent-hover:#eab273;--on-accent:#241607;
--success:#7ee081;--warning:#f0bd5d;--danger:#ec6a5e;--info:#66b6e8;
}
*{box-sizing:border-box}
html{color-scheme:dark}
body{margin:0;background:var(--bg);color:var(--text);font-family:var(--font);font-size:14px;line-height:1.45;-webkit-font-smoothing:antialiased}
h1,h2,h3,h4{margin:0;font-weight:700;line-height:1.25}
a{color:var(--accent)}
button{font-family:var(--font)}
.app{display:grid;grid-template-columns:1fr;grid-template-rows:auto 1fr;min-height:100vh}
.topbar{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:0 18px;height:58px;background:var(--surface);border-bottom:1px solid var(--border);position:sticky;top:0;z-index:50}
.brand{display:flex;align-items:center;gap:9px;font-size:17px;font-weight:800;letter-spacing:.2px}
.brand-sub{font-size:10px;font-weight:700;color:var(--accent);background:var(--surface-2);border:1px solid var(--border);padding:2px 7px;border-radius:999px}
.top-actions{display:flex;align-items:center;gap:14px}
.status-chip{display:inline-flex;align-items:center;gap:6px;font-size:12px;font-weight:600;color:var(--text-muted)}
.lang-switch{display:flex;border:1px solid var(--border);border-radius:999px;overflow:hidden;background:var(--surface-2)}
.langbtn{border:none;background:transparent;color:var(--text-muted);padding:4px 12px;font-size:12px;font-weight:800;letter-spacing:.4px;cursor:pointer;transition:background .15s,color .15s}
.langbtn:hover{background:var(--surface-hover);color:var(--text)}
.langbtn.active{background:var(--accent);color:var(--on-accent)}
#themePicker{display:flex;gap:7px}
.themeDot{width:20px;height:20px;border-radius:50%;cursor:pointer;border:2px solid var(--border);padding:0;transition:transform .15s,box-shadow .15s;background:var(--surface-2)}
.themeDot:hover{transform:scale(1.15)}
.themeDot:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.themeDot[data-bg="#414141"]{background:#414141}.themeDot[data-bg="#4c834e"]{background:#4c834e}.themeDot[data-bg="#c1af8d"]{background:#c1af8d}.themeDot[data-bg="#f7f2ff"]{background:#f7f2ff}
.nav{background:var(--surface);border-right:1px solid var(--border);padding:14px 10px;display:flex;flex-direction:column;gap:4px}
.navitem{display:flex;align-items:center;gap:10px;width:100%;padding:10px 12px;border:none;border-radius:var(--radius-sm);background:transparent;color:var(--text-muted);font-weight:600;font-size:13px;cursor:pointer;text-align:left;transition:background .15s,color .15s}
.navitem:hover{background:var(--surface-hover);color:var(--text)}
.navitem.active{background:var(--accent);color:var(--on-accent)}
.navitem svg{flex:0 0 auto}
.nav-sep{height:1px;background:var(--border);margin:6px 4px}
.main{padding:22px;width:100%;max-width:1280px}
.pages{}
.page-head{display:flex;align-items:flex-end;justify-content:space-between;gap:12px;margin-bottom:18px;flex-wrap:wrap}
.page-head h1{font-size:20px}
.page-sub{color:var(--text-muted);font-size:13px;margin:2px 0 0}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;padding:9px 14px;border-radius:var(--radius-sm);border:1px solid transparent;background:var(--surface-2);color:var(--text);font-weight:600;font-size:13px;cursor:pointer;transition:background .15s,border-color .15s}
.btn:hover{background:var(--surface-hover)}
.btn.primary{background:var(--accent);color:var(--on-accent)}
.btn.primary:hover{background:var(--accent-hover)}
.btn.ghost{background:transparent;border-color:var(--border)}
.btn.ghost:hover{background:var(--surface-hover)}
.btn.danger{background:var(--danger);color:#fff}
.btn.sm{padding:6px 10px;font-size:12px}
.chip{display:inline-flex;align-items:center;gap:5px;padding:2px 8px;border-radius:999px;font-size:10px;font-weight:700;letter-spacing:.6px;text-transform:uppercase;background:var(--surface-2);color:var(--text-muted);border:1px solid var(--border)}
.chip.ok{background:rgba(34,197,94,.14);color:var(--success);border-color:rgba(34,197,94,.35)}
.chip.danger{background:rgba(239,68,68,.14);color:var(--danger);border-color:rgba(239,68,68,.35)}
button.chip{cursor:pointer}
button.chip:hover{background:var(--surface-hover);color:var(--text)}
button.chip.ok:hover{background:rgba(34,197,94,.22);color:var(--success)}
button.chip:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);box-shadow:var(--shadow-mild);padding:14px}
.dot{width:8px;height:8px;border-radius:50%;display:inline-block;background:var(--text-muted);flex:0 0 auto}
.dot.ok{background:var(--success)}.dot.warn{background:var(--warning)}.dot.bad{background:var(--danger)}.dot.info{background:var(--info)}
.status{display:inline-flex;align-items:center;gap:5px;font-size:10px;font-weight:700;color:var(--text-muted);letter-spacing:.4px;text-transform:uppercase}
.metric{font-size:26px;font-weight:800;font-variant-numeric:tabular-nums;line-height:1.1;text-align:center}
.metric.sm{font-size:18px}
.metric .unit{font-size:15px;font-weight:700;color:var(--text-muted);margin-left:2px}
.eyebrow{font-size:10px;font-weight:700;letter-spacing:.8px;color:var(--accent);text-transform:uppercase}
.notice{display:flex;align-items:center;gap:8px;padding:10px 14px;border-radius:var(--radius-sm);font-size:13px;margin-bottom:14px;font-weight:600}
.notice.success{background:rgba(34,197,94,.12);color:var(--success);border:1px solid rgba(34,197,94,.35)}
.field{display:flex;flex-direction:column;gap:6px;margin-bottom:12px}
.field label{font-size:12px;font-weight:600;color:var(--text)}
.input,select,input[type=date],input[type=number],input[type=password]{width:100%;padding:9px 11px;border-radius:var(--radius-sm);border:1px solid var(--border);background:var(--bg-2);color:var(--text);font:inherit;font-size:13px}
.input:focus,select:focus,input[type=date]:focus,input[type=number]:focus,input[type=password]:focus{border-color:var(--accent);outline:none;box-shadow:0 0 0 3px rgba(59,130,246,.18)}
select option{background:var(--surface)}
.input.sm{width:auto;min-width:90px;padding:7px 9px}
.check{display:flex;align-items:center;gap:8px;font-size:13px;color:var(--text);cursor:pointer;padding:4px 0}
.check input[type=checkbox]{width:16px;height:16px;accent-color:var(--accent);cursor:pointer}
.field-row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.btn-row{display:flex;gap:8px;flex-wrap:wrap;margin:4px 0 0}
.kv{display:flex;justify-content:space-between;gap:8px;font-size:13px;padding:3px 0}
.kv span{color:var(--text-muted)}




.switch{position:relative;display:inline-flex;align-items:center;width:46px;height:26px;flex:0 0 auto;cursor:pointer;border-radius:999px;background:var(--border);border:1px solid var(--border);padding:0;transition:background .2s}
.switch .knob{position:absolute;top:2px;left:2px;width:20px;height:20px;border-radius:50%;background:var(--text-muted);transition:transform .2s,background .2s;pointer-events:none}
.switch.on{background:var(--accent);border-color:var(--accent)}
.switch.on .knob{transform:translateX(20px);background:var(--on-accent)}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:8px;border-radius:999px;background:var(--border);outline:none;margin:12px 0;cursor:pointer;border:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:20px;height:20px;border-radius:50%;background:var(--accent);border:2px solid var(--surface);box-shadow:0 1px 4px rgba(0,0,0,.4);cursor:pointer}
input[type=range]::-moz-range-thumb{width:16px;height:16px;border-radius:50%;background:var(--accent);border:none;cursor:pointer}
.dash-stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px;margin-bottom:18px}
.stat-card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);padding:14px 16px;display:flex;flex-direction:column;gap:3px;box-shadow:var(--shadow-mild)}
.stat-label{font-size:11px;font-weight:700;letter-spacing:.6px;text-transform:uppercase;color:var(--text-muted)}
.stat-value{font-size:26px;font-weight:800;font-variant-numeric:tabular-nums}
.dash-section{margin-bottom:18px}
.section-title{font-size:12px;font-weight:700;letter-spacing:.8px;text-transform:uppercase;color:var(--text-muted);margin-bottom:10px}
.devices-dashboard{}
.devices-desktop-layout{display:grid;gap:18px;align-items:start;grid-template-columns:1fr}
.devices-column{min-height:0;border:1px solid var(--border);border-radius:var(--radius-md);padding:14px;background:var(--surface-2);display:flex;flex-direction:column;gap:12px}
.devices-mobile-list{display:none}
.devices-actuator-grid,.devices-sensor-grid{display:grid;gap:12px;grid-template-columns:repeat(auto-fill,minmax(180px,1fr))}
@media (min-width:1100px){.devices-desktop-layout{grid-template-columns:minmax(280px,38%) minmax(440px,1fr)}}
@media (max-width:899px){.devices-mobile-list{display:grid;gap:12px}.devices-desktop-layout{display:none}}
.device-card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);box-shadow:var(--shadow-mild);padding:14px;display:flex;flex-direction:column;gap:10px}
.device-head{display:flex;align-items:center;justify-content:space-between;gap:8px}
.device-name{font-weight:700;font-size:14px;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;margin-right:auto}
.device-row{display:flex;align-items:center;justify-content:space-between;gap:10px}
.dimmer-row{display:flex;align-items:flex-end;justify-content:space-between;gap:10px;margin-top:2px}
.state-text{font-weight:800;font-size:13px;letter-spacing:.5px}
.state-text.on{color:var(--success)}.state-text.off{color:var(--text-muted)}
.value-lg{font-size:24px;font-weight:800;font-variant-numeric:tabular-nums;color:var(--accent)}
.time-value{text-align:center;font-variant-numeric:tabular-nums;font-weight:700}
.settings-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(290px,1fr));gap:14px}
.settings-general{grid-column:1/-1}
.settings-row{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:14px}
.settings-card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);box-shadow:var(--shadow-mild);padding:16px;display:flex;flex-direction:column;gap:12px}
.settings-card h3{margin:2px 0 0;font-size:15px}

.settings-card-head{display:flex;justify-content:space-between;align-items:flex-start;gap:8px}
.rule-list{display:flex;flex-direction:column;gap:12px}
.rule-card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);box-shadow:var(--shadow-mild);padding:14px 16px;display:flex;flex-direction:column;gap:8px}
.rule-head{display:flex;justify-content:space-between;align-items:center;gap:8px;flex-wrap:wrap}
.rule-head h3{font-size:14px}
.rule-actions{display:flex;gap:6px}
.rule-info{display:flex;justify-content:space-between;gap:12px;font-size:13px}
.rule-info span{color:var(--text-muted);font-size:12px}
.rule-info b{font-weight:600}
.empty{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;text-align:center;color:var(--text-muted);padding:40px 16px;background:var(--surface);border:1px dashed var(--border);border-radius:var(--radius-md);font-size:14px}
.empty.sm{padding:18px}
.log-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:12px}
.log-panel{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);padding:12px;min-height:260px;max-height:56vh;overflow-y:auto;font-family:var(--mono);font-size:12px;line-height:1.5}
.log-panel h3{margin:0 0 10px;font-size:11px;letter-spacing:1px;text-transform:uppercase;color:var(--text-muted);border-bottom:1px solid var(--border);padding-bottom:8px;text-align:center}
.log-header{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:12px;flex-wrap:wrap}
.log-entry{margin-bottom:5px;padding:4px 8px;border-radius:4px;background:var(--bg-2);word-break:break-all;border-left:3px solid transparent}
.log-entry.core{border-left-color:var(--info)}
.log-entry.evnt{border-left-color:var(--warning)}
.log-entry.sens{border-left-color:var(--success)}
.log-entry .t{color:var(--text-muted);font-size:10px;margin-right:4px}
.log-entry .l{font-weight:700;margin:0 4px;font-size:10px}
.log-entry .l.inf{color:var(--success)}.log-entry .l.wrn{color:var(--warning)}.log-entry .l.err{color:var(--danger)}
.auto-actions{display:flex;gap:8px}
.modal{position:fixed;inset:0;display:none;align-items:center;justify-content:center;padding:16px;background:rgba(0,0,0,.55);z-index:1000;animation:modalIn .15s ease}
@keyframes modalIn{from{opacity:0}to{opacity:1}}
.modal-content{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-lg);width:400px;max-width:94vw;max-height:88vh;overflow-y:auto;padding:18px;box-shadow:var(--shadow-lg)}
.modal-head{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:12px}
.modal-head h2{margin:0;font-size:17px}
.icon-btn{border:none;background:var(--surface-2);color:var(--text-muted);width:30px;height:30px;border-radius:var(--radius-sm);cursor:pointer;font-size:16px;line-height:1}
.icon-btn:hover{background:var(--surface-hover);color:var(--text)}
.modal-foot{display:flex;justify-content:flex-end;padding-top:12px;margin-top:12px;border-top:1px solid var(--border)}
.wizard-step{display:flex;flex-direction:column;gap:12px}
.wizard-option{display:flex;gap:10px;align-items:flex-start;padding:11px 12px;border:2px solid var(--border);border-radius:var(--radius-md);cursor:pointer;background:var(--surface-2)}
.wizard-option:has(input:checked){border-color:var(--accent);background:var(--surface)}
.wizard-option input{margin-top:4px;accent-color:var(--accent)}
.wizard-option strong{display:block;font-size:13px}
.wizard-option small{display:block;color:var(--text-muted);font-size:12px;margin-top:2px}
.wizard-nav{display:flex;justify-content:space-between;gap:8px;border-top:1px solid var(--border);padding-top:12px;margin-top:4px}
.wizard-hint{font-size:12px;color:var(--text-muted)}
.cond-box,.action-box{border:1px solid var(--border);border-radius:var(--radius-md);padding:10px 12px;display:flex;flex-direction:column;gap:8px}
.small-note{font-size:11px;color:var(--text-muted)}
@media (min-width:900px){
.app{grid-template-columns:220px 1fr;grid-template-rows:auto 1fr}
.topbar{grid-column:1/-1}
.nav{grid-row:2;grid-column:1;min-height:calc(100vh - 58px);position:sticky;top:58px;align-self:start;height:calc(100vh - 58px)}
.main{grid-column:2;grid-row:2;padding:24px}
}
@media (max-width:899px){
.nav{position:fixed;left:0;right:0;bottom:0;top:auto;flex-direction:row;justify-content:space-around;gap:0;padding:6px 4px;border-top:1px solid var(--border);border-right:none;z-index:110;background:var(--surface);box-shadow:0 -4px 16px rgba(0,0,0,.25)}
.navitem{flex:1;flex-direction:column;gap:3px;padding:6px;font-size:10px;justify-content:center;text-align:center}
.navitem svg{width:20px;height:20px}
.nav-sep{display:none}
.main{padding:16px 14px 80px}
.topbar{height:54px;padding:0 14px}
}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style></head><body>
)rawliteral";


const char Tabs[] PROGMEM = R"rawliteral(
<div class="app">
<header class="topbar">
  <div class="brand"><span>Qymeras</span><span class="brand-sub">1.1</span></div>
  <div class="top-actions">
    <span class="status-chip" id="linkStatus"><span class="dot ok"></span><span data-i18n="chip.online">Online</span></span>
    <div id="themePicker" role="group" aria-label="Tema de color">
      <button type="button" class="themeDot" data-bg="#414141" aria-label="Tema oscuro"></button>
      <button type="button" class="themeDot" data-bg="#f7f2ff" aria-label="Tema claro"></button>
      <button type="button" class="themeDot" data-bg="#4c834e" aria-label="Tema forest"></button>
      <button type="button" class="themeDot" data-bg="#c1af8d" aria-label="Tema sand"></button>
    </div>
    <div class="lang-switch" role="group" aria-label="Idioma / Language">
      <button type="button" class="langbtn" data-lang="es" aria-label="Español">ES</button>
      <button type="button" class="langbtn" data-lang="en" aria-label="English">EN</button>
    </div>
  </div>
</header>
<nav class="nav" aria-label="Navegación principal">
  <button type="button" class="navitem active" id="t_control" onclick="show('control')"><svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/></svg><span data-i18n="nav.devices">Devices</span></button>
  <button type="button" class="navitem" id="t_auto" onclick="show('auto')"><svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><path d="M13 2 3 14h7l-1 8 11-13h-7z" stroke-linejoin="round"/></svg><span data-i18n="nav.automations">Automations</span></button>
  <span class="nav-sep"></span>
  <button type="button" class="navitem" id="t_config" onclick="show('config')"><svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .34 1.87l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.7 1.7 0 0 0-1.87-.34 1.7 1.7 0 0 0-1 1.55V21a2 2 0 1 1-4 0v-.09a1.7 1.7 0 0 0-1-1.55 1.7 1.7 0 0 0-1.87.34l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.7 1.7 0 0 0 .34-1.87 1.7 1.7 0 0 0-1.55-1H3a2 2 0 1 1 0-4h.09a1.7 1.7 0 0 0 1.55-1 1.7 1.7 0 0 0-.34-1.87l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.7 1.7 0 0 0 1.87.34h.01a1.7 1.7 0 0 0 1-1.55V3a2 2 0 1 1 4 0v.09a1.7 1.7 0 0 0 1 1.55 1.7 1.7 0 0 0 1.87-.34l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.7 1.7 0 0 0-.34 1.87v.01a1.7 1.7 0 0 0 1.55 1H21a2 2 0 1 1 0 4h-.09a1.7 1.7 0 0 0-1.55 1z" stroke-linejoin="round"/></svg><span data-i18n="nav.settings">Settings</span></button>
  <button type="button" class="navitem" id="t_logs" onclick="show('logs')"><svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><path d="M8 6h13M8 12h13M8 18h13" stroke-linecap="round"/><circle cx="4" cy="6" r="1.2" fill="currentColor"/><circle cx="4" cy="12" r="1.2" fill="currentColor"/><circle cx="4" cy="18" r="1.2" fill="currentColor"/></svg><span data-i18n="nav.logs">Logs</span></button>
</nav>
<main class="main">
  <section id="control" class="view content">
    <div class="pages">
      <div class="page-head">
        <div><h1 data-i18n="page.devices">Devices</h1><p class="page-sub" data-i18n="page.devices.sub">Real-time views of actuators and sensors</p></div>
      </div>
      <div id="devices_cards"></div>
    </div>
  </section>
  <section id="auto" class="view content" style="display:none">
    <div class="pages">
      <div class="page-head">
        <div><h1 data-i18n="page.auto">Automations</h1><p class="page-sub" data-i18n="page.auto.sub">Automation rules</p></div>
        <button class="btn primary" onclick="newRule()"><svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2.2" aria-hidden="true"><path d="M12 5v14M5 12h14" stroke-linecap="round"/></svg><span data-i18n="btn.newRule">New rule</span></button>
      </div>
      <div id="auto_table"></div>
      <div class="empty" id="auto_empty" style="display:none"><span data-i18n="rule.empty">There are no automation rules.</span><br><span data-i18n="rule.emptyHint">Create the first one to get started.</span></div>
    </div>
  </section>
  <section id="config" class="view content" style="display:none">
    <div class="pages">
      <div class="page-head">
        <div><h1 data-i18n="page.config">Settings</h1><p class="page-sub" data-i18n="page.config.sub">Calibration, persistence and node configuration</p></div>
      </div>
      <div id="savedNotice" class="notice success" style="display:none" data-i18n="saved.notice">Settings saved. The device is restarting...</div>
      <div id="cards"></div>
    </div>
  </section>
  <section id="logs" class="view content" style="display:none">
    <div class="pages">
      <div class="page-head">
        <div><h1 data-i18n="page.logs">Logs</h1><p class="page-sub" data-i18n="page.logs.sub">Auto-refresh every 2 s</p></div>
        <button class="btn ghost" onclick="refreshLogs()"><span data-i18n="btn.refresh">Refresh</span></button>
      </div>
      <div class="log-grid">
        <div class="log-panel" id="log-core"><h3 data-i18n="log.core">Core</h3></div>
        <div class="log-panel" id="log-events"><h3 data-i18n="log.events">Events</h3></div>
        <div class="log-panel" id="log-sensors"><h3 data-i18n="log.sensors">Sensors / Warn / Error</h3></div>
      </div>
    </div>
  </section>
</main>
</div>
<script>
async function show(tab){
  window.activeTab = tab;
  document.querySelectorAll('.content').forEach(c=>c.style.display='none');
  document.querySelectorAll('.navitem').forEach(t=>t.classList.remove('active'));
  document.getElementById(tab).style.display='block';
  document.getElementById('t_'+tab).classList.add('active');
  localStorage.setItem('tab',tab);
  if(tab==='control') loadDevices();
  if(tab==='auto') loadRules();
  if (tab === 'config'){ await loadCalib(); await syncOtaCheckbox(); }
  if (tab === 'logs'){ refreshLogs(); startLogAutoRefresh(); }
    else { stopLogAutoRefresh(); }
}

/* -------------------- I18N -------------------- */

const I18N = {
es: {
  title:'Qymeras 1.1', nav:{
  devices:'Dispositivos', automations:'Automatizaciones', settings:'Ajustes', logs:'Registros'}, page:{
  devices:'Dispositivos', 'devices.sub':'Vistas de actuadores y sensores en tiempo real',
  auto:'Automatizaciones', 'auto.sub':'Reglas de automatización',
  config:'Ajustes', 'config.sub':'Calibración, persistencia y configuración del nodo',
  logs:'Registros', 'logs.sub':'Actualización automática cada 2 s'}, btn:{
  newRule:'Nueva regla', refresh:'Refrescar'}, saved:{
  notice:'Ajustes guardados. El dispositivo se está reiniciando...'}, log:{
  core:'Core', events:'Eventos', sensors:'Sensores / Aviso / Error'}, stat:{
  actuators:'Actuadores', sensors:'Sensores', updated:'Actualizado', time:'Tiempo'}, chip:{
  enabled:'Habilitado', disabled:'Deshabilitado', online:'En línea'}, status:{
  local:'Local', remote:'Remoto', offline:'Desconectado'}, dev:{
  on:'ON', off:'OFF', drag:'Arrastra para ajustar nivel'}, no:{
  actuators:'No hay actuadores', sensors:'No hay sensores'}, air:{
  good:'Bueno', warn:'Aviso', bad:'Malo'}, yn:{ yes:'Sí', no:'No', closed:'Cerrado', open:'Abierto'}, cal:{
  'btn.set0':'Fijar 0%', 'btn.set100':'Fijar 100%', 'btn.ref':'Fijar valor', 'btn.reset':'Resetear',
  'btn.fade':'Fijar fade', 'ph.ref':'Valor', 'ph.fade':'Fade (ms)', 'ph.pulse':'Tiempo de pulso (ms)',
  'check.persist':'Persistencia', 'check.pulse':'Modo pulso (ms)'}, timezone:{
  title:'Zona horaria'}, cfg:{
  title:'Configuración del nodo', broadcast:'Puerto broadcast', command:'Puerto comando',
  interval:'Intervalo de reporte', 'ph.broadcast':'Broadcast', 'ph.command':'Command',
  'ph.interval':'Intervalo (ms)', ota:'Arduino OTA', save:'Guardar', factory:'Restablecer a fábrica'}, net:{
  title:'Red del nodo', ssid:'SSID', 'ssid.ph':'Nombre de la red', pass:'Contraseña',
  'pass.ph':'Contraseña WiFi', save:'Guardar y reiniciar',
  note:'Tras guardar, el dispositivo se reiniciará y se conectará a la nueva red.'}, alert:{
  saved:'Guardado', isVirtualTimeout:'Tiempo de espera agotado: {ip} no respondió',
  isVirtualNet:'Error de red: no se pudo contactar a {ip}',
  isVirtualHttp:'Error en {ip} (HTTP {status}{detail})', localError:'Error local (HTTP {status}{detail})',
  localNet:'Error de red al contactar este dispositivo'}, factory:{
  confirm:'¿Seguro? Esto borrará todos los ajustes y la información.', doing:'Reiniciando...',
  err:'Error enviando reset'}, ota:{ enableMsg:'El dispositivo se reiniciará para activar OTA. ¿Continuar?',
  disableMsg:'El dispositivo se reiniciará para desactivar OTA. ¿Continuar?'}, wiz:{
  titleNew:'Nueva regla de automatización', titleEdit:'Editar regla de automatización', cancel:'Cancelar',
  back:'Atrás', next:'Siguiente', save:'Guardar', type:'Tipo de regla',
  edge:'EDGE - Cambios de estado', 'edge.desc':'Se ejecuta cuando un sensor cambia de estado',
  thresh:'THRESHOLD - Valores límite', 'thresh.desc':'Se ejecuta cuando un valor cruza el umbral',
  time:'TIME - A una hora', 'time.desc':'Ejecuta a una hora fija dentro de un rango de fechas',
  intervalTxt:'INTERVAL - Cada X tiempo', 'interval.desc':'Ejecuta periódicamente cada intervalo',
  sensors:'Seleccionar sensores', hint:'Ctrl/Cmd + clic para seleccionar varios',
  noSensors:'Sin sensores', conditions:'Condiciones', rising:'Subida', falling:'Caída',
  dates:'Rango de fechas y hora', from:'Desde cuándo:', to:'Hasta cuándo:', runAt:'Hora de ejecución:',
  logic:'Lógica', all:'TODAS las condiciones (AND)', any:'AL MENOS UNA condición (OR)',
  actuators:'Actuadores', noActuators:'Sin actuadores', actions:'Acciones',
  intervalMs:'Intervalo (ms)', delayMs:'Delay (ms)', cooldownMs:'Cooldown (ms)',
  selectType:'Debes seleccionar un tipo de regla', needSensor:'Debes seleccionar al menos un sensor para este tipo de regla',
  needThreshold:'Debes completar el threshold para el sensor', threshRange:'El threshold debe estar entre -1000 y 10000',
  timeRange:'La hora debe estar entre 00:00 y 23:59', dateOrder:'La fecha "desde" no puede ser posterior a "hasta"',
  needActuator:'Debes seleccionar al menos un actuador', loadActions:'Error al cargar las acciones',
  loadActuator:'Error al cargar el actuador', levelDimmerOnly:'La acción LEVEL solo se puede usar en dimmers',
  levelRange:'El level debe estar entre 0 y 100', delayRange:'El delay debe estar entre 0 y 60000 ms',
  cooldownRange:'El cooldown debe estar entre 0 y 3600000 ms', intervalRange:'El intervalo debe estar entre 1000 y 3600000 ms',
  hourRange:'La hora debe estar entre 0 y 23', minRange:'Los minutos deben estar entre 0 y 59',
  levelActuator:'El level del actuador debe estar entre 0 y 100 (actual: {level})',
  saved:'Regla guardada correctamente', saveError:'Error al guardar:', connError:'Error de conexión:',
  deleteConfirm:'¿Eliminar la regla {id}?', edit:'Editar', delete:'Eliminar'}, rule:{
  sensors:'Sensor(es)', condition:'Condición', logic:'Lógica', actions:'Acciones', actuators:'Actuador(es)',
  delay:'Delay / Cooldown', title:'Regla', empty:'No hay reglas de automatización.',
  emptyHint:'Crea la primera para empezar.'}
},
en: {
  title:'Qymeras 1.1', nav:{
  devices:'Devices', automations:'Automations', settings:'Settings', logs:'Logs'}, page:{
  devices:'Devices', 'devices.sub':'Real-time views of actuators and sensors',
  auto:'Automations', 'auto.sub':'Automation rules',
  config:'Settings', 'config.sub':'Calibration, persistence and node configuration',
  logs:'Logs', 'logs.sub':'Auto-refresh every 2 s'}, btn:{
  newRule:'New rule', refresh:'Refresh'}, saved:{
  notice:'Settings saved. The device is restarting...'}, log:{
  core:'Core', events:'Events', sensors:'Sensors / Warn / Error'}, stat:{
  actuators:'Actuators', sensors:'Sensors', updated:'Updated', time:'Time'}, chip:{
  enabled:'Enabled', disabled:'Disabled', online:'Online'}, status:{
  local:'Local', remote:'Remote', offline:'Offline'}, dev:{
  on:'ON', off:'OFF', drag:'Drag to adjust level'}, no:{
  actuators:'No actuators', sensors:'No sensors'}, air:{
  good:'Good', warn:'Warn', bad:'Bad'}, yn:{ yes:'Yes', no:'No', closed:'Closed', open:'Open'}, cal:{
  'btn.set0':'Set 0%', 'btn.set100':'Set 100%', 'btn.ref':'Set ref value', 'btn.reset':'Reset',
  'btn.fade':'Set fade', 'ph.ref':'Value', 'ph.fade':'Fade (ms)', 'ph.pulse':'Pulse time (ms)',
  'check.persist':'Persistence', 'check.pulse':'Pulse mode (ms)'}, timezone:{
  title:'Time zone'}, cfg:{
  title:'Node configuration', broadcast:'Broadcast port', command:'Command port',
  interval:'Report interval', 'ph.broadcast':'Broadcast', 'ph.command':'Command',
  'ph.interval':'Interval (ms)', ota:'Arduino OTA', save:'Save', factory:'Factory reset'}, net:{
  title:'Node network', ssid:'SSID', 'ssid.ph':'Network name', pass:'Password',
  'pass.ph':'WiFi password', save:'Save & restart',
  note:'After saving, the device will restart and connect to the new network.'}, alert:{
  saved:'Saved', isVirtualTimeout:'Aborted: {ip} did not respond',
  isVirtualNet:'Network error: could not reach {ip}',
  isVirtualHttp:'Error in {ip} (HTTP {status}{detail})', localError:'Local error (HTTP {status}{detail})',
  localNet:'Network error contacting this device'}, factory:{
  confirm:'Are you sure? This will erase all settings and data.', doing:'Restarting...',
  err:'Error sending reset'}, ota:{ enableMsg:'The device will restart to enable OTA. Continue?',
  disableMsg:'The device will restart to disable OTA. Continue?'}, wiz:{
  titleNew:'New automation rule', titleEdit:'Edit automation rule', cancel:'Cancel',
  back:'Back', next:'Next', save:'Save', type:'Rule type',
  edge:'EDGE - State changes', 'edge.desc':'Runs when a sensor changes state',
  thresh:'THRESHOLD - Threshold values', 'thresh.desc':'Runs when a value crosses a threshold',
  time:'TIME - At a fixed time', 'time.desc':'Runs at a fixed time within a date range',
  intervalTxt:'INTERVAL - Every X time', 'interval.desc':'Runs periodically every interval',
  sensors:'Select sensors', hint:'Ctrl/Cmd + click to select several',
  noSensors:'No sensors', conditions:'Conditions', rising:'Rising', falling:'Falling',
  dates:'Date range and time', from:'From:', to:'Until:', runAt:'Execution time:',
  logic:'Logic', all:'ALL conditions (AND)', any:'AT LEAST ONE condition (OR)',
  actuators:'Actuators', noActuators:'No actuators', actions:'Actions',
  intervalMs:'Interval (ms)', delayMs:'Delay (ms)', cooldownMs:'Cooldown (ms)',
  selectType:'Please select a rule type', needSensor:'Select at least one sensor for this rule type',
  needThreshold:'You must fill in the threshold for the sensor', threshRange:'The threshold must be between -1000 and 10000',
  timeRange:'The time must be between 00:00 and 23:59', dateOrder:'The start date cannot be after the end date',
  needActuator:'Select at least one actuator', loadActions:'Error loading the actions',
  loadActuator:'Error loading the actuator', levelDimmerOnly:'The LEVEL action can only be used on dimmers',
  levelRange:'The level must be between 0 and 100', delayRange:'The delay must be between 0 and 60000 ms',
  cooldownRange:'Cooldown must be between 0 and 3600000 ms', intervalRange:'The interval must be between 1000 and 3600000 ms',
  hourRange:'Hour must be between 0 and 23', minRange:'Minutes must be between 0 and 59',
  levelActuator:'The actuator level must be between 0 and 100 (current: {level})',
  saved:'Rule saved successfully', saveError:'Save error:', connError:'Connection error:',
  deleteConfirm:'Delete rule {id}?', edit:'Edit', delete:'Delete'}, rule:{
  sensors:'Sensor(s)', condition:'Condition', logic:'Logic', actions:'Actions', actuators:'Actuator(s)',
  delay:'Delay / Cooldown', title:'Rule', empty:'There are no automation rules.',
  emptyHint:'Create the first one to get started.'}
}
};

const SENSOR_LABEL = {
  1:['Luminosidad','Luminosity'], 2:['Humedad','Humidity'], 3:['Temperatura','Temperature'],
  4:['Presión','Pressure'], 5:['Nivel','Level'], 6:['Calidad del aire','Air quality'],
  7:['Lluvia','Rain'], 8:['Dimmer','Dimmer'], 9:['Relé','Relay'],
  10:['Hora','Time'], 11:['Personalizado','Custom'], 12:['Contacto','Contact']
};

let LANG = 'es';
try { LANG = localStorage.getItem('lang') || 'es'; } catch(e) {}

const I18N_FLAT = (() => {
  const flat = (d, prefix, out) => {
    for (const k of Object.keys(d)) {
      const key = prefix ? prefix + '.' + k : k;
      const v = d[k];
      if (v && typeof v === 'object') flat(v, key, out);
      else out[key] = v;
    }
    return out;
  };
  return { es: flat(I18N.es, '', {}), en: flat(I18N.en, '', {}) };
})();

function t(k) {
  const d = I18N_FLAT[LANG] || I18N_FLAT.es;
  let v = d[k];
  if (v == null) v = I18N_FLAT.es[k];
  return v == null ? k : v;
}

function tf(k, map) {
  let s = t(k);
  if (map) for (const m in map) s = s.split('{' + m + '}').join(map[m]);
  return s;
}

function sLabel(type) {
  const a = SENSOR_LABEL[type];
  return a ? a[LANG === 'en' ? 1 : 0] : 'GENERAL';
}
)rawliteral";


const char Rules[] PROGMEM = R"rawliteral(
function renderAutomationTable(rules){
  let html = '<div class="rule-list">';
  if(!rules || !rules.length){
    html += '<div class="empty">' + t('rule.empty') + '</div>';
  } else {
    rules.forEach((r,i)=>{
      html += `
      <div class="rule-card">
        <div class="rule-head">
          <span class="chip neutral">${['EDGE','THRESHOLD','TIME','INTERVAL'][r.type] || 'RULE'} #${r.id}</span>
          <h3>${r.name || t('rule.title')}</h3>
          <div class="rule-actions"></div>
        </div>
        <div class="rule-info"><span>${t('rule.sensors')}</span><b>${r.sensors.join(", ")}</b></div>
        <div class="rule-info"><span>${t('rule.logic')}</span><b>${r.logical_and?'AND':'OR'}</b></div>
        <div class="rule-info"><span>${t('rule.actions')}</span><b>${r.actions.join(", ")}</b></div>
        <div class="rule-info"><span>${t('rule.delay')}</span><b>${r.delay_ms} / ${r.cooldown_ms} ms</b></div>
        <div class="rule-actions">
          <button class="btn ghost sm" onclick="editRule(${i})">${t('wiz.edit')}</button>
          <button class="btn danger sm" onclick="deleteRule(${i})">${t('wiz.delete')}</button>
        </div>
      </div>`;
    });
  }
  html += '</div>';
  document.getElementById("auto_table").innerHTML = html;
}
function newRule(){
  loadSensorsAndActuators().then(()=>{
    document.getElementById('ruleModalTitle').textContent=t('wiz.titleNew');
    startWizard();
    document.getElementById('ruleModal').style.display='flex';
  });
}
)rawliteral";


const char CardsSettings[] PROGMEM = R"rawliteral(
function sensorCalibCard(s, i, cfg) {
  const minMaxBtns = cfg.hasMinMax ? `
    <div class="field-row">
      <button class="btn ghost sm" onclick='setCalib(${i},"min","${s.name}")'>${t('cal.btn.set0')}</button>
      <button class="btn ghost sm" onclick='setCalib(${i},"max","${s.name}")'>${t('cal.btn.set100')}</button>
    </div>` : '';
  return `<div class="settings-card">
    <div class="settings-card-head">
      <div><span class="eyebrow">${cfg.label}</span><h3>${s.name}</h3></div>
      <button type="button" onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id="matterBtn${i}" data-name="${s.id}" class="chip ${s.avail?'ok':''}" aria-pressed="${s.avail?'true':'false'}">${s.avail ? t('chip.enabled') : t('chip.disabled')}</button>
    </div>
    <div class="metric" id="v${i}">${cfg.format(s.value)}</div>
    <div class="field-row">
      <input id="ref${i}" class="input sm" placeholder="${t('cal.ph.ref')}">
      <button class="btn ghost sm" onclick='setCalib(${i},"ref","${s.name}")'>${t('cal.btn.ref')}</button>
    </div>
    ${minMaxBtns}
    <div class="btn-row">
      <button class="btn ghost sm" onclick='setCalib(${i},"res","${s.name}")'>${t('cal.btn.reset')}</button>
    </div>
    
  </div>`;
}

const cardRenderers = {

HUMI: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : v + ' %', hasMinMax: true }),

LEVE: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : v + ' %', hasMinMax: true }),

LUMI: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : (v * 108.9432 / 7074).toFixed(0) + ' lx', hasMinMax: false }),

TEMP: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : v.toFixed(2) + ' °C', hasMinMax: false }),

PRES: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : v.toFixed(2) + ' kPa', hasMinMax: false }),

GENERIC: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : Number(v).toFixed(2), hasMinMax: false }),

AIRQ: (s, i) => `<div class="settings-card">
  <div class="settings-card-head">
    <div><span class="eyebrow">${sLabel(s.type)}</span><h3>${s.name}</h3></div>
    <button type="button" onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id="matterBtn${i}" data-name="${s.id}" class="chip ${s.avail?'ok':''}" aria-pressed="${s.avail?'true':'false'}">${s.avail ? t('chip.enabled') : t('chip.disabled')}</button>
  </div>
  <div class="metric" id="v${i}">${s.value === 255 || s.value == null ? 'N/A' : s.value == 0 ? t('air.good') : s.value == 1 ? t('air.warn') : s.value == 2 ? t('air.bad') : 'N/A'}</div>
  
</div>`,

RAIN: (s, i) => `<div class="settings-card">
  <div class="settings-card-head">
    <div><span class="eyebrow">${sLabel(s.type)}</span><h3>${s.name}</h3></div>
    <button type="button" onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id="matterBtn${i}" data-name="${s.id}" class="chip ${s.avail?'ok':''}" aria-pressed="${s.avail?'true':'false'}">${s.avail ? t('chip.enabled') : t('chip.disabled')}</button>
  </div>
  <div class="metric" id="v${i}">${s.value === 255 || s.value == null ? 'N/A' : s.value ? t('yn.yes') : t('yn.no')}</div>
  
</div>`,

CONTACT: (s, i) => `<div class="settings-card">
  <div class="settings-card-head">
    <div><span class="eyebrow">${sLabel(s.type)}</span><h3>${s.name}</h3></div>
    <button type="button" onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id="matterBtn${i}" data-name="${s.id}" class="chip ${s.avail?'ok':''}" aria-pressed="${s.avail?'true':'false'}">${s.avail ? t('chip.enabled') : t('chip.disabled')}</button>
  </div>
  <div class="metric" id="v${i}">${s.value === 255 || s.value == null ? 'N/A' : s.state ? t('yn.closed') : t('yn.open')}</div>
  
</div>`,

DIMM: (s, i) => `<div class="settings-card">
  <div class="settings-card-head">
    <div><span class="eyebrow">${sLabel(s.type)}</span><h3>${s.name}</h3></div>
    <button type="button" onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id="matterBtn${i}" data-name="${s.id}" class="chip ${s.avail?'ok':''}" aria-pressed="${s.avail?'true':'false'}">${s.avail ? t('chip.enabled') : t('chip.disabled')}</button>
  </div>
  <div class="kv"><span>${t('cal.ph.fade')}</span><b id="v${i}">${s.fade}</b></div>
  <div class="field-row">
    <input id="ref${i}" class="input sm" placeholder="${t('cal.ph.fade')}" style="min-width:120px">
    <button class="btn ghost sm" onclick='setCalib(${i},"fad","${s.name}")'>${t('cal.btn.fade')}</button>
  </div>
  
</div>`,

REL: (s, i) => `<div class="settings-card">
  <div class="settings-card-head">
    <div><span class="eyebrow">${sLabel(s.type)}</span><h3>${s.name}</h3></div>
    <button type="button" onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id="matterBtn${i}" data-name="${s.id}" class="chip ${s.avail?'ok':''}" aria-pressed="${s.avail?'true':'false'}">${s.avail ? t('chip.enabled') : t('chip.disabled')}</button>
  </div>
  <label class="check"><input type="checkbox" id="persistChk${i}" ${s.persist ? 'checked' : ''} onchange='togglePersist(${i},"${s.name}")'>${t('cal.check.persist')}</label>
  <label class="check"><input type="checkbox" id="pulseChk${i}" ${s.pulse ? 'checked' : ''} onchange='togglePulse(${i},"${s.name}")'>${t('cal.check.pulse')}</label>
  <div class="field-row">
    <input id="ref${i}" class="input sm" placeholder="${t('cal.ph.pulse')}" value="${s.pulse_ms ?? ''}" onchange='setCalib(${i},"pulse","${s.name}",this.value)' style="min-width:120px;${s.pulse ? '' : 'display:none;'}">
  </div>
  
</div>`,

TIME: (s, i) => `<div class="settings-card">
  <div class="settings-card-head">
    <div><span class="eyebrow">${sLabel(s.type)}</span><h3>${t('timezone.title')}</h3></div>
  </div>
  <div class="metric sm" id="v${i}">--</div>
  <select onchange="setCalib(${i},'timezone','TIME',this.value)">
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

DEFAULT: (s, i) => `<div class="settings-card">
  <div class="settings-card-head">
    <div><span class="eyebrow">GENERAL</span><h3>${t('cfg.title')}</h3></div>
  </div>
  <div class="kv"><span>${t('cfg.broadcast')}</span><b>${genset.broadcast_port}</b></div>
  <div class="kv"><span>${t('cfg.command')}</span><b>${genset.command_port}</b></div>
  <div class="kv"><span>${t('cfg.interval')}</span><b>${genset.report_interval} ms</b></div>
  <div class="field-row">
    <input id="broadcast_port" class="input sm" placeholder="${t('cfg.ph.broadcast')}">
    <input id="command_port" class="input sm" placeholder="${t('cfg.ph.command')}">
    <input id="ref${i}" class="input sm" placeholder="${t('cfg.ph.interval')}">
  </div>
  <label class="check"><input type="checkbox" id="otaChk" onchange="toggleOta(this.checked)"> ${t('cfg.ota')}</label>
  <div class="btn-row">
    <button class="btn primary" onclick="setPort(${i})">${t('cfg.save')}</button>
    <button class="btn danger" onclick="factoryReset()">${t('cfg.factory')}</button>
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
  SENSOR_CONTACT: 12
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
  [SensorType.SENSOR_GENERIC]: 28
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
  return `${pad(t.getUTCHours())}:${pad(t.getUTCMinutes())}:${pad(t.getUTCSeconds())} - ${t.getUTCFullYear()}-${pad(t.getUTCMonth()+1)}-${pad(t.getUTCDate())}`;
}

function devStatus(s) {
  if (!s || s.local) return '<span class="status"><span class="dot ok"></span>' + t('status.local') + '</span>';
  return (s.age_ms != null && s.age_ms <= 30000)
    ? '<span class="status"><span class="dot info"></span>' + t('status.remote') + '</span>'
    : '<span class="status"><span class="dot bad"></span>' + t('status.offline') + '</span>';
}

function deviceCard(name, value, id, state, fade, type, sensor = null) {
  if (type === SensorType.SENSOR_TIME) return '';
  if (type === SensorType.TYPE_RELAY) {
    return `<div class="device-card" data-name="${name}" data-type="${type}">
      <div class="device-head">
        <span class="chip neutral">RELAY</span>
        <span class="device-name">${name}</span>
        <span class="state-text ${state?'on':'off'}" id="dev_${id}">${state ? t('dev.on') : t('dev.off')}</span>
      </div>
      <div class="device-row">
        ${devStatus(sensor)}
        <button type="button" class="switch ${state?'on':''}" role="switch" aria-checked="${state?state:false}" aria-label="Alternar relé ${name}" onclick="toggleDevice(${id})"><span class="knob"></span></button>
      </div>
    </div>`;
  }
  if (type === SensorType.TYPE_DIMMER) {
    const displayValue = state ? value : 0;
    return `<div class="device-card dimmer" data-name="${name}" data-type="${type}">
      <div class="device-head">
        <span class="chip neutral">DIMMER</span>
        <span class="device-name">${name}</span>
        <button type="button" class="switch ${state?'on':''}" role="switch" aria-checked="${state?state:false}" aria-label="Alternar dimmer ${name}" onclick="toggleDevice(${id})"><span class="knob"></span></button>
      </div>
      <div class="dimmer-row">
        <span class="value-lg"><b id="dev_val_${id}">${displayValue}</b><span class="unit">%</span></span>
        <span class="state-text ${state?'on':'off'}" id="dev_state_${id}">${state ? t('dev.on') : t('dev.off')}</span>
      </div>
      <input type="range" min="0" max="100" name="${name}" value="${displayValue}" id="slider_${id}" style="background:linear-gradient(to right,var(--accent) ${displayValue}%,var(--border) ${displayValue}%)" oninput="onDimmerInput(${id}, this.value)" onchange="onDimmerChange(${id}, this.value)" aria-label="Nivel ${name}">
      <div class="device-row">${devStatus(sensor)}<span class="small-note">${t('dev.drag')}</span></div>
    </div>`;
  }
  const SENSOR_DISPLAY = {
    [SensorType.SENSOR_TEMP]:  { label: () => sLabel(SensorType.SENSOR_TEMP), format: v => v.toFixed(2) + ' °C' },
    [SensorType.SENSOR_HUMI]:  { label: () => sLabel(SensorType.SENSOR_HUMI), format: v => v.toFixed(0) + ' %' },
    [SensorType.SENSOR_PRESS]: { label: () => sLabel(SensorType.SENSOR_PRESS), format: v => v.toFixed(0) + ' kPa' },
    [SensorType.SENSOR_AIRQ]:  { label: () => sLabel(SensorType.SENSOR_AIRQ), format: v => v == 0 ? t('air.good') : v == 1 ? t('air.warn') : v == 2 ? t('air.bad') : 'N/A' },
    [SensorType.SENSOR_RAIN]:  { label: () => sLabel(SensorType.SENSOR_RAIN), format: v => v ? t('yn.yes') : t('yn.no') },
    [SensorType.SENSOR_LUMI]:  { label: () => sLabel(SensorType.SENSOR_LUMI), format: v => (v * 108.9432 / 7074).toFixed(0) + ' lx' },
    [SensorType.SENSOR_LEVEL]: { label: () => sLabel(SensorType.SENSOR_LEVEL), format: v => v.toFixed(0) + ' %' },
    [SensorType.SENSOR_GENERIC]: { label: () => sLabel(SensorType.SENSOR_GENERIC), format: v => Number(v).toFixed(2) },
    [SensorType.SENSOR_CONTACT]: { label: () => sLabel(SensorType.SENSOR_CONTACT), format: (v,s) => s ? t('yn.closed') : t('yn.open') },
  };
  const cfg = SENSOR_DISPLAY[type];
  if (cfg) {
    const dv = (value === 255 || value == null) ? 'N/A' : cfg.format(value, state);
    return `<div class="device-card sensor" data-name="${name}" data-type="${type}">
      <div class="device-head">
        <span class="chip neutral">${cfg.label()}</span>
        <span class="device-name">${name}</span>
      </div>
      <div class="metric" id="dev_${id}">${dv}</div>
      <div class="device-row">${devStatus(sensor)}</div>
    </div>`;
  }
  return `<div class="device-card" data-name="${name}" data-type="${type}">
    <div class="device-head"><span class="device-name">${name}</span></div>
    <div class="metric" id="dev_${id}">${value}</div>
  </div>`;
}
)rawliteral";


const char JS[] PROGMEM = R"rawliteral(

/* -------------------- DEVICES -------------------- */

var calibPromise = null;
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

function isDeviceVisible(s) {
  if (!s) return false;
  if (s.id === 0 || s.id == null) return false;
  if (s.type === undefined || s.type === SensorType.SENSOR_NONE) return false;
  if (!(s.type in TYPE_ORDER)) return false;
  if (s.local === true) return true;
  return typeof s.age_ms === 'number' && s.age_ms <= 30000;
}

async function loadDevices() {
  try {
    const data = (await getCalib(true)).filter(isDeviceVisible);
    let mobile = '';
    let actuators = '';
    let deviceSensors = '';
    let actCount = 0;
    let sensCount = 0;
    visualSort(data).forEach((s, i) => {
      const card = deviceCard(s.name, s.value, s.id, s.state, s.fade, s.type, s);
      mobile += card;
      if (s.type === SensorType.TYPE_RELAY || s.type === SensorType.TYPE_DIMMER || s.type === SensorType.SENSOR_TIME) {
        actuators += card;
        actCount++;
      } else {
        deviceSensors += card;
        sensCount++;
      }
    });
    const now = new Date();
    const clock = String(now.getHours()).padStart(2,'0') + ':' + String(now.getMinutes()).padStart(2,'0') + ':' + String(now.getSeconds()).padStart(2,'0') + ' - ' + String(now.getUTCFullYear()) + '-' + String(now.getUTCMonth() + 1).padStart(2, '0') + '-' + String(now.getUTCDate()).padStart(2, '0');
    const html = `
      <div class="dash-stats">
        <div class="stat-card"><span class="stat-label">${t('stat.actuators')}</span><span class="stat-value">${actCount}</span></div>
        <div class="stat-card"><span class="stat-label">${t('stat.sensors')}</span><span class="stat-value">${sensCount}</span></div>
        <div class="stat-card"><span class="stat-label">${t('stat.time')}</span><span class="stat-value" style="font-size:18px;align-self:center">${clock}</span></div>
      </div>
      <div class="devices-mobile-list">${mobile}</div>
      <div class="devices-desktop-layout">
        <div class="devices-column devices-actuators">
          <div class="section-title">${t('stat.actuators')}</div>
          <div class="devices-actuator-grid">${actuators || "<div class='empty sm'>" + t('no.actuators') + "</div>"}</div>
        </div>
        <div class="devices-column devices-sensors">
          <div class="section-title">${t('stat.sensors')}</div>
          <div class="devices-sensor-grid">${deviceSensors || "<div class='empty sm'>" + t('no.sensors') + "</div>"}</div>
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

function sensorCardRenderer(s) {
  const renderer = TYPE_RENDERERS[s.type];
  if (!renderer) {
    console.warn(`[calib] tipo desconocido ${s.type} para '${s.name}', omitido`);
    return null;
  }
  return renderer;
}

const TYPE_RENDERERS = {
  [SensorType.TYPE_RELAY]: cardRenderers.REL,
  [SensorType.TYPE_DIMMER]: cardRenderers.DIMM,
  [SensorType.SENSOR_TEMP]: cardRenderers.TEMP,
  [SensorType.SENSOR_LUMI]: cardRenderers.LUMI,
  [SensorType.SENSOR_PRESS]: cardRenderers.PRES,
  [SensorType.SENSOR_RAIN]: cardRenderers.RAIN,
  [SensorType.SENSOR_AIRQ]: cardRenderers.AIRQ,
  [SensorType.SENSOR_LEVEL]: cardRenderers.LEVE,
  [SensorType.SENSOR_GENERIC]: cardRenderers.GENERIC,
  [SensorType.SENSOR_CONTACT]: cardRenderers.CONTACT,
  [SensorType.SENSOR_HUMI]: cardRenderers.HUMI
};

async function loadCalib() {
  try {
    const data = await getCalib(true);
    let html = "<div class='settings-grid'>";
    data.forEach((s, i) => {
      const render = sensorCardRenderer(s);
      if (!render) return;
      html += render(s, i);
    });
    html += `
      <div class="settings-general">
        <div class="settings-row">
          <div class="settings-card">
            <div class="settings-card-head">
              <div><span class="eyebrow">NETWORK</span><h3>${t('net.title')}</h3></div>
            </div>
            <form action="/save" method="post">
              <div class="field">
                <label for="ssid">${t('net.ssid')}</label>
                <input id="ssid" class="input" name="ssid" autocomplete="off" placeholder="${t('net.ssid.ph')}" required>
              </div>
              <div class="field">
                <label for="pass">${t('net.pass')}</label>
                <input id="pass" class="input" name="pass" type="password" autocomplete="off" placeholder="${t('net.pass.ph')}" required>
              </div>
              <button type="submit" class="btn primary">${t('net.save')}</button>
            </form>
            <p class="small-note" style="margin:12px 0 0">${t('net.note')}</p>
          </div>
          ${cardRenderers.DEFAULT(
            { value: 0, min: 0, max: 0 },
            data.length
          )}
        </div>
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
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : s.value ? t('yn.yes') : t('yn.no');
      else if (s.type === SensorType.SENSOR_AIRQ)
        el.innerText = s.value === 255 || s.value == null ? 'N/A' : s.value == 0 ? t('air.good') : s.value == 1 ? t('air.warn') : s.value == 2 ? t('air.bad') : 'N/A';
      else if (s.type === SensorType.SENSOR_LEVEL)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : s.value.toFixed(0) + ' %';
      else if (s.type === SensorType.TYPE_DIMMER)
        el.innerText = s.fade;
      else if (s.type === SensorType.SENSOR_LUMI)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : (s.value * 108.9432 / 7074).toFixed(0) + ' lx';
      else if (s.type === SensorType.SENSOR_GENERIC)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : Number(s.value).toFixed(2);
      else if (s.type === SensorType.SENSOR_CONTACT)
        el.innerText = (s.value == null || s.value === 255) ? 'N/A' : s.state ? t('yn.closed') : t('yn.open');
      else if (s.type === SensorType.SENSOR_TIME)
        el.innerHTML = s ? formatTime(s) : 'N/A';
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
    return { handled:false, ok:false };
  let res;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), 5000);
    res = await fetch(`http://${sensor.ip}${path}`, {
      method:'POST',
      headers:{ 'Content-Type': 'application/x-www-form-urlencoded' },
      body,
      signal: ctrl.signal
    });
    clearTimeout(timer);
  } catch (e) {
    if (e && e.name === 'AbortError')
      alert(tf('alert.isVirtualTimeout', { ip: sensor.ip }));
    else
      alert(tf('alert.isVirtualNet', { ip: sensor.ip }));
    return { handled:true, ok:false };
  }
  if (!res.ok) {
    let detail = '';
    try { detail = await res.text(); } catch(_) {}
    alert(tf('alert.isVirtualHttp', { ip: sensor.ip, status: res.status, detail: detail ? ': ' + detail : '' }));
    return { handled:true, ok:false };
  }
  return { handled:true, ok:true };
}

/* -------------------- ACTIONS -------------------- */

async function toggleMatterSwitch(i, id, name) {
  const btn = document.getElementById(`matterBtn${i}`);
  const wasOn = btn.classList.contains('ok');
  const on = !wasOn;
  btn.classList.toggle('ok', on);
  btn.setAttribute('aria-pressed', on ? 'true' : 'false');
  btn.textContent = on ? t('chip.enabled') : t('chip.disabled');
  const body =
    `id=${encodeURIComponent(id)}` +
    `&type=avail` +
    `&ref=${encodeURIComponent(on ? 1 : 0)}`;
  const r = await isVirtual(id, '/calib/set', body);
  if (r.handled) {
    if (!r.ok) {
      btn.classList.toggle('ok', wasOn);
      btn.setAttribute('aria-pressed', wasOn ? 'true' : 'false');
      btn.textContent = wasOn ? t('chip.enabled') : t('chip.disabled');
    }
    return;
  }
  await fetch('/calib/set', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body
  });
}

async function setPort(i) {
  const params = new URLSearchParams();
  const b = document.getElementById('broadcast_port').value.trim();
  const c = document.getElementById('command_port').value.trim();
  const r = document.getElementById(`ref${i}`).value.trim();
  if (b) params.append('broadcast', b);
  if (c) params.append('command', c);
  if (r) params.append('interval', r);
  await fetch('/genset/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: params.toString()
  });
  alert(t('alert.saved'));
}

async function setCalib(i, type, name, refOverride = null) {
  const sensor = sensors[i];
  if (!sensor)
    return false;
  const ref = refOverride !== null
    ? refOverride
    : (document.getElementById(`ref${i}`)?.value ?? '');
  const body =
    `id=${encodeURIComponent(sensor.id)}` +
    `&type=${encodeURIComponent(type)}` +
    `&ref=${encodeURIComponent(ref)}`;
  const r = await isVirtual(sensor.id, '/calib/set', body);
  if (r.handled)
    return r.ok;
  try {
    const res = await fetch('/calib/set', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    if (!res.ok) {
      let detail = '';
      try { detail = await res.text(); } catch(_) {}
      alert(tf('alert.localError', { status: res.status, detail: detail ? ': ' + detail : '' }));
      return false;
    }
    return true;
  } catch (e) {
    alert(t('alert.localNet'));
    return false;
  }
}

function togglePersist(i, name) {
  const persist = document.getElementById(`persistChk${i}`);
  const pulse   = document.getElementById(`pulseChk${i}`);
  const input   = document.getElementById(`ref${i}`);
  const wasPersist = persist.checked;
  if (persist.checked) {
    pulse.checked = false;
    input.style.display = 'none';
    setCalib(i, 'pulse', name, 0);
  }
  setCalib(i, 'persist', name, persist.checked ? 1 : 0).then(ok => {
    if (!ok) {
      persist.checked = wasPersist;
      pulse.checked = !wasPersist;
      if (input) input.style.display = wasPersist ? 'none' : 'inline-block';
    }
  });
}

function togglePulse(i, name) {
  const pulse   = document.getElementById(`pulseChk${i}`);
  const persist = document.getElementById(`persistChk${i}`);
  const input   = document.getElementById(`ref${i}`);
  const wasPulse = pulse.checked;
  if (pulse.checked) {
    persist.checked = false;
    setCalib(i, 'persist', name, 0).then(ok => {
      if (!ok) {
        pulse.checked = wasPulse;
        if (persist) persist.checked = !wasPulse;
        if (input) input.style.display = 'none';
      }
    });
    input.style.display = 'inline-block';
  } else {
    input.style.display = 'none';
    setCalib(i, 'pulse',  name, 0).then(ok => {
      if (!ok) {
        pulse.checked = wasPulse;
        if (input) input.style.display = 'inline-block';
      }
    });
  }
}

async function toggleDevice(id) {
  const body = 'id=' + encodeURIComponent(id);
  const r = await isVirtual(id, '/toggle', body);
  if (r.handled) {
    if (r.ok) setTimeout(loadDevices, 100);
    return;
  }
  await fetch('/toggle', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body
  });
  setTimeout(loadDevices, 100);
}

function onDimmerInput(id, value) {
  const el = document.getElementById('dev_val_' + id);
  if (el) el.innerText = value;
  const sl = document.getElementById('slider_' + id);
  if (sl) {
    const pct = Number(value) || 0;
    sl.style.background = `linear-gradient(to right,var(--accent) ${pct}%,var(--border) ${pct}%)`;
  }
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
  const body = `id=${id}&value=${value}`;
  try {
    const r = await isVirtual(id, '/dimmer', body);
    if (r.handled) {
      if (r.ok) setTimeout(loadDevices, 100);
      return;
    }
    const res = await fetch('/dimmer', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    if (!res.ok) {
      let detail = '';
      try { detail = await res.text(); } catch(_) {}
      alert(tf('alert.localError', { status: res.status, detail: detail ? ': ' + detail : '' }));
      return;
    }
    setTimeout(loadDevices, 100);
  } catch (e) {
    console.log('sendDimmer err', e);
  }
}

function factoryReset() {
  if (!confirm(t('factory.confirm'))) return;
  fetch('/factory', { method: 'POST' })
    .then(() => alert(t('factory.doing')))
    .catch(() => alert(t('factory.err')));
}

async function toggleOta(enabled) {
  if (!confirm(tf(enabled ? 'ota.enableMsg' : 'ota.disableMsg'))) return;
  try {
    await fetch('/ota/toggle?enabled=' + (enabled ? 1 : 0));
  } catch(e) {
    console.log('toggleOta err', e);
  }
}

async function syncOtaCheckbox() {
  for (let i = 0; i < 10; i++) {
    const el = document.getElementById('otaChk');
    if (el) {
      try {
        const r = await fetch('/ota/status');
        const j = await r.json();
        el.checked = j.ota === 1;
        el.style.display = 'none';
        el.offsetHeight;
        el.style.display = '';
      } catch(e) {
        console.log('syncOta err', e);
      }
      return;
    }
    await new Promise(r => setTimeout(r, 100));
  }
}

/* -------------------- THEMES -------------------- */

const THEME_MAP = {
  '#414141':'dark', '#f7f2ff':'light', '#4c834e':'forest', '#c1af8d':'sand'
};

function setBackground(color){
  const theme = THEME_MAP[color] || 'dark';
  document.documentElement.setAttribute('data-theme', theme);
  localStorage.setItem('bgColor', color);
  localStorage.setItem('theme', theme);
}

document.querySelectorAll('.themeDot').forEach(dot =>
  dot.onclick = () => setBackground(dot.dataset.bg)
);

const savedBg = localStorage.getItem('bgColor');

if(savedBg){
  setBackground(savedBg);
}

/* -------------------- LOGS -------------------- */

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
  coreEl.innerHTML = '<h3>' + t('log.core') + '</h3>';
  evntEl.innerHTML = '<h3>' + t('log.events') + '</h3>';
  sensEl.innerHTML = '<h3>' + t('log.sensors') + '</h3>';
  logs.forEach(e => {
    const el = document.createElement('div');
    el.className = 'log-entry ' + (e.l === 'CORE' ? 'core' : e.l === 'EVNT' ? 'evnt' : 'sens');
    el.innerHTML = `<span class="t">${e.t}</span><span class="l ${e.v.toLowerCase()}">${e.v}</span>${e.m}`;
    if(e.l === 'CORE') coreEl.appendChild(el);
    else if(e.l === 'EVNT') evntEl.appendChild(el);
    else sensEl.appendChild(el);
  });
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
    if(wizard.data.sensors.length > 1) {
      steps.push(3);
    }
  }

  if(wizard.data.type === 2) {
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
    content = `<div class="wizard-step">
      <h3>${t('wiz.type')}</h3>
       ${[0,1,2,3].map(ti=>{
         const cfg = [
           {txt:t('wiz.edge'),desc:t('wiz.edge.desc')},
           {txt:t('wiz.thresh'),desc:t('wiz.thresh.desc')},
           {txt:t('wiz.time'),desc:t('wiz.time.desc')},
           {txt:t('wiz.intervalTxt'),desc:t('wiz.interval.desc')}
         ][ti];
         return `
         <label class="wizard-option">
           <input type="radio" name="type" value="${ti}" ${wizard.data.type===ti?'checked':''}>
           <div>
             <strong>${cfg.txt}</strong>
             <small>${cfg.desc}</small>
           </div>
         </label>`;
       }).join('')}
    </div>`;
  }

  /* ================= STEP 1 ================= */
  else if(stepNum === 1) {
    content = `<div class="wizard-step">
      <h3>${t('wiz.sensors')}</h3>
      <select id="sensorList" multiple size="5" class="input"></select>
      <span class="wizard-hint">${t('wiz.hint')}</span>
    </div>`;
  }

  /* ================= STEP 2 ================= */
  else if(stepNum === 2) {

    if(wizard.data.sensors.length === 0){
      content = `<h3>${t('wiz.noSensors')}</h3>`;
    } else {

      content = `<div class="wizard-step"><h3>${t('wiz.conditions')}</h3>`;

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
              s.type === SensorType.SENSOR_AIRQ  ? (s.value==0?t('air.good'):s.value==1?t('air.warn'):s.value==2?t('air.bad'):'N/A') :
              s.type === SensorType.SENSOR_RAIN  ? (s.value ? t('yn.yes') : t('yn.no')) :
              s.type === SensorType.SENSOR_CONTACT  ? (s.state ? t('yn.closed') : t('yn.open')) :
              s.type === SensorType.SENSOR_GENERIC  ? Number(s.value).toFixed(2) :
              s.value
            );

        content += `
        <div class="cond-box">
          <div class="rule-info"><b>${s.name}</b><span>${val}</span></div>
          ${wizard.data.type === 0 ? `
            <select id="cmp_${sIdx}">
              <option value="0" ${cond.cmp===0?'selected':''}>${t('wiz.rising')}</option>
              <option value="1" ${cond.cmp===1?'selected':''}>${t('wiz.falling')}</option>
            </select>
          ` : `
            <div class="field-row">
              <select id="cmp_${sIdx}" class="input sm" style="flex:1">
                <option value="0" ${cond.cmp===0?'selected':''}>&gt; </option>
                <option value="1" ${cond.cmp===1?'selected':''}>&lt; </option>
                <option value="2" ${cond.cmp===2?'selected':''}>= </option>
              </select>
              <input id="thresh_${sIdx}" class="input sm" style="flex:1.4" type="number" step="any" value="${cond.threshold}">
            </div>
          `}
        </div>`;
      });
      content += '</div>';
    }
  }

  /* ================= STEP 3 ================= */
   else if(stepNum === 3) {

     if(wizard.data.type === 2) {
       content = `<div class="wizard-step">
         <h3>${t('wiz.dates')}</h3>
         <div class="field">
           <label for="date_start">${t('wiz.from')}</label>
           <input id="date_start" type="date" value="${wizard.data.date_start}">
         </div>
         <div class="field">
           <label for="date_end">${t('wiz.to')}</label>
           <input id="date_end" type="date" value="${wizard.data.date_end}">
         </div>
         <div class="field">
           <label>${t('wiz.runAt')}</label>
           <div class="field-row">
             <input id="time_hour" class="input sm" type="number" min="0" max="23" value="${wizard.data.time_hour}" placeholder="Hs" style="flex:1">
             <input id="time_minute" class="input sm" type="number" min="0" max="59" value="${wizard.data.time_minute}" placeholder="Min" style="flex:1">
           </div>
         </div>
       </div>`;
     } else if(wizard.data.sensors.length > 1) {
       content = `<div class="wizard-step">
         <h3>${t('wiz.logic')}</h3>
         <label class="wizard-option"><input type="radio" name="logic" value="1" ${wizard.data.logic===1?'checked':''}><div><strong>${t('wiz.all')}</strong></div></label>
         <label class="wizard-option"><input type="radio" name="logic" value="0" ${wizard.data.logic===0?'checked':''}><div><strong>${t('wiz.any')}</strong></div></label>
       </div>`;
     }
   }

  /* ================= STEP 4 ================= */
  else if(stepNum === 4) {
    content = `<div class="wizard-step">
      <h3>${t('wiz.actuators')}</h3>
      <select id="actuatorList" multiple size="5" class="input"></select>
      <span class="wizard-hint">${t('wiz.hint')}</span>
    </div>`;
  }

  /* ================= STEP 5 ================= */
  else if(stepNum === 5) {

    if(wizard.data.actuators.length === 0){
      content = `<h3>${t('wiz.noActuators')}</h3>`;
    } else {

      content = `<div class="wizard-step"><h3>${t('wiz.actions')}</h3>`;

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
        <div class="action-box">
          <div class="rule-info"><b>${a.name}</b><span>${state}</span></div>
          <div class="field-row">
            <select id="action_${i}" class="input sm" style="flex:1">
              <option value="0" ${action===0?'selected':''}>ON</option>
              <option value="1" ${action===1?'selected':''}>OFF</option>
              <option value="2" ${action===2?'selected':''}>TOGGLE</option>
              ${a.type===8?`<option value="3" ${action===3?'selected':''}>LEVEL</option>`:''}
            </select>
            ${a.type===8?`<input id="level_${i}" class="input sm" type="number" min="0" max="100" value="${level}" style="flex:1;width:80px;${action===3?'':'display:none;'}">`:''}
          </div>
        </div>`;
      });

      setTimeout(() => setupActionListeners(), 0);
      content += '</div>';
    }
  }

  /* ================= STEP 6 ================= */
  else if(stepNum === 6) {

    content = `<div class="wizard-step">`;

    if(wizard.data.type === 3){
      content += `<div class="field">
        <label for="interval">${t('wiz.intervalMs')}</label>
        <input id="interval" class="input" type="number" value="${wizard.data.interval||1000}">
      </div>`;
    }

    content += `
      <div class="field">
        <label for="delay">${t('wiz.delayMs')}</label>
        <input id="delay" class="input" type="number" value="${wizard.data.delay}">
      </div>
      <div class="field">
        <label for="cooldown">${t('wiz.cooldownMs')}</label>
        <input id="cooldown" class="input" type="number" value="${wizard.data.cooldown}">
      </div>
    </div>`;
  }

  /* ================= RENDER ================= */

  let html = `<div>${content}</div>`;
   html += `
  <div class="wizard-nav">
    ${n>0?'<button class="btn ghost" onclick="prevStep()">' + t('wiz.back') + '</button>':''}
    ${n<steps.length-1?'<button class="btn primary" onclick="nextStep()">' + t('wiz.next') + '</button>':'<button class="btn primary" onclick="finishWizard()">' + t('wiz.save') + '</button>'}
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
        const updateLevelVisibility = () => {
          if(levelInput) {
            levelInput.style.display = actionSelect.value === '3' ? 'inline-block' : 'none';
          }
        };
        updateLevelVisibility();
        actionSelect.addEventListener('change', updateLevelVisibility);
      }
    }
  });
}

function validateStep(stepNum) {
  if(stepNum === 0) {
    const typeRadio = document.querySelector('input[name="type"]:checked');
    if(!typeRadio) {
      alert(t('wiz.selectType'));
      return false;
    }
    return true;
  }

  if(stepNum === 1) {
    if((wizard.data.type === 0 || wizard.data.type === 1) && wizard.data.sensors.length === 0) {
      alert(t('wiz.needSensor'));
      return false;
    }
    return true;
  }

  if(stepNum === 2) {
    if(wizard.data.type === 0 || wizard.data.type === 1) {
      for(let sIdx of wizard.data.sensors) {
        const cmpSelect = document.getElementById(`cmp_${sIdx}`);
        const threshInput = document.getElementById(`thresh_${sIdx}`);

        if(wizard.data.type === 1) { // THRESHOLD
          if(!threshInput || threshInput.value === '') {
            alert(t('wiz.needThreshold'));
            return false;
          }
          const threshVal = parseFloat(threshInput.value);
          if(isNaN(threshVal) || threshVal < -1000 || threshVal > 10000) {
            alert(t('wiz.threshRange'));
            return false;
          }
        }
      }
    }
    return true;
  }

  if(stepNum === 3) {
    if(wizard.data.type === 2) { // TIME
      const dateStartEl = document.getElementById('date_start');
      const dateEndEl = document.getElementById('date_end');
      const timeHourEl = document.getElementById('time_hour');
      const timeMinEl = document.getElementById('time_minute');

      const hour = parseInt(timeHourEl.value) || 0;
      const min = parseInt(timeMinEl.value) || 0;

      if(hour < 0 || hour > 23 || min < 0 || min > 59) {
        alert(t('wiz.timeRange'));
        return false;
      }

      if(dateStartEl.value && dateEndEl.value) {
        const start = new Date(dateStartEl.value);
        const end = new Date(dateEndEl.value);
        if(start > end) {
          alert(t('wiz.dateOrder'));
          return false;
        }
      }
    }
    return true;
  }

  if(stepNum === 4) {
    if(wizard.data.actuators.length === 0) {
      alert(t('wiz.needActuator'));
      return false;
    }
    return true;
  }

  if(stepNum === 5) {
    for(let aPos = 0; aPos < wizard.data.actuators.length; aPos++) {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);

      if(!actionSelect) {
        alert(t('wiz.loadActions'));
        return false;
      }

      const action = parseInt(actionSelect.value);
      const aIdx = wizard.data.actuators[aPos];
      const actuator = sensorByIndex(aIdx);
      if(!actuator) {
        alert(t('wiz.loadActuator'));
        return false;
      }

      if(action === 3 && actuator.type !== 8) {
        alert(t('wiz.levelDimmerOnly'));
        return false;
      }

      if(action === 3 && levelInput) {
        const level = parseInt(levelInput.value);
        if(isNaN(level) || level < 0 || level > 100) {
          alert(t('wiz.levelRange'));
          return false;
        }
      }
    }
    return true;
  }

  if(stepNum === 6) {
    const delayEl = document.getElementById('delay');
    const cooldownEl = document.getElementById('cooldown');
    const intervalEl = document.getElementById('interval');

    const delay = parseInt(delayEl.value) || 0;
    const cooldown = parseInt(cooldownEl.value) || 0;

    if(delay < 0 || delay > 60000) {
      alert(t('wiz.delayRange'));
      return false;
    }

    if(cooldown < 0 || cooldown > 3600000) {
      alert(t('wiz.cooldownRange'));
      return false;
    }

    if(wizard.data.type === 3) {
      const interval = parseInt(intervalEl.value) || 0;
      if(interval < 1000 || interval > 3600000) {
        alert(t('wiz.intervalRange'));
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
      wizard.data.date_start = document.getElementById('date_start').value || '';
      wizard.data.date_end = document.getElementById('date_end').value || '';
      wizard.data.time_hour = parseInt(document.getElementById('time_hour').value) || 0;
      wizard.data.time_minute = parseInt(document.getElementById('time_minute').value) || 0;
    } else if(wizard.data.sensors.length > 1) {
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

  if(wizard.data.type === 2 && stepNum === 3) {
    wizard.data.date_start = document.getElementById('date_start').value || '';
    wizard.data.date_end = document.getElementById('date_end').value || '';
    wizard.data.time_hour = parseInt(document.getElementById('time_hour').value) || 0;
    wizard.data.time_minute = parseInt(document.getElementById('time_minute').value) || 0;
  }
  else if(stepNum === 5) {
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

  if(wizard.data.actuators.length === 0) {
    alert(t('wiz.needActuator'));
    return;
  }

  if((wizard.data.type === 0 || wizard.data.type === 1) && wizard.data.sensors.length === 0) {
    alert(t('wiz.needSensor'));
    return;
  }

  if(wizard.data.type === 2) {
    if(wizard.data.time_hour < 0 || wizard.data.time_hour > 23) {
      alert(t('wiz.hourRange'));
      return;
    }
    if(wizard.data.time_minute < 0 || wizard.data.time_minute > 59) {
      alert(t('wiz.minRange'));
      return;
    }
  }

  if(wizard.data.type === 2) {
    if(wizard.data.date_start && wizard.data.date_end) {
      const dateStart = new Date(wizard.data.date_start);
      const dateEnd = new Date(wizard.data.date_end);
      if(dateStart > dateEnd) {
        alert(t('wiz.dateOrder'));
        return;
      }
    }
  }

  if(wizard.data.delay < 0 || wizard.data.delay > 60000) {
    alert(t('wiz.delayRange'));
    return;
  }

  if(wizard.data.cooldown < 0 || wizard.data.cooldown > 3600000) {
    alert(t('wiz.cooldownRange'));
    return;
  }

  if(wizard.data.type === 3) {
    if(wizard.data.interval < 1000 || wizard.data.interval > 3600000) {
      alert(t('wiz.intervalRange'));
      return;
    }
  }

  wizard.data.actuators.forEach((aIdx, aPos) => {
    const level = wizard.data.levels[aPos] || 0;
    if(level < 0 || level > 100) {
      alert(tf('wiz.levelActuator', { level }));
      return;
    }
  });

  wizard.data.actuators.forEach((aIdx, aPos) => {
    const action = wizard.data.actions[aPos];
    const actuator = sensorByIndex(aIdx);
    if(!actuator) return;

    if(action === 3 && actuator.type !== 8) {
      alert(t('wiz.levelDimmerOnly'));
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

  let time_s = wizard.data.type === 2 ? wizard.data.time_hour * 3600 + wizard.data.time_minute * 60 : 0;

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
      alert(t('wiz.saved'));
      closeRule();
      loadRules();
    } else {
      const errMsg = await res.text();
      alert(t('wiz.saveError') + ' ' + errMsg);
    }
  } catch(e) {
    alert(t('wiz.connError') + ' ' + e.message);
  }
}

function editRule(i){
  loadSensorsAndActuators().then(()=>{
    document.getElementById('ruleModalTitle').textContent=t('wiz.titleEdit');
    startWizard(i);
    document.getElementById('ruleModal').style.display='flex';
  });
}

function closeRule(){
  document.getElementById('ruleModal').style.display='none';
}

async function deleteRule(i){
  if(!confirm(tf('wiz.deleteConfirm', { id: i }))) return;
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
    const empty = document.getElementById("auto_empty");

    if(!rules || rules.length === 0){
      table.innerHTML = '';
      if(empty) empty.style.display = 'flex';
      return;
    }
    if(empty) empty.style.display = 'none';

    const typeName = ['EDGE','THRESHOLD','TIME','INTERVAL'];
    let html = '<div class="rule-list">' + rules.map(r=>`
      <div class="rule-card">
        <div class="rule-head">
          <span class="chip neutral">${typeName[r.type] || 'RULE'} #${r.id}</span>
          <div class="rule-actions">
            <button class="btn ghost sm" onclick="editRule(${r.id})">${t('wiz.edit')}</button>
            <button class="btn danger sm" onclick="deleteRule(${r.id})">${t('wiz.delete')}</button>
          </div>
        </div>
        <div class="rule-info"><span>${t('rule.sensors')}</span><b>${r.sensors.join(", ")}</b></div>
        <div class="rule-info"><span>${t('rule.logic')}</span><b>${r.logical_and ? 'AND' : 'OR'}</b></div>
        <div class="rule-info"><span>${t('rule.actions')}</span><b>${r.actuators.join(", ")}</b></div>
        <div class="rule-info"><span>${t('rule.delay')}</span><b>${r.delay_ms} / ${r.cooldown_ms} ms</b></div>
      </div>`).join('') + '</div>';
    table.innerHTML = html;
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

if(location.search.indexOf('saved=1') >= 0){
  const n = document.getElementById('savedNotice');
  if(n) n.style.display = 'flex';
}

(function modalInit(){
  const modal = document.getElementById('ruleModal');
  if(!modal) return;
  modal.addEventListener('click', e => {
    if(e.target === modal) closeRule();
  });
  document.addEventListener('keydown', e => {
    if(e.key === 'Escape' && modal.style.display === 'flex') closeRule();
  });
})();

/* -------------------- LANGUAGE SELECTOR -------------------- */

function setLang(l) {
  LANG = (l === 'en') ? 'en' : 'es';
  try { localStorage.setItem('lang', LANG); } catch(e) {}
  document.querySelectorAll('[data-i18n]').forEach(el => {
    el.textContent = t(el.getAttribute('data-i18n'));
  });
  document.querySelectorAll('.langbtn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.lang === LANG);
  });
  const tab = window.activeTab || 'control';
  if (tab === 'control') loadDevices();
  else if (tab === 'auto') loadRules();
  else if (tab === 'config') { loadCalib(); syncOtaCheckbox(); }
  else if (tab === 'logs') refreshLogs();
}

document.querySelectorAll('.langbtn').forEach(btn => {
  btn.addEventListener('click', () => setLang(btn.dataset.lang || 'es'));
});

setLang(LANG);
</script>
<div id="ruleModal" class="modal" role="dialog" aria-modal="true" aria-labelledby="ruleModalTitle">
  <div class="modal-content">
    <div class="modal-head">
      <h2 id="ruleModalTitle" data-i18n="wiz.titleNew">Nueva regla de automatización</h2>
      <button type="button" class="icon-btn" onclick="closeRule()" aria-label="Cerrar">×</button>
    </div>
    <div id="wizardContent"></div>
    <div class="modal-foot">
      <button class="btn ghost sm" onclick="closeRule()" data-i18n="wiz.cancel">Cancelar</button>
    </div>
  </div>
</div>
</body>
</html>
)rawliteral";


}