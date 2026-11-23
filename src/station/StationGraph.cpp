// 담당: 장데니스
#include "station/StationGraph.h"
#include "station/TopKHeap.h"
#include <cmath>

namespace {

const double PI = 3.14159265358979323846;
const double METERS_PER_LAT = 111000.0;

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

    DynamicArray<double> current = outPm25;

    for (int step = 0; step < steps; ++step) {
        DynamicArray<double> delta;
        delta.resize(stations_.size(), 0.0);

        for (int i = 0; i < stations_.size(); ++i) {
            if (current[i] < 0.0) {
                continue;
            }

            int nearestWind = -1;
            double nearestDistance = 0.0;

            for (int w = 0; w < windStations_.size(); ++w) {
                const Station& wind = windStations_[w];

                if (wind.windDir < 0.0 ||
                    wind.windSpeed < 0.0) {
                    continue;
                }

                double meters =
                    distanceMeters(stations_[i], wind);

                if (nearestWind < 0 ||
                    meters < nearestDistance) {
                    nearestWind = w;
                    nearestDistance = meters;
                }
            }

            if (nearestWind < 0) {
                continue;
            }

            const Station& wind =
                windStations_[nearestWind];

            // 풍향은 불어오는 방향이므로 실제 이동 방향은 +180도
            double moveDirection =
                std::fmod(
                    wind.windDir + 180.0,
                    360.0
                );

            DynamicArray<double> ratios;
            ratios.resize(adj_[i].size(), 0.0);

            double ratioSum = 0.0;

            for (int e = 0; e < adj_[i].size(); ++e) {
                const Edge& edge = adj_[i][e];

                if (edge.to < 0 ||
                    edge.to >= stations_.size() ||
                    edge.meters <= 0.0 ||
                    current[edge.to] < 0.0) {
                    continue;
                }

                double edgeDirection =
                    directionDegrees(
                        stations_[i],
                        stations_[edge.to]
                    );

                double diff =
                    angleDifference(
                        moveDirection,
                        edgeDirection
                    );

                double cosine =
                    std::cos(
                        diff * PI / 180.0
                    );

                if (cosine <= 0.0) {
                    continue;
                }

                double ratio =
                    wind.windSpeed *
                    3600.0 /
                    edge.meters *
                    cosine;

                if (ratio > 0.5) {
                    ratio = 0.5;
                }

                if (ratio < 0.0) {
                    ratio = 0.0;
                }

                ratios[e] = ratio;
                ratioSum += ratio;
            }

            if (ratioSum <= 0.0) {
                continue;
            }

            // 여러 이웃으로 동시에 이동해도 한 시간 총 유출은 최대 50%
            double scale =
                ratioSum > 0.5
                ? 0.5 / ratioSum
                : 1.0;

            for (int e = 0; e < adj_[i].size(); ++e) {
                double ratio = ratios[e] * scale;

                if (ratio <= 0.0) {
                    continue;
                }

                int to = adj_[i][e].to;

                double moved =
                    current[i] * ratio;

                delta[i] -= moved;
                delta[to] += moved;
            }
        }

        for (int i = 0; i < current.size(); ++i) {
            if (current[i] < 0.0) {
                continue;
            }

            current[i] += delta[i];

            if (current[i] < 0.0) {
                current[i] = 0.0;
            }
        }
    }

    outPm25 = current;
}
