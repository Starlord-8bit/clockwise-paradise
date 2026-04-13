#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "CWWebUI.h"

inline void cw_sendWidgetsPage(WiFiClient& client) {
  cw_sendPageStart(client, "widgets");

  client.println(R"HTML(
  <div class="section">
    <div class="section-title">Runtime Status</div>
    <div class="row"><label>Active widget</label><div class="ctrl"><span class="ver-badge" id="ws-active">...</span></div></div>
    <div class="row"><label>Clockface context</label><div class="ctrl"><span class="small" id="ws-clockface">...</span></div></div>
    <div class="row"><label>Timer remaining</label><div class="ctrl"><span class="small" id="ws-timer">-</span></div></div>
    <div class="row"><label>Last command</label><div class="ctrl"><span class="small" id="ws-last">idle</span></div></div>
    <div class="row"><label>Auto refresh</label><div class="ctrl"><input type="checkbox" id="ws-poll" checked></div></div>
  </div>

  <div class="section">
    <div class="section-title">Temporary Activation</div>
    <div class="row">
      <label>Timer duration (seconds)</label>
      <div class="ctrl">
        <input type="number" id="timerSecs" min="1" max="3600" value="90">
        <div class="hint">Used when starting the timer widget.</div>
      </div>
    </div>
    <div class="footer">
      <button class="btn btn-secondary" onclick="setTimerPreset(30)">30s</button>
      <button class="btn btn-secondary" onclick="setTimerPreset(60)">60s</button>
      <button class="btn btn-secondary" onclick="setTimerPreset(300)">300s</button>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Widgets</div>
    <div id="widgets-cards"></div>
  </div>

  <div class="section">
    <div class="section-title">Return to Clock</div>
    <div class="footer">
      <button class="btn btn-primary" onclick="returnToClock()">Return to Clock Now</button>
      <a class="btn btn-secondary" href="/clock">Open Clock Settings</a>
    </div>
  </div>

  <script>
  var widgetState = {activeWidget:'clock', timerRemainingSec:0, clockfaceName:'', canReturnToClock:false};
  var widgetCatalog = [];
  var runtimePoll = null;
  var catalogPoll = null;

  function setTimerPreset(v){ $('timerSecs').value = v; }

  function setLast(v, ok){
    var el = $('ws-last');
    el.textContent = v;
    el.style.color = ok === false ? '#e57373' : '#90a4ae';
  }

  function fmtMmSs(total){
    var m = Math.floor(total / 60);
    var s = total % 60;
    return String(m).padStart(2,'0') + ':' + String(s).padStart(2,'0');
  }

  function chip(name, cls){
    return '<span class="ver-badge" style="margin-left:8px;background:'+cls+';color:#fff">'+name+'</span>';
  }

  function cardHtml(w){
    var active = widgetState.activeWidget === w.name || w.active;
    var stateChip = active ? chip('ACTIVE', '#2e7d32') : (w.implemented ? chip('READY', '#1976d2') : chip('PLACEHOLDER', '#ed6c02'));
    var title = '<div class="row"><label>'+w.name.toUpperCase()+'</label><div class="ctrl">'+stateChip+'</div></div>';

    if (w.name === 'clock') {
      return '<div class="section">'+title+'<div class="footer"><button class="btn btn-primary" onclick="activateWidget(\'clock\')">Activate Clock</button></div></div>';
    }

    if (w.name === 'timer') {
      return '<div class="section">'+title+
             '<div class="row"><label>Duration</label><div class="ctrl"><span class="small">Uses Timer duration from above.</span></div></div>'+
             '<div class="footer"><button class="btn btn-primary" onclick="activateTimer()">Start Timer</button></div></div>';
    }

    return '<div class="section">'+title+
           '<div class="row"><label>Status</label><div class="ctrl"><span class="small">Placeholder in this firmware.</span></div></div>'+
           '<div class="footer"><button class="btn btn-secondary" disabled>Not implemented</button></div></div>';
  }

  function renderCards(){
    $('widgets-cards').innerHTML = widgetCatalog.map(cardHtml).join('');
  }

  function renderRuntime(){
    $('ws-active').textContent = widgetState.activeWidget || 'unknown';
    $('ws-clockface').textContent = widgetState.clockfaceName || 'unknown';
    $('ws-timer').textContent = widgetState.activeWidget === 'timer' ? fmtMmSs(widgetState.timerRemainingSec || 0) : '-';
  }

  async function fetchWidgetCatalog(){
    var r = await fetch('/api/widgets');
    if (!r.ok) throw new Error('widgets_status_'+r.status);
    widgetCatalog = await r.json();
    renderCards();
  }

  async function fetchWidgetState(){
    var r = await fetch('/api/widget-state');
    if (!r.ok) throw new Error('widget_state_'+r.status);
    widgetState = await r.json();
    renderRuntime();
  }

  async function activateSpec(spec){
    setLast('applying', true);
    try {
      var r = await fetch('/api/widget/show?spec='+encodeURIComponent(spec), {method:'POST'});
      if (!r.ok && r.status !== 204) throw new Error('show_status_'+r.status);
      setLast('success', true);
      await burstRefresh();
      toast('Widget applied');
    } catch (e) {
      setLast('failed', false);
      toast('Widget apply failed', false);
    }
  }

  function activateWidget(name){
    activateSpec(name);
  }

  function activateTimer(){
    var secs = parseInt($('timerSecs').value || '0', 10);
    if (isNaN(secs) || secs < 1 || secs > 3600) {
      toast('Timer duration must be 1..3600', false);
      return;
    }
    activateSpec('timer:'+secs);
  }

  function returnToClock(){
    activateSpec('clock');
  }

  async function burstRefresh(){
    for (var i = 0; i < 5; i++) {
      await fetchWidgetState();
      await new Promise(res=>setTimeout(res, 400));
    }
    await fetchWidgetCatalog();
  }

  function startPollers(){
    if (runtimePoll) clearInterval(runtimePoll);
    if (catalogPoll) clearInterval(catalogPoll);
    runtimePoll = setInterval(function(){
      if (!$('ws-poll').checked) return;
      fetchWidgetState().catch(function(){});
    }, 2000);
    catalogPoll = setInterval(function(){
      if (!$('ws-poll').checked) return;
      fetchWidgetCatalog().catch(function(){});
    }, 15000);
  }

  document.addEventListener('visibilitychange', function(){
    if (!document.hidden) {
      fetchWidgetState().catch(function(){});
      fetchWidgetCatalog().catch(function(){});
    }
  });

  async function bootWidgets(){
    try {
      await fetchWidgetState();
      await fetchWidgetCatalog();
      setLast('idle', true);
    } catch (e) {
      setLast('offline', false);
    }
    startPollers();
  }

  bootWidgets();
  </script>
  )HTML");

  cw_sendPageEnd(client);
}
