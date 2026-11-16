// 담당: 이준호
#include "common/Export.h"
#include <fstream>
#include <cstdio>
#include <sstream>

// 소수점 자릿수를 고정해서 숫자를 문자열로 (6자리 ≈ 0.1m)
static std::string num(double v, int digits = 6) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", digits, v);
    return buf;
}

// JSON 문자열 안에 넣을 수 있게 따옴표·역슬래시 처리
static std::string quote(const std::string& s) {
    std::string r = "\"";
    for (int i = 0; i < (int)s.size(); ++i) {
        char c = s[i];
        if (c == '"' || c == '\\') r += '\\';
        r += c;
    }
    return r + "\"";
}

bool writeGridGeoJSON(const Grid& grid, const std::string& path, int step) {
    if (step < 1) step = 1;
    if (grid.rows <= 0 || grid.cols <= 0 || grid.pm25.size() < grid.rows * grid.cols) return false;
    std::ofstream f(path);
    if (!f) return false;

    f << "{\"type\":\"FeatureCollection\",\"features\":[\n";
    bool first = true;
    for (int r = 0; r < grid.rows; r += step) {
        for (int c = 0; c < grid.cols; c += step) {
            double sum = 0.0;
            int cnt = 0;
            for (int dr = 0; dr < step && r + dr < grid.rows; ++dr) {
                for (int dc = 0; dc < step && c + dc < grid.cols; ++dc) {
                    int k = (r + dr) * grid.cols + (c + dc);
                    bool ok = grid.passable.size() <= k || grid.passable[k];
                    if (ok && grid.pm25[k] >= 0) { sum += grid.pm25[k]; ++cnt; }
                }
            }
            if (cnt == 0) continue;

            int r2 = (r + step < grid.rows) ? r + step : grid.rows;
            int c2 = (c + step < grid.cols) ? c + step : grid.cols;
            double lat1 = grid.minLat + r * grid.cellLat, lat2 = grid.minLat + r2 * grid.cellLat;
            double lng1 = grid.minLng + c * grid.cellLng, lng2 = grid.minLng + c2 * grid.cellLng;

            if (!first) f << ",\n";
            first = false;
            // GeoJSON 좌표 순서는 [경도, 위도]
            f << "{\"type\":\"Feature\",\"properties\":{\"pm25\":" << num(sum / cnt, 1) << "},"
              << "\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[["
              << "[" << num(lng1) << "," << num(lat1) << "],"
              << "[" << num(lng2) << "," << num(lat1) << "],"
              << "[" << num(lng2) << "," << num(lat2) << "],"
              << "[" << num(lng1) << "," << num(lat2) << "],"
              << "[" << num(lng1) << "," << num(lat1) << "]]]}}";
        }
    }
    f << "\n]}\n";
    return (bool)f;
}

// step×step 칸 묶음의 평균. 통행 가능하고 값이 있는 칸이 없으면 -1
static double blockMean(const Grid& grid, int r, int c, int step) {
    double sum = 0.0;
    int cnt = 0;
    for (int dr = 0; dr < step && r + dr < grid.rows; ++dr) {
        for (int dc = 0; dc < step && c + dc < grid.cols; ++dc) {
            int k = (r + dr) * grid.cols + (c + dc);
            bool ok = grid.passable.size() <= k || grid.passable[k];
            if (ok && grid.pm25[k] >= 0) { sum += grid.pm25[k]; ++cnt; }
        }
    }
    return cnt > 0 ? sum / cnt : -1.0;
}

