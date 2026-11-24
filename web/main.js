// 담당: 이준호
// 웹 화면. 두 가지 방식으로 동작합니다.
//  1) 실시간: WebAssembly(app.js, app.wasm)가 있으면 C++ 함수(src/core/Api.cpp)를 바로 불러서
//     기간·출발·도착·미세먼지 반영 비중을 바꿀 때마다 경로를 새로 찾음
//  2) 저장 파일: app.js가 없으면 ./build/app 이 만든 web/data 파일(grid.json, routes.geojson, ranking.json)을 표시
const SEOUL = [37.5665, 126.9780];
const map = L.map('map').setView(SEOUL, 11);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
  maxZoom: 18,
  attribution: '&copy; OpenStreetMap contributors'
}).addTo(map);

const $ = id => document.getElementById(id);
const setHTML = (id, html) => { $(id).innerHTML = html; };

// ───────── 색 ─────────
// 데이터의 최솟값~최댓값을 파랑 → 초록 → 주황 → 빨강으로 이어서 칠함
// (한 달 평균은 값 차이가 작아서 환경부 4단계 기준으로 칠하면 서울 전체가 한 색이 됨)
const RAMP = [[47, 128, 237], [39, 174, 96], [242, 153, 74], [235, 87, 87]];
let pmMin = 0, pmMax = 100;
function pmRGB(pm) {
  let t = pmMax > pmMin ? (pm - pmMin) / (pmMax - pmMin) : 0.5;
  t = Math.min(1, Math.max(0, t)) * (RAMP.length - 1);
  const i = Math.min(Math.floor(t), RAMP.length - 2), f = t - i;
  return RAMP[i].map((v, k) => Math.round(v + (RAMP[i + 1][k] - v) * f));
}

