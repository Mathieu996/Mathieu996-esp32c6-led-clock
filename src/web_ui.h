#pragma once
#include <Arduino.h>

// Page web de configuration (mono-fichier, embarquee en flash).
// Utilise fetch() vers /api/status, /api/config, /api/wifiscan,
// /api/reboot et /api/factoryreset (voir web_portal.cpp).
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Horloge LED</title>
<style>
  :root {
    --bg: #0f1115; --card: #171a21; --border: #262b36;
    --text: #e8ebf1; --muted: #8a92a6; --accent: #ff6a00;
    --accent2: #2dd4bf; --danger: #ef4444; --ok: #22c55e;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0; background: var(--bg); color: var(--text);
    font-family: -apple-system, Segoe UI, Roboto, Arial, sans-serif;
    padding: 16px; max-width: 640px; margin: 0 auto;
  }
  h1 { font-size: 1.4rem; display:flex; align-items:center; gap:10px; }
  h1 .dot { width:10px; height:10px; border-radius:50%; background:var(--accent); box-shadow:0 0 8px var(--accent); }
  .card {
    background: var(--card); border: 1px solid var(--border);
    border-radius: 12px; padding: 16px; margin-bottom: 16px;
  }
  .card h2 { margin: 0 0 12px; font-size: 1rem; color: var(--accent2); }
  .row { display:flex; flex-wrap:wrap; gap:10px; margin-bottom:10px; align-items:center; }
  .row label { flex: 0 0 150px; color: var(--muted); font-size: .9rem; }
  .row input[type=text], .row input[type=password], .row input[type=number], .row input[type=time], select {
    flex: 1 1 160px; background:#0c0e13; border:1px solid var(--border); color:var(--text);
    padding: 8px 10px; border-radius:8px; font-size:.95rem; min-width:0;
  }
  .switch-row { display:flex; justify-content:space-between; align-items:center; margin-bottom:10px; }
  .switch-row span { color: var(--muted); font-size:.9rem; }
  input[type=checkbox] { width:20px; height:20px; }
  input[type=range] { flex:1 1 160px; }
  button {
    background: var(--accent); border:none; color:#111; font-weight:600;
    padding:10px 16px; border-radius:8px; cursor:pointer; font-size:.9rem;
  }
  button.secondary { background:#2a2f3b; color:var(--text); }
  button.danger { background:var(--danger); color:#fff; }
  .actions { display:flex; gap:10px; flex-wrap:wrap; }
  .status { font-size:.85rem; color:var(--muted); }
  .status b { color:var(--text); }
  #toast {
    position:fixed; bottom:16px; left:50%; transform:translateX(-50%);
    background:#1f2430; border:1px solid var(--border); padding:10px 16px;
    border-radius:8px; opacity:0; transition:opacity .3s; pointer-events:none;
  }
  #toast.show { opacity:1; }
  #netList { max-height:160px; overflow:auto; margin-top:8px; }
  .net { display:flex; justify-content:space-between; padding:6px 8px; border-radius:6px; cursor:pointer; font-size:.88rem; }
  .net:hover { background:#20242f; }
  .pill { display:inline-block; padding:2px 8px; border-radius:999px; font-size:.75rem; }
  .pill.ok { background:rgba(34,197,94,.15); color:var(--ok); }
  .pill.warn { background:rgba(239,68,68,.15); color:var(--danger); }
  small.hint { color: var(--muted); display:block; margin-top:-4px; margin-bottom:10px; }
</style>
</head>
<body>
  <h1><span class="dot"></span> Horloge LED <span id="modePill" class="pill"></span></h1>
  <div class="card status">
    IP : <b id="stIp">-</b> &nbsp;|&nbsp; Heure : <b id="stTime">-</b> &nbsp;|&nbsp; NTP : <b id="stNtp">-</b><br>
    <span id="stTempWrap" style="display:none">Temperature : <b id="stTemp">-</b> &deg;C<br></span>
    Version firmware : <b id="stVer">-</b> &nbsp;|&nbsp; Uptime : <b id="stUptime">-</b>
  </div>

  <div class="card">
    <h2>Wi-Fi</h2>
    <div class="row"><label>Reseau (SSID)</label><input type="text" id="ssid" maxlength="32" placeholder="Nom du reseau"></div>
    <div class="row"><label>Mot de passe</label><input type="password" id="pass" maxlength="64" placeholder="(laisser vide = inchange)"></div>
    <div class="actions">
      <button type="button" class="secondary" onclick="scanWifi()">Scanner les reseaux</button>
    </div>
    <div id="netList"></div>
    <small class="hint">En mode point d'acces, la carte cree son propre reseau "HorlogeLED-XXXX" (mot de passe : 12345678) le temps de la configuration.</small>
  </div>

  <div class="card">
    <h2>Heure &amp; NTP</h2>
    <div class="row"><label>Serveur NTP</label><input type="text" id="ntp" maxlength="64" placeholder="pool.ntp.org"></div>
    <div class="row"><label>Fuseau horaire</label>
      <select id="tzPreset" onchange="applyTzPreset()">
        <option value="CET-1CEST,M3.5.0,M10.5.0/3">Europe/Paris (CET/CEST)</option>
        <option value="GMT0BST,M3.5.0/1,M10.5.0">Europe/Londres (GMT/BST)</option>
        <option value="UTC0">UTC</option>
        <option value="custom">Personnalise (TZ POSIX)...</option>
      </select>
    </div>
    <div class="row"><label>Chaine TZ POSIX</label><input type="text" id="tz" maxlength="64"></div>
    <small class="hint">Format TZ POSIX standard (gere automatiquement le passage heure ete/hiver).</small>
    <div class="switch-row"><span>Format 24h</span><input type="checkbox" id="f24h"></div>
  </div>

  <div class="card">
    <h2>Affichage</h2>
    <div class="row"><label>Luminosite</label><input type="range" id="bright" min="0" max="15" step="1"><span id="brightVal">-</span></div>
    <div class="switch-row"><span>Afficher les secondes (defilement continu)</span><input type="checkbox" id="secs"></div>
    <div class="switch-row"><span>Faire defiler la date periodiquement</span><input type="checkbox" id="datesc"></div>
    <div class="row"><label>Intervalle date (s)</label><input type="number" id="dateiv" min="5" max="600"></div>
    <div class="switch-row"><span>Afficher la temperature avec la date</span><input type="checkbox" id="showTemp"></div>
    <div class="row"><label>Correction temp. (&deg;C)</label><input type="number" id="tempOff" min="-10" max="10" step="0.1"></div>
    <small class="hint">La temperature defile avec la date (sonde BME280/BMP280 requise). La correction compense un module qui chauffe un peu.</small>
    <div class="switch-row"><span>Retourner l'affichage (180 deg)</span><input type="checkbox" id="flip"></div>
  </div>

  <div class="card">
    <h2>Mode nuit</h2>
    <div class="switch-row"><span>Activer le mode nuit</span><input type="checkbox" id="nightOn"></div>
    <div class="row"><label>Debut</label><input type="time" id="nightStart"></div>
    <div class="row"><label>Fin</label><input type="time" id="nightEnd"></div>
    <div class="row"><label>Luminosite de nuit</label><input type="range" id="nightBr" min="0" max="15" step="1"><span id="nightBrVal">-</span></div>
    <div class="switch-row"><span>Eteindre completement l'affichage</span><input type="checkbox" id="nightOff"></div>
    <small class="hint">Entre le debut et la fin (minuit compris), la luminosite passe au niveau de nuit, ou l'affichage s'eteint si la case est cochee.</small>
  </div>

  <div class="card actions">
    <button type="button" onclick="saveConfig()">Enregistrer</button>
    <button type="button" class="secondary" onclick="reboot()">Redemarrer</button>
    <button type="button" class="danger" onclick="factoryReset()">Reinitialiser</button>
  </div>

  <div id="toast"></div>

<script>
function toast(msg) {
  const t = document.getElementById('toast');
  t.textContent = msg; t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2500);
}

async function loadStatus() {
  try {
    const r = await fetch('/api/status'); const j = await r.json();
    document.getElementById('stIp').textContent = j.ip;
    document.getElementById('stTime').textContent = j.time;
    document.getElementById('stNtp').textContent = j.ntpSynced ? 'synchronise' : 'en attente...';
    document.getElementById('stVer').textContent = j.version;
    document.getElementById('stUptime').textContent = j.uptime;
    const hasTemp = j.temp !== undefined;
    document.getElementById('stTempWrap').style.display = hasTemp ? '' : 'none';
    if (hasTemp) document.getElementById('stTemp').textContent = j.temp;
    const pill = document.getElementById('modePill');
    pill.textContent = j.mode === 'AP' ? 'Mode configuration (AP)' : ('Connecte : ' + j.ssid);
    pill.className = 'pill ' + (j.mode === 'AP' ? 'warn' : 'ok');
  } catch (e) { /* ignore */ }
}

async function loadConfig() {
  const r = await fetch('/api/config'); const c = await r.json();
  document.getElementById('ssid').value = c.ssid;
  document.getElementById('ntp').value = c.ntp;
  document.getElementById('tz').value = c.tz;
  document.getElementById('f24h').checked = c.f24h;
  document.getElementById('bright').value = c.bright;
  document.getElementById('brightVal').textContent = c.bright;
  document.getElementById('secs').checked = c.secs;
  document.getElementById('datesc').checked = c.datesc;
  document.getElementById('dateiv').value = c.dateiv;
  document.getElementById('flip').checked = c.flip;
  document.getElementById('showTemp').checked = c.showTemp;
  document.getElementById('tempOff').value = c.tempOff;
  document.getElementById('nightOn').checked = c.nightOn;
  document.getElementById('nightStart').value = minToStr(c.nightStart);
  document.getElementById('nightEnd').value = minToStr(c.nightEnd);
  document.getElementById('nightBr').value = c.nightBr;
  document.getElementById('nightBrVal').textContent = c.nightBr;
  document.getElementById('nightOff').checked = c.nightOff;
  const preset = document.getElementById('tzPreset');
  let matched = false;
  for (const o of preset.options) if (o.value === c.tz) { preset.value = c.tz; matched = true; }
  if (!matched) preset.value = 'custom';
}

function applyTzPreset() {
  const preset = document.getElementById('tzPreset');
  if (preset.value !== 'custom') document.getElementById('tz').value = preset.value;
}

document.getElementById('bright').addEventListener('input', e => {
  document.getElementById('brightVal').textContent = e.target.value;
});
document.getElementById('nightBr').addEventListener('input', e => {
  document.getElementById('nightBrVal').textContent = e.target.value;
});

// Minutes depuis minuit <-> "HH:MM" (champs <input type="time">)
function minToStr(m) {
  return String(Math.floor(m / 60)).padStart(2, '0') + ':' + String(m % 60).padStart(2, '0');
}
function strToMin(s) {
  const p = (s || '00:00').split(':');
  return (parseInt(p[0], 10) || 0) * 60 + (parseInt(p[1], 10) || 0);
}

async function scanWifi() {
  const list = document.getElementById('netList');
  list.innerHTML = 'Scan en cours...';
  try {
    const r = await fetch('/api/wifiscan'); const nets = await r.json();
    list.innerHTML = '';
    nets.forEach(n => {
      const d = document.createElement('div');
      d.className = 'net';
      d.innerHTML = '<span>' + (n.secure ? '&#128274; ' : '') + n.ssid + '</span><span>' + n.rssi + ' dBm</span>';
      d.onclick = () => { document.getElementById('ssid').value = n.ssid; };
      list.appendChild(d);
    });
    if (nets.length === 0) list.innerHTML = 'Aucun reseau trouve.';
  } catch (e) { list.innerHTML = 'Erreur de scan.'; }
}

async function saveConfig() {
  const body = {
    ssid: document.getElementById('ssid').value,
    pass: document.getElementById('pass').value,
    ntp: document.getElementById('ntp').value,
    tz: document.getElementById('tz').value,
    f24h: document.getElementById('f24h').checked,
    bright: parseInt(document.getElementById('bright').value, 10),
    secs: document.getElementById('secs').checked,
    datesc: document.getElementById('datesc').checked,
    dateiv: parseInt(document.getElementById('dateiv').value, 10),
    flip: document.getElementById('flip').checked,
    showTemp: document.getElementById('showTemp').checked,
    tempOff: parseFloat(document.getElementById('tempOff').value) || 0,
    nightOn: document.getElementById('nightOn').checked,
    nightStart: strToMin(document.getElementById('nightStart').value),
    nightEnd: strToMin(document.getElementById('nightEnd').value),
    nightBr: parseInt(document.getElementById('nightBr').value, 10),
    nightOff: document.getElementById('nightOff').checked
  };
  const r = await fetch('/api/config', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify(body) });
  if (r.ok) {
    toast('Enregistre. Redemarrage si le Wi-Fi a change...');
    document.getElementById('pass').value = '';
    setTimeout(loadStatus, 3000);
  } else toast('Erreur lors de l\'enregistrement');
}

async function reboot() {
  if (!confirm('Redemarrer la carte ?')) return;
  await fetch('/api/reboot', { method: 'POST' });
  toast('Redemarrage...');
}

async function factoryReset() {
  if (!confirm('Reinitialiser tous les reglages (Wi-Fi inclus) ?')) return;
  await fetch('/api/factoryreset', { method: 'POST' });
  toast('Reinitialisation...');
}

loadConfig();
loadStatus();
setInterval(loadStatus, 5000);
</script>
</body>
</html>
)rawliteral";
