#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "CWWebUI.h"

inline void cw_sendClockPage(WiFiClient& client) {
  cw_sendPageStart(client, "clock");

  client.println(R"HTML(
  <div class="section">
    <div class="section-title">Clock</div>

    <div class="row">
      <label>Clockface</label>
      <div class="ctrl">
        <select id="clockFace">
          <option value="0">Super Mario</option>
          <option value="1">Time in Words</option>
          <option value="2">World Map</option>
          <option value="3">Castlevania</option>
          <option value="4">Pac-Man</option>
          <option value="5">Pokedex</option>
          <option value="6">Canvas</option>
        </select>
        <div class="hint">⚠ Live-switching not yet implemented — requires reboot to take effect.</div>
      </div>
    </div>

    <div class="row">
      <label>Auto-change</label>
      <div class="ctrl">
        <select id="autoChange">
          <option value="0">Off</option>
          <option value="1">Sequence (at midnight)</option>
          <option value="2">Random (at midnight)</option>
        </select>
        <div class="hint">⚠ Requires multi-clockface dispatcher (planned).</div>
      </div>
    </div>

    <div class="row">
      <label>24h format</label>
      <div class="ctrl"><input type="checkbox" id="use24h"></div>
    </div>

    <div class="row">
      <label>Brightness <span id="brightVal" class="ver-badge">50</span></label>
      <div class="ctrl">
        <input type="range" id="displayBright" min="0" max="255" value="50"
          oninput="$('brightVal').textContent=this.value">
      </div>
    </div>

    <div class="row">
      <label>Auto-bright (LDR)</label>
      <div class="ctrl">
        <div style="display:flex;gap:8px;align-items:center">
          <input type="number" id="autoBrightMin" min="0" max="4095" placeholder="min (dark)" style="flex:1">
          <span class="small">–</span>
          <input type="number" id="autoBrightMax" min="0" max="4095" placeholder="max (bright)" style="flex:1">
        </div>
        <div class="hint">LDR ADC range 0–4095.</div>
      </div>
    </div>

  </div>

  <div class="section">
    <div class="section-title">Night mode</div>

    <div class="row">
      <label>Mode</label>
      <div class="ctrl">
        <select id="nightMode">
          <option value="0">Off</option>
          <option value="1">Schedule</option>
          <option value="2">Always dim</option>
        </select>
      </div>
    </div>

    <div class="row">
      <label>Schedule start</label>
      <div class="ctrl">
        <div style="display:flex;gap:8px">
          <input type="number" id="nightStartH" min="0" max="23" placeholder="HH" style="flex:1">
          <input type="number" id="nightStartM" min="0" max="59" placeholder="MM" style="flex:1">
        </div>
      </div>
    </div>

    <div class="row">
      <label>Schedule end</label>
      <div class="ctrl">
        <div style="display:flex;gap:8px">
          <input type="number" id="nightEndH" min="0" max="23" placeholder="HH" style="flex:1">
          <input type="number" id="nightEndM" min="0" max="59" placeholder="MM" style="flex:1">
        </div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Actions</div>
    <div class="footer">
      <button class="btn btn-primary" onclick="applyClock()">Apply</button>
      <button class="btn btn-danger" onclick="if(confirm('Reboot device?'))restart()">Reboot</button>
    </div>
  </div>

  <script>
  function onSettingsLoaded(h){
    // clockface_name like cw-cf-0x01..0x07, map to index 0..6
    var cf = (h['clockface_name']||'').toLowerCase();
    var map={'cw-cf-0x01':0,'cw-cf-0x02':1,'cw-cf-0x03':2,'cw-cf-0x04':3,'cw-cf-0x05':4,'cw-cf-0x06':5,'cw-cf-0x07':6};
    if(map.hasOwnProperty(cf)) $('clockFace').value = String(map[cf]);
    $('autoChange').value  = h['autochange']||'0';
    $('use24h').checked    = (h['use24hformat']==='1');
    var bv = h['displaybright']||'50';
    $('displayBright').value = bv; $('brightVal').textContent = bv;
    $('autoBrightMin').value  = h['autobrightmin']||'';
    $('autoBrightMax').value  = h['autobrightmax']||'';
    $('nightMode').value    = h['nightmode']||'0';
    $('nightStartH').value  = h['nightstarth']||'';
    $('nightStartM').value  = h['nightstartm']||'';
    $('nightEndH').value    = h['nightendh']||'';
    $('nightEndM').value    = h['nightendm']||'';
  }

  async function applyClock(){
    try{
      await setKey('clockFaceIndex', $('clockFace').value);
      await setKey('autoChange',     $('autoChange').value);
      await setKey('use24hFormat',   $('use24h').checked?1:0);
      await setKey('displayBright',  $('displayBright').value);
      if($('autoBrightMin').value!=='' && $('autoBrightMax').value!==''){
        var minv = String($('autoBrightMin').value).padStart(4,'0');
        var maxv = String($('autoBrightMax').value).padStart(4,'0');
        await setKey('autoBright', minv+','+maxv);
      }
      await setKey('nightMode',   $('nightMode').value);
      if($('nightStartH').value!=='') await setKey('nightStartH', $('nightStartH').value);
      if($('nightStartM').value!=='') await setKey('nightStartM', $('nightStartM').value);
      if($('nightEndH').value!=='')   await setKey('nightEndH',   $('nightEndH').value);
      if($('nightEndM').value!=='')   await setKey('nightEndM',   $('nightEndM').value);
      toast('Applied ✓');
    } catch(e){ toast('Failed: '+e, false); }
  }
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
