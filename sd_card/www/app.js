// ============================================================================
//  app.js  -  Frontend fuer die Raumluft-Messung (ohne externe Bibliotheken)
//  Holt Live-Werte (/api/current) und Verlauf (/api/history) vom ESP32 und
//  zeichnet einen Canvas-Liniengraphen der letzten Messungen.
// ============================================================================
'use strict';

const METRICS = {
  co2:  { label: 'CO₂',        unit: 'ppm', color: '#38bdf8', decimals: 0 },
  temp: { label: 'Temperatur', unit: '°C',  color: '#f97316', decimals: 1 },
  hum:  { label: 'Luftfeuchte',unit: '%',   color: '#22c55e', decimals: 0 },
  heap: { label: 'RAM frei',   unit: 'kB',  color: '#a78bfa', decimals: 0, div: 1024 },
};

let history = [];       // [{t, co2, temp, hum}]
let activeKey = 'co2';

// ---- Hilfsfunktionen -------------------------------------------------------
async function getJSON(url) {
  const r = await fetch(url, { cache: 'no-store' });
  if (!r.ok) throw new Error(url + ' -> ' + r.status);
  return r.json();
}

function fmtTime(epoch) {
  const d = new Date(epoch * 1000);
  return d.toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}
function fmtHM(epoch) {
  const d = new Date(epoch * 1000);
  return d.toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit' });
}
function fmtUptime(sec) {
  const d = Math.floor(sec / 86400), h = Math.floor(sec % 86400 / 3600), m = Math.floor(sec % 3600 / 60);
  if (d) return d + 'd ' + h + 'h';
  if (h) return h + 'h ' + m + 'm';
  return m + 'm';
}

function setDot(id, state) {          // state: 'ok' | 'warn' | 'err'
  const el = document.getElementById(id);
  el.className = 'dot ' + state;
}

// ---- Live-Werte ------------------------------------------------------------
async function refreshCurrent() {
  try {
    const c = await getJSON('/api/current');
    document.getElementById('clock').textContent = c.time ? fmtTime(c.time) : '--:--:--';

    if (c.valid) {
      document.getElementById('co2').textContent  = Math.round(c.co2);
      document.getElementById('temp').textContent = c.temp.toFixed(1);
      document.getElementById('hum').textContent  = Math.round(c.hum);

      const card = document.getElementById('card-co2');
      const hint = document.getElementById('co2-hint');
      card.classList.remove('good', 'medium', 'bad');
      if (c.co2 <= 1000)      { card.classList.add('good');   hint.textContent = 'OK'; }
      else if (c.co2 <= 1400) { card.classList.add('medium'); hint.textContent = 'Lüften empfohlen'; }
      else                    { card.classList.add('bad');    hint.textContent = 'Raum lüften'; }
    }

    // Statusleiste
    setDot('dot-net', (c.wifi || c.ap) ? 'ok' : 'err');
    document.getElementById('net').textContent =
      c.wifi ? (c.ssid + ' · ' + c.ip) : (c.ap ? ('AP · ' + c.ip) : 'offline');
    setDot('dot-sd',  c.sd ? 'ok' : 'err');
    setDot('dot-rtc', c.rtc ? (c.synced ? 'ok' : 'warn') : 'err');
    document.getElementById('rtc').textContent = c.synced ? 'RTC (NTP)' : 'RTC';

    if (c.heap !== undefined) {
      document.getElementById('heap').textContent =
        'Heap ' + Math.round(c.heap / 1024) + 'k (min ' + Math.round(c.heapmin / 1024) + 'k) · ' + fmtUptime(c.uptime);
    }
  } catch (e) {
    setDot('dot-net', 'err');
    document.getElementById('net').textContent = 'keine Verbindung';
  }
}

// ---- Verlauf ---------------------------------------------------------------
async function refreshHistory() {
  try {
    const h = await getJSON('/api/history');
    history = h.points || [];
    drawChart();
  } catch (e) { /* still */ }
}