// ───────── 격자 (칸별 미세먼지) ─────────
let gridLayer = null, legend = null;
// keepRange: 바람 예측(1·2시간 뒤)은 그 시각 지도와 같은 색 범위로 칠해야 변화가 보임
function drawGrid(g, keepRange) {
  if (gridLayer) map.removeLayer(gridLayer);
  if (!g || !g.values) return;
  if (!keepRange) { pmMin = g.min; pmMax = g.max; }
  // 칸 하나 = 그림의 점 하나. 0행이 남쪽이라 위아래를 뒤집어서 그림
  const cv = document.createElement('canvas');
  cv.width = g.cols; cv.height = g.rows;
  const ctx = cv.getContext('2d');
  const img = ctx.createImageData(g.cols, g.rows);
  for (let r = 0; r < g.rows; r++) {
    for (let c = 0; c < g.cols; c++) {
      const v = g.values[r * g.cols + c];
      const p = ((g.rows - 1 - r) * g.cols + c) * 4;
      if (v === null) { img.data[p + 3] = 0; continue; }
      const [R, G, B] = pmRGB(v);
      img.data[p] = R; img.data[p + 1] = G; img.data[p + 2] = B; img.data[p + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const bounds = [[g.minLat, g.minLng], [g.minLat + g.rows * g.cellLat, g.minLng + g.cols * g.cellLng]];
  gridLayer = L.imageOverlay(cv.toDataURL(), bounds, { opacity: 0.45, interactive: false }).addTo(map);
  gridLayer.bringToBack();
  drawLegend();
}

function drawLegend() {
  if (legend) map.removeControl(legend);
  legend = L.control({ position: 'bottomleft' });
  legend.onAdd = () => {
    const div = L.DomUtil.create('div', 'legend');
    const stops = [0, 1 / 3, 2 / 3, 1].map(t => `rgb(${pmRGB(pmMin + (pmMax - pmMin) * t).join(',')}) ${t * 100}%`);
    div.innerHTML = `<b>PM2.5 (㎍/㎥)</b>
      <div class="bar" style="background:linear-gradient(to right, ${stops.join(',')})"></div>
      <div class="ends"><span>${pmMin.toFixed(0)}</span><span>${pmMax.toFixed(0)}</span></div>`;
    return div;
  };
  legend.addTo(map);
}

// ───────── 경로 ─────────
let routeLayers = [];
function drawRoutes(fc) {
  routeLayers.forEach(l => map.removeLayer(l));
  routeLayers = [];
  const byType = {};
  fc.features.forEach(f => { byType[f.properties.type] = f; });
  // 최단 경로를 먼저 그리고 최적 경로를 위에 겹쳐 그림 (흰 테두리로 지도 색과 구분)
  if (byType.shortest) routeLayers.push(L.geoJSON(byType.shortest, { style: { color: '#555', weight: 4, dashArray: '6 6' } }).addTo(map));
  if (byType.best) {
    routeLayers.push(L.geoJSON(byType.best, { style: { color: '#fff', weight: 9, opacity: 0.9 } }).addTo(map));
    routeLayers.push(L.geoJSON(byType.best, { style: { color: '#2F6FEB', weight: 5 } }).addTo(map));
  }
  return byType;
}

function showCompare(byType) {
  const b = byType.best && byType.best.properties;
  const s = byType.shortest && byType.shortest.properties;
  if (!b || !s) return;
  const cut = s.exposure > 0 ? (s.exposure - b.exposure) / s.exposure * 100 : 0;
  const more = s.distance > 0 ? (b.distance - s.distance) / s.distance * 100 : 0;
  const km = m => (m / 1000).toFixed(2) + ' km';
  const hasProfile = b.pm25s && s.pm25s;
  setHTML('result', `
    <h2>경로 비교</h2>
    <table class="cmp">
      <tr><th></th><th><span class="sw best"></span>최적</th><th><span class="sw short"></span>최단</th></tr>
      <tr><td>거리</td><td>${km(b.distance)}</td><td>${km(s.distance)}</td></tr>
      <tr><td>소요 시간</td><td>${Math.round(b.minutes)}분</td><td>${Math.round(s.minutes)}분</td></tr>
      <tr><td>노출량</td><td>${Math.round(b.exposure).toLocaleString()}</td><td>${Math.round(s.exposure).toLocaleString()}</td></tr>
    </table>
    <p class="summary">미세먼지 노출 <b>${cut.toFixed(1)}% 감소</b> · 거리 ${more >= 0 ? '+' : ''}${more.toFixed(1)}%</p>
    ${hasProfile ? '<canvas id="profile" class="profile"></canvas><p class="hint">경로를 따라 지나는 칸의 PM2.5 (왼쪽 출발 → 오른쪽 도착)</p>' : ''}`);
  if (hasProfile) drawProfile($('profile'), b.pm25s, s.pm25s);
}

// 경로를 따라 지나는 칸의 미세먼지를 작은 선 그래프로
function drawProfile(cv, best, shortest) {
  const w = cv.width = cv.clientWidth * 2, h = cv.height = cv.clientHeight * 2;
  const ctx = cv.getContext('2d');
  const all = best.concat(shortest).filter(v => v >= 0);
  const lo = Math.min(...all), hi = Math.max(...all), pad = 8;
  const y = v => h - pad - (hi > lo ? (v - lo) / (hi - lo) : 0.5) * (h - pad * 2);
  const line = (vals, color, dash, width) => {
    ctx.beginPath(); ctx.setLineDash(dash); ctx.strokeStyle = color; ctx.lineWidth = width;
    vals.forEach((v, i) => { const x = pad + i / Math.max(1, vals.length - 1) * (w - pad * 2); i ? ctx.lineTo(x, y(v)) : ctx.moveTo(x, y(v)); });
    ctx.stroke();
  };
  line(shortest, '#555', [10, 8], 3);
  line(best, '#2F6FEB', [], 4);
  ctx.setLineDash([]); ctx.fillStyle = '#888'; ctx.font = '20px sans-serif';
  ctx.textAlign = 'right'; ctx.fillText(hi.toFixed(1), w - pad, 22); ctx.fillText(lo.toFixed(1), w - pad, h - pad);
}

let rankNote = 'PM2.5 월 평균';
function showRanking(rank) {
  if (!rank) return;
  const list = (items, title) => `<div><h3>${title}</h3><ol>${items.map(i => `<li>${i.name}<span>${i.score.toFixed(1)}</span></li>`).join('')}</ol></div>`;
  setHTML('ranking', `<h2>Top / Worst 지역 <small>${rankNote}</small></h2>
    <div class="rank">${list(rank.top, '깨끗한 곳')}${list(rank.worst, '나쁜 곳')}</div>`);
}

// ───────── 실시간 (WebAssembly) ─────────
let api = null;   // { call(name, ...args) }
const points = { start: null, goal: null };
const markers = { start: null, goal: null };

function setPoint(which, lat, lng, name) {
  points[which] = { lat, lng };
  $(which).value = name || `${lat.toFixed(4)}, ${lng.toFixed(4)}`;
  if (markers[which]) markers[which].setLatLng([lat, lng]);
  else {
    markers[which] = L.marker([lat, lng], {
      draggable: true,
      icon: L.divIcon({ className: '', html: `<i class="dot ${which}" style="width:16px;height:16px;border:3px solid #fff;box-shadow:0 0 3px rgba(0,0,0,.5)"></i>`, iconSize: [22, 22], iconAnchor: [11, 11] })
    }).addTo(map);
    markers[which].on('dragend', e => {
      const p = e.target.getLatLng();
      setPoint(which, p.lat, p.lng);
      findRoute();
    });
  }
}

function findRoute() {
  if (!api || !points.start || !points.goal) return;
  const alpha = parseFloat($('alpha').value);
  const res = JSON.parse(api.call('api_find_route', points.start.lat, points.start.lng, points.goal.lat, points.goal.lng, alpha));
  if (res.ok === false) {
    routeLayers.forEach(l => map.removeLayer(l)); routeLayers = [];
    setHTML('result', `<h2>경로 비교</h2><p class="error">${res.error}</p>`);
    return;
  }
  showCompare(drawRoutes(res));
}

// 보기 상태: 월 평균 / 시각 지정(그 시각 · 1시간 뒤 · 2시간 뒤)
const view = { mode: 'month', ahead: 0 };
const LAST_DAY = { '02': 28, '05': 31, '08': 31, '11': 30 };

function segValue(id, v) {
  document.querySelectorAll(`#${id} button`).forEach(b => b.classList.toggle('on', b.dataset.v === String(v)));
}

// 기간을 바꾸면 날짜 선택 범위도 그 달로 (처음에는 15일 정오)
function resetDay() {
  const [y, m] = $('period').value.split('-');
  $('day').min = `${y}-${m}-01`;
  $('day').max = `${y}-${m}-${LAST_DAY[m]}`;
  $('day').value = `${y}-${m}-15`;
}

function refresh() {
  let res;
  if (view.mode === 'month') {
    res = JSON.parse(api.call('api_set_period', $('period').value));
  } else {
    res = JSON.parse(api.call('api_set_time', $('day').value, parseInt($('hour').value), view.ahead));
  }
  if (res.ok === false) {
    setHTML('result', `<h2>경로 비교</h2><p class="error">${res.error}</p>`);
    $('aheadHint').textContent = '';
    return;
  }
  const h = parseInt($('hour').value), d = $('day').value.slice(5).replace('-', '월 ') + '일';
  rankNote = view.mode === 'month' ? 'PM2.5 월 평균'
    : view.ahead ? `${d} ${(h + view.ahead) % 24}시 추정` : `${d} ${h}시`;
  $('aheadHint').textContent = view.mode === 'time' && view.ahead
    ? `${d} ${h}시 측정값을 S-DoT 바람 센서 ${res.winds}곳의 바람으로 ${view.ahead}시간 옮겨 추정한 지도입니다. 색 범위는 ${h}시 지도와 같습니다.`
    : view.mode === 'time' ? '1·2시간 뒤는 바람으로 미세먼지 이동을 추정합니다 (실제 값과 비교해 오차 약 3% 감소).' : '';
  // 25m 격자를 그대로 보내면 260만 칸이라 100m 단위로 묶어서 받음
  drawGrid(JSON.parse(api.call('api_get_grid', Math.max(1, Math.round(100 / (res.cellMeters || 100))))), view.mode === 'time' && view.ahead > 0);
  showRanking(JSON.parse(api.call('api_get_ranking', 5)));
  findRoute();
}

// 시각을 바꾸면 항상 '그 시각'부터 그려서 색 범위가 정해지므로, 1·2시간 뒤는 그 범위를 그대로 씀
function setAhead(a) {
  view.ahead = a;
  segValue('ahead', a);
  refresh();
}

function setupView() {
  for (let h = 0; h < 24; h++) $('hour').insertAdjacentHTML('beforeend', `<option value="${h}">${h}시</option>`);
  $('hour').value = '12';
  resetDay();
  $('period').addEventListener('change', () => { resetDay(); view.ahead = 0; segValue('ahead', 0); refresh(); });
  $('view').addEventListener('click', e => {
    const b = e.target.closest('button'); if (!b) return;
    view.mode = b.dataset.v; view.ahead = 0;
    segValue('view', view.mode); segValue('ahead', 0);
    $('timeBox').hidden = view.mode !== 'time';
    refresh();
  });
  $('ahead').addEventListener('click', e => { const b = e.target.closest('button'); if (b) setAhead(parseInt(b.dataset.v)); });
  const timeChanged = () => { view.ahead = 0; segValue('ahead', 0); refresh(); };
  $('day').addEventListener('change', timeChanged);
  $('hour').addEventListener('change', timeChanged);
}

// 지역명 자동완성 (트라이)
function setupSearch(which) {
  const input = $(which), list = document.querySelector(`.suggest[data-for="${which}"]`);
  let items = [];
  const close = () => { list.style.display = 'none'; };
  const choose = p => { setPoint(which, p.lat, p.lng, p.name); close(); findRoute(); };
  input.addEventListener('input', () => {
    const q = input.value.trim();
    items = q ? JSON.parse(api.call('api_search', q, 8)) : [];
    list.innerHTML = items.map((p, i) => `<li data-i="${i}">${p.name}</li>`).join('');
    list.style.display = items.length ? 'block' : 'none';
  });
  input.addEventListener('keydown', e => { if (e.key === 'Enter' && items.length) { e.preventDefault(); choose(items[0]); } });
  list.addEventListener('mousedown', e => { const li = e.target.closest('li'); if (li) choose(items[+li.dataset.i]); });
  input.addEventListener('blur', () => setTimeout(close, 150));
}

async function startLive(createApp) {
  const Module = await createApp();
  api = { call: (name, ...args) => Module.ccall(name, 'string', args.map(a => typeof a === 'string' ? 'string' : 'number'), args) };
  const init = JSON.parse(api.call('api_init', ''));
  if (init.ok === false) throw new Error(init.error);
  $('mode').textContent = '실시간 탐색 (WebAssembly)';
  setupSearch('start');
  setupSearch('goal');
  setupView();
  $('alpha').addEventListener('input', () => { $('alphaValue').textContent = parseFloat($('alpha').value).toFixed(1); });
  $('alpha').addEventListener('change', findRoute);
  map.on('click', e => {
    // 출발이 없으면 출발, 도착이 없으면 도착, 둘 다 있으면 출발부터 다시
    const which = !points.start ? 'start' : !points.goal ? 'goal' : 'start';
    if (which === 'start' && points.goal) {
      points.goal = null; map.removeLayer(markers.goal); markers.goal = null; $('goal').value = '';
      routeLayers.forEach(l => map.removeLayer(l)); routeLayers = [];
      setHTML('result', '<h2>경로 비교</h2><p>도착지를 정해 주세요.</p>');
    }
    setPoint(which, e.latlng.lat, e.latlng.lng);
    findRoute();
  });
  refresh();
  // 처음 열었을 때 예시 경로 (종로구 → 강남구)
  setPoint('start', 37.571973, 127.005082, '종로구');
  setPoint('goal', 37.518147, 127.047296, '강남구');
  findRoute();
}

// ───────── 저장 파일 (WebAssembly 없을 때) ─────────
async function loadJSON(url) {
  try {
    const res = await fetch(url, { cache: 'no-store' });
    return res.ok ? await res.json() : null;
  } catch (e) { return null; }
}

async function startStatic() {
  $('mode').textContent = '저장된 결과 보기 (./build/app 실행 결과)';
  ['period', 'start', 'goal', 'alpha'].forEach(id => { $(id).disabled = true; });
  document.querySelectorAll('#view button').forEach(b => { b.disabled = true; });
  document.querySelector('.hint').textContent = '실시간 탐색은 WebAssembly 빌드(make wasm)나 배포된 페이지에서 됩니다.';
  const [grid, routes, ranking] = await Promise.all([
    loadJSON('data/grid.json'), loadJSON('data/routes.geojson'), loadJSON('data/ranking.json')
  ]);
  if (!grid && !routes) {
    setHTML('result', '<h2>경로 비교</h2><p>결과 파일이 없습니다.<br>레포 폴더에서 <code>./build/app 출발지 목적지</code> 를 먼저 실행하세요.</p>');
    return;
  }
  drawGrid(grid);
  if (routes) {
    const byType = drawRoutes(routes);
    showCompare(byType);
    const coords = byType.best ? byType.best.geometry.coordinates : [];
    if (coords.length) {
      const s = coords[0], g = coords[coords.length - 1];
      setPoint('start', s[1], s[0]); setPoint('goal', g[1], g[0]);
      Object.values(markers).forEach(m => m.dragging.disable());
      map.fitBounds(routeLayers[routeLayers.length - 1].getBounds().pad(0.2));
    }
  }
  showRanking(ranking);
}

// app.js가 있으면 실시간, 없거나 실패하면 저장 파일
function loadScript(src) {
  return new Promise((ok, fail) => {
    const s = document.createElement('script');
    s.src = src; s.onload = ok; s.onerror = fail;
    document.head.appendChild(s);
  });
}

(async () => {
  try {
    await loadScript('app.js');
    await startLive(window.createApp);
  } catch (e) {
    console.log('WebAssembly 없이 저장 파일로 표시:', e.message || e);
    await startStatic();
  }
})();