std::string gridJSON(const Grid& grid, int step) {
    if (step < 1) step = 1;
    if (grid.rows <= 0 || grid.cols <= 0 || grid.pm25.size() < grid.rows * grid.cols) return "null";
    int rows = (grid.rows + step - 1) / step, cols = (grid.cols + step - 1) / step;

    std::ostringstream vals;
    double lo = 1e18, hi = -1e18;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            double v = blockMean(grid, r * step, c * step, step);
            if (r > 0 || c > 0) vals << ",";
            if (v < 0) { vals << "null"; continue; }
            vals << num(v, 1);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    }
    if (lo > hi) { lo = 0; hi = 0; }

    std::ostringstream o;
    o << "{\"rows\":" << rows << ",\"cols\":" << cols
      << ",\"minLat\":" << num(grid.minLat, 7) << ",\"minLng\":" << num(grid.minLng, 7)
      << ",\"cellLat\":" << num(grid.cellLat * step, 9) << ",\"cellLng\":" << num(grid.cellLng * step, 9)
      << ",\"step\":" << step << ",\"min\":" << num(lo, 1) << ",\"max\":" << num(hi, 1)
      << ",\"values\":[" << vals.str() << "]}";
    return o.str();
}

static void routeFeature(std::ostringstream& o, const Route& r, const char* type) {
    int n = r.lats.size() < r.lngs.size() ? r.lats.size() : r.lngs.size();
    o << "{\"type\":\"Feature\",\"properties\":{"
      << "\"type\":\"" << type << "\","
      << "\"distance\":" << num(r.distance, 1) << ","
      << "\"exposure\":" << num(r.exposure, 1) << ","
      << "\"minutes\":" << num(r.minutes, 1);
    if (r.pm25s.size() == n && n > 0) {          // 칸별 미세먼지가 있으면 함께
        o << ",\"pm25s\":[";
        for (int i = 0; i < n; ++i) o << (i ? "," : "") << num(r.pm25s[i], 1);
        o << "]";
    }
    o << "},\"geometry\":{\"type\":\"LineString\",\"coordinates\":[";
    for (int i = 0; i < n; ++i) {
        if (i > 0) o << ",";
        o << "[" << num(r.lngs[i]) << "," << num(r.lats[i]) << "]";   // GeoJSON은 [경도, 위도]
    }
    o << "]}}";
}

std::string routesGeoJSON(const Route& best, const Route& shortest) {
    std::ostringstream o;
    o << "{\"type\":\"FeatureCollection\",\"features\":[\n";
    routeFeature(o, best, "best");
    o << ",\n";
    routeFeature(o, shortest, "shortest");
    o << "\n]}\n";
    return o.str();
}

static void rankList(std::ostringstream& o, const DynamicArray<RankItem>& list) {
    o << "[";
    for (int i = 0; i < list.size(); ++i) {
        if (i > 0) o << ",";
        o << "{\"name\":" << quote(list[i].name) << ",\"score\":" << num(list[i].score, 1) << "}";
    }
    o << "]";
}

std::string rankingJSON(const DynamicArray<RankItem>& top, const DynamicArray<RankItem>& worst) {
    std::ostringstream o;
    o << "{\"top\":";
    rankList(o, top);
    o << ",\"worst\":";
    rankList(o, worst);
    o << "}\n";
    return o.str();
}

std::string placesJSON(const DynamicArray<Place>& places) {
    std::ostringstream o;
    o << "[";
    for (int i = 0; i < places.size(); ++i) {
        if (i > 0) o << ",";
        o << "{\"name\":" << quote(places[i].name) << ",\"lat\":" << num(places[i].lat)
          << ",\"lng\":" << num(places[i].lng) << "}";
    }
    o << "]";
    return o.str();
}

bool writeTextFile(const std::string& path, const std::string& text) {
    std::ofstream f(path);
    if (!f) return false;
    f << text;
    return (bool)f;
}

bool writeRoutesGeoJSON(const Route& best, const Route& shortest, const std::string& path) {
    return writeTextFile(path, routesGeoJSON(best, shortest));
}

bool writeRankingJSON(const DynamicArray<RankItem>& top, const DynamicArray<RankItem>& worst,
                      const std::string& path) {
    return writeTextFile(path, rankingJSON(top, worst));
}
