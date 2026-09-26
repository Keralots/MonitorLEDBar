(function () {
'use strict';
var $  = function (s, r) { return (r || document).querySelector(s); };
var $$ = function (s, r) { return Array.prototype.slice.call((r || document).querySelectorAll(s)); };

function post(url, data) {
  return fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: new URLSearchParams(data || {}) })
    .then(function (r) { return r.json(); });
}

// Navigation
var navToggle = $('#navToggle'), navScrim = $('#navScrim');
function setNav(open) {
  document.documentElement.classList.toggle('nav-open', open);
  if (navToggle) navToggle.setAttribute('aria-expanded', open ? 'true' : 'false');
}
function closeNav() { setNav(false); }
if (navToggle) navToggle.addEventListener('click', function () { setNav(!document.documentElement.classList.contains('nav-open')); });
if (navScrim) navScrim.addEventListener('click', closeNav);
document.addEventListener('keydown', function (e) { if (e.key === 'Escape') closeNav(); });
var navItems = $$('.nav-item');
var pages = $$('.page');
var crumb = $('#crumb');
var saveBar = $('#saveBar');
function showPage(key) {
  pages.forEach(function (p) { p.classList.toggle('active', p.dataset.page === key); });
  navItems.forEach(function (n) { n.classList.toggle('active', n.dataset.nav === key); });
  var active = navItems.filter(function (n) { return n.dataset.nav === key; })[0];
  if (active && crumb) crumb.textContent = active.textContent.trim();
  // The Light page is live control; the save bar only belongs to settings pages.
  if (saveBar) saveBar.style.display = (key === 'light' || key === 'pairing') ? 'none' : '';
  if (key === 'pairing') refreshPair();
  window.scrollTo(0, 0);
  closeNav();
  try { localStorage.setItem('ledbar_section', key); } catch (e) {}
}
navItems.forEach(function (n) { n.addEventListener('click', function () { showPage(n.dataset.nav); }); });
var startPage = 'light';
try { var s = localStorage.getItem('ledbar_section'); if (s && $('[data-page="' + s + '"]')) startPage = s; } catch (e) {}
showPage(startPage);

// Theme
var accSw = $$('.acc-sw');
function setAccent(acc) {
  document.documentElement.setAttribute('data-accent', acc);
  accSw.forEach(function (b) { b.classList.toggle('on', b.dataset.acc === acc); });
  try { localStorage.setItem('ledbar_accent', acc); } catch (e) {}
}
accSw.forEach(function (b) { b.addEventListener('click', function () { setAccent(b.dataset.acc); }); });
try { var a = localStorage.getItem('ledbar_accent'); if (a) setAccent(a); } catch (e) {}
var modeBtns = $$('.mode-toggle button');
function setMode(mode) {
  document.documentElement.setAttribute('data-mode', mode);
  modeBtns.forEach(function (b) { b.classList.toggle('on', b.dataset.mode === mode); });
  var meta = $('meta[name="theme-color"]'); if (meta) meta.setAttribute('content', mode === 'dark' ? '#161512' : '#f4f0e7');
  try { localStorage.setItem('ledbar_mode', mode); } catch (e) {}
}
modeBtns.forEach(function (b) { b.addEventListener('click', function () { setMode(b.dataset.mode); }); });
try { var m = localStorage.getItem('ledbar_mode'); if (m) setMode(m); } catch (e) {}

