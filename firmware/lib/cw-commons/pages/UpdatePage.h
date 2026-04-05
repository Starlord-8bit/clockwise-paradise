#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "CWWebUI.h"

inline void cw_sendUpdatePage(WiFiClient& client) {
  cw_sendPageStart(client, "update");

  client.println(R"HTML(
  <div class="section">
    <div class="section-title">Firmware</div>
    <div class="row"><label>Current version</label><div class="ctrl"><span class="ver-badge" id="ver">?</span></div></div>
    <div class="row"><label>Check GitHub update</label>
      <div class="ctrl"><button class="btn btn-secondary btn-block" onclick="checkUpdate()">Check</button>
      <div class="hint">Uses /ota/check and /ota/update (GitHub releases).</div></div>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Manual OTA (.bin upload)</div>
    <div class="row">
      <label>Upload app binary</label>
      <div class="ctrl">
        <input type="file" id="bin" accept=".bin" />
        <button class="btn btn-primary btn-block" onclick="upload()">Upload & Flash</button>
        <div class="hint">If using curl: add header <code>Expect:</code> to avoid 100-continue.</div>
        <div class="hint" id="upStatus"></div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Actions</div>
    <div class="footer">
      <button class="btn btn-danger" onclick="restart()">Reboot</button>
    </div>
  </div>

  <script>
  function onSettingsLoaded(h){ $('ver').textContent = 'v'+(h['cw_fw_version']||'?'); }

  function checkUpdate(){
    toast('Checking…');
    fetch('/ota/check').then(r=>r.json()).then(data=>{
      if(data.available){
        if(confirm('Update available: '+data.latest+'\nCurrent: '+data.current+'\n\nFlash now?')){
          fetch('/ota/update',{method:'POST'}).then(()=>toast('Updating… reconnect in ~30s'));
        }
      } else {
        toast('Up to date');
      }
    }).catch(()=>toast('Check failed', false));
  }

  function upload(){
    const f=$('bin').files[0];
    if(!f){toast('Pick a .bin first', false);return;}
    $('upStatus').textContent='Uploading '+Math.round(f.size/1024)+' KB…';
    fetch('/ota/upload',{
      method:'POST',
      headers:{'Content-Type':'application/octet-stream','Content-Length':String(f.size)},
      body:f
    }).then(r=>r.json()).then(j=>{
      if(j.status==='ok') { $('upStatus').textContent='Done. Rebooting…'; }
      else { $('upStatus').textContent='Failed: '+(j.message||''); }
    }).catch(()=>{
      // device may reboot mid-response
      $('upStatus').textContent='Rebooting… reconnect in ~15s';
    });
  }
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
