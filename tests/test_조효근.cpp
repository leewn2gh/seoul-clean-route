// 담당: 조효근
// Router 기능 검증 및 격자 크기별(100m, 50m, 25m, 10m) 다익스트라 vs A* 성능 벤치마크
#include <cstdio>
#include <cmath>
#include <ctime>
#include <cassert>

#include "core/App.h"
#include "route/Router.h"
#include "route/MinHeap.h"
#include "route/Stack.h"
#include "route/Queue.h"
#include "graph/GridGraph.h"

static void runUnitTests() {
    Grid grid;
    grid.rows = 10;
    grid.cols = 10;
    grid.minLat = 37.5000;
    grid.minLng = 127.0000;
    grid.cellLat = 0.001;
    grid.cellLng = 0.001;

    int totalCells = grid.rows * grid.cols;
    grid.pm25.resize(totalCells, 20.0);
    grid.temp.resize(totalCells, 15.0);
    grid.passable.resize(totalCells, true);

    for (int r = 3; r <= 6; ++r) {
        for (int c = 3; c <= 6; ++c) {
            grid.passable[r * grid.cols + c] = false;
        }
    }

    GridGraph graph(grid);

    // 1. nearestPassable 테스트
    int insideBuilding = 4 * 10 + 4;
    assert(nearestPassable(graph, insideBuilding, 1) == -1);
    int nearCell = nearestPassable(graph, insideBuilding);
    assert(nearCell != -1 && grid.passable[nearCell] == true);

    // 2. 다익스트라 vs A* 비용 일치 및 pm25s 테스트
    WeightOption opt;
    opt.alpha = 1.0;
    opt.beta = 0.0;

    Route rD, rA;
    int popD = 0, popA = 0;
    assert(findRoute(graph, 0, 99, opt, rD, &popD) == true);
    assert(findRouteAStar(graph, 0, 99, opt, rA, &popA) == true);
    assert(std::fabs(rD.distance - rA.distance) < 1e-3);
    assert(std::fabs(rD.exposure - rA.exposure) < 1e-3);
    assert(rA.lats.size() == rA.pm25s.size() && rA.lats.size() > 0);
}

int main() {
    // 1. 기본 단위 테스트 먼저 검증
    runUnitTests();
    std::printf("[기본 기능 검증 통과]\n\n");

    // 2. 실제 서울 데이터 불러오기
    App app;
    if (!app.init("data/processed/")) {
        std::printf("에러: data/processed/ 데이터를 읽지 못했습니다.\n");
        return 1;
    }

    // 격자 생성을 위해 기본 100m로 한 번 초기화 후 종로구·강남구 좌표 검색
    app.setCellMeters(100.0);
    if (!app.setPeriod(2025, 2)) {
        std::printf("에러: 2025년 2월 데이터가 없습니다.\n");
        return 1;
    }

    DynamicArray<Place> hitsStart, hitsGoal;
    app.search("종로구", 5, hitsStart);
    app.search("강남구", 5, hitsGoal);
    if (hitsStart.empty() || hitsGoal.empty()) {
        std::printf("에러: 종로구 또는 강남구 좌표를 찾지 못했습니다.\n");
        return 1;
    }

    double sLat = hitsStart[0].lat, sLng = hitsStart[0].lng;
    double gLat = hitsGoal[0].lat,  gLng = hitsGoal[0].lng;

    WeightOption opt;
    opt.alpha = 1.0;
    opt.beta = 0.0;

    double sizes[4] = {100.0, 50.0, 25.0, 10.0};

    std::printf("아래 표를 복사해서 docs/bench_route.md 에 붙여넣으세요!\n\n");
    std::printf("| 격자 | 칸 수 | 다익스트라(ms) | A*(ms) | 꺼낸 횟수 D | 꺼낸 횟수 A* | 메모리(MB) |\n");
    std::printf("|---|---|---|---|---|---|---|\n");

    for (int i = 0; i < 4; ++i) {
        double m = sizes[i];
        app.setCellMeters(m);
        app.setPeriod(2025, 2);

        const Grid& grid = app.grid();
        GridGraph graph(grid);
        int cellCount = graph.vertexCount();

        int startCell = nearestPassable(graph, graph.cellAt(sLat, sLng));
        int goalCell  = nearestPassable(graph, graph.cellAt(gLat, gLng));

        Route outD, outA;
        int popD = 0, popA = 0;

        // 다익스트라 시간 및 popCount 측정
        std::clock_t t1 = std::clock();
        bool okD = findRoute(graph, startCell, goalCell, opt, outD, &popD);
        std::clock_t t2 = std::clock();
        double msD = (double)(t2 - t1) * 1000.0 / CLOCKS_PER_SEC;

        // A* 시간 및 popCount 측정
        std::clock_t t3 = std::clock();
        bool okA = findRouteAStar(graph, startCell, goalCell, opt, outA, &popA);
        std::clock_t t4 = std::clock();
        double msA = (double)(t4 - t3) * 1000.0 / CLOCKS_PER_SEC;

        if (!okD || !okA) {
            std::printf("| %.0fm | %d | 경로없음 | 경로없음 | - | - | - |\n", m, cellCount);
            continue;
        }

        // dist(double 8바이트) + prev(int 4바이트) 메모리 계산 (MB 단위)
        double memMB = (double)cellCount * (sizeof(double) + sizeof(int)) / (1024.0 * 1024.0);

        std::printf("| %.0fm | %d | %.2f | %.2f | %d | %d | %.2f |\n",
                    m, cellCount, msD, msA, popD, popA, memMB);
    }

    return 0;
}