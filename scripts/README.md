# 데이터 전처리 스크립트

원본 데이터를 `data/processed/`의 프로그램용 CSV로 바꿉니다. 레포 맨 위 폴더에서 실행하세요.
원본은 `data/raw/`에 두고 GitHub에는 올리지 않습니다(`.gitignore`).

준비: `pip install pandas openpyxl`

| 스크립트 | 만드는 파일 | 읽는 쪽 |
|---|---|---|
| `fetch_osm_walk.py` | `osm_walk.csv` (보행로 선분 `lat1,lng1,lat2,lng2`) | 김재겸 `applyRoads` |
| `prep_sensors.py sdot 위치파일 측정파일...` | `sdot_stations.csv`, `sdot_2025_MM.csv` | 방지혁 `loadStations(.., SDOT)`, `loadSdot` |
| `prep_sensors.py simple 위치파일 측정파일...` | `simple_stations.csv`, `simple_2025_MM.csv` | 방지혁 `loadStations(.., SIMPLE)`, `loadSimple` |

- `fetch_osm_walk.py`는 인터넷으로 OpenStreetMap(Overpass API)에서 직접 받습니다. 받은 원본은 `data/raw/osm/`에 남아서 다시 실행하면 빠릅니다.
- `prep_sensors.py`는 원본 열 이름을 단어로 찾습니다. 못 찾으면 원본 열 이름을 보여주고 멈추니, 파일 안 `COLUMNS`에 그 이름을 추가하면 됩니다.
- S-DoT는 2분·10분 단위 값을 **시간 평균**으로 줄이고, 풍향은 바람 벡터로 평균 냅니다.