// Ranges and conditional panels
function fmtRange(inp) {
  var span = $('.range-val[data-for="' + inp.id + '"]');
  if (!span) return;
  var div = parseFloat(inp.dataset.div || '1');
  var fixed = parseInt(inp.dataset.fixed || '0', 10);
  span.textContent = (inp.value / div).toFixed(fixed) + (inp.dataset.suffix || '');
}
$$('input[type="range"]').forEach(function (inp) { fmtRange(inp); inp.addEventListener('input', function () { fmtRange(inp); }); });
function toggle(el, on) { if (el) el.style.display = on ? '' : 'none'; }
function syncPanels() {
  toggle($('#stepFields'), $('#tapMode').value === '1');
  toggle($('#fixedFields'), $('#onLevelMode').value === '1');
  toggle($('#mqttFields'), $('#mqttEnabled').checked);
  $('#mqttHost').required = $('#mqttEnabled').checked;
}
$('#tapMode').addEventListener('change', syncPanels);
$('#onLevelMode').addEventListener('change', syncPanels);
$('#mqttEnabled').addEventListener('change', syncPanels);
var minR = $('#minPct'), maxR = $('#maxPct');
minR.addEventListener('input', function () { if (+minR.value > +maxR.value) { maxR.value = minR.value; fmtRange(maxR); } });
maxR.addEventListener('input', function () { if (+maxR.value < +minR.value) { minR.value = maxR.value; fmtRange(minR); } });
var dn = $('#deviceName');
dn.addEventListener('input', function () { $('#hostPreview').textContent = dn.value || '--'; });

// Settings form
var form = $('#cfgForm');
var saveMeta = $('#saveMeta');
function markDirty() { if (saveMeta) { saveMeta.classList.remove('clean'); $('.txt', saveMeta).textContent = 'Unsaved changes'; } }
function markClean(txt) { if (saveMeta) { saveMeta.classList.add('clean'); $('.txt', saveMeta).textContent = txt || 'All saved'; } }
form.addEventListener('input', markDirty);
form.addEventListener('change', markDirty);

function loadConfig() {
  return fetch('/api/config').then(function (r) { return r.json(); }).then(function (c) {
    $$('[name]', form).forEach(function (el) {
      if (!(el.name in c)) return;
      if (el.type === 'checkbox') el.checked = !!c[el.name];
      else el.value = c[el.name];
      if (el.type === 'range') fmtRange(el);
    });
    $('#hostPreview').textContent = c.deviceName;
    var mp = $('#mqttPass');
    mp.value = '';
    mp.placeholder = c.mqttPassSet ? 'saved - leave empty to keep' : '';
    syncPanels();
    markClean();
  });
}
loadConfig().catch(function () { markClean('Could not load settings'); });

// Checkboxes are sent explicitly as 0/1 so an unticked box is not mistaken for a missing field.
function formBody() {
  var data = {};
  $$('[name]', form).forEach(function (el) { data[el.name] = el.type === 'checkbox' ? (el.checked ? '1' : '0') : el.value; });
  return data;
}
form.addEventListener('submit', function (e) {
  e.preventDefault();
  if (!form.reportValidity()) return;
  var btn = $('#saveBtn'), orig = btn.textContent;
  btn.disabled = true; btn.textContent = 'Saving...';
  post('/save', formBody()).then(function (d) {
    btn.disabled = false; btn.textContent = orig;
    if (d.success) { markClean('Saved'); loadConfig(); refreshLight(); if ($('#mqttEnabled').checked) setTimeout(watchMqtt, 500); }
    else alert('Error saving settings: ' + (d.message || 'unknown error'));
  }).catch(function (err) { btn.disabled = false; btn.textContent = orig; alert('Error saving settings: ' + err); });
});

// Live light control
var live = $('#liveLevel'), liveTimer = null, liveBusy = 0;
function showLight(d) {
  var tag = $('#lightTag');
  if (tag) tag.textContent = d.on ? 'on' : 'off';
  if (!liveBusy && document.activeElement !== live) { live.value = Math.max(1, d.level); fmtRange(live); }
  var title = $('#srTitle');
  if (title) title.textContent = 'online · ' + (d.on ? 'on ' + d.level + '%' : 'off');
}
function refreshLight() {
  return fetch('/api/state').then(function (r) { return r.json(); }).then(showLight).catch(function () {});
}
live.addEventListener('input', function () {
  clearTimeout(liveTimer);
  liveBusy++;
  liveTimer = setTimeout(function () {
    post('/api/light', { level: live.value }).then(showLight).catch(function () {}).then(function () { liveBusy = 0; });
  }, 80);
});
$('#lightOn').addEventListener('click', function () { post('/api/light', { on: 1 }).then(showLight).catch(function () {}); });
$('#lightOff').addEventListener('click', function () { post('/api/light', { on: 0 }).then(showLight).catch(function () {}); });

