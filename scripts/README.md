# 데이터 전처리 스크립트

원본 데이터를 `data/processed/`의 프로그램용 CSV로 바꿉니다. 레포 맨 위 폴더에서 실행하세요.
원본은 `data/raw/`에 두고 GitHub에는 올리지 않습니다(`.gitignore`).

준비: `pip install pandas openpyxl`

| 스크립트 | 만드는 파일 | 읽는 쪽 |
|---|---|---|
| `fetch_osm_walk.py` | `osm_walk.csv` (보행로 선분 `lat1,lng1,lat2,lng2`, 약 115만 줄·44MB) | 김재겸 `applyRoads` |
| `prep_sdot.py 위치정보.xlsx 압축푼폴더` | `sdot_stations.csv`(1,142곳), `sdot_2025_MM.csv`(월 약 60만 줄·30MB) | 방지혁 `loadStations(.., SDOT)`, `loadSdot` |
| `fetch_simple.js` (브라우저 콘솔) → `prep_simple.py data/raw/simple` | `simple_stations.csv`(131곳), `simple_2025_MM.csv`(보정한 초미세먼지) | 방지혁 `loadStations(.., SIMPLE)`, `loadSimple` |
| `prep_wind.py` | `sdot_wind_2025_MM.csv`(S-DoT에서 풍향·풍속 있는 줄만, 월 약 2MB) | 웹(WebAssembly) 바람 예측용, `App::init`이 sdot 전체가 없을 때 읽음 |

- `fetch_osm_walk.py`는 인터넷으로 OpenStreetMap(Overpass API)에서 직접 받습니다. 받은 원본은 `data/raw/osm/`에 남아서 다시 실행하면 빠릅니다.
- 간이측정망은 사이트에서 하루·한 시간씩만 조회돼서 `fetch_simple.js`를 대기환경정보 페이지 콘솔에 붙여 넣어 한꺼번에 받습니다. 좌표가 없어 카카오 로컬 API로 주소를 좌표로 바꿉니다(키는 실행할 때 입력, 저장하지 않음).
- 간이측정망은 357곳 중 **녹색교통지역·집중관리구역·취약계층보호** 중 그 달 측정값이 절반 이상인 곳만 씁니다(월 96~129곳). 공사장·기타배출원은 바로 옆 배출원 영향이 커서 뺍니다.
- 간이측정기는 공식 값과 차이가 있어서, 공식 측정소 옆 간이측정기와 같은 구 공식 값의 비율로 달마다 보정합니다 (2월 0.78, 5월 0.85, 8월 0.72, 11월 1.15).
- **S-DoT는 미세먼지를 공개하지 않습니다** (미세먼지법). 그래서 `pm10`, `pm25`는 빈칸이고 기온·습도·바람·이산화질소·오존만 있습니다.
  바람이 있는 센서는 약 55곳(16개 구), 기온은 약 960곳입니다.
- 원본 S-DoT 파일은 줄마다 열이 헤더보다 6개 많아서 그대로 읽으면 열이 밀립니다 (`index_col=False`로 해결).