// ---- Canvas-Graph ----------------------------------------------------------
function drawChart() {
  const canvas = document.getElementById('chart');
  const dpr = window.devicePixelRatio || 1;
  const cssW = canvas.clientWidth, cssH = canvas.clientHeight;
  canvas.width = cssW * dpr; canvas.height = cssH * dpr;
  const ctx = canvas.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, cssW, cssH);

  const m = METRICS[activeKey];
  const val = p => { const v = p[activeKey]; return m.div ? v / m.div : v; };
  const pad = { l: 44, r: 12, t: 12, b: 26 };
  const w = cssW - pad.l - pad.r, h = cssH - pad.t - pad.b;

  if (history.length < 2) {
    ctx.fillStyle = '#94a3b8'; ctx.font = '14px system-ui'; ctx.textAlign = 'center';
    ctx.fillText('Noch nicht genug Messdaten …', cssW / 2, cssH / 2);
    return;
  }

  const vals = history.map(val);
  let mn = Math.min(...vals), mx = Math.max(...vals);
  if (mx - mn < 1) { mx += 1; mn -= 1; }
  const range = mx - mn;
  mn -= range * 0.1; mx += range * 0.1;          // etwas Rand

  const X = i => pad.l + (history.length === 1 ? 0 : i / (history.length - 1) * w);
  const Y = v => pad.t + h - (v - mn) / (mx - mn) * h;

  // Gitter + Y-Beschriftung
  ctx.strokeStyle = '#334155'; ctx.fillStyle = '#94a3b8';
  ctx.lineWidth = 1; ctx.font = '11px system-ui'; ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
  const TICKS = 4;
  for (let i = 0; i <= TICKS; i++) {
    const v = mn + (mx - mn) * i / TICKS;
    const y = Y(v);
    ctx.globalAlpha = 0.5; ctx.beginPath(); ctx.moveTo(pad.l, y); ctx.lineTo(pad.l + w, y); ctx.stroke();
    ctx.globalAlpha = 1;   ctx.fillText(v.toFixed(m.decimals), pad.l - 6, y);
  }

  // X-Beschriftung (erste, mittlere, letzte Zeit)
  ctx.textAlign = 'center'; ctx.textBaseline = 'top';
  [0, Math.floor((history.length - 1) / 2), history.length - 1].forEach(i => {
    ctx.fillText(fmtHM(history[i].t), X(i), pad.t + h + 6);
  });

  // Flaeche + Linie
  ctx.beginPath();
  history.forEach((p, i) => { const x = X(i), y = Y(val(p)); i ? ctx.lineTo(x, y) : ctx.moveTo(x, y); });
  const grad = ctx.createLinearGradient(0, pad.t, 0, pad.t + h);
  grad.addColorStop(0, m.color + '55'); grad.addColorStop(1, m.color + '00');
  ctx.lineTo(X(history.length - 1), pad.t + h); ctx.lineTo(X(0), pad.t + h); ctx.closePath();
  ctx.fillStyle = grad; ctx.fill();

  ctx.beginPath();
  history.forEach((p, i) => { const x = X(i), y = Y(val(p)); i ? ctx.lineTo(x, y) : ctx.moveTo(x, y); });
  ctx.strokeStyle = m.color; ctx.lineWidth = 2; ctx.stroke();

  // Punkte
  ctx.fillStyle = m.color;
  history.forEach((p, i) => { ctx.beginPath(); ctx.arc(X(i), Y(val(p)), 2.5, 0, Math.PI * 2); ctx.fill(); });
}

// ---- Tabs & Resize ---------------------------------------------------------
document.getElementById('tabs').addEventListener('click', e => {
  if (e.target.tagName !== 'BUTTON') return;
  activeKey = e.target.dataset.key;
  [...e.currentTarget.children].forEach(b => b.classList.toggle('active', b === e.target));
  document.querySelector('.range').textContent =
    'letzte ' + history.length + ' Messungen · ' + METRICS[activeKey].label;
  drawChart();
});
window.addEventListener('resize', drawChart);

// ---- Start -----------------------------------------------------------------
refreshCurrent();
refreshHistory();
setInterval(refreshCurrent, 8000);
setInterval(refreshHistory, 30000);