// Maintenance
$('#rebootBtn').addEventListener('click', function () {
  if (!confirm('Restart the device now?')) return;
  post('/api/reboot').catch(function () {});
  setTimeout(function () { location.reload(); }, 6000);
});
$('#wifiResetBtn').addEventListener('click', function () {
  if (!confirm('Forget WiFi credentials?\n\nThe bar restarts into its LEDBar-XXXX setup access point. Connect to it to choose a network. Light settings are kept.')) return;
  post('/api/wifireset').catch(function () {});
});
$('#resetBtn').addEventListener('click', function () {
  if (!confirm('ARE YOU SURE?\n\nThis permanently erases ALL settings and WiFi credentials.\nThe device restarts into setup access point mode. This cannot be undone.')) return;
  post('/api/reset').catch(function () {});
});

var drop = $('#otaDrop'), otaFile = $('#otaFile');
$('#otaBrowse').addEventListener('click', function () { otaFile.click(); });
otaFile.addEventListener('change', function () { if (otaFile.files[0]) doUpload(otaFile.files[0]); });
['dragenter', 'dragover'].forEach(function (ev) { drop.addEventListener(ev, function (e) { e.preventDefault(); drop.classList.add('drag'); }); });
['dragleave', 'drop'].forEach(function (ev) { drop.addEventListener(ev, function (e) { e.preventDefault(); drop.classList.remove('drag'); }); });
drop.addEventListener('drop', function (e) { var f = e.dataTransfer.files[0]; if (f) doUpload(f); });
function doUpload(file) {
  if (!file.name || file.name.slice(-4) !== '.bin') { alert('Please select a valid .bin firmware file.'); return; }
  var prog = $('#otaProgress'), fill = $('#otaFill'), pct = $('#otaPct');
  prog.classList.add('show'); fill.style.width = '0%'; pct.textContent = 'Uploading ' + file.name + '... 0%';
  var xhr = new XMLHttpRequest();
  xhr.upload.addEventListener('progress', function (e) {
    if (e.lengthComputable) { var p = Math.round((e.loaded / e.total) * 100); fill.style.width = p + '%'; pct.textContent = 'Uploading ' + file.name + '... ' + p + '%'; }
  });
  xhr.addEventListener('load', function () {
    if (xhr.status === 200) { fill.style.width = '100%'; pct.textContent = 'Written - rebooting device...'; setTimeout(function () { window.location.href = '/'; }, 8000); }
    else { pct.textContent = (xhr.responseText || 'Upload failed - please try again.'); }
  });
  xhr.addEventListener('error', function () { pct.textContent = 'Upload error - please try again.'; });
  var fd = new FormData(); fd.append('firmware', file);
  xhr.open('POST', '/update'); xhr.send(fd);
}

