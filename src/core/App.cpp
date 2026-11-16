// 담당: 이준호
#include "core/App.h"
#include "route/Router.h"
#include <cstdio>

static const int MONTHS[] = {2, 5, 8, 11};   // 분석 기간 (2025년)

App::App() {}

App::~App() {
    delete graph_;
    delete stationGraph_;
}

bool App::init(const std::string& dataDir) {
    if (!loader_.loadStations(dataDir + "stations.csv")) return false;

    int loaded = 0;
    for (int i = 0; i < 4; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "air_2025_%02d.csv", MONTHS[i]);
        if (loader_.loadDistrict(dataDir + name)) ++loaded;
    }
    // TODO(11/23 병합 후): 간이측정망·S-DoT (loadStations(.., type), loadSimple, loadSdot) 연결

    // 지역명 검색: 자치구 측정소 이름을 트라이에 넣음 (S-DoT 같은 센서 번호는 넣지 않음)
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

bool App::setPeriod(int year, int month) {
    DateTime from; from.year = year; from.month = month; from.day = 1;  from.hour = 0;
    DateTime to;   to.year   = year; to.month   = month; to.day   = 31; to.hour   = 23;
    DynamicArray<Station> next;
    loader_.averageStations(from, to, next);

    bool any = false;
    for (int i = 0; i < next.size(); ++i) if (next[i].pm25 >= 0) { any = true; break; }
    if (!any) return false;   // 그 달 데이터가 없음

    delete graph_;        graph_ = nullptr;
    delete stationGraph_; stationGraph_ = nullptr;

    stations_ = next;
    grid_ = Grid();
    buildGrid(stations_, cellMeters_, grid_);
    // TODO(11/23 병합 후): applyRoads("osm_walk.csv", grid_) 로 보행로만 통행 가능하게

    graph_ = new GridGraph(grid_);
    stationGraph_ = new StationGraph(stations_, 3);
    return grid_.rows > 0 && grid_.cols > 0;
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

    WeightOption opt;
    opt.alpha = alpha;
    if (!compareRoutes(*graph_, start, goal, opt, best, shortest)) return false;

    // 웹 구간 색칠용 칸별 미세먼지 (경로 탐색 쪽에서 채우기 전까지 여기서 채움)
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
