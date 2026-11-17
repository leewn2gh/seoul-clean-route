"""간이측정망 원본 파일을 프로그램이 읽는 형식으로 바꿉니다. (S-DoT는 prep_sdot.py 사용)

원본 받기 (data/raw/ 아래에 넣기, GitHub에는 올리지 않음)
  S-DoT 환경정보 (OA-15969): https://data.seoul.go.kr/dataList/OA-15969/S/1/datasetView.do
    → 2025년 2·5·8·11월 파일, 센서 설치 위치 파일 → data/raw/sdot/
  간이측정망 초미세먼지: https://cleanair.seoul.go.kr/airquality/simpleAvg
    → 2025년 2·5·8·11월 시간별 자료, 측정소 위치 → data/raw/simple/

실행 (레포 맨 위 폴더에서):
  python scripts/prep_sensors.py sdot   data/raw/sdot/위치파일.csv   data/raw/sdot/*.csv
  python scripts/prep_sensors.py simple data/raw/simple/위치파일.csv data/raw/simple/*.csv
  (위치 파일을 먼저, 측정값 파일은 몇 개든 뒤에. csv, xlsx 둘 다 됨)

결과 (data/processed/)
  {kind}_stations.csv : code,name,lat,lng,address,note   (stations.csv와 같은 형식, code = name = 센서 번호)
  {kind}_2025_MM.csv  : datetime,station_code,station_name,pm10,pm25[,temp,humidity,wind_dir,wind_speed]
    - datetime은 YYYYMMDDHH, 한 시간 안의 값은 평균 (S-DoT는 10분·2분 단위라 시간 평균으로 줄임)
    - 값이 없으면 빈칸

원본의 열 이름은 파일마다 조금씩 달라서, 이름에 들어간 단어로 찾습니다 (아래 COLUMNS).
못 찾은 열이 있으면 원본 열 이름을 출력하고 멈추니, 그 이름을 COLUMNS에 추가하면 됩니다.
"""
import glob
import os
import sys

import pandas as pd

MONTHS = {2, 5, 8, 11}
OUT_DIR = os.path.join("data", "processed")

# 표준 이름 → 원본 열 이름에 들어 있을 단어들 (앞에 있는 것부터 찾음). 제외할 단어는 '-'로 시작
COLUMNS = {
    "code": [["시리얼"], ["serial"], ["센서", "번호"], ["측정소", "코드"], ["측정소명"], ["측정소"], ["설치", "장소"], ["장소"]],
    "time": [["측정", "시간"], ["측정", "일시"], ["일시"], ["시간"], ["datetime"], ["date"]],
    "lat": [["위도"], ["lat"]],
    "lng": [["경도"], ["lng"], ["lon"]],
    "address": [["주소"], ["설치", "위치"], ["위치"]],
    "pm25": [["초미세먼지"], ["pm2.5"], ["pm25"]],
    "pm10": [["미세먼지", "-초미세"], ["pm10"]],
    "temp": [["기온"], ["온도"], ["temp"]],
    "humidity": [["습도"], ["humid"]],
    "wind_dir": [["풍향"], ["wind", "dir"]],
    "wind_speed": [["풍속"], ["wind", "speed"]],
}


def read_any(path):
    if path.lower().endswith((".xlsx", ".xls")):
        return pd.read_excel(path)
    for enc in ("utf-8-sig", "cp949"):
        try:
            return pd.read_csv(path, encoding=enc, low_memory=False)
        except UnicodeDecodeError:
            continue
    raise SystemExit(f"{path}: 인코딩을 알 수 없습니다 (UTF-8, CP949 아님)")


def find_column(df, key, required=True):
    names = [str(c) for c in df.columns]
    for words in COLUMNS[key]:
        need = [w.lower() for w in words if not w.startswith("-")]
        avoid = [w[1:].lower() for w in words if w.startswith("-")]
        for c in names:
            low = c.lower().replace(" ", "")
            if all(w in low for w in need) and not any(w in low for w in avoid):
                return c
    if required:
        raise SystemExit(f"'{key}' 열을 찾지 못했습니다. 원본 열 이름: {names}\n→ scripts/prep_sensors.py의 COLUMNS에 추가해 주세요.")
    return None


def to_hour_key(series):
    """여러 시각 형식을 YYYYMMDDHH 정수로. 2025-02-01 13:20, 202502011320, 2025020113 등"""
    s = series.astype(str).str.strip()
    digits = s.str.replace(r"\D", "", regex=True)
    out = pd.Series(pd.NA, index=s.index, dtype="Int64")
    for n in (14, 12, 10):   # YYYYMMDDHHMMSS, YYYYMMDDHHMM, YYYYMMDDHH
        m = digits.str.len() == n
        out[m] = digits[m].str[:10].astype("int64")
    rest = out.isna()
    if rest.any():           # 그 밖의 형식은 pandas에 맡김
        t = pd.to_datetime(s[rest], errors="coerce")
        out[rest] = (t.dt.year * 1000000 + t.dt.month * 10000 + t.dt.day * 100 + t.dt.hour).astype("Int64")
    # 24시 표기(… 24시 = 다음 날 0시)는 23시로 묶음
    hh = out % 100
    out[hh == 24] = out[hh == 24] - 1
    return out


