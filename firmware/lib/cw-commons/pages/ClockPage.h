#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "web/CWWebUI.h"

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
        <div class="hint">Device will reboot automatically after apply (~5s). Clockface selection persists across reboots.</div>
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
      <label>Manual brightness <span id="brightVal" class="ver-badge">50%</span></label>
      <div class="ctrl">
        <input type="checkbox" id="manualMode">
        <label for="manualMode"> Manual mode</label>
        <input type="range" id="displayBright" min="0" max="100" value="50"
          oninput="$('brightVal').textContent=this.value+'%'">
        <div class="hint">Used when Manual mode is enabled.</div>
      </div>
    </div>

    <div class="row">
      <label>Auto brightness (LDR)</label>
      <div class="ctrl">
        <input type="checkbox" id="autoMode">
        <label for="autoMode"> Auto brightness</label>
        <div style="display:flex;gap:8px;align-items:center">
          <input type="number" id="autoBrightMin" min="0" max="4095" placeholder="min (dark)" style="flex:1">
          <span class="small">–</span>
          <input type="number" id="autoBrightMax" min="0" max="4095" placeholder="max (bright)" style="flex:1">
        </div>
        <div class="hint">LDR calibration range, mapped to brightness.</div>
      </div>
    </div>

  </div>

  <div class="section">
    <div class="section-title">Night mode</div>

    <div class="row">
      <label>Enable night mode</label>
      <div class="ctrl">
        <input type="checkbox" id="nightEnable">
      </div>
    </div>

    <div class="row">
      <label>Trigger</label>
      <div class="ctrl">
        <select id="nightTrig">
          <option value="0">Time window</option>
          <option value="1">LDR threshold</option>
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
      <label>LDR threshold</label>
      <div class="ctrl">
        <input type="number" id="nightLdrThr" min="0" max="4095" placeholder="128">
      </div>
    </div>

    <div class="row">
      <label>When triggered</label>
      <div class="ctrl">
        <select id="nightAction">
          <option value="0">Display off</option>
          <option value="1">Set minimum brightness</option>
        </select>
      </div>
    </div>

    <div class="row">
      <label>Night minimum brightness</label>
      <div class="ctrl">
        <input type="range" id="nightMinBr" min="0" max="100" value="8"
          oninput="$('nightMinBrVal').textContent=this.value+'%'">
        <div class="hint" id="nightMinBrVal">8%</div>
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
    $('clockFace').dataset.orig = $('clockFace').value;
    $('autoChange').value  = h['autochange']||'0';
    $('use24h').checked    = (h['use24hformat']==='1');
    var displayRaw = parseInt(h['displaybright']||'50', 10);
    var displayPct = Math.round((displayRaw * 100) / 255);
    $('displayBright').value = String(displayPct);
    $('brightVal').textContent = displayPct + '%';

    var method = parseInt(h['brightmethod']||'2', 10);
    $('autoMode').checked = (method === 0);
    $('manualMode').checked = (method === 2);

    $('autoBrightMin').value  = h['autobrightmin']||'';
    $('autoBrightMax').value  = h['autobrightmax']||'';

    $('nightEnable').checked = (h['nightmode']||'0') !== '0';
    $('nightTrig').value = h['nighttrig']||'0';
    $('nightLdrThr').value = h['nightldrthr']||'128';
    $('nightAction').value = h['nightaction']||'0';
    var nightMinRaw = parseInt(h['nightminbr']||'8', 10);
    var nightMinPct = Math.round((nightMinRaw * 100) / 255);
    $('nightMinBr').value = String(nightMinPct);
    $('nightMinBrVal').textContent = nightMinPct + '%';

    $('nightStartH').value  = h['nightstarth']||'';
    $('nightStartM').value  = h['nightstartm']||'';
    $('nightEndH').value    = h['nightendh']||'';
    $('nightEndM').value    = h['nightendm']||'';
  }

  document.addEventListener('DOMContentLoaded', function(){
    document.getElementById('autoMode').addEventListener('change', function(){
      if (this.checked) document.getElementById('manualMode').checked = false;
    });
    document.getElementById('manualMode').addEventListener('change', function(){
      if (this.checked) document.getElementById('autoMode').checked = false;
    });
  });

  async function applyClock(){
    try{
      // Clockface index triggers auto-reboot — send it last
      var cfChanged = $('clockFace').value !== $('clockFace').dataset.orig;
      await setKey('autoChange',     $('autoChange').value);
      await setKey('use24hFormat',   $('use24h').checked?1:0);
      var displayBrightRaw = Math.round((parseInt($('displayBright').value || '0', 10) * 255) / 100);
      await setKey('displayBright',  displayBrightRaw);

      var brightMethod = 1;
      if ($('autoMode').checked) brightMethod = 0;
      else if ($('manualMode').checked) brightMethod = 2;
      await setKey('brightMethod', brightMethod);

      if($('autoBrightMin').value!=='' && $('autoBrightMax').value!==''){
        var minv = String($('autoBrightMin').value).padStart(4,'0');
        var maxv = String($('autoBrightMax').value).padStart(4,'0');
        await setKey('autoBright', minv+','+maxv);
      }
      await setKey('nightMode', $('nightEnable').checked ? 1 : 0);
      await setKey('nightTrig', $('nightTrig').value);
      if($('nightLdrThr').value!=='') await setKey('nightLdrThr', $('nightLdrThr').value);
      await setKey('nightAction', $('nightAction').value);
      var nightMinRaw = Math.round((parseInt($('nightMinBr').value || '0', 10) * 255) / 100);
      await setKey('nightMinBr', nightMinRaw);
      if($('nightStartH').value!=='') await setKey('nightStartH', $('nightStartH').value);
      if($('nightStartM').value!=='') await setKey('nightStartM', $('nightStartM').value);
      if($('nightEndH').value!=='')   await setKey('nightEndH',   $('nightEndH').value);
      if($('nightEndM').value!=='')   await setKey('nightEndM',   $('nightEndM').value);
      if(cfChanged){
        toast('Clockface saved — rebooting in ~3s…');
        await setKey('clockFaceIndex', $('clockFace').value);
        // Device will reboot; reload page after ~8s
        setTimeout(()=>location.reload(), 8000);
      } else {
        toast('Applied ✓');
      }
    } catch(e){ toast('Failed: '+e, false); }
  }
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
