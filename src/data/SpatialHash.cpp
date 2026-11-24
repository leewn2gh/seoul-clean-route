// 담당: 이준호
#include "data/SpatialHash.h"
#include <cmath>

static const double MIN_LAT = 37.41, MIN_LNG = 126.73;   // buildGrid의 격자 시작점과 같음
static const double PI = 3.14159265358979323846;

SpatialHash::SpatialHash(double bucketMeters) : buckets_(512) {
    bucketLat_ = bucketMeters / 111000.0;
    bucketLng_ = bucketMeters / (111000.0 * std::cos(MIN_LAT * PI / 180.0));
}

void SpatialHash::bucketOf(double lat, double lng, int& row, int& col) const {
    row = (int)std::floor((lat - MIN_LAT) / bucketLat_);
    col = (int)std::floor((lng - MIN_LNG) / bucketLng_);
}

void SpatialHash::build(const DynamicArray<Station>& stations, const DynamicArray<bool>& use) {
    for (int i = 0; i < stations.size(); ++i) {
        if (use.size() == stations.size() && !use[i]) continue;
        int r, c;
        bucketOf(stations[i].lat, stations[i].lng, r, c);
        int key = r * 1000 + c;   // 서울 범위 밖 관측소는 음수 키가 될 수 있지만 같은 규칙이라 상관없음
        DynamicArray<int>* list = buckets_.get(key);
        if (list) {
            list->push_back(i);
        } else {
            DynamicArray<int> one;
            one.push_back(i);
            buckets_.put(key, one);
        }
        ++count_;
    }
}

void SpatialHash::nearby(double lat, double lng, int ring, DynamicArray<int>& out) const {
    out.clear();
    int r0, c0;
    bucketOf(lat, lng, r0, c0);
    for (int r = r0 - ring; r <= r0 + ring; ++r) {
        for (int c = c0 - ring; c <= c0 + ring; ++c) {
            const DynamicArray<int>* list = buckets_.get(r * 1000 + c);
            if (!list) continue;
            for (int k = 0; k < list->size(); ++k) out.push_back((*list)[k]);
        }
    }
}
