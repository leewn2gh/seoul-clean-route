// 웹 API(src/core/Api.cpp) 테스트. 레포 맨 위 폴더에서 make test 로 실행 (data/processed 필요)
#include <cstdio>
#include <string>

extern "C" {
const char* api_init(const char* dataDir);
const char* api_set_period(const char* period);
const char* api_get_grid(int step);
const char* api_find_route(double sLat, double sLng, double gLat, double gLng, double alpha);
const char* api_get_ranking(int k);
const char* api_search(const char* prefix, int limit);
}

static int failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  실패: %s (%s:%d)\n", #cond, __FILE__, __LINE__); ++failed; } } while (0)

static bool has(const std::string& s, const char* word) { return s.find(word) != std::string::npos; }

int main() {
    std::printf("[웹 API 테스트]\n");
    CHECK(has(api_set_period("2025-02"), "\"ok\":false"));     // init 전에는 실패
    CHECK(has(api_init("data/processed/"), "\"ok\":true"));
    CHECK(has(api_set_period("2025-13"), "\"ok\":false"));      // 없는 달
    CHECK(has(api_set_period("abc"), "\"ok\":false"));
    for (const char* p : {"2025-02", "2025-05", "2025-08", "2025-11"}) {
        std::string r = api_set_period(p);
        CHECK(has(r, "\"ok\":true") && has(r, "\"rows\":345") && has(r, "\"cols\":477"));
    }
    CHECK(has(api_set_period("2025-02"), "\"ok\":true"));

    std::string grid = api_get_grid(5);
    CHECK(has(grid, "\"rows\":69") && has(grid, "\"cols\":96") && has(grid, "\"values\":["));

    // 종로구 측정소 → 강남구 측정소 (중간발표 예시와 같은 결과)
    std::string route = api_find_route(37.571973, 127.005082, 37.518147, 127.047296, 1.0);
    CHECK(has(route, "\"type\":\"best\"") && has(route, "\"type\":\"shortest\""));
    CHECK(has(route, "\"exposure\":149693.0") && has(route, "\"exposure\":154229.9"));
    CHECK(has(route, "\"pm25s\":["));
    CHECK(has(api_find_route(36.0, 127.0, 37.5, 127.0, 1.0), "\"ok\":false"));   // 서울 밖

    std::string rank = api_get_ranking(5);
    CHECK(has(rank, "\"top\":[{\"name\":\"은평구\""));

    CHECK(std::string(api_search("강", 10)).find("강남구") != std::string::npos);
    CHECK(std::string(api_search("없는곳", 10)) == "[]");

    if (failed == 0) std::printf("  모두 통과\n");
    return failed == 0 ? 0 : 1;
}
