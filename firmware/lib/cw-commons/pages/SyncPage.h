#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "CWWebUI.h"

inline void cw_sendSyncPage(WiFiClient& client) {
  cw_sendPageStart(client, "sync");

  client.println(R"HTML(
  <div class="section">
    <div class="section-title">Time sync</div>

    <div class="row">
      <label>Timezone</label>
      <div class="ctrl">
        <select id="timeZone">
          <option value="UTC">UTC</option>
          <option value="America/New_York">America/New_York</option>
          <option value="America/Chicago">America/Chicago</option>
          <option value="America/Denver">America/Denver</option>
          <option value="America/Los_Angeles">America/Los_Angeles</option>
          <option value="America/Toronto">America/Toronto</option>
          <option value="America/Mexico_City">America/Mexico_City</option>
          <option value="America/Sao_Paulo">America/Sao_Paulo</option>
          <option value="Europe/London">Europe/London</option>
          <option value="Europe/Paris">Europe/Paris</option>
          <option value="Europe/Berlin">Europe/Berlin</option>
          <option value="Europe/Rome">Europe/Rome</option>
          <option value="Europe/Madrid">Europe/Madrid</option>
          <option value="Europe/Athens">Europe/Athens</option>
          <option value="Europe/Moscow">Europe/Moscow</option>
          <option value="Asia/Dubai">Asia/Dubai</option>
          <option value="Asia/Kolkata">Asia/Kolkata</option>
          <option value="Asia/Singapore">Asia/Singapore</option>
          <option value="Asia/Tokyo">Asia/Tokyo</option>
          <option value="Australia/Sydney">Australia/Sydney</option>
          <option value="Pacific/Auckland">Pacific/Auckland</option>
        </select>
        <div class="hint">Frequently used timezones.</div>
      </div>
    </div>

    <div class="row">
      <label>NTP server</label>
      <div class="ctrl">
        <input type="text" id="ntpServer" placeholder="pool.ntp.org">
      </div>
    </div>

    <div class="row">
      <label>Manual POSIX (optional)</label>
      <div class="ctrl">
        <input type="text" id="manualPosix" placeholder="">
        <div class="hint">Leave blank unless you need to override TZ lookup.</div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="section-title">MQTT</div>

    <div class="row">
      <label>Enable MQTT</label>
      <div class="ctrl"><input type="checkbox" id="mqttEnabled"></div>
    </div>
    <div class="row"><label>Broker host</label><div class="ctrl"><input type="text" id="mqttBroker" placeholder="192.168.1.10"></div></div>
    <div class="row"><label>Port</label><div class="ctrl"><input type="number" id="mqttPort" min="1" max="65535" placeholder="1883"></div></div>
    <div class="row"><label>Username</label><div class="ctrl"><input type="text" id="mqttUser" placeholder="(optional)"></div></div>
    <div class="row"><label>Password</label><div class="ctrl"><input type="password" id="mqttPass" placeholder="(optional)"></div></div>
    <div class="row"><label>Topic prefix</label><div class="ctrl"><input type="text" id="mqttPrefix" placeholder="clockwise"></div></div>
    <div class="row">
      <label>HA Discovery</label>
      <div class="ctrl">
        <input type="checkbox" id="haDiscovery" disabled>
        <div class="hint">⚠ Not yet implemented in firmware — planned for a future release.</div>
      </div>
    </div>
    <div class="hint" style="padding:10px 14px">Password is stored on device only; not included in config exports.</div>
  </div>

  <div class="section">
    <div class="section-title">Actions</div>
    <div class="footer">
      <button class="btn btn-primary" onclick="applySync()">Apply</button>
      <button class="btn btn-danger" onclick="if(confirm('Reboot device?'))restart()">Reboot</button>
    </div>
  </div>

  <script>
  function decodeVal(v){ try{ return decodeURIComponent(v||''); } catch(e){ return v||''; } }
  function onSettingsLoaded(h){
    $('timeZone').value    = decodeVal(h['timezone']);
    $('ntpServer').value   = decodeVal(h['ntpserver']);
    $('manualPosix').value = decodeVal(h['manualposix']);
    $('mqttEnabled').checked = (h['mqttenabled']==='1');
    $('mqttBroker').value  = h['mqttbroker']||'';
    $('mqttPort').value    = h['mqttport']||'1883';
    $('mqttUser').value    = h['mqttuser']||'';
    $('mqttPrefix').value  = h['mqttprefix']||'clockwise';
    // mqttPass not returned by server
  }

  async function applySync(){
    try{
      await setKey('timeZone',    $('timeZone').value);
      await setKey('ntpServer',   $('ntpServer').value);
      await setKey('manualPosix', $('manualPosix').value);
      await setKey('mqttEnabled', $('mqttEnabled').checked?1:0);
      await setKey('mqttBroker',  $('mqttBroker').value);
      await setKey('mqttPort',    $('mqttPort').value);
      await setKey('mqttUser',    $('mqttUser').value);
      if($('mqttPass').value) await setKey('mqttPass', $('mqttPass').value);
      await setKey('mqttPrefix',  $('mqttPrefix').value);
      toast('Applied ✓');
    } catch(e){ toast('Failed: '+e, false); }
  }
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
