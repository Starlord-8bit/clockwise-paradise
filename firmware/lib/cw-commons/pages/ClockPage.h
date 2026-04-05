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
        <div class="hint">Saves immediately. Restart to apply.</div>
      </div>
    </div>

    <div class="row">
      <label>Auto-change</label>
      <div class="ctrl">
        <select id="autoChange">
          <option value="0">Off</option>
          <option value="1">Sequence</option>
          <option value="2">Random</option>
        </select>
      </div>
    </div>

    <div class="row">
      <label>24h format</label>
      <div class="ctrl">
        <input type="checkbox" id="use24h">
      </div>
    </div>

    <div class="row">
      <label>Brightness</label>
      <div class="ctrl">
        <input type="range" id="displayBright" min="0" max="255" value="50" oninput="$('brightVal').textContent=this.value">
        <div class="hint">Value: <b id="brightVal">50</b></div>
      </div>
    </div>

    <div class="row">
      <label>Auto-bright min/max</label>
      <div class="ctrl">
        <input type="number" id="autoBrightMin" min="0" max="4095" placeholder="min (dark)">
        <div style="height:6px"></div>
        <input type="number" id="autoBrightMax" min="0" max="4095" placeholder="max (bright)">
        <div class="hint">LDR range 0–4095.</div>
      </div>
    </div>

    <div class="row">
      <label>Night mode</label>
      <div class="ctrl">
        <select id="nightMode">
          <option value="0">Off</option>
          <option value="1">Schedule</option>
          <option value="2">Always on</option>
        </select>
      </div>
    </div>

    <div class="row">
      <label>Night schedule</label>
      <div class="ctrl">
        <div style="display:flex;gap:10px">
          <input type="number" id="nightStartH" min="0" max="23" placeholder="start h">
          <input type="number" id="nightStartM" min="0" max="59" placeholder="start m">
        </div>
        <div style="height:6px"></div>
        <div style="display:flex;gap:10px">
          <input type="number" id="nightEndH" min="0" max="23" placeholder="end h">
          <input type="number" id="nightEndM" min="0" max="59" placeholder="end m">
        </div>
      </div>
    </div>

  </div>

  <div class="section">
    <div class="section-title">Actions</div>
    <div class="footer">
      <button class="btn btn-primary" onclick="saveClock()">Save</button>
      <button class="btn btn-danger" onclick="restart()">Reboot</button>
    </div>
  </div>

  <script>
  function onSettingsLoaded(h){
    // clockface_name like cw-cf-0x01..0x07, map to index 0..6
    var cf = (h['clockface_name']||'').toLowerCase();
    var map = {'cw-cf-0x01':0,'cw-cf-0x02':1,'cw-cf-0x03':2,'cw-cf-0x04':3,'cw-cf-0x05':4,'cw-cf-0x06':5,'cw-cf-0x07':6};
    if(map.hasOwnProperty(cf)) $('clockFace').value = String(map[cf]);
    $('autoChange').value = h['autochange']||'0';
    $('use24h').checked = (h['use24hformat']==='1');
    $('displayBright').value = h['displaybright']||'50'; $('brightVal').textContent = $('displayBright').value;
    $('autoBrightMin').value = h['autobrightmin']||'';
    $('autoBrightMax').value = h['autobrightmax']||'';
    $('nightMode').value = h['nightmode']||'0';
    $('nightStartH').value = h['nightstarth']||'';
    $('nightStartM').value = h['nightstartm']||'';
    $('nightEndH').value   = h['nightendh']||'';
    $('nightEndM').value   = h['nightendm']||'';
  }

  async function saveClock(){
    try{
      // dispatcher index is 0-based
      await setKey('clockFaceIndex', $('clockFace').value);
      await setKey('autoChange', $('autoChange').value);
      await setKey('use24hFormat', $('use24h').checked?1:0);
      await setKey('displayBright', $('displayBright').value);
      if($('autoBrightMin').value!=='' && $('autoBrightMax').value!==''){
        const minv=String($('autoBrightMin').value).padStart(4,'0');
        const maxv=String($('autoBrightMax').value).padStart(4,'0');
        await setKey('autoBright', minv+','+maxv);
      }
      await setKey('nightMode', $('nightMode').value);
      if($('nightStartH').value!=='') await setKey('nightStartH', $('nightStartH').value);
      if($('nightStartM').value!=='') await setKey('nightStartM', $('nightStartM').value);
      if($('nightEndH').value!=='') await setKey('nightEndH', $('nightEndH').value);
      if($('nightEndM').value!=='') await setKey('nightEndM', $('nightEndM').value);
      toast('Saved — reboot to apply');
    } catch(e){ toast('Save failed', false); }
  }
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
