// 담당: 장데니스 (검증 틀은 김재겸 bench_wind.cpp와 같음)
// 관측소 노드 그래프(StationGraph::estimateByWind) 바람 예측 검증: bench_wind와 같은 시각·같은 측정소로 비교
// 자치구 측정소 25곳의 실제 t+1·2·3시 값과 평균 절대 오차(MAE)를 비교합니다.
// 비교 기준(지속 예측): 시각 t 격자를 그대로 두는 것 (바람을 쓰지 않을 때)
// make test에는 들어가지 않습니다 (몇 분 걸림). 저장소 맨 위 폴더에서 따로 실행:
//   g++ -std=c++17 -O2 -Isrc tests/bench_station_wind.cpp $(ls src/*/*.cpp | grep -v main.cpp) -o build/bench_station_wind && ./build/bench_station_wind
#include <cstdio>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <string>
#include "common/Types.h"
#include "data/DataLoader.h"
#include "data/HashTable.h"
#include "station/StationGraph.h"

namespace {
const std::string DIR = "data/processed/";
const int MAX_HOURS = 31 * 24;
const int STRIDE = 3;          // 시각 t를 3시간 간격으로 뽑음
const int MIN_WIND = 20;       // 시각 t에 바람 센서가 이만큼 이상 있어야 사용
const double CELL = 100.0;     // 격자 간격 (m)
const double WINDY = 1.5;      // S-DoT 평균 풍속이 이 값 이상이면 '바람 강함'으로 따로 집계 (m/s)

// 쉼표로 나누기 (따옴표 없는 파일)
void split(const std::string& line, DynamicArray<std::string>& out) {
    out.clear();
    std::string cur;
    for (char ch : line) {
        if (ch == ',') { out.push_back(cur); cur.clear(); }
        else if (ch != '\r') cur += ch;
    }
    out.push_back(cur);
}

// YYYYMMDDHH → 그 달 안의 시간 번호 (1일 0시 = 0)
int hourIndex(const std::string& dt) {
    if (dt.size() < 10) return -1;
    int day = std::atoi(dt.substr(6, 2).c_str()), hour = std::atoi(dt.substr(8, 2).c_str());
    int h = (day - 1) * 24 + hour;
    return (h >= 0 && h < MAX_HOURS) ? h : -1;
}

struct District { std::string code, name; double lat, lng; };


struct Sum {
    double persist = 0, wind = 0; long long n = 0;
    void add(const Sum& o) { persist += o.persist; wind += o.wind; n += o.n; }
};

void printRow(const char* label, int h, const Sum& s, int times) {
    double p = s.n ? s.persist / s.n : 0, w = s.n ? s.wind / s.n : 0;
    std::printf("| %s | %d시간 뒤 | %d | %lld | %.2f | %.2f | %+.1f%% |\n",
                label, h, times, s.n, p, w, p > 0 ? 100.0 * (p - w) / p : 0.0);
}
}  // namespace

