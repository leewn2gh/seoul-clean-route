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

    // dataDir 안의 측정 데이터(2·5·8·11월)를 모두 읽음. 자치구 파일을 하나도 못 읽으면 false
    //   자치구 stations.csv + air_2025_MM.csv (필수), 간이측정망 simple_* · S-DoT sdot_* (있으면)
    //   보행로: osm_walk.csv가 있으면 그것으로, 없으면 미리 만든 passable_25m.bin (웹용)
    // 콘솔: "data/processed/", 웹: "/data/" (WebAssembly 가상 파일)
    bool init(const std::string& dataDir);

    // 보행로를 반영한 통행 가능 칸을 파일로 저장 (웹에 44MB 보행로 대신 싣는 용도). setPeriod 다음에 호출
    bool savePassable(const std::string& path) const;

    // 분석 기간을 월 단위로 바꿈: 관측소 평균 → 격자 보간 → 그래프·순위 다시 만듦. 데이터가 없으면 false
    bool setPeriod(int year, int month);

    // 위경도 두 점 사이 최적 경로(alpha)와 최단 경로(alpha = 0). 격자 밖이거나 경로가 없으면 false
    bool findRoute(double sLat, double sLng, double gLat, double gLng, double alpha,
                   Route& best, Route& shortest) const;

    void ranking(int k, DynamicArray<RankItem>& top, DynamicArray<RankItem>& worst) const;
    void search(const std::string& prefix, int limit, DynamicArray<Place>& out) const;

    // 바람으로 hours시간 뒤 미세먼지 추정 (자치구·간이 측정소 노드 그래프). 측정소 이름과 값
    void forecastByWind(int hours, DynamicArray<RankItem>& out) const;

    const Grid& grid() const { return grid_; }
    int stationCount() const { return loader_.stations().size(); }
    int countOf(StationType type) const;            // 종류별 관측소 수
    bool hasRoads() const { return roadMask_.size() > 0; }
    double passableRatio() const;                   // 통행 가능 칸 비율 (0~1)
    int lastPopCount() const { return lastPops_; }  // 마지막 최적 경로 탐색에서 힙에서 꺼낸 횟수
    double cellMeters() const { return cellMeters_; }
    void setCellMeters(double m) { cellMeters_ = m; roadMask_.clear(); }   // setPeriod 전에 호출 (기본 25m)

private:
    DataLoader loader_;
    DynamicArray<Station> stations_;   // 현재 기간의 관측소 평균
    Grid grid_;
    GridGraph* graph_ = nullptr;       // grid_를 참조하므로 grid_를 새로 만들 때마다 다시 만듦
    StationGraph* stationGraph_ = nullptr;
    DynamicArray<Station> districts_;  // 순위용 자치구 측정소만 (StationGraph가 참조)
    DynamicArray<Station> pmStations_; // 바람 추정용 자치구 + 간이
    DynamicArray<Station> winds_;      // 바람 있는 S-DoT
    Trie trie_;
    std::string dataDir_;
    DynamicArray<bool> roadMask_;      // 보행로가 지나는 칸 (격자 크기마다 한 번만 만듦)
    double cellMeters_ = 25.0;
    mutable int lastPops_ = 0;

    bool loadRoads();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
};
