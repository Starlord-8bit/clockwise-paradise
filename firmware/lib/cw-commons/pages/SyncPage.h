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
        <input type="text" id="timeZone" placeholder="Europe/Stockholm">
        <div class="hint">TZ database id (e.g. Europe/Stockholm).</div>
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
        <div class="hint">Leave blank unless you know you need it.</div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="section-title">MQTT</div>

    <div class="row"><label>Enable MQTT</label><div class="ctrl"><input type="checkbox" id="mqttEnabled"></div></div>

    <div class="row"><label>Broker</label><div class="ctrl"><input type="text" id="mqttBroker" placeholder="192.168.1.10"></div></div>
    <div class="row"><label>Port</label><div class="ctrl"><input type="number" id="mqttPort" min="1" max="65535" placeholder="1883"></div></div>
    <div class="row"><label>User</label><div class="ctrl"><input type="text" id="mqttUser" placeholder=""></div></div>
    <div class="row"><label>Password</label><div class="ctrl"><input type="password" id="mqttPass" placeholder=""></div></div>
    <div class="row"><label>Topic prefix</label><div class="ctrl"><input type="text" id="mqttPrefix" placeholder="clockwise"></div></div>

    <div class="hint" style="padding:10px 14px">Note: password is stored on device; not exported in backups.</div>
  </div>

  <div class="section">
    <div class="section-title">Actions</div>
    <div class="footer">
      <button class="btn btn-primary" onclick="saveSync()">Save</button>
      <button class="btn btn-danger" onclick="restart()">Reboot</button>
    </div>
  </div>

  <script>
  function onSettingsLoaded(h){
    $('timeZone').value = h['timezone']||'';
    $('ntpServer').value = h['ntpserver']||'';
    $('manualPosix').value = h['manualposix']||'';

    $('mqttEnabled').checked = (h['mqttenabled']==='1');
    $('mqttBroker').value = h['mqttbroker']||'';
    $('mqttPort').value = h['mqttport']||'1883';
    $('mqttUser').value = h['mqttuser']||'';
    $('mqttPass').value = h['mqttpass']||'';
    $('mqttPrefix').value = h['mqttprefix']||'clockwise';
  }

  async function saveSync(){
    try{
      await setKey('timeZone', $('timeZone').value);
      await setKey('ntpServer', $('ntpServer').value);
      await setKey('manualPosix', $('manualPosix').value);

      await setKey('mqttEnabled', $('mqttEnabled').checked?1:0);
      await setKey('mqttBroker', $('mqttBroker').value);
      await setKey('mqttPort', $('mqttPort').value);
      await setKey('mqttUser', $('mqttUser').value);
      await setKey('mqttPass', $('mqttPass').value);
      await setKey('mqttPrefix', $('mqttPrefix').value);
      toast('Saved — reboot to apply');
    } catch(e){ toast('Save failed', false); }
  }
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
