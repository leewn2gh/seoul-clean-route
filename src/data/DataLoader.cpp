// 담당: 방지혁
#include "data/DataLoader.h"
#include "data/SpatialHash.h"
#include "data/HashTable.h"
#include "graph/DynamicArray.h"
#include "common/Types.h"
#include <fstream>
#include <sstream>
#include <cmath>

// CSV 한 줄 파싱 보조 함수
static void parseCSVLine(const std::string& line, DynamicArray<std::string>& tokens) {
    tokens.clear();
    size_t start = 0;
    size_t len = line.length();
    while (start <= len) {
        size_t comma = line.find(',', start);
        if (comma == std::string::npos) comma = len;

        size_t s = start;
        while (s < comma && (line[s] == ' ' || line[s] == '\t' || line[s] == '\r' || line[s] == '\n')) ++s;
        size_t e = comma;
        while (e > s && (line[e - 1] == ' ' || line[e - 1] == '\t' || line[e - 1] == '\r' || line[e - 1] == '\n')) --e;

        if (s < e) {
            tokens.push_back(line.substr(s, e - s));
        } else {
            tokens.push_back("");
        }
        if (comma == len) break;
        start = comma + 1;
    }
}

// --------------------------------------------------------
// 1. loadStations (stations.csv, simple_stations.csv, sdot_stations.csv 읽기)
// --------------------------------------------------------
bool DataLoader::loadStations(const std::string& path, StationType type) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    if (!std::getline(file, line)) return false;

    DynamicArray<std::string> tokens;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        parseCSVLine(line, tokens);
        if (tokens.size() < 4) continue;

        Station st;
        st.name = tokens[1]; // 관측소 이름 (예: "종로구", "OA-2221")
        try {
            st.lat = std::stod(tokens[2]);
            st.lng = std::stod(tokens[3]);
        } catch (...) {
            continue;
        }
        st.type = type;

        stations_.push_back(st);
    }
    return true;
}

// 빈칸(결측값)이면 -1.0, 아니면 숫자로 변환
static double parseValue(const std::string& token) {
    if (token.empty()) return -1.0;
    try {
        return std::stod(token);
    } catch (...) {
        return -1.0;
    }
}

// 기온 결측값이면 -999.0, 아니면 숫자로 변환
static double parseTemp(const std::string& token) {
    if (token.empty()) return -999.0;
    try {
        return std::stod(token);
    } catch (...) {
        return -999.0;
    }
}

// --------------------------------------------------------
// 2. loadDistrict (자치구 대기 데이터 파싱 및 AVL 트리 삽입)
// 열 순서: datetime,station_code,station_name,pm10,pm25,o3,no2,co,so2
// 시각: YYYYMMDDHH (예: 2025020112)
// --------------------------------------------------------
bool DataLoader::loadDistrict(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    if (!std::getline(file, line)) return false;   // 머리글 건너뛰기

    // 관측소 이름 → 관측소 목록에서의 위치 (해시 테이블, 체이닝)
    int bucketCount = stations_.size() > 0 ? static_cast<int>(stations_.size()) * 2 : 64;
    HashTable<std::string, int> stationIndexByName(bucketCount);
    for (int i = 0; i < static_cast<int>(stations_.size()); ++i) {
        stationIndexByName.put(stations_[i].name, i);
    }

    DynamicArray<std::string> tokens;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        parseCSVLine(line, tokens);
        if (tokens.size() < 5) continue;
        if (tokens[0].length() < 10) continue;      // 시각 형식이 아니면 건너뛰기

        // 관측소는 이름(station_name)으로 해시 테이블에서 찾기, O(1) 평균
        int* found = stationIndexByName.get(tokens[2]);
        if (found == nullptr) continue;
        int stationIndex = *found;

        Record r;
        r.stationIndex = stationIndex;
        try {
            r.time.year = std::stoi(tokens[0].substr(0, 4));
            r.time.month = std::stoi(tokens[0].substr(4, 2));
            r.time.day = std::stoi(tokens[0].substr(6, 2));
            r.time.hour = std::stoi(tokens[0].substr(8, 2));
        } catch (...) {
            continue;
        }
        r.pm10 = parseValue(tokens[3]);
        r.pm25 = parseValue(tokens[4]);

        records_.insert(r);
    }
    return true;
}

