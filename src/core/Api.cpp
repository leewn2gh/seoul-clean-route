// 담당: 이준호
// 웹(WebAssembly)에서 부르는 함수. 결과는 JSON 문자열로 돌려줍니다.
// JavaScript 쪽: Module.ccall('api_find_route', 'string', [...], [...])
// 돌려주는 문자열은 다음 호출 전까지만 유효합니다. (JavaScript가 바로 복사해 감)
#include <cstdio>
#include <string>
#include "core/App.h"
#include "common/Export.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define API extern "C" EMSCRIPTEN_KEEPALIVE
static const char* DATA_DIR = "/data/";
#else
#define API extern "C"
static const char* DATA_DIR = "data/processed/";
#endif

static App* app = nullptr;
static std::string result;   // 마지막으로 돌려준 문자열

static const char* give(const std::string& s) {
    result = s;
    return result.c_str();
}

static std::string error(const char* message) {
    return std::string("{\"ok\":false,\"error\":\"") + message + "\"}";
}

// 데이터 읽기. 페이지가 열릴 때 한 번 호출. dataDir이 비어 있으면 기본 위치
API const char* api_init(const char* dataDir) {
    delete app;
    app = new App();
    std::string dir = (dataDir && dataDir[0]) ? dataDir : DATA_DIR;
    if (!app->init(dir)) return give(error("데이터 파일을 읽지 못했습니다"));
    char buf[64];
    std::snprintf(buf, sizeof(buf), "{\"ok\":true,\"stations\":%d}", app->stationCount());
    return give(buf);
}

// 기간 선택: "2025-02" 형식. 격자를 다시 만들고 격자 크기와 미세먼지 범위를 돌려줌
API const char* api_set_period(const char* period) {
    if (!app) return give(error("api_init을 먼저 호출하세요"));
    int year = 0, month = 0;
    if (!period || std::sscanf(period, "%d-%d", &year, &month) != 2) return give(error("기간 형식은 2025-02"));
    if (!app->setPeriod(year, month)) return give(error("그 기간의 데이터가 없습니다"));
    const Grid& g = app->grid();
    char buf[128];
    std::snprintf(buf, sizeof(buf), "{\"ok\":true,\"rows\":%d,\"cols\":%d,\"cellMeters\":%.0f}",
                  g.rows, g.cols, app->cellMeters());
    return give(buf);
}

// 특정 시각 보기: date "2025-02-15", hour 0~23, ahead 0(그 시각) · 1 · 2(바람으로 추정한 몇 시간 뒤)
API const char* api_set_time(const char* date, int hour, int ahead) {
    if (!app) return give(error("api_init을 먼저 호출하세요"));
    int year = 0, month = 0, day = 0;
    if (!date || std::sscanf(date, "%d-%d-%d", &year, &month, &day) != 3) return give(error("날짜 형식은 2025-02-15"));
    if (!app->setTime(year, month, day, hour, ahead))
        return give(error(ahead > 0 ? "그 시각의 측정값이나 바람 데이터가 없습니다" : "그 시각의 측정값이 없습니다"));
    const Grid& g = app->grid();
    char buf[160];
    std::snprintf(buf, sizeof(buf), "{\"ok\":true,\"rows\":%d,\"cols\":%d,\"cellMeters\":%.0f,\"winds\":%d}",
                  g.rows, g.cols, app->cellMeters(), app->windCount());
    return give(buf);
}

// 지도 색칠용 칸별 미세먼지. step×step 칸을 하나로 묶어서 돌려줌
API const char* api_get_grid(int step) {
    if (!app) return give("null");
    return give(gridJSON(app->grid(), step));
}

// 출발·도착 위경도와 미세먼지 반영 비중으로 최적 경로 + 최단 경로 (GeoJSON)
API const char* api_find_route(double sLat, double sLng, double gLat, double gLng, double alpha) {
    if (!app) return give(error("api_init을 먼저 호출하세요"));
    Route best, shortest;
    if (!app->findRoute(sLat, sLng, gLat, gLng, alpha, best, shortest))
        return give(error("경로를 찾지 못했습니다 (서울 격자 밖이거나 길이 막힘)"));
    return give(routesGeoJSON(best, shortest));
}

// Top / Worst k 지역
API const char* api_get_ranking(int k) {
    if (!app) return give("{\"top\":[],\"worst\":[]}");
    DynamicArray<RankItem> top, worst;
    app->ranking(k, top, worst);
    return give(rankingJSON(top, worst));
}

// 지역명 자동완성 (앞 글자로 최대 limit개)
API const char* api_search(const char* prefix, int limit) {
    if (!app || !prefix) return give("[]");
    DynamicArray<Place> out;
    app->search(prefix, limit, out);
    return give(placesJSON(out));
}
