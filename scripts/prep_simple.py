"""간이측정망 원본(fetch_simple.js 결과)을 프로그램이 읽는 형식으로 바꿉니다.

실행 (레포 맨 위 폴더에서):
  python scripts/prep_simple.py data/raw/simple

거르는 기준
  - 종류: 녹색교통지역, 집중관리구역, 취약계층보호만 사용
      공사장·기타배출원(정비소·공장 앞)은 바로 옆 배출원 영향을 받아 도시 전체 보간에 맞지 않음
      (2025년 기타배출원과 서울 평균의 상관계수 0.11~0.65로 들쭉날쭉)
  - 그 달 측정값이 절반(50%) 이상 있는 지점만
  - 좌표를 못 찾은 지점 제외
보정
  - 간이측정기는 공식 측정소와 값 차이가 있음. 공식 측정소 바로 옆에 둔 간이측정기("대기측정소" 종류)와
    같은 구 공식 값을 비교해서, 달마다 비율(간이 합 / 공식 합)로 나눠 맞춤
결과 (data/processed/)
  simple_stations.csv : code,name,lat,lng,address,note   (code = name = 간이측정소 번호, note = 종류)
  simple_2025_MM.csv  : datetime,station_code,station_name,pm10,pm25   (pm10은 빈칸, pm25는 보정값)
"""
import os
import sys

import pandas as pd

MONTHS = ("02", "05", "08", "11")
USE = ("녹색교통지역", "집중관리구역", "취약계층보호")
OUT = os.path.join("data", "processed")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    raw = sys.argv[1]
    sites = pd.read_csv(os.path.join(raw, "simple_raw_sites.csv"), dtype={"code": str})
    used = set()
    print(" 월 | 지점 | 보정 비율(간이/공식) | 같은 구 상관")
    for m in MONTHS:
        d = pd.read_csv(os.path.join(raw, f"simple_raw_2025_{m}.csv"), dtype={"code": str})
        d = d.merge(sites[["code", "kind", "area", "lat"]], on="code")

        # 보정 비율: 공식 측정소 옆 간이측정기 vs 같은 구 공식 값
        official = pd.read_csv(os.path.join(OUT, f"air_2025_{m}.csv"))[["datetime", "station_name", "pm25"]]
        official = official.rename(columns={"station_name": "area", "pm25": "official"})
        pair = d[d.kind == "대기측정소"].merge(official, on=["datetime", "area"]).dropna(subset=["pm25", "official"])
        ratio = pair.pm25.sum() / pair.official.sum()
        corr = pair[["pm25", "official"]].corr().iloc[0, 1]

        hours = pd.Timestamp(f"2025-{m}-01").days_in_month * 24
        good = d[d.kind.isin(USE) & d.lat.notna()]
        coverage = good.groupby("code").pm25.count() / hours
        keep = set(coverage[coverage >= 0.5].index)
        good = good[good.code.isin(keep)].sort_values(["datetime", "code"])
        used |= keep

        path = os.path.join(OUT, f"simple_2025_{m}.csv")
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write("datetime,station_code,station_name,pm10,pm25\n")
            for r in good.itertuples(index=False):
                f.write(f"{r.datetime},{r.code},{r.code},,{r.pm25 / ratio:.1f}\n")
        print(f" {m} | {len(keep):3d} | {ratio:.2f} | {corr:.2f}  → {path} ({len(good):,}줄)")

    s = sites[sites.code.isin(used)]
    with open(os.path.join(OUT, "simple_stations.csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("code,name,lat,lng,address,note\n")
        for r in s.itertuples(index=False):
            addr = f"{r.name} {r.addr}".replace(",", " ").replace('"', "")
            f.write(f"{r.code},{r.code},{r.lat:.6f},{r.lng:.6f},{addr},{r.kind}\n")
    print(f" → simple_stations.csv: {len(s)}곳")


if __name__ == "__main__":
    main()