// --------------------------------------------------------
// 2-2. loadSimple (간이측정망 대기 데이터 읽기)
// simple_2025_MM.csv: datetime,station_code,station_name,pm10,pm25
// loadDistrict와 열 순서가 같아서 그대로 재사용 (pm10은 빈칸 → -1)
// --------------------------------------------------------
bool DataLoader::loadSimple(const std::string& path) {
    return loadDistrict(path);
}

// --------------------------------------------------------
// 3. loadSdot (S-DoT 대기/기상 데이터 읽기)
// sdot_2025_MM.csv: datetime,station_code,station_name,pm10,pm25,temp,humidity,wind_dir,wind_speed,no2,o3
// pm10, pm25는 빈칸(-1), temp·humidity·windDir·windSpeed 채우기 (기온 빈칸은 -999)
// --------------------------------------------------------
bool DataLoader::loadSdot(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    if (!std::getline(file, line)) return false;   // 머리글 건너뛰기

    int bucketCount = stations_.size() > 0 ? static_cast<int>(stations_.size()) * 2 : 64;
    HashTable<std::string, int> stationIndexByName(bucketCount);
    for (int i = 0; i < static_cast<int>(stations_.size()); ++i) {
        stationIndexByName.put(stations_[i].name, i);
    }

    DynamicArray<std::string> tokens;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        parseCSVLine(line, tokens);
        if (tokens.size() < 5) continue;
        if (tokens[0].length() < 10) continue;

        int* found = stationIndexByName.get(tokens[2]);
        if (found == nullptr) continue;
        int stationIndex = *found;

        Record r;
        r.stationIndex = stationIndex;
        try {
            r.time.year = std::stoi(tokens[0].substr(0, 4));
            r.time.month = std::stoi(tokens[0].substr(4, 2));
            r.time.day = std::stoi(tokens[0].substr(6, 2));
            r.time.hour = std::stoi(tokens[0].substr(8, 2));
        } catch (...) {
            continue;
        }

        r.pm10 = parseValue(tokens[3]);
        r.pm25 = parseValue(tokens[4]);
        if (tokens.size() > 5) r.temp = parseTemp(tokens[5]);
        if (tokens.size() > 6) r.humidity = parseValue(tokens[6]);
        if (tokens.size() > 7) r.windDir = parseValue(tokens[7]);
        if (tokens.size() > 8) r.windSpeed = parseValue(tokens[8]);

        records_.insert(r);
    }
    return true;
}

