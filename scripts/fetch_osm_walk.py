"""OpenStreetMap에서 서울의 걸을 수 있는 길을 받아 data/processed/osm_walk.csv로 저장합니다.

실행 (레포 맨 위 폴더에서, 인터넷 필요, 몇 분 걸림):
    python scripts/fetch_osm_walk.py

결과 형식 (한 줄 = 길의 선분 하나, 김재겸 applyRoads가 읽음):
    lat1,lng1,lat2,lng2

- 서울을 4×4 조각으로 나눠 Overpass API에 요청합니다. (한 번에 받으면 시간 초과)
- 자동차 전용 도로(고속도로·자동차전용도로)와 보행 금지(foot=no) 길은 뺍니다.
- 받은 원본은 data/raw/osm/ 에 저장해 두고, 다시 실행하면 원본을 다시 쓰므로 빠릅니다.
"""
import json
import os
import sys
import time
import urllib.parse
import urllib.request

OVERPASS = "https://overpass-api.de/api/interpreter"
# buildGrid의 격자 범위와 같음
MIN_LAT, MAX_LAT = 37.41, 37.72
MIN_LNG, MAX_LNG = 126.73, 127.27
TILES = 4

# 걸을 수 있는 길 종류 (highway 태그)
WALKABLE = ("footway|pedestrian|path|steps|living_street|residential|service|unclassified|"
            "tertiary|tertiary_link|secondary|secondary_link|primary|primary_link|track|cycleway|corridor")

RAW_DIR = os.path.join("data", "raw", "osm")
OUT = os.path.join("data", "processed", "osm_walk.csv")


def query(s, w, n, e):
    bbox = f"{s},{w},{n},{e}"
    return f"""[out:json][timeout:300];
way["highway"~"^({WALKABLE})$"]["foot"!="no"]["access"!="private"]({bbox});
out geom qt;"""


def fetch(tile_path, q):
    if os.path.exists(tile_path):
        with open(tile_path, encoding="utf-8") as f:
            return json.load(f)
    data = urllib.parse.urlencode({"data": q}).encode()
    for attempt in range(4):
        try:
            req = urllib.request.Request(OVERPASS, data=data, headers={"User-Agent": "seoul-clean-route (student project)"})
            with urllib.request.urlopen(req, timeout=400) as res:
                body = res.read().decode("utf-8")
            result = json.loads(body)
            with open(tile_path, "w", encoding="utf-8") as f:
                f.write(body)
            return result
        except Exception as e:  # 서버가 바쁘면 잠시 쉬고 다시
            print(f"    실패 ({e}), {30 * (attempt + 1)}초 뒤 다시 시도")
            time.sleep(30 * (attempt + 1))
    sys.exit("Overpass 서버에서 받지 못했습니다. 잠시 뒤 다시 실행해 주세요.")


def main():
    os.makedirs(RAW_DIR, exist_ok=True)
    seen = set()          # 조각 경계에서 같은 길이 두 번 나오는 것 방지
    segments = 0
    dlat = (MAX_LAT - MIN_LAT) / TILES
    dlng = (MAX_LNG - MIN_LNG) / TILES
    with open(OUT, "w", encoding="utf-8", newline="\n") as out:
        out.write("lat1,lng1,lat2,lng2\n")
        for i in range(TILES):
            for j in range(TILES):
                s, n = MIN_LAT + i * dlat, MIN_LAT + (i + 1) * dlat
                w, e = MIN_LNG + j * dlng, MIN_LNG + (j + 1) * dlng
                print(f"  조각 {i * TILES + j + 1}/{TILES * TILES} 받는 중")
                tile = os.path.join(RAW_DIR, f"tile_{i}_{j}.json")
                cached = os.path.exists(tile)
                result = fetch(tile, query(s, w, n, e))
                for way in result.get("elements", []):
                    if way.get("type") != "way" or way["id"] in seen:
                        continue
                    seen.add(way["id"])
                    pts = way.get("geometry") or []
                    for a, b in zip(pts, pts[1:]):
                        out.write(f"{a['lat']:.6f},{a['lon']:.6f},{b['lat']:.6f},{b['lon']:.6f}\n")
                        segments += 1
                if not cached:
                    time.sleep(2)   # 공용 서버 배려
    size = os.path.getsize(OUT) / 1e6
    print(f"완료: 길 {len(seen):,}개, 선분 {segments:,}개 → {OUT} ({size:.1f} MB)")


if __name__ == "__main__":
    main()
