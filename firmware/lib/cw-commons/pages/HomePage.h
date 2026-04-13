#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "CWWebUI.h"

inline void cw_sendHomePage(WiFiClient& client) {
  cw_sendPageStart(client, "home");

  client.println(R"HTML(
  <div class="section">
    <div class="section-title">Device Status</div>
    <div class="row"><label>Firmware</label><div class="ctrl"><span class="ver-badge" id="fw-ver">…</span></div></div>
    <div class="row"><label>Clockface</label><div class="ctrl"><span class="small" id="cf-name">…</span></div></div>
    <div class="row"><label>WiFi</label><div class="ctrl"><span class="small" id="wifi-info">…</span></div></div>
    <div class="row"><label>Time</label><div class="ctrl"><span class="small" id="dev-time">…</span></div></div>
    <div class="row"><label>Uptime</label><div class="ctrl"><span class="small" id="uptime">…</span></div></div>
    <div class="row"><label>Brightness</label><div class="ctrl"><span class="small" id="bright-info">…</span></div></div>
    <div class="row"><label>MQTT</label><div class="ctrl"><span class="small" id="mqtt-status">…</span></div></div>
    <div class="row"><label>HA Discovery</label><div class="ctrl"><span class="small" id="ha-status">⚠ Not implemented</span></div></div>
  </div>

  <div class="section">
    <div class="section-title">Quick actions</div>
    <div class="footer">
      <a class="btn btn-secondary" href="/clock">🕐 Clock</a>
      <a class="btn btn-secondary" href="/widgets">🧩 Widgets</a>
      <a class="btn btn-secondary" href="/sync">🔗 Sync</a>
      <a class="btn btn-secondary" href="/hardware">🔧 Hardware</a>
      <a class="btn btn-primary" href="/update">⬆ Update</a>
      <button class="btn btn-secondary" onclick="location.href='/backup'">⬇ Backup</button>
      <button class="btn btn-danger" onclick="if(confirm('Reboot device?'))restart()">⚡ Reboot</button>
    </div>
  </div>

  <script>
  function $(id){return document.getElementById(id)}
  var devTZ = 'UTC';
  var uptimeDays = 0;
  var uptimeStart = Date.now();
  function onSettingsLoaded(h){
    $('fw-ver').textContent = (h['cw_fw_name']||'Clockwise Paradise') + ' v' + (h['cw_fw_version']||'?');
    $('cf-name').textContent = h['clockface_name'] || '?';
    var ip = h['wifiip'] || '';
    var ssid = h['wifissid'] || '';
    $('wifi-info').textContent = ip ? (ip + ' @ ' + ssid) : ssid;
    $('bright-info').textContent = 'Level ' + (h['displaybright']||'?') +
      ' / LDR range ' + (h['autobrightmin']||'0') + '–' + (h['autobrightmax']||'?');
    $('mqtt-status').textContent = (h['mqttenabled']==='1')
      ? '✅ Enabled — broker: ' + (h['mqttbroker']||'?')
      : '❌ Disabled';
    devTZ = h['timezone'] || 'UTC';
    uptimeDays = parseInt(h['totaldays']||'0', 10);
  }
  function tickClock(){
    $('dev-time').textContent = new Date().toLocaleString('en-GB',{timeZone:devTZ,hour12:false,hour:'2-digit',minute:'2-digit',second:'2-digit'});
  }
  tickClock(); setInterval(tickClock, 1000);
  setInterval(function(){
    var s = Math.floor((Date.now()-uptimeStart)/1000);
    var hrs=Math.floor(s/3600), mins=Math.floor((s%3600)/60), secs=s%60;
    $('uptime').textContent = uptimeDays+'d '+hrs+'h '+mins+'m '+secs+'s';
  }, 1000);
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
