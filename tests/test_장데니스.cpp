// 장데니스 담당 StationGraph 테스트
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

#include "data/DataLoader.h"
#include "data/HashTable.h"
#include "station/StationGraph.h"

static int failed = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf( \
                "FAIL: %s (%s:%d)\n", \
                #cond, __FILE__, __LINE__ \
            ); \
            ++failed; \
        } \
    } while (0)

namespace {

const char* DIR = "data/processed/";

void splitCsv(
    const std::string& line,
    DynamicArray<std::string>& out
) {
    out.clear();

    std::string current;

    for (char ch : line) {
        if (ch == ',') {
            out.push_back(current);
            current.clear();
        }
        else if (ch != '\r') {
            current += ch;
        }
    }

    out.push_back(current);
}

std::string monthFile(
    const char* prefix,
    int month
) {
    char path[128];

    std::snprintf(
        path,
        sizeof(path),
        "%s%s2025_%02d.csv",
        DIR,
        prefix,
        month
    );

    return std::string(path);
}

std::string dateTimeText(
    int month,
    int day,
    int hour
) {
    char text[16];

    std::snprintf(
        text,
        sizeof(text),
        "2025%02d%02d%02d",
        month,
        day,
        hour
    );

    return std::string(text);
}

void printRanking(
    const char* title,
    const DynamicArray<RankItem>& items
) {
    std::printf("%s\n", title);

    for (int i = 0; i < items.size(); ++i) {
        std::printf(
            "%d. %s : %.2f\n",
            i + 1,
            items[i].name.c_str(),
            items[i].score
        );
    }
}

bool loadSdotWindAt(
    int month,
    int day,
    int hour,
    DynamicArray<Station>& out
) {
    out.clear();

    DynamicArray<Station> positions;
    HashTable<std::string, int> byCode(2048);

    std::ifstream stationFile(
        std::string(DIR) +
        "sdot_stations.csv"
    );

    if (!stationFile.is_open()) {
        return false;
    }

    std::string line;
    DynamicArray<std::string> tokens;

    std::getline(stationFile, line);

    while (std::getline(stationFile, line)) {
        if (line.empty()) {
            continue;
        }

        splitCsv(line, tokens);

        if (tokens.size() < 4) {
            continue;
        }

        Station station;
        station.name = tokens[0];
        station.type = StationType::SDOT;
        station.lat =
            std::atof(tokens[2].c_str());
        station.lng =
            std::atof(tokens[3].c_str());

        byCode.put(
            station.name,
            positions.size()
        );

        positions.push_back(station);
    }

    std::ifstream dataFile(
        monthFile("sdot_", month)
    );

    if (!dataFile.is_open()) {
        return false;
    }

    const std::string target =
        dateTimeText(month, day, hour);

    std::getline(dataFile, line);

    while (std::getline(dataFile, line)) {
        if (line.empty()) {
            continue;
        }

        splitCsv(line, tokens);

        if (tokens.size() < 9) {
            continue;
        }

        if (tokens[0] != target) {
            continue;
        }

        if (tokens[7].empty() ||
            tokens[8].empty()) {
            continue;
        }

        int* index =
            byCode.get(tokens[1]);

        if (index == nullptr) {
            continue;
        }

        Station station =
            positions[*index];

        station.windDir =
            std::atof(tokens[7].c_str());

        station.windSpeed =
            std::atof(tokens[8].c_str());

        if (station.windDir < 0.0 ||
            station.windSpeed < 0.0) {
            continue;
        }

        out.push_back(station);
    }

    return true;
}

void testMonthRanking(
    int month,
    int lastDay
) {
    DataLoader loader;

    bool stationsOk =
        loader.loadStations(
            "data/processed/stations.csv"
        );

    CHECK(stationsOk);

    if (!stationsOk) {
        return;
    }

    CHECK(loader.stations().size() == 25);

    bool dataOk =
        loader.loadDistrict(
            monthFile("air_", month)
        );

    CHECK(dataOk);

    if (!dataOk) {
        return;
    }

    DateTime from;
    from.year = 2025;
    from.month = month;
    from.day = 1;
    from.hour = 0;

    DateTime to;
    to.year = 2025;
    to.month = month;
    to.day = lastDay;
    to.hour = 23;

    DynamicArray<Station> averaged;

    loader.averageStations(
        from,
        to,
        averaged
    );

    CHECK(averaged.size() == 25);

    StationGraph graph(
        averaged,
        5
    );

    DynamicArray<RankItem> top;
    DynamicArray<RankItem> worst;

    graph.topK(5, top);
    graph.worstK(5, worst);

    CHECK(top.size() == 5);
    CHECK(worst.size() == 5);

    std::printf(
        "\n==============================\n"
    );

    std::printf(
        "2025-%02d PM2.5 ranking\n",
        month
    );

    std::printf(
        "==============================\n"
    );

    printRanking(
        "Top 5 (lowest PM2.5)",
        top
    );

    std::printf("\n");

    printRanking(
        "Worst 5 (highest PM2.5)",
        worst
    );
}

void testWindDirection() {
    DynamicArray<Station> pmStations;

    Station west;
    west.name = "west";
    west.lat = 37.5;
    west.lng = 127.0;
    west.pm25 = 100.0;
    pmStations.push_back(west);

    Station east;
    east.name = "east";
    east.lat = 37.5;
    east.lng = 127.01;
    east.pm25 = 0.0;
    pmStations.push_back(east);

    DynamicArray<Station> windStations;

    Station wind;
    wind.name = "wind";
    wind.type = StationType::SDOT;
    wind.lat = west.lat;
    wind.lng = west.lng;

    // 서쪽에서 불어오므로 실제 이동은 동쪽
    wind.windDir = 270.0;
    wind.windSpeed = 2.0;

    windStations.push_back(wind);

    StationGraph graph(
        pmStations,
        windStations,
        1
    );

    DynamicArray<double> predicted;

    graph.estimateByWind(
        1,
        predicted
    );

    CHECK(predicted.size() == 2);
    CHECK(predicted[0] < 100.0);
    CHECK(predicted[1] > 0.0);

    CHECK(
        std::fabs(
            predicted[0] +
            predicted[1] -
            100.0
        ) < 1e-9
    );

    windStations.clear();

    // 동쪽에서 불어오면 west -> east 방향으로는 이동하지 않아야 함
    wind.windDir = 90.0;
    windStations.push_back(wind);

    StationGraph reverse(
        pmStations,
        windStations,
        1
    );

    reverse.estimateByWind(
        1,
        predicted
    );

    CHECK(
        std::fabs(
            predicted[0] - 100.0
        ) < 1e-9
    );

    CHECK(
        std::fabs(
            predicted[1]
        ) < 1e-9
    );
}

struct ErrorSum {
    double persistence = 0.0;
    double wind = 0.0;
    int count = 0;

