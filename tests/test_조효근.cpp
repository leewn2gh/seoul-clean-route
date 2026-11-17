// 담당: 조효근
// Router.h / Router.cpp 수정 사항 검증용 테스트 코드
#include <cstdio>
#include <cmath>
#include <ctime>
#include <cassert>

#include "route/Router.h"
#include "route/MinHeap.h"
#include "route/Stack.h"
#include "route/Queue.h"
#include "graph/GridGraph.h"

int main() {
    printf("=== Router 기능 및 A* 알고리즘 동작 테스트 시작 ===\n\n");

    // 1. 테스트용 10x10 가짜 서울 격자(Grid) 만들기
    Grid grid;
    grid.rows = 10;
    grid.cols = 10;
    grid.minLat = 37.5000;
    grid.minLng = 127.0000;
    grid.cellLat = 0.001; // 약 111m
    grid.cellLng = 0.001; // 약 88m

    int totalCells = grid.rows * grid.cols;
    grid.pm25.resize(totalCells, 20.0);       // 기본 미세먼지 20 (좋음)
    grid.temp.resize(totalCells, 15.0);       // 기본 기온 15도
    grid.passable.resize(totalCells, true);   // 일단 모두 통행 가능으로 설정

    // [테스트 준비] 가운데(3~6행, 3~6열)에 거대한 '건물(통행 불가)' 배치
    for (int r = 3; r <= 6; ++r) {
        for (int c = 3; c <= 6; ++c) {
            grid.passable[r * grid.cols + c] = false;
        }
    }

    // [테스트 준비] 위쪽 길(1행)은 미세먼지가 매우 나쁨(150.0)으로 설정
    for (int c = 0; c < grid.cols; ++c) {
        grid.pm25[1 * grid.cols + c] = 150.0;
    }

    GridGraph graph(grid);

    // ---------------------------------------------------------
    // 테스트 1: nearestPassable (건물 안 좌표 보정 & maxRadius 제한)
    // ---------------------------------------------------------
    int insideBuilding = 4 * 10 + 4; // (4, 4)는 건물 한가운데 (통행 불가)
    
    // 반경 1칸 안에는 건물이 꽉 차 있어서 못 찾아야 함 -> -1 반환 기대
    int failSearch = nearestPassable(graph, insideBuilding, 1);
    printf("[테스트 1-1] 반경 1칸 제한 탐색 결과: %d (기대값: -1)\n", failSearch);
    assert(failSearch == -1);

    // 기본 반경(200칸)으로는 건물 밖 가장 가까운 길(예: (2,4) 등)을 찾아야 함
    int nearCell = nearestPassable(graph, insideBuilding);
    printf("[테스트 1-2] 기본 반경(200) 탐색 결과: %d번 칸 (통행가능여부: %d)\n", 
           nearCell, grid.passable[nearCell]);
    assert(nearCell != -1 && grid.passable[nearCell] == true);


    // ---------------------------------------------------------
    // 테스트 2 & 3: 다익스트라 vs A* 비용 일치 여부 & popCount 비교
    // ---------------------------------------------------------
    int start = 0 * 10 + 0; // 왼쪽 아래 (0, 0)
    int goal  = 9 * 10 + 9; // 오른쪽 위 (9, 9)

    WeightOption opt;
    opt.alpha = 1.0; // 미세먼지 회피 반영
    opt.beta = 0.0;

    Route routeDijkstra, routeAStar;
    int dijkstraPops = 0;
    int astarPops = 0;

    clock_t t1 = clock();
    bool okD = findRoute(graph, start, goal, opt, routeDijkstra, &dijkstraPops);
    clock_t t2 = clock();

    clock_t t3 = clock();
    bool okA = findRouteAStar(graph, start, goal, opt, routeAStar, &astarPops);
    clock_t t4 = clock();

    assert(okD == true && okA == true);

    printf("\n[테스트 2] 다익스트라 vs A* 결과 비교:\n");
    printf(" - 다익스트라: 거리 = %.2fm, 노출량 = %.2f, 꺼낸 횟수(pop) = %d번, 시간 = %.3fms\n",
           routeDijkstra.distance, routeDijkstra.exposure, dijkstraPops,
           (double)(t2 - t1) / CLOCKS_PER_SEC * 1000.0);
    printf(" - A* 탐색   : 거리 = %.2fm, 노출량 = %.2f, 꺼낸 횟수(pop) = %d번, 시간 = %.3fms\n",
           routeAStar.distance, routeAStar.exposure, astarPops,
           (double)(t4 - t3) / CLOCKS_PER_SEC * 1000.0);

    // 두 알고리즘의 총 거리와 노출량이 오차 범위(0.001) 내에서 완전히 같은지 검증
    assert(std::fabs(routeDijkstra.distance - routeAStar.distance) < 1e-3);
    assert(std::fabs(routeDijkstra.exposure - routeAStar.exposure) < 1e-3);
    printf(" -> 검증 성공! 다익스트라와 A*의 경로 비용이 100%% 일치합니다.\n");


    // ---------------------------------------------------------
    // 테스트 4: out.pm25s 배열에 칸별 미세먼지 값이 잘 들어갔는지 확인
    // ---------------------------------------------------------
    printf("\n[테스트 3] 경로 복원 및 pm25s 배열 확인:\n");
    printf(" - 경로 칸 수: lats=%d개, lngs=%d개, pm25s=%d개\n",
           routeAStar.lats.size(), routeAStar.lngs.size(), routeAStar.pm25s.size());
    
    assert(routeAStar.lats.size() == routeAStar.pm25s.size());
    assert(routeAStar.lats.size() > 0);
    printf(" - 출발지 PM2.5 = %.1f, 도착지 PM2.5 = %.1f\n",
           routeAStar.pm25s[0], routeAStar.pm25s[routeAStar.pm25s.size() - 1]);

    printf("\n=== 모든 테스트를 에러 없이 완벽하게 통과했습니다! ===\n");
    return 0;
}