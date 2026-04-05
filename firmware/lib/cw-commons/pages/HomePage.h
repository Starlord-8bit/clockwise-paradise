#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "CWWebUI.h"

inline void cw_sendHomePage(WiFiClient& client) {
  cw_sendPageStart(client, "home");

  client.println(R"HTML(
  <div class="section">
    <div class="section-title">Status</div>
    <div class="row"><label>Firmware</label><div class="ctrl"><span class="ver-badge" id="fw"></span></div></div>
    <div class="row"><label>Clockface</label><div class="ctrl"><span id="cf" class="small"></span></div></div>
    <div class="row"><label>WiFi</label><div class="ctrl"><span id="wifi" class="small"></span></div></div>
  </div>

  <div class="section">
    <div class="section-title">Navigation</div>
    <div class="row"><label>Clock settings</label><div class="ctrl"><a class="btn btn-secondary btn-block" href="/clock">Open</a></div></div>
    <div class="row"><label>Sync & Connectivity</label><div class="ctrl"><a class="btn btn-secondary btn-block" href="/sync">Open</a></div></div>
    <div class="row"><label>Hardware</label><div class="ctrl"><a class="btn btn-secondary btn-block" href="/hardware">Open</a></div></div>
    <div class="row"><label>Firmware update</label><div class="ctrl"><a class="btn btn-primary btn-block" href="/update">Open</a></div></div>
  </div>

  <div class="section">
    <div class="section-title">Quick actions</div>
    <div class="footer">
      <button class="btn btn-secondary" onclick="location.href='/backup'">⬇ Backup config</button>
      <button class="btn btn-danger" onclick="restart()">⚡ Reboot</button>
    </div>
  </div>

  <script>
  function onSettingsLoaded(h){
    $('fw').textContent = (h['cw_fw_name']||'Clockwise Paradise') + ' v' + (h['cw_fw_version']||'?');
    $('cf').textContent = h['clockface_name'] || 'UNKNOWN';
    $('wifi').textContent = (h['wifissid']? (h['wifissid']+' @ '+(h['sta ip']||'')) : '');
  }
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