int main() {
    DynamicArray<std::string> tok;
    std::string line;

    // 자치구 측정소 위치
    DynamicArray<District> districts;
    {
        std::ifstream in(DIR + "stations.csv");
        if (!in) { std::printf("data/processed를 찾지 못했습니다 (저장소 맨 위 폴더에서 실행하세요)\n"); return 1; }
        std::getline(in, line);
        while (std::getline(in, line)) {
            split(line, tok);
            if (tok.size() < 4) continue;
            districts.push_back({tok[0], tok[1], std::atof(tok[2].c_str()), std::atof(tok[3].c_str())});
        }
    }
    int nd = districts.size();
    HashTable<std::string, int> districtByCode(64);
    for (int i = 0; i < nd; ++i) districtByCode.put(districts[i].code, i);

    // S-DoT 센서 위치
    HashTable<std::string, int> sdotByCode(2048);
    DynamicArray<double> sdotLat, sdotLng;
    {
        std::ifstream in(DIR + "sdot_stations.csv");
        std::getline(in, line);
        while (std::getline(in, line)) {
            split(line, tok);
            if (tok.size() < 4) continue;
            sdotByCode.put(tok[0], sdotLat.size());
            sdotLat.push_back(std::atof(tok[2].c_str()));
            sdotLng.push_back(std::atof(tok[3].c_str()));
        }
    }

    const char* months[] = {"02", "05", "08", "11"};
    Sum total[4], windy[4], calm[4];
    int totalTimes = 0, windyTimes = 0;
    double buildMs = 0, advectMs = 0, meanWindSum = 0;
    std::printf("| 월 | 예측 | 시각 수 | 비교 수 | 그대로 둘 때 MAE | 바람 이동 MAE | 개선 |\n");
    std::printf("|---|---|---|---|---|---|---|\n");

    for (const char* mm : months) {
        // 자치구 시간별 PM2.5: pm[시간 × nd + 측정소]
        DynamicArray<double> pm;
        pm.resize(MAX_HOURS * nd, -1.0);
        {
            std::ifstream in(DIR + "air_2025_" + mm + ".csv");
            std::getline(in, line);
            while (std::getline(in, line)) {
                split(line, tok);
                if (tok.size() < 5 || tok[4].empty()) continue;
                int h = hourIndex(tok[0]);
                int* s = districtByCode.get(tok[1]);
                if (h < 0 || !s) continue;
                pm[h * nd + *s] = std::atof(tok[4].c_str());
            }
        }
        // S-DoT 시간별 바람: 시간마다 관측소 목록
        DynamicArray<DynamicArray<Station>> windByHour;
        windByHour.resize(MAX_HOURS);
        {
            std::ifstream in(DIR + "sdot_2025_" + mm + ".csv");
            std::getline(in, line);
            while (std::getline(in, line)) {
                split(line, tok);
                if (tok.size() < 9 || tok[7].empty() || tok[8].empty()) continue;
                int h = hourIndex(tok[0]);
                int* s = sdotByCode.get(tok[1]);
                if (h < 0 || !s) continue;
                Station st;
                st.type = StationType::SDOT;
                st.lat = sdotLat[*s]; st.lng = sdotLng[*s];
                st.windDir = std::atof(tok[7].c_str());
                st.windSpeed = std::atof(tok[8].c_str());
                windByHour[h].push_back(st);
            }
        }

        Sum month[4];
        int times = 0;
        for (int t = 0; t + 3 < MAX_HOURS; t += STRIDE) {
            if (windByHour[t].size() < MIN_WIND) continue;
            DynamicArray<Station> now;
            for (int s = 0; s < nd; ++s) {
                if (pm[t * nd + s] < 0) continue;
                Station st;
                st.name = districts[s].name;
                st.lat = districts[s].lat; st.lng = districts[s].lng;
                st.pm25 = pm[t * nd + s];
                now.push_back(st);
            }
            if (now.size() < 20) continue;

            double ms = 0;
            for (int i = 0; i < windByHour[t].size(); ++i) ms += windByHour[t][i].windSpeed;
            meanWindSum += ms / windByHour[t].size();
            bool strong = ms / windByHour[t].size() >= WINDY;
            if (strong) ++windyTimes;
            ++times;

            std::clock_t c0 = std::clock();
            StationGraph graph(now, windByHour[t], 5);
            buildMs += 1000.0 * (std::clock() - c0) / CLOCKS_PER_SEC;
            for (int h = 1; h <= 3; ++h) {
                DynamicArray<double> moved;
                std::clock_t a0 = std::clock();
                graph.estimateByWind(h, moved);
                advectMs += 1000.0 * (std::clock() - a0) / CLOCKS_PER_SEC;
                for (int i = 0; i < now.size(); ++i) {
                    int s = -1;
                    for (int k = 0; k < nd; ++k) if (districts[k].name == now[i].name) { s = k; break; }
                    double actual = s < 0 ? -1 : pm[(t + h) * nd + s];
                    if (actual < 0 || moved[i] < 0) continue;
                    Sum one;
                    one.persist = std::fabs(now[i].pm25 - actual);
                    one.wind = std::fabs(moved[i] - actual);
                    one.n = 1;
                    month[h].add(one);
                    (strong ? windy[h] : calm[h]).add(one);
                }
            }
        }
        for (int h = 1; h <= 3; ++h) {
            printRow(mm, h, month[h], times);
            total[h].add(month[h]);
        }
        totalTimes += times;
        std::fflush(stdout);
    }
    for (int h = 1; h <= 3; ++h) printRow("전체", h, total[h], totalTimes);
    for (int h = 1; h <= 3; ++h) printRow("바람 강함", h, windy[h], windyTimes);
    for (int h = 1; h <= 3; ++h) printRow("바람 약함", h, calm[h], totalTimes - windyTimes);

    if (totalTimes > 0) {
        std::printf("\n시각 %d개 (3시간 간격, 바람 센서 %d곳 이상), 자치구 측정소 노드 그래프 (k = 5)\n", totalTimes, MIN_WIND);
        std::printf("S-DoT 평균 풍속 %.2f m/s, 바람 강함(평균 %.1f m/s 이상) 시각 %d개\n", meanWindSum / totalTimes, WINDY, windyTimes);
        std::printf("그래프 만들기 평균 %.2f ms, 추정 평균 %.2f ms\n", buildMs / totalTimes, advectMs / (3.0 * totalTimes));
    }
    return 0;
}
