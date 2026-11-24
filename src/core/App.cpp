// 담당: 이준호
#include "core/App.h"
#include "route/Router.h"
#include "graph/RoadMask.h"
#include <cstdio>
#include <fstream>

static const int MONTHS[] = {2, 5, 8, 11};   // 분석 기간 (2025년)

App::App() {}

App::~App() {
    delete graph_;
    delete stationGraph_;
}

bool App::init(const std::string& dataDir) {
    dataDir_ = dataDir;
    if (!loader_.loadStations(dataDir + "stations.csv", StationType::DISTRICT)) return false;
    bool simple = loader_.loadStations(dataDir + "simple_stations.csv", StationType::SIMPLE);
    bool sdot = loader_.loadStations(dataDir + "sdot_stations.csv", StationType::SDOT);

    int loaded = 0;
    for (int i = 0; i < 4; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "_2025_%02d.csv", MONTHS[i]);
        if (loader_.loadDistrict(dataDir + "air" + name)) ++loaded;
        if (simple) loader_.loadSimple(dataDir + "simple" + name);
        if (sdot) loader_.loadSdot(dataDir + "sdot" + name);   // 웹에는 S-DoT를 싣지 않음 (용량)
    }

    // 지역명 검색: 자치구 측정소 이름을 트라이에 넣음 (간이·S-DoT 번호는 넣지 않음)
    const DynamicArray<Station>& all = loader_.stations();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i].type != StationType::DISTRICT) continue;
        Place p;
        p.name = all[i].name;
        p.lat = all[i].lat;
        p.lng = all[i].lng;
        trie_.insert(p);
    }
    return loaded > 0;
}

int App::countOf(StationType type) const {
    int n = 0;
    const DynamicArray<Station>& all = loader_.stations();
    for (int i = 0; i < all.size(); ++i) if (all[i].type == type) ++n;
    return n;
}

double App::passableRatio() const {
    if (grid_.passable.empty()) return 0.0;
    int n = 0;
    for (int i = 0; i < grid_.passable.size(); ++i) if (grid_.passable[i]) ++n;
    return (double)n / grid_.passable.size();
}

// 보행로 칸 만들기: osm_walk.csv(콘솔) → 없으면 passable_{m}m.bin(웹). 둘 다 없으면 모든 칸 통행 가능
// passable 파일 형식: 첫 줄 "rows cols", 그다음 칸마다 1비트 (행 순서, 8칸 = 1바이트)
bool App::loadRoads() {
    roadMask_.clear();
    Grid tmp = grid_;
    if (applyRoads(dataDir_ + "osm_walk.csv", tmp)) {
        roadMask_ = tmp.passable;
        return true;
    }
    char name[48];
    std::snprintf(name, sizeof(name), "passable_%.0fm.bin", cellMeters_);
    std::ifstream in(dataDir_ + name, std::ios::binary);
    int rows = 0, cols = 0;
    if (!(in >> rows >> cols) || rows != grid_.rows || cols != grid_.cols) return false;
    in.get();   // 줄바꿈
    int n = rows * cols;
    roadMask_.resize(n, false);
    for (int i = 0; i < n; i += 8) {
        int byte = in.get();
        if (byte < 0) { roadMask_.clear(); return false; }
        for (int b = 0; b < 8 && i + b < n; ++b) roadMask_[i + b] = (byte >> b) & 1;
    }
    return true;
}

bool App::savePassable(const std::string& path) const {
    if (grid_.passable.empty()) return false;
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << grid_.rows << " " << grid_.cols << "\n";
    int n = grid_.rows * grid_.cols;
    for (int i = 0; i < n; i += 8) {
        int byte = 0;
        for (int b = 0; b < 8 && i + b < n; ++b) if (grid_.passable[i + b]) byte |= 1 << b;
        out.put((char)byte);
    }
    return (bool)out;
}

