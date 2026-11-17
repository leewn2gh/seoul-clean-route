// 서울 대기환경정보 간이측정망 2025년 2·5·8·11월 시간별 초미세먼지 + 측정소 좌표 받기
// 사용: https://cleanair.seoul.go.kr/airquality/simpleAvg 를 열고 개발자 도구(F12) 콘솔에 붙여 넣기
//       카카오 REST API 키를 묻습니다 (카카오맵 기능을 켠 앱의 키, 주소 → 좌표 변환용)
// 결과: 다운로드 폴더에 simple_raw_sites.csv, simple_raw_2025_MM.csv → data/raw/simple/ 로 옮긴 뒤 prep_simple.py 실행
(async () => {
  const KEY = prompt('카카오 REST API 키');
  const sites = {}, rows = {};
  const q = (d, t) => fetch('/airquality/getSimpleAirPollutantInfoAvg?' + new URLSearchParams({
    mode: 'hour', station: 'Y', searchKind: '', searchText: '', searchDate: d, searchTime: t, searchArea: '', checkResult: true
  })).then(r => r.json());

  for (const m of [2, 5, 8, 11]) {
    const days = new Date(2025, m, 0).getDate(), out = [], jobs = [];
    for (let d = 1; d <= days; d++) for (let h = 1; h <= 24; h++) jobs.push([d, h]);
    for (let i = 0; i < jobs.length; i += 8) {
      await Promise.all(jobs.slice(i, i + 8).map(async ([d, h]) => {
        const ds = `2025-${String(m).padStart(2, '0')}-${String(d).padStart(2, '0')}`;
        let a; for (let k = 0; k < 3 && !a; k++) { try { a = await q(ds, String(h).padStart(2, '0') + '00'); } catch (e) {} }
        for (const r of a || []) {
          if (r.datKnd !== 1) continue;
          sites[r.msradmCd] = sites[r.msradmCd] || { code: r.msradmCd, name: r.name, kind: r.locKindNm, area: r.area, addr: r.addr };
          if (r.pm25 === undefined || r.pm25 === null || r.pm25 === '') continue;
          // 'HH:00'은 그 시각까지의 1시간 평균 → 자치구 파일처럼 시각 HH, 24시는 다음 날 0시
          const t = new Date(2025, m - 1, d, h);
          if (t.getMonth() + 1 !== m) continue;
          out.push([t.getFullYear() * 1e6 + (t.getMonth() + 1) * 1e4 + t.getDate() * 100 + t.getHours(), r.msradmCd, r.pm10 ?? '', r.pm25]);
        }
      }));
    }
    rows[m] = out;
    console.log(`2025-${m}: ${out.length}줄`);
  }

  // 주소 → 좌표 (카카오 로컬 API). 주소로 안 되면 이름으로 검색
  const K = { headers: { Authorization: 'KakaoAK ' + KEY } };
  const inSeoul = (y, x) => y > 37.41 && y < 37.72 && x > 126.73 && x < 127.27;
  const find = async (type, query) => {
    const j = await fetch(`https://dapi.kakao.com/v2/local/search/${type}.json?query=` + encodeURIComponent(query), K).then(r => r.json());
    const d = (j.documents || []).find(d => inSeoul(+d.y, +d.x));
    return d ? [+d.y, +d.x] : null;
  };
  for (const s of Object.values(sites)) {
    const a = (s.addr || '').trim(), area = s.area === '경기도' ? '' : s.area;
    for (const [type, query, how] of [['address', /^(서울|경기)/.test(a) ? a : `서울 ${area} ${a}`, '주소'], ['keyword', `${area} ${s.name}`, '이름']]) {
      const p = await find(type, query).catch(() => null);
      if (p) { [s.lat, s.lng, s.how] = [...p, how]; break; }
    }
  }

  const save = (name, text) => {
    const el = document.createElement('a');
    el.href = URL.createObjectURL(new Blob([text], { type: 'text/csv' }));
    el.download = name; document.body.appendChild(el); el.click(); el.remove();
  };
  const esc = v => `"${String(v ?? '').replace(/"/g, '""')}"`;
  save('simple_raw_sites.csv', 'code,name,kind,area,addr,lat,lng,how\n' +
    Object.values(sites).map(s => [s.code, s.name, s.kind, s.area, s.addr, s.lat ?? '', s.lng ?? '', s.how ?? ''].map(esc).join(',')).join('\n') + '\n');
  for (const m of [2, 5, 8, 11]) {
    await new Promise(r => setTimeout(r, 700));
    save(`simple_raw_2025_${String(m).padStart(2, '0')}.csv`, 'datetime,code,pm10,pm25\n' + rows[m].map(r => r.join(',')).join('\n') + '\n');
  }
})();
