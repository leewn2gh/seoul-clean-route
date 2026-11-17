// 바람장·미세먼지 이동 테스트 (make test 로 실행)
#include <cstdio>
#include <cmath>
#include "graph/WindField.h"
#include "common/TestData.h"

static int failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  실패: %s (%s:%d)\n", #cond, __FILE__, __LINE__); ++failed; } } while (0)

// 격자 한가운데에 관측소 하나, 풍향 dir(불어오는 방향), 풍속 speed
static DynamicArray<Station> oneStation(const Grid& g, double dir, double speed) {
    Station s;
    s.lat = g.minLat + g.rows * g.cellLat / 2;
    s.lng = g.minLng + g.cols * g.cellLng / 2;
    s.windDir = dir;
    s.windSpeed = speed;
    DynamicArray<Station> list;
    list.push_back(s);
    return list;
}

int main() {
    std::printf("[WindField 테스트]\n");
    Grid g = makeTestGrid(40, 40);
    for (int k = 0; k < g.pm25.size(); ++k) g.pm25[k] = 10.0;
    int center = 20 * 40 + 10;   // 가운데 줄, 서쪽 칸
    g.pm25[center] = 100.0;

    // 바람 관측소가 없으면 그대로
    DynamicArray<Station> none;
    Grid out;
    predictPm(g, none, 60, out);
    CHECK(out.pm25[center] == 100.0);

    // 서풍(270°, 서 → 동) 2 m/s: 바람장은 동쪽(u > 0)
    DynamicArray<Station> west = oneStation(g, 270.0, 2.0);
    WindField wind;
    CHECK(wind.build(g, west));
    CHECK(wind.stationCount() == 1);
    double u, v;
    wind.windAt(20 * 40 + 20, u, v);
    CHECK(std::fabs(u - 2.0) < 0.2 && std::fabs(v) < 0.2);
    double dirErr, spErr;
    wind.fitError(dirErr, spErr);
    CHECK(dirErr < 5.0 && spErr < 0.2);

    // 10분 × 2 m/s = 1.2km ≈ 동쪽으로 약 12칸 이동 → 가장 진한 칸이 동쪽으로 옮겨감
    predictPm(g, west, 10, out);
    int best = 0;
    for (int k = 0; k < out.pm25.size(); ++k) if (out.pm25[k] > out.pm25[best]) best = k;
    int col = best % 40, row = best / 40;
    std::printf("  최고 농도 칸: (%d, %d) → (%d, %d)\n", 20, 10, row, col);
    CHECK(row == 20 && col >= 19 && col <= 25);
    CHECK(out.pm25[center] < 20.0);

    // 질량 보존 근사 (경계 유출입 없는 위치): 합이 크게 변하지 않음
    double s0 = 0, s1 = 0;
    for (int k = 0; k < g.pm25.size(); ++k) { s0 += g.pm25[k]; s1 += out.pm25[k]; }
    CHECK(std::fabs(s1 - s0) / s0 < 0.02);

    // 값 없는 칸(음수)은 그대로
    g.pm25[0] = -1.0;
    predictPm(g, west, 30, out);
    CHECK(out.pm25[0] == -1.0);

    if (failed == 0) std::printf("  모두 통과\n");
    return failed == 0 ? 0 : 1;
}
