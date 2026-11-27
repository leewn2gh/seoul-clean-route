# 담당: 이준호
# 웹(WebAssembly)용 S-DoT 바람 파일: sdot_2025_MM.csv(월 30MB)에서 풍향·풍속이 있는 줄만 뽑아 sdot_wind_2025_MM.csv(월 약 2MB)로 저장
# 형식은 원본과 같아서 DataLoader::loadSdot으로 그대로 읽음
#   python scripts/prep_wind.py
import csv, os

DIR = 'data/processed'
for mm in ['02', '05', '08', '11']:
    src = os.path.join(DIR, f'sdot_2025_{mm}.csv')
    dst = os.path.join(DIR, f'sdot_wind_2025_{mm}.csv')
    if not os.path.exists(src):
        print('없음:', src)
        continue
    n = 0
    with open(src, newline='') as fi, open(dst, 'w', newline='') as fo:
        r, w = csv.reader(fi), csv.writer(fo, lineterminator='\n')
        w.writerow(next(r))
        for row in r:
            if len(row) >= 9 and row[7] != '' and row[8] != '':
                w.writerow(row)
                n += 1
    print(f'{dst}: {n}줄, {os.path.getsize(dst) / 1e6:.1f}MB')