// Pairing
function esc(x) { return String(x).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }
function barRow(b, sub, btnClass, btnText, action) {
  return '<div class="check-row" style="align-items:center"><span class="check-text" style="margin-right:auto"><strong>' + esc(b.name) +
    '</strong><span class="ct-hint">' + sub + '</span></span>' +
    (btnText ? '<button type="button" class="btn ' + btnClass + '" data-act="' + action + '" data-mac="' + esc(b.mac) + '">' + btnText + '</button>' : '') + '</div>';
}
function renderPair(d) {
  $('#grpTag').textContent = d.grouped ? (d.members.length + 1) + ' bars' : 'not paired';
  var html = barRow({ name: d.name, mac: d.self }, esc(d.self) + ' &middot; this bar', '', '', '');
  d.members.forEach(function (m) { html += barRow(m, esc(m.mac) + ' &middot; ' + (m.online ? 'online' : 'offline'), 'btn-danger', 'Remove', 'remove'); });
  $('#grpList').innerHTML = html;
  $('#grpActions').style.display = d.grouped ? '' : 'none';
  var near = d.nearby.map(function (n) { return barRow(n, esc(n.mac) + (n.grouped ? ' &middot; in another group' : ''), 'btn-accent', 'Pair', 'add'); }).join('');
  $('#nearList').innerHTML = near || '<p class="field-hint" style="margin:0">' + (d.ready ? 'Searching for other bars...' : 'Pairing radio is not running.') + '</p>';
}
function refreshPair() {
  return fetch('/api/pair').then(function (r) { return r.json(); }).then(renderPair).catch(function () {});
}
function pairAction(url, mac) {
  post(url, mac ? { mac: mac } : {}).then(function (d) {
    if (!d.success) alert(d.message || 'Pairing action failed');
    setTimeout(refreshPair, 400);
  }).catch(function (err) { alert('Pairing action failed: ' + err); });
}
$('#grpList').addEventListener('click', function (e) {
  var b = e.target.closest('button[data-act="remove"]'); if (!b) return;
  if (confirm('Remove this bar from the group?')) pairAction('/api/pair/remove', b.dataset.mac);
});
$('#nearList').addEventListener('click', function (e) {
  var b = e.target.closest('button[data-act="add"]'); if (b) pairAction('/api/pair/add', b.dataset.mac);
});
$('#grpLeave').addEventListener('click', function () {
  if (confirm('Leave the group? This bar will be controlled on its own again.')) pairAction('/api/pair/leave');
});
setInterval(function () { if ($('[data-page="pairing"]').classList.contains('active')) refreshPair(); }, 2000);

// MQTT connection feedback
function renderMqtt(d) {
  $('#mqttState').textContent = d.mqtt || '--';
  var box = $('#mqttResult'), k = $('#mqttResultK'), txt = $('#mqttResultText');
  box.classList.remove('warn', 'plain');
  if (d.mqtt === 'disabled') { box.style.display = 'none'; return false; }
  box.style.display = '';
  if (d.mqtt === 'connected') { k.textContent = 'ok'; txt.textContent = 'Connected to the broker. The bar is published to Home Assistant.'; return true; }
  if (d.mqttError) { box.classList.add('warn'); k.textContent = 'error'; txt.textContent = d.mqttError + ' Retrying automatically.'; return true; }
  box.classList.add('plain'); k.textContent = 'wait'; txt.textContent = 'Connecting to the broker...'; return false;
}
// After a save, poll quickly until the first connection attempt has a result.
function watchMqtt() {
  var tries = 0;
  (function tick() {
    fetch('/api/info').then(function (r) { return r.json(); }).then(function (d) {
      if (!renderMqtt(d) && ++tries < 20) setTimeout(tick, 1000);
    }).catch(function () { if (++tries < 20) setTimeout(tick, 1000); });
  })();
}

// Status rail
function fmtUptime(sec) {
  var d = Math.floor(sec / 86400), h = Math.floor((sec % 86400) / 3600), m = Math.floor((sec % 3600) / 60), s = sec % 60;
  function p2(n) { return (n < 10 ? '0' : '') + n; }
  return (d > 0 ? d + 'd ' : '') + p2(h) + ':' + p2(m) + ':' + p2(s);
}
function refreshStatus() {
  var led = $('#srLed');
  fetch('/api/info').then(function (r) { return r.json(); }).then(function (d) {
    if (led) { led.classList.add('online'); led.classList.remove('offline'); }
    $('#srIp').textContent = d.ip;
    $('#srHost').textContent = d.hostname;
    $('#srUptime').textContent = fmtUptime(d.uptime);
    $('#srRssi').textContent = d.rssi + ' dBm';
    $('#wifiSsid').textContent = d.ssid || '--';
    $('#fwHeap').textContent = (d.freeHeap / 1024).toFixed(1) + ' KB';
    renderMqtt(d);
    if (d.otaSlot) $('#fwSlot').textContent = Math.round(d.sketchSize / 1024) + ' / ' + Math.round(d.otaSlot / 1024) + ' KB' +
      (d.otaSlot < 1900000 ? ' (old layout - flash once over USB)' : '');
  }).catch(function () {
    if (led) { led.classList.remove('online'); led.classList.add('offline'); }
    $('#srTitle').textContent = 'offline';
  });
  refreshLight();
}
refreshStatus();
setInterval(refreshStatus, 3000);
})();
