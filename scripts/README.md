# 데이터 전처리 스크립트

원본 데이터를 `data/processed/`의 프로그램용 CSV로 바꿉니다. 레포 맨 위 폴더에서 실행하세요.
원본은 `data/raw/`에 두고 GitHub에는 올리지 않습니다(`.gitignore`).

준비: `pip install pandas openpyxl`

| 스크립트 | 만드는 파일 | 읽는 쪽 |
|---|---|---|
| `fetch_osm_walk.py` | `osm_walk.csv` (보행로 선분 `lat1,lng1,lat2,lng2`, 약 115만 줄·44MB) | 김재겸 `applyRoads` |
| `prep_sdot.py 위치정보.xlsx 압축푼폴더` | `sdot_stations.csv`(1,142곳), `sdot_2025_MM.csv`(월 약 60만 줄·30MB) | 방지혁 `loadStations(.., SDOT)`, `loadSdot` |
| `prep_sensors.py simple 위치파일 측정파일...` | `simple_stations.csv`, `simple_2025_MM.csv` | 방지혁 `loadStations(.., SIMPLE)`, `loadSimple` |

- `fetch_osm_walk.py`는 인터넷으로 OpenStreetMap(Overpass API)에서 직접 받습니다. 받은 원본은 `data/raw/osm/`에 남아서 다시 실행하면 빠릅니다.
- `prep_sensors.py`는 원본 열 이름을 단어로 찾습니다. 못 찾으면 원본 열 이름을 보여주고 멈추니, 파일 안 `COLUMNS`에 그 이름을 추가하면 됩니다.
- **S-DoT는 미세먼지를 공개하지 않습니다** (미세먼지법). 그래서 `pm10`, `pm25`는 빈칸이고 기온·습도·바람·이산화질소·오존만 있습니다.
  바람이 있는 센서는 약 55곳(16개 구), 기온은 약 960곳입니다.
- 원본 S-DoT 파일은 줄마다 열이 헤더보다 6개 많아서 그대로 읽으면 열이 밀립니다 (`index_col=False`로 해결).
