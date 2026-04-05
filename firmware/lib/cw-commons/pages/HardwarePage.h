#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "CWWebUI.h"

inline void cw_sendHardwarePage(WiFiClient& client) {
  cw_sendPageStart(client, "hardware");

  client.println(R"HTML(
  <div class="section">
    <div class="section-title">LED panel</div>

    <div class="row">
      <label>LED colour order</label>
      <div class="ctrl">
        <select id="ledColorOrder">
          <option value="0">RGB (default)</option>
          <option value="1">RBG</option>
          <option value="2">GBR</option>
        </select>
      </div>
    </div>

    <div class="row">
      <label>Reverse phase (CLKPHASE)</label>
      <div class="ctrl"><input type="checkbox" id="reversePhase"></div>
    </div>

    <div class="row">
      <label>Driver chip</label>
      <div class="ctrl">
        <select id="driver">
          <option value="0">SHIFTREG</option>
          <option value="1">FM6124</option>
          <option value="2">FM6126A</option>
          <option value="3">ICN2038S</option>
          <option value="4">MBI5124</option>
          <option value="5">DP3246</option>
        </select>
      </div>
    </div>

    <div class="row">
      <label>I2S speed</label>
      <div class="ctrl">
        <select id="i2cSpeed">
          <option value="8000000">8 MHz</option>
          <option value="16000000">16 MHz</option>
          <option value="20000000">20 MHz</option>
        </select>
      </div>
    </div>

    <div class="row">
      <label>Rotation</label>
      <div class="ctrl">
        <select id="displayRotation">
          <option value="0">0°</option>
          <option value="1">90°</option>
          <option value="2">180°</option>
          <option value="3">270°</option>
        </select>
      </div>
    </div>

    <div class="row">
      <label>E pin (64-row panels)</label>
      <div class="ctrl"><input type="number" id="E_pin" min="0" max="39"></div>
    </div>

    <div class="row">
      <label>LDR pin</label>
      <div class="ctrl"><input type="number" id="ldrPin" min="0" max="39"></div>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Actions</div>
    <div class="footer">
      <button class="btn btn-primary" onclick="saveHw()">Apply</button>
      <button class="btn btn-danger" onclick="if(confirm('Reboot device?'))restart()">Reboot</button>
    </div>
  </div>

  <script>
  function onSettingsLoaded(h){
    $('ledColorOrder').value = h['ledcolororder']||'0';
    $('reversePhase').checked = (h['reversephase']==='1');
    $('driver').value = h['driver']||'0';
    $('i2cSpeed').value = h['i2cspeed']||'8000000';
    $('displayRotation').value = h['displayrotation']||'0';
    $('E_pin').value = h['e_pin']||'';
    $('ldrPin').value = h['ldrpin']||'';
  }

  async function saveHw(){
    try{
      await setKey('ledColorOrder', $('ledColorOrder').value);
      await setKey('reversePhase', $('reversePhase').checked?1:0);
      await setKey('driver', $('driver').value);
      await setKey('i2cSpeed', $('i2cSpeed').value);
      await setKey('displayRotation', $('displayRotation').value);
      await setKey('E_pin', $('E_pin').value);
      await setKey('ldrPin', $('ldrPin').value);
      toast('Applied ✓');
    } catch(e){ toast('Save failed', false); }
  }
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