bool App::setPeriod(int year, int month) {
    DateTime from; from.year = year; from.month = month; from.day = 1;  from.hour = 0;
    DateTime to;   to.year   = year; to.month   = month; to.day   = 31; to.hour   = 23;
    DynamicArray<Station> next;
    loader_.averageStations(from, to, next);

    bool any = false;
    for (int i = 0; i < next.size(); ++i) {
        if (next[i].type == StationType::DISTRICT && next[i].pm25 >= 0) { any = true; break; }
    }
    if (!any) return false;   // 그 달 데이터가 없음

    delete graph_;        graph_ = nullptr;
    delete stationGraph_; stationGraph_ = nullptr;

    stations_ = next;
    districts_.clear();
    pmStations_.clear();
    winds_.clear();
    for (int i = 0; i < stations_.size(); ++i) {
        const Station& s = stations_[i];
        if (s.type == StationType::DISTRICT) districts_.push_back(s);
        if (s.type != StationType::SDOT && s.pm25 >= 0) pmStations_.push_back(s);
        if (s.type == StationType::SDOT && s.windDir >= 0 && s.windSpeed >= 0) winds_.push_back(s);
    }

    grid_ = Grid();
    buildGrid(stations_, cellMeters_, grid_);
    // 보행로: 격자 크기가 같으면 처음 한 번만 만들어 두고 계속 씀
    if (roadMask_.size() != grid_.rows * grid_.cols) loadRoads();
    if (roadMask_.size() == grid_.rows * grid_.cols) grid_.passable = roadMask_;

    graph_ = new GridGraph(grid_);
    stationGraph_ = new StationGraph(districts_, 3);
    return grid_.rows > 0 && grid_.cols > 0;
}

void App::forecastByWind(int hours, DynamicArray<RankItem>& out) const {
    out.clear();
    if (pmStations_.empty() || winds_.empty()) return;
    StationGraph g(pmStations_, winds_, 5);
    DynamicArray<double> pm;
    g.estimateByWind(hours, pm);
    for (int i = 0; i < pmStations_.size(); ++i) {
        RankItem r;
        r.name = pmStations_[i].name;
        r.score = pm[i];
        out.push_back(r);
    }
}

bool App::findRoute(double sLat, double sLng, double gLat, double gLng, double alpha,
                    Route& best, Route& shortest) const {
    if (graph_ == nullptr) return false;
    int start = graph_->cellAt(sLat, sLng);
    int goal = graph_->cellAt(gLat, gLng);
    if (start < 0 || goal < 0) return false;
    start = nearestPassable(*graph_, start);
    goal = nearestPassable(*graph_, goal);
    if (start < 0 || goal < 0) return false;

    // 최적 경로와 최단 경로 모두 A* (다익스트라와 비용은 같고 꺼내는 횟수가 적음, 조효근 측정)
    WeightOption opt;
    opt.alpha = alpha;
    WeightOption shortOpt;
    shortOpt.alpha = 0.0;
    if (!findRouteAStar(*graph_, start, goal, opt, best, &lastPops_)) return false;
    if (!findRouteAStar(*graph_, start, goal, shortOpt, shortest)) return false;

    // 웹 구간 색칠용 칸별 미세먼지 (경로 탐색에서 채우지 않았을 때만)
    Route* routes[2] = {&best, &shortest};
    for (int k = 0; k < 2; ++k) {
        Route& r = *routes[k];
        if (r.pm25s.size() == r.lats.size()) continue;
        r.pm25s.clear();
        for (int i = 0; i < r.lats.size(); ++i) {
            int c = graph_->cellAt(r.lats[i], r.lngs[i]);
            r.pm25s.push_back(c >= 0 ? grid_.pm25[c] : -1.0);
        }
    }
    return true;
}

void App::ranking(int k, DynamicArray<RankItem>& top, DynamicArray<RankItem>& worst) const {
    top.clear();
    worst.clear();
    if (stationGraph_ == nullptr) return;
    stationGraph_->topK(k, top);
    stationGraph_->worstK(k, worst);
}

void App::search(const std::string& prefix, int limit, DynamicArray<Place>& out) const {
    trie_.searchPrefix(prefix, limit, out);
}
