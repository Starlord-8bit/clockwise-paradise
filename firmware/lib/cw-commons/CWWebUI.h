#pragma once
#include <Arduino.h>

// ─── Shared CSS + nav skeleton ────────────────────────────────────────────────
// Minimal inline CSS — no external CDN dependencies
// Each page uses the NAV_BAR macro to inject the top navigation

static const char CSS[] PROGMEM = R"rawcss(
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:sans-serif;background:#1a1a2e;color:#eee;min-height:100vh}
.header{background:linear-gradient(135deg,#16213e,#0f3460);padding:14px 20px;display:flex;align-items:center;justify-content:space-between}
.header h1{font-size:1.3em;color:#e94560;letter-spacing:1px}
.header .sub{font-size:.75em;color:#aaa;margin-top:2px}
.nav{background:#16213e;padding:0 10px;display:flex;flex-wrap:wrap;gap:2px;border-bottom:2px solid #0f3460}
.nav a{display:inline-block;padding:10px 16px;color:#aaa;text-decoration:none;font-size:.9em;border-bottom:3px solid transparent;transition:.2s}
.nav a:hover,.nav a.active{color:#e94560;border-bottom-color:#e94560}
.container{max-width:700px;margin:0 auto;padding:20px}
.card{background:#16213e;border-radius:8px;padding:18px;margin-bottom:16px;border:1px solid #0f3460}
.card h2{font-size:1em;color:#e94560;margin-bottom:12px;padding-bottom:6px;border-bottom:1px solid #0f3460}
.row{display:flex;align-items:center;margin-bottom:12px;gap:10px;flex-wrap:wrap}
.row label{min-width:160px;font-size:.88em;color:#ccc}
.row input[type=text],.row input[type=number],.row input[type=password],.row select{
  flex:1;background:#0f3460;border:1px solid #344;color:#eee;
  padding:7px 10px;border-radius:5px;font-size:.9em;min-width:140px}
.row input[type=range]{flex:1;min-width:120px}
.row input[type=checkbox]{width:18px;height:18px;accent-color:#e94560}
.val{color:#e94560;font-weight:bold;min-width:30px;text-align:center;font-size:.9em}
.btn{display:inline-block;padding:9px 22px;border:none;border-radius:6px;cursor:pointer;font-size:.9em;font-weight:bold;text-decoration:none;transition:.2s}
.btn-primary{background:#e94560;color:#fff}.btn-primary:hover{background:#c73050}
.btn-warn{background:#f0a500;color:#000}.btn-warn:hover{background:#d09000}
.btn-danger{background:#c0392b;color:#fff}.btn-danger:hover{background:#a03020}
.btn-secondary{background:#344;color:#eee}.btn-secondary:hover{background:#455}
.btn-sm{padding:5px 14px;font-size:.82em}
.actions{margin-top:6px;display:flex;gap:8px;flex-wrap:wrap}
.status{display:none;padding:8px 14px;border-radius:5px;font-size:.88em;margin-top:8px}
.status.ok{background:#1a4a1a;color:#4caf50;border:1px solid #4caf50}
.status.err{background:#4a1a1a;color:#f44336;border:1px solid #f44336}
.badge{display:inline-block;padding:2px 8px;border-radius:10px;font-size:.75em;font-weight:bold}
.badge-green{background:#1a4a1a;color:#4caf50}.badge-red{background:#4a1a1a;color:#f44336}
.info-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.info-item{background:#0f3460;padding:10px;border-radius:6px}
.info-item .lbl{font-size:.75em;color:#888;margin-bottom:3px}
.info-item .val2{font-size:1em;color:#eee;font-weight:bold}
@media(max-width:500px){.row{flex-direction:column;align-items:flex-start}.info-grid{grid-template-columns:1fr}}
</style>
)rawcss";

// ─── Helper: render full page with nav ───────────────────────────────────────
inline String cwPage(const String& title, const String& activePath, const String& body) {
  String p = F("<!DOCTYPE html><html lang='en'><head>"
               "<meta charset='UTF-8'>"
               "<meta name='viewport' content='width=device-width,initial-scale=1'>"
               "<title>Clockwise Paradise</title>");
  p += FPSTR(CSS);
  p += F("</head><body>");
  p += F("<div class='header'>"
         "<div><h1>&#127804; Clockwise Paradise</h1>"
         "<div class='sub' id='hdr-info'>Loading...</div></div>"
         "<div id='hdr-status'></div></div>");
  p += F("<div class='nav'>"
         "<a href='/'" );
  if (activePath == "/") p += F(" class='active'");
  p += F(">&#128200; Home</a>"
         "<a href='/clock'");
  if (activePath == "/clock") p += F(" class='active'");
  p += F(">&#9200; Clock</a>"
         "<a href='/sync'");
  if (activePath == "/sync") p += F(" class='active'");
  p += F(">&#128268; Sync</a>"
         "<a href='/hardware'");
  if (activePath == "/hardware") p += F(" class='active'");
  p += F(">&#9881; Hardware</a>"
         "<a href='/update'");
  if (activePath == "/update") p += F(" class='active'");
  p += F(">&#128190; Update</a>"
         "</div>");
  p += F("<div class='container'>");
  p += body;
  p += F("</div>");
  // Shared JS: load header info + save helper
  p += F("<script>"
         "fetch('/get').then(r=>{const h={};r.headers.forEach((v,k)=>{h[k.toLowerCase().replace('x-','')]=v;});return h;})"
         ".then(s=>{"
         "document.getElementById('hdr-info').textContent="
         "(s.wifissid?'&#128246; '+s.wifissid+' ':'')"
         "+(s.cw_fw_version?'| v'+s.cw_fw_version:'');"
         "window._s=s;});"
         "function save(k,v,btn){"
         "if(btn)btn.textContent='Saving...';"
         "fetch('/set',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
         "body:k+'='+encodeURIComponent(v)})"
         ".then(()=>{"
         "const el=document.getElementById('save-status');"
         "if(el){el.textContent='Saved!';el.className='status ok';el.style.display='block';"
         "setTimeout(()=>el.style.display='none',2500);}"
         "if(btn)btn.textContent='Save';});"
         "}"
         "</script>"
         "</body></html>");
  return p;
}