def number(series):
    return pd.to_numeric(series.astype(str).str.replace(",", "").str.strip(), errors="coerce")


def main():
    if len(sys.argv) < 4 or sys.argv[1] not in ("sdot", "simple"):
        sys.exit(__doc__)
    kind, loc_path, value_paths = sys.argv[1], sys.argv[2], []
    for p in sys.argv[3:]:
        value_paths += sorted(glob.glob(p)) or [p]
    sdot = kind == "sdot"

    # 1. 위치
    loc = read_any(loc_path)
    c_code, c_lat, c_lng = find_column(loc, "code"), find_column(loc, "lat"), find_column(loc, "lng")
    c_addr = find_column(loc, "address", required=False)
    st = pd.DataFrame({
        "code": loc[c_code].astype(str).str.strip(),
        "lat": number(loc[c_lat]), "lng": number(loc[c_lng]),
        "address": loc[c_addr].astype(str).str.strip() if c_addr else "",
    }).dropna(subset=["lat", "lng"]).drop_duplicates("code")
    st = st[(st.lat.between(37.41, 37.72)) & (st.lng.between(126.73, 127.27))]   # 격자 범위 안만
    known = set(st.code)

    # 2. 측정값 → 시간 평균
    keys = ["pm10", "pm25"] + (["temp", "humidity", "wind_dir", "wind_speed"] if sdot else [])
    parts = []
    for path in value_paths:
        df = read_any(path)
        c_code, c_time = find_column(df, "code"), find_column(df, "time")
        cols = {k: find_column(df, k, required=(k == "pm25")) for k in keys}
        missing = [k for k, c in cols.items() if c is None]
        if missing:
            print(f"  주의: {os.path.basename(path)}에 {missing} 열이 없어 빈칸으로 둡니다")
        part = pd.DataFrame({"code": df[c_code].astype(str).str.strip(), "hour": to_hour_key(df[c_time])})
        for k, c in cols.items():
            part[k] = number(df[c]) if c else float("nan")
        parts.append(part[part.code.isin(known)].dropna(subset=["hour"]))
        print(f"  {os.path.basename(path)}: {len(df):,}줄 읽음")
    if not parts:
        sys.exit("측정값 파일이 없습니다.")
    data = pd.concat(parts)

    # 이상값 제거 (센서 오류)
    data.loc[(data.pm25 < 0) | (data.pm25 > 500), "pm25"] = float("nan")
    data.loc[(data.pm10 < 0) | (data.pm10 > 1000), "pm10"] = float("nan")
    if sdot:
        data.loc[(data.temp < -40) | (data.temp > 50), "temp"] = float("nan")
        data.loc[(data.humidity < 0) | (data.humidity > 100), "humidity"] = float("nan")
        data.loc[(data.wind_dir < 0) | (data.wind_dir > 360), "wind_dir"] = float("nan")
        data.loc[(data.wind_speed < 0) | (data.wind_speed > 60), "wind_speed"] = float("nan")
        # 풍향은 각도라 그냥 평균 내면 안 됨 → 바람 벡터(u, v)로 평균 낸 뒤 되돌림
        rad = data.wind_dir * 3.141592653589793 / 180
        data["u"] = data.wind_speed * rad.map(__import__("math").sin)
        data["v"] = data.wind_speed * rad.map(__import__("math").cos)

    agg = data.groupby(["code", "hour"]).mean(numeric_only=True).reset_index()
    if sdot:
        import numpy as np
        agg["wind_dir"] = (np.degrees(np.arctan2(agg.u, agg.v)) + 360) % 360
        agg.loc[agg.u.isna(), "wind_dir"] = float("nan")
        agg = agg.drop(columns=["u", "v"])

    # 3. 저장
    used = st[st.code.isin(set(agg.code))]
    with open(os.path.join(OUT_DIR, f"{kind}_stations.csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("code,name,lat,lng,address,note\n")
        for r in used.itertuples():
            f.write(f"{r.code},{r.code},{r.lat:.6f},{r.lng:.6f},{str(r.address).replace(',', ' ')},\n")

    agg["month"] = (agg.hour // 10000) % 100
    agg["year"] = agg.hour // 1000000
    fmt = lambda v, d: "" if pd.isna(v) else f"{v:.{d}f}"
    header = "datetime,station_code,station_name," + ",".join(keys)
    for (year, month), g in agg[agg.month.isin(MONTHS)].groupby(["year", "month"]):
        path = os.path.join(OUT_DIR, f"{kind}_{year}_{month:02d}.csv")
        g = g.sort_values(["hour", "code"])
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(header + "\n")
            for r in g.itertuples():
                vals = [fmt(getattr(r, k), 1) for k in keys]
                f.write(f"{r.hour},{r.code},{r.code}," + ",".join(vals) + "\n")
        print(f"  → {path}: {len(g):,}줄, 센서 {g.code.nunique():,}곳")
    print(f"  → {kind}_stations.csv: {len(used):,}곳")


if __name__ == "__main__":
    main()