// --------------------------------------------------------
// 4. averageStations (기간 평균 산출)
// 기록(matched)을 한 번만 순회하여 관측소별 합계/개수 누적
// temp, humidity, windSpeed 산술 평균, windDir는 벡터(u, v) 평균 후 atan2로 복원
// --------------------------------------------------------
void DataLoader::averageStations(const DateTime& from, const DateTime& to, DynamicArray<Station>& out) const {
    out.clear();
    int n = static_cast<int>(stations_.size());
    if (n == 0) return;

    DynamicArray<Record> matched;
    records_.rangeSearch(from, to, matched);

    struct StationAccumulator {
        double sumPm25 = 0.0;
        int countPm25 = 0;
        double sumPm10 = 0.0;
        int countPm10 = 0;
        double sumTemp = 0.0;
        int countTemp = 0;
        double sumHumidity = 0.0;
        int countHumidity = 0;
        double sumWindSpeed = 0.0;
        int countWindSpeed = 0;
        double sumU = 0.0;
        double sumV = 0.0;
        int countWindDir = 0;
    };

    DynamicArray<StationAccumulator> acc;
    acc.resize(n);

    const double PI = 3.14159265358979323846;

    for (int j = 0; j < static_cast<int>(matched.size()); ++j) {
        const Record& r = matched[j];
        if (r.stationIndex < 0 || r.stationIndex >= n) continue;

        StationAccumulator& a = acc[r.stationIndex];

        if (r.pm25 >= 0.0) {
            a.sumPm25 += r.pm25;
            ++a.countPm25;
        }
        if (r.pm10 >= 0.0) {
            a.sumPm10 += r.pm10;
            ++a.countPm10;
        }
        if (r.temp > -900.0) {
            a.sumTemp += r.temp;
            ++a.countTemp;
        }
        if (r.humidity >= 0.0) {
            a.sumHumidity += r.humidity;
            ++a.countHumidity;
        }
        if (r.windSpeed >= 0.0) {
            a.sumWindSpeed += r.windSpeed;
            ++a.countWindSpeed;
        }
        if (r.windSpeed >= 0.0 && r.windDir >= 0.0) {
            double rad = r.windDir * PI / 180.0;
            a.sumU += r.windSpeed * std::sin(rad);
            a.sumV += r.windSpeed * std::cos(rad);
            ++a.countWindDir;
        }
    }

    for (int i = 0; i < n; ++i) {
        Station st = stations_[i];
        const StationAccumulator& a = acc[i];

        st.pm25 = (a.countPm25 > 0) ? (a.sumPm25 / a.countPm25) : -1.0;
        st.pm10 = (a.countPm10 > 0) ? (a.sumPm10 / a.countPm10) : -1.0;
        st.temp = (a.countTemp > 0) ? (a.sumTemp / a.countTemp) : -999.0;
        st.humidity = (a.countHumidity > 0) ? (a.sumHumidity / a.countHumidity) : -1.0;
        st.windSpeed = (a.countWindSpeed > 0) ? (a.sumWindSpeed / a.countWindSpeed) : -1.0;

        if (a.countWindDir > 0) {
            double avgDir = std::atan2(a.sumU, a.sumV) * 180.0 / PI;
            if (avgDir < 0.0) avgDir += 360.0;
            if (avgDir >= 360.0 || std::fabs(avgDir - 360.0) < 1e-9) avgDir = 0.0;
            if (std::fabs(avgDir) < 1e-9) avgDir = 0.0;
            st.windDir = avgDir;
        } else {
            st.windDir = -1.0;
        }

        out.push_back(st);
    }
}

