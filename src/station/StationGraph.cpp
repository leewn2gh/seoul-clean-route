// 담당: 장데니스
#include "station/StationGraph.h"
#include "station/TopKHeap.h"
#include <cmath>

namespace {

struct NeighborItem {
    std::string name;
    int index = -1;
    double meters = 0.0;
};

double distanceMeters(const Station& a, const Station& b) {
    const double PI = 3.14159265358979323846;
    const double metersPerLat = 111000.0;

    double meanLat = (a.lat + b.lat) * 0.5 * PI / 180.0;
    double metersPerLng = 111000.0 * std::cos(meanLat);

    double dy = (b.lat - a.lat) * metersPerLat;
    double dx = (b.lng - a.lng) * metersPerLng;

    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

StationGraph::StationGraph(
    const DynamicArray<Station>& stations,
    int k
) : stations_(stations) {
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

            double meters = distanceMeters(stations_[i], stations_[j]);

            NeighborItem item;
            item.name = stations_[j].name;
            item.index = j;
            item.meters = meters;

            nearest.offer(meters, item);
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

    if (k <= 0) return;

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

    if (k <= 0) return;

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
    // 여유가 되면 구현
    (void)steps;
    (void)outPm25;
}