// 담당: 이준호
// 모듈 통합. 데이터 읽기 → 기간 평균 → 격자 → 그래프 → 경로 탐색 / 순위 / 검색을 한곳에서 관리합니다.
// 콘솔 실행(main.cpp)과 웹(core/Api.cpp, WebAssembly)이 같은 App을 씁니다.
#pragma once
#include <string>
#include "common/Types.h"
#include "data/DataLoader.h"
#include "graph/GridGraph.h"
#include "station/StationGraph.h"
#include "search/Trie.h"

class App {
public:
    App();
    ~App();

    // dataDir 안의 stations.csv와 air_2025_MM.csv(2·5·8·11월)를 모두 읽음. 하나도 못 읽으면 false
    // 콘솔: "data/processed/", 웹: "/data/" (WebAssembly 가상 파일)
    bool init(const std::string& dataDir);

    // 분석 기간을 월 단위로 바꿈: 관측소 평균 → 격자 보간 → 그래프·순위 다시 만듦. 데이터가 없으면 false
    bool setPeriod(int year, int month);

    // 위경도 두 점 사이 최적 경로(alpha)와 최단 경로(alpha = 0). 격자 밖이거나 경로가 없으면 false
    bool findRoute(double sLat, double sLng, double gLat, double gLng, double alpha,
                   Route& best, Route& shortest) const;

    void ranking(int k, DynamicArray<RankItem>& top, DynamicArray<RankItem>& worst) const;
    void search(const std::string& prefix, int limit, DynamicArray<Place>& out) const;

    const Grid& grid() const { return grid_; }
    int stationCount() const { return loader_.stations().size(); }
    double cellMeters() const { return cellMeters_; }
    void setCellMeters(double m) { cellMeters_ = m; }   // setPeriod 전에 호출 (기본 100m)

private:
    DataLoader loader_;
    DynamicArray<Station> stations_;   // 현재 기간의 관측소 평균
    Grid grid_;
    GridGraph* graph_ = nullptr;       // grid_를 참조하므로 grid_를 새로 만들 때마다 다시 만듦
    StationGraph* stationGraph_ = nullptr;
    Trie trie_;
    double cellMeters_ = 100.0;

    App(const App&) = delete;
    App& operator=(const App&) = delete;
};