// --------------------------------------------------------
// 5. buildGrid (전역 함수 - Types.h의 Grid 구조체 호환)
// 서울 격자 생성 및 PM2.5, 기온(temp) 역거리 가중 보간(IDW)
// temp가 -999인 관측소 제외, pm25는 pm25 >= 0인 관측소만 대상
// --------------------------------------------------------
void buildGrid(const DynamicArray<Station>& stations, double cellMeters, Grid& out) {
    if (stations.empty()) return;

    // 격자 범위: 측정소 위치가 아니라 서울 전체로 고정 (외곽 지역도 경로 탐색 가능하도록)
    const double minLat = 37.41, maxLat = 37.72;
    const double minLng = 126.73, maxLng = 127.27;

    // 위도 1도 ≈ 111,000m, 경도 1도 ≈ 111,000m × cos(위도) → 칸 한 변이 cellMeters가 되도록 간격 계산
    const double PI = 3.14159265358979323846;
    double metersPerLat = 111000.0;
    double metersPerLng = 111000.0 * std::cos(minLat * PI / 180.0);
    double latDegreeStep = cellMeters / metersPerLat;
    double lngDegreeStep = cellMeters / metersPerLng;

    out.rows = static_cast<int>((maxLat - minLat) / latDegreeStep) + 1;
    out.cols = static_cast<int>((maxLng - minLng) / lngDegreeStep) + 1;
    out.minLat = minLat;
    out.minLng = minLng;
    out.cellLat = latDegreeStep;
    out.cellLng = lngDegreeStep;

    int cellCount = out.rows * out.cols;
    out.pm25.clear();
    out.temp.clear();
    out.passable.clear();
    out.pm25.resize(cellCount, -1.0);
    out.temp.resize(cellCount, -999.0);
    out.passable.resize(cellCount, true); // 중간발표: 모든 칸 통행 가능 (OSM 반영은 최종)

    // IDW (역거리 가중 보간법): Σ(측정값 / 거리²) / Σ(1 / 거리²)
    // 관측소가 많아(S-DoT 기온 약 960곳) 칸마다 전부 보면 느리므로, 공간 해시로 주변 관측소만 봄 (이준호)
    //   미세먼지: 주변 7km × 7km(ring 3)에서 6곳 이상, 기온: 3km × 3km(ring 1)에서 6곳 이상이 되도록 범위를 넓힘
    DynamicArray<bool> usePm, useTemp;
    int pmCount = 0, tempCount = 0;
    for (int i = 0; i < stations.size(); ++i) {
        usePm.push_back(stations[i].pm25 >= 0.0);
        useTemp.push_back(stations[i].temp > -900.0);
        if (usePm[i]) ++pmCount;
        if (useTemp[i]) ++tempCount;
    }
    SpatialHash pmHash(1000.0), tempHash(1000.0);
    pmHash.build(stations, usePm);
    tempHash.build(stations, useTemp);

    const int MIN_STATIONS = 6, MAX_RING = 20;
    DynamicArray<int> pmNear, tempNear;
    // 같은 1km 버킷 안의 칸은 후보 관측소가 같으므로 버킷이 바뀔 때만 다시 찾음
    auto gather = [&](const SpatialHash& hash, int total, int ring, double lat, double lng, DynamicArray<int>& out) {
        int need = total < MIN_STATIONS ? total : MIN_STATIONS;
        hash.nearby(lat, lng, ring, out);
        while (out.size() < need && ring < MAX_RING) hash.nearby(lat, lng, ++ring, out);
    };
    int cachedRow = -100000, cachedCol = -100000;

    for (int r = 0; r < out.rows; ++r) {
        double cellLatPos = minLat + (r + 0.5) * latDegreeStep;   // 칸 중심
        for (int c = 0; c < out.cols; ++c) {
            double cellLngPos = minLng + (c + 0.5) * lngDegreeStep;

            int br, bc;
            pmHash.bucketOf(cellLatPos, cellLngPos, br, bc);
            if (br != cachedRow || bc != cachedCol) {
                gather(pmHash, pmCount, 3, cellLatPos, cellLngPos, pmNear);
                gather(tempHash, tempCount, 1, cellLatPos, cellLngPos, tempNear);
                cachedRow = br;
                cachedCol = bc;
            }

            int idx = r * out.cols + c;
            for (int pass = 0; pass < 2; ++pass) {   // 0: PM2.5, 1: 기온
                const DynamicArray<int>& near = pass == 0 ? pmNear : tempNear;
                double weightSum = 0.0, valueSum = 0.0;
                bool exact = false;
                for (int k = 0; k < near.size(); ++k) {
                    const Station& st = stations[near[k]];
                    double value = pass == 0 ? st.pm25 : st.temp;

                    // 위경도 차이를 미터로 바꿔서 거리 계산 (경도 1도가 위도 1도보다 짧음)
                    double dy = (cellLatPos - st.lat) * metersPerLat;
                    double dx = (cellLngPos - st.lng) * metersPerLng;
                    double distSq = dx * dx + dy * dy;

                    if (distSq < 1.0) {   // 1m 이내면 그 측정소 값 그대로
                        valueSum = value;
                        weightSum = 1.0;
                        exact = true;
                        break;
                    }
                    double w = 1.0 / distSq;
                    weightSum += w;
                    valueSum += w * value;
                }
                if (weightSum <= 0.0) continue;
                double v = exact ? valueSum : valueSum / weightSum;
                if (pass == 0) out.pm25[idx] = v;
                else out.temp[idx] = v;
            }
        }
    }
}
