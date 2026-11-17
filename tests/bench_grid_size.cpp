// 담당: 김재겸
// 격자 크기 비교 (100 / 50 / 25 / 10m): 칸 수, 메모리, buildGrid·applyRoads 시간, 보행로 칸 비율
// make test에는 들어가지 않습니다 (10m는 수십 초 걸림). 저장소 맨 위 폴더에서 따로 실행:
//   g++ -std=c++17 -O2 -Isrc -static tests/bench_grid_size.cpp src/*/*.cpp -o build/bench_grid_size && ./build/bench_grid_size
#include <cstdio>
#include <ctime>
#include "common/Types.h"
#include "data/DataLoader.h"
#include "graph/RoadMask.h"

static double seconds(std::clock_t from, std::clock_t to) {
    return (double)(to - from) / CLOCKS_PER_SEC;
}

int main() {
    const std::string dir = "data/processed/";
    DataLoader loader;
    if (!loader.loadStations(dir + "stations.csv") || !loader.loadDistrict(dir + "air_2025_02.csv")) {
        std::printf("데이터 파일을 읽지 못했습니다 (저장소 맨 위 폴더에서 실행하세요)\n");
        return 1;
    }
    DateTime from; from.year = 2025; from.month = 2; from.day = 1;  from.hour = 0;
    DateTime to;   to.year = 2025;   to.month = 2;   to.day = 31;   to.hour = 23;
    DynamicArray<Station> stations;
    loader.averageStations(from, to, stations);
    int withPm = 0;
    for (int i = 0; i < stations.size(); ++i) if (stations[i].pm25 >= 0) ++withPm;
    std::printf("측정소 %d곳 (초미세먼지 값 있음 %d곳), 2025년 2월 평균\n\n", stations.size(), withPm);

    const double sizes[] = {100.0, 50.0, 25.0, 10.0};
    std::printf("| 칸 크기 | 행 × 열 | 칸 수 | 메모리 | buildGrid | applyRoads | 보행로 칸 비율 |\n");
    std::printf("|---|---|---|---|---|---|---|\n");
    for (double m : sizes) {
        Grid grid;
        std::clock_t t0 = std::clock();
        buildGrid(stations, m, grid);
        std::clock_t t1 = std::clock();
        bool ok = applyRoads(dir + "osm_walk.csv", grid);
        std::clock_t t2 = std::clock();
        if (!ok) { std::printf("osm_walk.csv를 읽지 못했습니다\n"); return 1; }

        long long cells = (long long)grid.rows * grid.cols;
        int open = 0;
        for (int k = 0; k < grid.passable.size(); ++k) if (grid.passable[k]) ++open;
        // 칸별 값 배열 3개 (pm25·temp는 double, passable은 bool). 실제로 잡힌 용량 기준
        double bytes = (double)grid.pm25.capacity() * sizeof(double) + (double)grid.temp.capacity() * sizeof(double)
                     + (double)grid.passable.capacity() * sizeof(bool);
        std::printf("| %.0fm | %d × %d | %lld | %.1f MB | %.2f초 | %.2f초 | %.1f%% |\n",
                    m, grid.rows, grid.cols, cells, bytes / (1024.0 * 1024.0),
                    seconds(t0, t1), seconds(t1, t2), 100.0 * open / cells);
        std::fflush(stdout);
    }
    return 0;
}
