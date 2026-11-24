// 담당: 김재겸
#include "graph/RoadMask.h"
#include <cmath>
#include <cstdlib>
#include <fstream>

namespace {
// 위경도 → 칸 번호 (격자 밖이면 -1). GridGraph::cellAt과 같은 규칙
int cellOf(const Grid& g, double lat, double lng) {
    int r = (int)std::floor((lat - g.minLat) / g.cellLat);
    int c = (int)std::floor((lng - g.minLng) / g.cellLng);
    if (r < 0 || r >= g.rows || c < 0 || c >= g.cols) return -1;
    return r * g.cols + c;
}

// "lat1,lng1,lat2,lng2" 한 줄을 숫자 4개로. 형식이 틀리면 false (머리줄 포함)
bool parseLine(const std::string& line, double v[4]) {
    const char* p = line.c_str();
    for (int i = 0; i < 4; ++i) {
        char* end = nullptr;
        v[i] = std::strtod(p, &end);
        if (end == p) return false;               // 숫자가 아님
        if (i < 3) {
            if (*end != ',') return false;
            p = end + 1;
        }
    }
    return true;
}
}  // namespace

bool applyRoads(const std::string& path, Grid& grid) {
    if (grid.rows <= 0 || grid.cols <= 0 || grid.cellLat <= 0 || grid.cellLng <= 0) return false;
    std::ifstream in(path);
    if (!in) return false;

    int cellCount = grid.rows * grid.cols;
    DynamicArray<bool> mask;
    mask.resize(cellCount, false);

    std::string line;
    int segments = 0;
    while (std::getline(in, line)) {
        double v[4];
        if (!parseLine(line, v)) continue;
        ++segments;

        // 위도·경도 방향 각각 반 칸보다 촘촘하게 점을 찍음 → 선분이 지나는 칸을 빠뜨리지 않음
        double dLat = v[2] - v[0], dLng = v[3] - v[1];
        double steps = std::fmax(std::fabs(dLat) / (grid.cellLat / 2.0), std::fabs(dLng) / (grid.cellLng / 2.0));
        int n = (int)std::ceil(steps);
        if (n < 1) n = 1;
        for (int i = 0; i <= n; ++i) {
            double t = (double)i / n;
            int cell = cellOf(grid, v[0] + dLat * t, v[1] + dLng * t);
            if (cell >= 0) mask[cell] = true;
        }
    }
    if (segments == 0) return false;

    grid.passable.swap(mask);
    return true;
}
