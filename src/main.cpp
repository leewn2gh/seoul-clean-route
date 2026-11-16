// 담당: 이준호
// 콘솔 실행. 웹과 같은 App(src/app)을 써서 결과를 출력하고 web/data에 저장합니다.
// 실행: ./build/app [출발지] [목적지] [alpha] [기간]
//   예) ./build/app 종로 강남 1 2025-05   (이름은 앞 글자만 써도 됨, alpha 생략 1, 기간 생략 2025-02)
// 결과: web/data/grid.json, routes.geojson, ranking.json → 웹 화면이 WebAssembly 없이도 이 파일로 표시
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "core/App.h"
#include "common/Export.h"

static const char* DATA_DIR = "data/processed/";
static const int GRID_STEP = 1;   // 웹 표시용으로 묶을 칸 수 (1 = 묶지 않음)

static bool findPlace(const App& app, const char* name, Place& out) {
    DynamicArray<Place> hits;
    app.search(name, 5, hits);
    if (hits.empty()) {
        std::printf("  '%s' 에 해당하는 지역이 없습니다.\n", name);
        return false;
    }
    out = hits[0];
    return true;
}

static void printRoute(const char* title, const Route& r) {
    std::printf("  %s: 거리 %.0f m, 약 %.0f분, 미세먼지 노출량 %.1f, 칸 %d개\n",
                title, r.distance, r.minutes, r.exposure, r.lats.size());
}

static void printRanking(const char* title, const DynamicArray<RankItem>& list) {
    std::printf("  %s:", title);
    for (int i = 0; i < list.size(); ++i) std::printf(" %d.%s(%.1f)", i + 1, list[i].name.c_str(), list[i].score);
    std::printf("\n");
}

static double msSince(std::clock_t t) { return (std::clock() - t) * 1000.0 / CLOCKS_PER_SEC; }

int main(int argc, char** argv) {
    const char* startName = argc > 1 ? argv[1] : "종로구";
    const char* goalName  = argc > 2 ? argv[2] : "강남구";
    double alpha          = argc > 3 ? std::atof(argv[3]) : 1.0;
    const char* period    = argc > 4 ? argv[4] : "2025-02";
    int year = 2025, month = 2;
    if (std::sscanf(period, "%d-%d", &year, &month) != 2) { std::printf("기간 형식은 2025-02\n"); return 1; }

    App app;
    std::clock_t t = std::clock();
    std::printf("[1] 데이터 읽기\n");
    if (!app.init(DATA_DIR)) { std::printf("  %s 의 데이터 파일을 읽지 못했습니다.\n", DATA_DIR); return 1; }
    std::printf("  관측소 %d곳 (%.0f ms)\n", app.stationCount(), msSince(t));

    std::printf("[2] 기간 %d년 %d월, 격자 생성 (%.0fm)\n", year, month, app.cellMeters());
    t = std::clock();
    if (!app.setPeriod(year, month)) { std::printf("  그 기간의 데이터가 없습니다.\n"); return 1; }
    const Grid& grid = app.grid();
    std::printf("  %d × %d = %d칸 (%.0f ms)\n", grid.rows, grid.cols, grid.rows * grid.cols, msSince(t));

    std::printf("[3] 지역명 검색\n");
    Place s, g;
    if (!findPlace(app, startName, s) || !findPlace(app, goalName, g)) return 1;
    std::printf("  출발 %s → 도착 %s\n", s.name.c_str(), g.name.c_str());

    std::printf("[4] 경로 탐색\n");
    t = std::clock();
    Route best, shortest;
    if (!app.findRoute(s.lat, s.lng, g.lat, g.lng, alpha, best, shortest)) { std::printf("  경로를 찾지 못했습니다.\n"); return 1; }
    printRoute("최적 경로", best);
    printRoute("최단 경로", shortest);
    if (shortest.exposure > 0) {
        double cut = (shortest.exposure - best.exposure) / shortest.exposure * 100.0;
        double more = shortest.distance > 0 ? (best.distance - shortest.distance) / shortest.distance * 100.0 : 0.0;
        std::printf("  → 노출량 %.1f%% 감소, 거리 %.1f%% 증가 (%.0f ms)\n", cut, more, msSince(t));
    }

    std::printf("[5] 지역 순위\n");
    DynamicArray<RankItem> top, worst;
    app.ranking(5, top, worst);
    printRanking("Top 5  ", top);
    printRanking("Worst 5", worst);

    std::printf("[6] 웹 지도 파일 저장\n");
    bool ok = writeTextFile("web/data/grid.json", gridJSON(grid, GRID_STEP))
           && writeRoutesGeoJSON(best, shortest, "web/data/routes.geojson")
           && writeRankingJSON(top, worst, "web/data/ranking.json");
    std::printf(ok ? "  web/data 에 저장 완료\n" : "  저장 실패 (web/data 폴더가 있는지 확인)\n");
    return ok ? 0 : 1;
}
