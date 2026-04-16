#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "web/CWWebUI.h"

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

  <script>
  function $(id){return document.getElementById(id)}
  var devTZ = 'UTC';
  var bootEpochMs = Date.now();
  function fmtUptimeMinutes(totalSec){
    var days = Math.floor(totalSec / 86400);
    var hrs = Math.floor((totalSec % 86400) / 3600);
    var mins = Math.floor((totalSec % 3600) / 60);
    return days+'d '+hrs+'h '+mins+'m';
  }
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
    var uptimeSec = parseInt(h['uptimesec']||'0', 10);
    if (Number.isFinite(uptimeSec) && uptimeSec >= 0) {
      bootEpochMs = Date.now() - (uptimeSec * 1000);
      $('uptime').textContent = fmtUptimeMinutes(uptimeSec);
    }
  }
  function tickClock(){
    $('dev-time').textContent = new Date().toLocaleTimeString('en-GB',{timeZone:devTZ,hour12:false,hourCycle:'h23',hour:'2-digit',minute:'2-digit'});
  }
  tickClock(); setInterval(tickClock, 1000);
  setInterval(function(){
    var s = Math.max(0, Math.floor((Date.now()-bootEpochMs)/1000));
    $('uptime').textContent = fmtUptimeMinutes(s);
  }, 1000);
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
