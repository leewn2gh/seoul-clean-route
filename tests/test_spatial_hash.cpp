// 공간 해시(src/data/SpatialHash) 테스트: nearby가 직접 거리로 찾은 결과와 같은지
#include <cstdio>
#include <cmath>
#include "data/SpatialHash.h"

static int failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  실패: %s (%s:%d)\n", #cond, __FILE__, __LINE__); ++failed; } } while (0)

int main() {
    std::printf("[공간 해시 테스트]\n");
    // 서울 범위에 관측소 400곳을 고르게 흩뿌림 (난수 대신 일정한 규칙)
    DynamicArray<Station> st;
    for (int i = 0; i < 400; ++i) {
        Station s;
        s.lat = 37.42 + 0.29 * ((i * 37) % 400) / 400.0;
        s.lng = 126.74 + 0.52 * ((i * 91) % 400) / 400.0;
        st.push_back(s);
    }
    DynamicArray<bool> use;   // 비어 있으면 전부 사용
    SpatialHash hash(1000.0);
    hash.build(st, use);
    CHECK(hash.count() == 400);

    // 버킷 1km, ring 2 → 가로·세로 ±2km 안의 관측소는 반드시 들어 있어야 함
    const double qLat = 37.5665, qLng = 126.9780;   // 서울시청
    DynamicArray<int> near;
    hash.nearby(qLat, qLng, 2, near);
    double mLat = 111000.0, mLng = 111000.0 * std::cos(37.41 * 3.14159265358979 / 180.0);
    int inside = 0, found = 0;
    for (int i = 0; i < st.size(); ++i) {
        double dy = std::fabs(st[i].lat - qLat) * mLat, dx = std::fabs(st[i].lng - qLng) * mLng;
        if (dy > 2000 || dx > 2000) continue;   // ±2km 사각형 안
        ++inside;
        for (int k = 0; k < near.size(); ++k) if (near[k] == i) { ++found; break; }
    }
    CHECK(inside > 0 && found == inside);
    CHECK(near.size() < st.size());   // 전체를 다 돌려주지는 않음

    // use로 일부만 넣기
    DynamicArray<bool> half;
    for (int i = 0; i < st.size(); ++i) half.push_back(i % 2 == 0);
    SpatialHash evenOnly(1000.0);
    evenOnly.build(st, half);
    CHECK(evenOnly.count() == 200);
    evenOnly.nearby(qLat, qLng, 30, near);   // 충분히 넓게 → 넣은 200곳 전부
    bool allEven = true;
    for (int k = 0; k < near.size(); ++k) if (near[k] % 2) allEven = false;
    CHECK(near.size() == 200 && allEven);

    if (failed == 0) std::printf("  모두 통과\n");
    return failed == 0 ? 0 : 1;
}
