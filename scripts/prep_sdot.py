"""S-DoT 환경정보 원본을 프로그램이 읽는 형식으로 바꿉니다.

원본 (열린데이터광장 OA-15969, data/raw/sdot/ 에 두기, GitHub에는 안 올림)
  - S-DoT_NATURE_2025년(2025.01.01~2026.01.04).zip 압축 푼 폴더
      1~7월: 하루 파일(…_PUBDATA_20250201.csv), 8~12월: 주 파일(S-DoT_NATURE_2025.08.04-08.10.csv)
  - 서울시 도시데이터 센서(S-DoT) 환경정보 설치 위치정보.xlsx

실행 (레포 맨 위 폴더에서):
  python scripts/prep_sdot.py 위치정보.xlsx 압축푼폴더

결과 (data/processed/)
  sdot_stations.csv  : code,name,lat,lng,address,note   (code = name = 센서 시리얼)
  sdot_2025_MM.csv   : datetime,station_code,station_name,pm10,pm25,temp,humidity,wind_dir,wind_speed,no2,o3
    - 2·5·8·11월, datetime은 YYYYMMDDHH (원본이 이미 1시간 평균)
    - S-DoT는 미세먼지를 공개하지 않아서 pm10, pm25는 항상 빈칸 (열은 자치구 파일과 맞추려고 둠)
    - 값이 하나도 없는 줄은 뺌, 값이 없으면 빈칸
"""
import glob
import os
import sys

import pandas as pd

MONTHS = (2, 5, 8, 11)
OUT_DIR = os.path.join("data", "processed")
COLS = {   # 원본 열 → 결과 열 (평균값만 씀)
    "온도 평균(℃)": "temp",
    "습도 평균(%)": "humidity",
    "풍향 평균(m/s)": "wind_dir",    # 원본 단위 표기가 m/s로 잘못돼 있지만 실제로는 각도(도)
    "풍속 평균(m/s)": "wind_speed",
    "이산화질소 평균(ppm)": "no2",
    "오존 평균(ppm)": "o3",
}
RANGE = {"temp": (-40, 50), "humidity": (0, 100), "wind_dir": (0, 360), "wind_speed": (0, 60), "no2": (0, 1), "o3": (0, 1)}


def load_locations(path):
    loc = pd.read_excel(path)
    cur, lat, lng, addr, old = loc.columns[1], loc.columns[4], loc.columns[5], loc.columns[2], loc.columns[7]
    st = pd.DataFrame({"code": loc[cur].astype(str).str.strip(), "lat": pd.to_numeric(loc[lat], errors="coerce"),
                       "lng": pd.to_numeric(loc[lng], errors="coerce"), "address": loc[addr].astype(str)})
    st = st.dropna(subset=["lat", "lng"])
    st = st[st.lat.between(37.41, 37.72) & st.lng.between(126.73, 127.27)]
    # 데이터에는 시리얼이 바뀌기 전 이름으로 적힌 센서가 있음 → 지금 시리얼로 맞춤
    alias = {s: s for s in st.code}
    for c, o in zip(loc[cur].astype(str).str.strip(), loc[old]):
        if isinstance(o, str) and o.strip():
            alias[o.strip()] = c
    return st.drop_duplicates("code"), alias


def month_of(path):
    """파일 이름으로 이 파일이 2·5·8·11월을 포함하는지 판단"""
    name = os.path.basename(path)
    if "PUBDATA_" in name:
        return {int(name.split("PUBDATA_")[1][4:6])}
    span = name.replace("S-DoT_NATURE_", "").replace(".csv", "")   # 2025.08.04-08.10
    a, b = span.split("-")
    return {int(a.split(".")[1]), int(b.split(".")[0])}


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    st, alias = load_locations(sys.argv[1])
    files = [f for f in sorted(glob.glob(os.path.join(sys.argv[2], "**", "*.csv"), recursive=True)) if month_of(f) & set(MONTHS)]
    print(f"위치 {len(st):,}곳, 원본 파일 {len(files)}개")

    parts = []
    for f in files:
        # 줄마다 열이 헤더보다 많아서 index_col=False (안 하면 앞 열이 밀림)
        d = pd.read_csv(f, encoding="cp949", usecols=["시리얼", "측정시간"] + list(COLS), dtype=str, index_col=False)
        d = d.rename(columns=COLS)
        d["code"] = d["시리얼"].str.strip().map(alias)
        t = d["측정시간"].str.replace(r"\D", "", regex=True).str[:10]
        d = d[d.code.notna() & (t.str.len() == 10)].copy()
        d["hour"] = t[d.index].astype("int64")
        for c in COLS.values():
            v = pd.to_numeric(d[c], errors="coerce")
            lo, hi = RANGE[c]
            d[c] = v.where(v.between(lo, hi))
        parts.append(d[["code", "hour"] + list(COLS.values())])
        print(f"  {os.path.basename(f)[-30:]}: {len(d):,}줄")
    data = pd.concat(parts)
    data = data.dropna(how="all", subset=list(COLS.values()))
    data = data.groupby(["code", "hour"], as_index=False).mean()   # 주 파일 경계에서 겹친 시간 정리

    month = (data.hour // 10000) % 100
    fmt = lambda v, d: "" if pd.isna(v) else f"{v:.{d}f}"
    digits = {"temp": 1, "humidity": 0, "wind_dir": 0, "wind_speed": 1, "no2": 3, "o3": 3}
    used = set()
    for m in MONTHS:
        g = data[month == m].sort_values(["hour", "code"])
        path = os.path.join(OUT_DIR, f"sdot_2025_{m:02d}.csv")
        with open(path, "w", encoding="utf-8", newline="\n") as out:
            out.write("datetime,station_code,station_name,pm10,pm25," + ",".join(COLS.values()) + "\n")
            for r in g.itertuples(index=False):
                vals = ",".join(fmt(getattr(r, c), digits[c]) for c in COLS.values())
                out.write(f"{r.hour},{r.code},{r.code},,,{vals}\n")
        used |= set(g.code)
        wind = g.dropna(subset=["wind_dir"]).code.nunique()
        print(f"→ {path}: {len(g):,}줄, 센서 {g.code.nunique():,}곳 (바람 있는 센서 {wind}곳), {os.path.getsize(path) / 1e6:.1f} MB")

    st = st[st.code.isin(used)]
    with open(os.path.join(OUT_DIR, "sdot_stations.csv"), "w", encoding="utf-8", newline="\n") as out:
        out.write("code,name,lat,lng,address,note\n")
        for r in st.itertuples(index=False):
            out.write(f"{r.code},{r.code},{r.lat:.6f},{r.lng:.6f},{r.address.replace(',', ' ')},\n")
    print(f"→ sdot_stations.csv: {len(st):,}곳")


if __name__ == "__main__":
    main()
