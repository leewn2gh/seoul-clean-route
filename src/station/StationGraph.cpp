// 담당: 장데니스
#include "station/StationGraph.h"
#include "station/TopKHeap.h"
#include <cmath>

namespace {

const double PI = 3.14159265358979323846;
const double METERS_PER_LAT = 111000.0;
const double BLEND = 0.3;          // 바람 이동 변화량 반영 비율 (예측 검증으로 정함)

struct NeighborItem {
    std::string name;
    int index = -1;
    double meters = 0.0;
};

double distanceMeters(const Station& a, const Station& b) {
    double meanLat = (a.lat + b.lat) * 0.5 * PI / 180.0;
    double metersPerLng = METERS_PER_LAT * std::cos(meanLat);

    double dy = (b.lat - a.lat) * METERS_PER_LAT;
    double dx = (b.lng - a.lng) * metersPerLng;

    return std::sqrt(dx * dx + dy * dy);
}

double directionDegrees(const Station& from, const Station& to) {
    double meanLat = (from.lat + to.lat) * 0.5 * PI / 180.0;

    double north = (to.lat - from.lat) * METERS_PER_LAT;
    double east =
        (to.lng - from.lng) *
        METERS_PER_LAT *
        std::cos(meanLat);

    double degrees =
        std::atan2(east, north) * 180.0 / PI;

    if (degrees < 0.0) {
        degrees += 360.0;
    }

    return degrees;
}

double angleDifference(double a, double b) {
    double diff = std::fabs(a - b);

    while (diff >= 360.0) {
        diff -= 360.0;
    }

    if (diff > 180.0) {
        diff = 360.0 - diff;
    }

    return diff;
}

} // namespace

StationGraph::StationGraph(
    const DynamicArray<Station>& stations,
    int k
) : stations_(stations) {
    buildAdjacency(k);
}

StationGraph::StationGraph(
    const DynamicArray<Station>& stations,
    const DynamicArray<Station>& windStations,
    int k
) : stations_(stations),
    windStations_(windStations) {
    buildAdjacency(k);
}

void StationGraph::buildAdjacency(int k) {
    adj_.clear();
    adj_.resize(stations_.size());

    if (k <= 0) {
        return;
    }

    for (int i = 0; i < stations_.size(); ++i) {
        TopKHeap<NeighborItem> nearest(k, true);

        for (int j = 0; j < stations_.size(); ++j) {
            if (i == j) {
                continue;
            }

            NeighborItem item;
            item.name = stations_[j].name;
            item.index = j;
            item.meters =
                distanceMeters(stations_[i], stations_[j]);

            nearest.offer(item.meters, item);
        }

        DynamicArray<NeighborItem> result;
        nearest.result(result);

        for (int j = 0; j < result.size(); ++j) {
            Edge edge;
            edge.to = result[j].index;
            edge.meters = result[j].meters;

            adj_[i].push_back(edge);
        }
    }
}

void StationGraph::topK(
    int k,
    DynamicArray<RankItem>& out
) const {
    out.clear();

    if (k <= 0) {
        return;
    }

    TopKHeap<RankItem> heap(k, true);

    for (int i = 0; i < stations_.size(); ++i) {
        const Station& station = stations_[i];

        if (station.pm25 < 0.0) {
            continue;
        }

        RankItem item;
        item.name = station.name;
        item.score = station.pm25;

        heap.offer(item.score, item);
    }

    heap.result(out);
}

void StationGraph::worstK(
    int k,
    DynamicArray<RankItem>& out
) const {
    out.clear();

    if (k <= 0) {
        return;
    }

    TopKHeap<RankItem> heap(k, false);

    for (int i = 0; i < stations_.size(); ++i) {
        const Station& station = stations_[i];

        if (station.pm25 < 0.0) {
            continue;
        }

        RankItem item;
        item.name = station.name;
        item.score = station.pm25;

        heap.offer(item.score, item);
    }

    heap.result(out);
}

void StationGraph::estimateByWind(
    int steps,
    DynamicArray<double>& outPm25
) const {
    outPm25.clear();
    outPm25.resize(stations_.size(), -1.0);

    for (int i = 0; i < stations_.size(); ++i) {
        outPm25[i] = stations_[i].pm25;
    }

    if (steps <= 0 ||
        stations_.empty() ||
        windStations_.empty()) {
        return;
    }

    // 관측소마다 가장 가까운 S-DoT 바람 센서 (단계마다 같으므로 한 번만 찾음)
    DynamicArray<int> windOf;
    windOf.resize(stations_.size(), -1);
    for (int i = 0; i < stations_.size(); ++i) {
        double best = 0.0;
        for (int w = 0; w < windStations_.size(); ++w) {
            const Station& wind = windStations_[w];
            if (wind.windDir < 0.0 || wind.windSpeed < 0.0) continue;
            double meters = distanceMeters(stations_[i], wind);
            if (windOf[i] < 0 || meters < best) {
                windOf[i] = w;
                best = meters;
            }
        }
    }

    // 농도 이류: 바람이 불어오는 쪽(바람 위쪽) 이웃의 값을 끌어옴
    //   c_i ← c_i + Σ r_j × (c_j − c_i),  r_j = 풍속 × 3600 / 거리 × cos(각도), 합이 1을 넘으면 1로 맞춤
    // (값을 밀어내기만 하면 바깥에서 들어오는 양이 없어 전체 농도가 계속 줄어듦 → 끌어오는 방식으로 바꿈)
    DynamicArray<double> current = outPm25;

    for (int step = 0; step < steps; ++step) {
        DynamicArray<double> next = current;

        for (int i = 0; i < stations_.size(); ++i) {
            if (current[i] < 0.0 || windOf[i] < 0) continue;
            const Station& wind = windStations_[windOf[i]];
            if (wind.windSpeed <= 0.0) continue;

            DynamicArray<double> ratios;
            ratios.resize(adj_[i].size(), 0.0);
            double ratioSum = 0.0;

            for (int e = 0; e < adj_[i].size(); ++e) {
                const Edge& edge = adj_[i][e];
                if (edge.to < 0 || edge.to >= stations_.size() ||
                    edge.meters <= 0.0 || current[edge.to] < 0.0) continue;

                // 풍향(불어오는 방향)과 i → 이웃 방향이 비슷하면 그 이웃이 바람 위쪽
                double diff = angleDifference(wind.windDir, directionDegrees(stations_[i], stations_[edge.to]));
                double cosine = std::cos(diff * PI / 180.0);
                if (cosine <= 0.0) continue;

                double ratio = wind.windSpeed * 3600.0 / edge.meters * cosine;
                ratios[e] = ratio;
                ratioSum += ratio;
            }
            if (ratioSum <= 0.0) continue;

            double scale = ratioSum > 1.0 ? 1.0 / ratioSum : 1.0;
            for (int e = 0; e < adj_[i].size(); ++e) {
                if (ratios[e] <= 0.0) continue;
                int to = adj_[i][e].to;
                next[i] += ratios[e] * scale * (current[to] - current[i]);
            }
        }
        current = next;
    }

    // 보정: 이동한 양을 그대로 쓰면 1시간 뒤 오차가 지속 예측보다 13% 커짐 (너무 많이 옮김)
    //   변화량의 BLEND(30%)만 반영하면 1·2·3시간 뒤 모두 지속 예측보다 정확해짐 (bench_station_wind)
    for (int i = 0; i < stations_.size(); ++i) {
        if (outPm25[i] < 0.0 || current[i] < 0.0) continue;
        outPm25[i] += BLEND * (current[i] - outPm25[i]);
    }
}
