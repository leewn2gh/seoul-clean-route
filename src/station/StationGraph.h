// 담당: 장데니스
// 관측소 노드 그래프. 관측소 = 노드, 가까운 관측소끼리 간선
// Top/Worst 지역 순위, 바람 기반 미세먼지 추정에 사용합니다.
#pragma once
#include "common/Types.h"

struct RankItem {
    std::string name;
    double score = 0.0;
};

class StationGraph {
public:
    // 관측소마다 가까운 k개와 연결
    StationGraph(const DynamicArray<Station>& stations, int k);

    // 미세먼지 지점 + 별도 S-DoT 바람 지점으로 그래프 구성
    StationGraph(
        const DynamicArray<Station>& stations,
        const DynamicArray<Station>& windStations,
        int k
    );

    // PM2.5 기준 Top k / Worst k
    void topK(int k, DynamicArray<RankItem>& out) const;
    void worstK(int k, DynamicArray<RankItem>& out) const;

    // 1단계 = 1시간. 가장 가까운 S-DoT 바람을 사용해 PM2.5 이동 추정 (이동 변화량의 30%만 반영)
    void estimateByWind(int steps, DynamicArray<double>& outPm25) const;

private:
    struct Edge {
        int to = -1;
        double meters = 0.0;
    };

    const DynamicArray<Station>& stations_;
    DynamicArray<Station> windStations_;

    // 각 관측소의 가까운 k개 이웃
    DynamicArray<DynamicArray<Edge>> adj_;

    void buildAdjacency(int k);
};
