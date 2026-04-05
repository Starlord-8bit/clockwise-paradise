#pragma once
#include <Arduino.h>
#include <WiFi.h>

// Shared CSS + header snippet used by all pages
// Tasmota/WLED-inspired: dark theme, large nav buttons, clean sections

static const char PAGE_HEAD[] PROGMEM = R""""(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Clockwise Paradise</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;min-height:100vh}
.hdr{background:linear-gradient(120deg,#155799,#159957);padding:12px 16px;display:flex;align-items:center;gap:12px}
.hdr-logo{font-size:1.5em}
.hdr-info{flex:1}
.hdr-title{font-size:1.1em;font-weight:bold;color:#fff}
.hdr-sub{font-size:.75em;color:#c8e6c9}
.nav{background:#16213e;padding:8px 12px;display:flex;flex-wrap:wrap;gap:8px}
.nav a{display:inline-block;padding:8px 16px;border-radius:6px;text-decoration:none;font-size:.85em;font-weight:600;color:#ccc;background:#0f3460;transition:background .2s}
.nav a:hover,.nav a.active{background:#e94560;color:#fff}
.content{padding:16px;max-width:600px;margin:0 auto}
.section{background:#16213e;border-radius:8px;margin-bottom:16px;overflow:hidden}
.section-title{background:#0f3460;padding:10px 14px;font-size:.9em;font-weight:600;color:#90caf9;letter-spacing:.5px;text-transform:uppercase}
.row{padding:10px 14px;border-bottom:1px solid #1a2a4a;display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}
.row:last-child{border-bottom:none}
.row label{font-size:.85em;color:#b0bec5;flex:1;min-width:120px}
.row .ctrl{flex:2;min-width:160px}
input[type=text],input[type=number],input[type=password],select{width:100%;padding:6px 8px;background:#0f3460;border:1px solid #1e3a5f;border-radius:4px;color:#e0e0e0;font-size:.85em}
input[type=range]{width:100%;accent-color:#e94560}
input[type=checkbox]{width:18px;height:18px;accent-color:#e94560}
.hint{font-size:.75em;color:#607d8b;margin-top:3px}
.btn{display:inline-block;padding:9px 20px;border-radius:6px;border:none;cursor:pointer;font-size:.9em;font-weight:600;transition:background .2s}
.btn-primary{background:#e94560;color:#fff}.btn-primary:hover{background:#c62a47}
.btn-secondary{background:#0f3460;color:#e0e0e0}.btn-secondary:hover{background:#1a4a7a}
.btn-danger{background:#b71c1c;color:#fff}.btn-danger:hover{background:#8b0000}
.btn-block{display:block;width:100%;margin-top:8px;text-align:center}
.footer{padding:14px;display:flex;gap:10px;flex-wrap:wrap}
.toast{position:fixed;bottom:20px;right:20px;background:#2e7d32;color:#fff;padding:10px 18px;border-radius:6px;font-size:.85em;opacity:0;transition:opacity .3s;pointer-events:none;z-index:99}
.toast.show{opacity:1}
.ver-badge{background:#0f3460;border-radius:4px;padding:3px 8px;font-size:.8em;color:#90caf9;font-family:monospace}
.small{font-size:.78em;color:#90a4ae}
</style>
</head>
)"""";

static const char NAV_FMT[] PROGMEM = R""""(
<div class="hdr">
  <div class="hdr-logo">🌴</div>
  <div class="hdr-info">
    <div class="hdr-title">Clockwise Paradise</div>
    <div class="hdr-sub" id="hdr-ver">Loading...</div>
  </div>
  <div id="hdr-ssid" style="font-size:.75em;color:#c8e6c9"></div>
</div>
<div class="nav">
  <a href="/" %s>🏠 Home</a>
  <a href="/clock" %s>🕐 Clock</a>
  <a href="/sync" %s>🔗 Sync</a>
  <a href="/hardware" %s>🔧 Hardware</a>
  <a href="/update" %s>⬆ Update</a>
</div>
)"""";

static const char PAGE_SCRIPTS[] PROGMEM = R""""(
<div class="toast" id="toast"></div>
<script>
function $(id){return document.getElementById(id)}
function toast(msg,ok){
  var t=$('toast');t.textContent=msg;
  t.style.background=ok===false?'#b71c1c':'#2e7d32';
  t.classList.add('show');setTimeout(()=>t.classList.remove('show'),2500);
}
function setKey(key,val){
  return fetch('/set?'+key+'='+encodeURIComponent(val),{method:'POST'})
    .then(r=>{ if(r.status!==204) throw new Error('bad_status'); return true; });
}
function restart(){fetch('/restart',{method:'POST'}).catch(()=>{}); toast('Rebooting…');}
function loadMeta(){
  fetch('/get').then(r=>{
    const h={};r.headers.forEach((v,k)=>h[k.toLowerCase().replace('x-','')]=v);
    if(h['cw_fw_version'])$('hdr-ver').textContent='v'+h['cw_fw_version']+' · '+h['clockface_name'];
    if(h['wifissid'])$('hdr-ssid').textContent='📶 '+h['wifissid'];
    if(typeof onSettingsLoaded==='function')onSettingsLoaded(h);
  }).catch(()=>{});
}
loadMeta();
</script>
)"""";

static const char PAGE_FOOT[] PROGMEM = "</body></html>";

inline void cw_sendNav(WiFiClient& client, const char* active) {
  auto cls = [&](const char* name){ return (active && strcmp(active,name)==0) ? "class=\"active\"" : ""; };
  char buf[512];
  snprintf(buf, sizeof(buf), NAV_FMT, cls("home"), cls("clock"), cls("sync"), cls("hardware"), cls("update"));
  client.print(buf);
}

inline void cw_sendPageStart(WiFiClient& client, const char* active) {
  client.print(PAGE_HEAD);
  client.println("<body>");
  cw_sendNav(client, active);
  client.println("<div class=\"content\">");
}

inline void cw_sendPageEnd(WiFiClient& client) {
  client.println("</div>");
  client.print(PAGE_SCRIPTS);
  client.print(PAGE_FOOT);
}