    void add(
        double persistenceError,
        double windError
    ) {
        persistence +=
            persistenceError;

        wind +=
            windError;

        ++count;
    }

    void add(
        const ErrorSum& other
    ) {
        persistence += other.persistence;
        wind += other.wind;
        count += other.count;
    }
};

void printErrorRow(
    const char* label,
    int hours,
    const ErrorSum& sum
) {
    double persistenceMae =
        sum.count > 0
        ? sum.persistence / sum.count
        : 0.0;

    double windMae =
        sum.count > 0
        ? sum.wind / sum.count
        : 0.0;

    double improvement =
        persistenceMae > 0.0
        ? 100.0 *
          (persistenceMae - windMae) /
          persistenceMae
        : 0.0;

    std::printf(
        "| %s | %d시간 뒤 | 1 | %d | %.2f | %.2f | %+.1f%% |\n",
        label,
        hours,
        sum.count,
        persistenceMae,
        windMae,
        improvement
    );
}

void validateWindMonth(
    int month,
    ErrorSum total[4]
) {
    // 월별 대표 시각 하나
    const int day = 15;
    const int hour = 12;

    DataLoader districtLoader;

    bool districtStationsOk =
        districtLoader.loadStations(
            "data/processed/stations.csv"
        );

    CHECK(districtStationsOk);

    if (!districtStationsOk) {
        return;
    }

    bool districtDataOk =
        districtLoader.loadDistrict(
            monthFile("air_", month)
        );

    CHECK(districtDataOk);

    if (!districtDataOk) {
        return;
    }

    DataLoader simpleLoader;

    bool simpleStationsOk =
        simpleLoader.loadStations(
            "data/processed/simple_stations.csv"
        );

    CHECK(simpleStationsOk);

    if (!simpleStationsOk) {
        return;
    }

    bool simpleDataOk =
        simpleLoader.loadSimple(
            monthFile("simple_", month)
        );

    CHECK(simpleDataOk);

    if (!simpleDataOk) {
        return;
    }

    DateTime t;
    t.year = 2025;
    t.month = month;
    t.day = day;
    t.hour = hour;

    DynamicArray<Station> districts;

    districtLoader.averageStations(
        t,
        t,
        districts
    );

    CHECK(districts.size() == 25);

    if (districts.size() != 25) {
        return;
    }

    DynamicArray<Station> simple;

    simpleLoader.averageStations(
        t,
        t,
        simple
    );

    DynamicArray<Station> pmStations;

    for (int i = 0;
         i < districts.size();
         ++i) {
        districts[i].type =
            StationType::DISTRICT;

        pmStations.push_back(
            districts[i]
        );
    }

    for (int i = 0;
         i < simple.size();
         ++i) {
        if (simple[i].pm25 < 0.0) {
            continue;
        }

        simple[i].type =
            StationType::SIMPLE;

        pmStations.push_back(
            simple[i]
        );
    }

    DynamicArray<Station> winds;

    bool windOk =
        loadSdotWindAt(
            month,
            day,
            hour,
            winds
        );

    CHECK(windOk);
    CHECK(!winds.empty());

    if (!windOk ||
        winds.empty()) {
        return;
    }

    StationGraph graph(
        pmStations,
        winds,
        5
    );

    char monthLabel[8];

    std::snprintf(
        monthLabel,
        sizeof(monthLabel),
        "%02d",
        month
    );

    for (int h = 1;
         h <= 3;
         ++h) {
        DynamicArray<double> predicted;

        graph.estimateByWind(
            h,
            predicted
        );

        DateTime future = t;
        future.hour += h;

        DynamicArray<Station> actual;

        districtLoader.averageStations(
            future,
            future,
            actual
        );

        ErrorSum row;

        for (int i = 0;
             i < 25;
             ++i) {
            if (districts[i].pm25 < 0.0 ||
                actual[i].pm25 < 0.0 ||
                predicted[i] < 0.0) {
                continue;
            }

            row.add(
                std::fabs(
                    districts[i].pm25 -
                    actual[i].pm25
                ),
                std::fabs(
                    predicted[i] -
                    actual[i].pm25
                )
            );
        }

        CHECK(row.count > 0);

        printErrorRow(
            monthLabel,
            h,
            row
        );

        total[h].add(row);
    }
}

void validateWindForecast() {
    std::printf(
        "\n| 월 | 예측 | 시각 수 | 비교 수 | 그대로 둘 때 MAE | "
        "바람 이동 MAE | 개선 |\n"
    );

    std::printf(
        "|---|---|---|---|---|---|---|\n"
    );

    ErrorSum total[4];

    validateWindMonth(2, total);
    validateWindMonth(5, total);
    validateWindMonth(8, total);
    validateWindMonth(11, total);

    for (int h = 1;
         h <= 3;
         ++h) {
        printErrorRow(
            "전체",
            h,
            total[h]
        );
    }
}

} // namespace

int main() {
    std::printf(
        "[StationGraph - 장데니스 테스트]\n"
    );

    testMonthRanking(2, 28);
    testMonthRanking(5, 31);
    testMonthRanking(8, 31);
    testMonthRanking(11, 30);

    testWindDirection();
    validateWindForecast();

    if (failed == 0) {
        std::printf(
            "\n>> StationGraph tests passed!\n"
        );

        return 0;
    }

    std::printf(
        "\n>> %d test(s) failed\n",
        failed
    );

    return 1;
}
