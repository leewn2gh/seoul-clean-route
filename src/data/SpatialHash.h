// 담당: 이준호
// 공간 해시: 서울을 한 변 bucketMeters(기본 1km)인 칸(버킷)으로 나누고, 버킷마다 그 안의 관측소 번호를 모아 둡니다.
// 격자 칸 하나의 값을 IDW로 구할 때 관측소 전체(S-DoT 기온 약 960곳)가 아니라 주변 버킷의 관측소만 보면 됩니다.
//   키 = 버킷 행 × 1000 + 버킷 열 → HashTable<int, DynamicArray<int>>
#pragma once
#include "common/Types.h"
#include "data/HashTable.h"

class SpatialHash {
public:
    explicit SpatialHash(double bucketMeters = 1000.0);

    // stations 중 use[i]가 true인 관측소만 버킷에 넣음 (use가 비어 있으면 전부)
    void build(const DynamicArray<Station>& stations, const DynamicArray<bool>& use);

    // (lat, lng)가 속한 버킷과 상하좌우 ring칸 안 버킷의 관측소 번호를 out에 채움 (out은 먼저 비움)
    void nearby(double lat, double lng, int ring, DynamicArray<int>& out) const;

    // (lat, lng)가 속한 버킷의 행·열 (같은 버킷인지 비교할 때 사용)
    void bucketOf(double lat, double lng, int& row, int& col) const;

    int count() const { return count_; }   // 넣은 관측소 수

private:
    mutable HashTable<int, DynamicArray<int>> buckets_;   // HashTable::get이 const가 아니라서 mutable
    double bucketLat_, bucketLng_;                        // 버킷 한 변의 위도·경도 크기
    int count_ = 0;
};
