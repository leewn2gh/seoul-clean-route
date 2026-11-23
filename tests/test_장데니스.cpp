// 장데니스 담당 StationGraph 테스트
#include <cstdio>
#include <string>

#include "data/DataLoader.h"
#include "station/StationGraph.h"

static int failed = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            ++failed; \
        } \
    } while (0)

static void printRanking(
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

static void testMonth(int month, int lastDay) {
    DataLoader loader;

    CHECK(loader.loadStations("data/processed/stations.csv"));
    CHECK(loader.stations().size() == 25);

    std::string path = "data/processed/air_2025_";

    if (month < 10) {
        path += "0";
    }

    path += std::to_string(month);
    path += ".csv";

    CHECK(loader.loadDistrict(path));

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
    loader.averageStations(from, to, averaged);

    CHECK(averaged.size() == 25);

    // 각 관측소마다 가까운 5곳을 연결
    StationGraph graph(averaged, 5);

    DynamicArray<RankItem> top;
    DynamicArray<RankItem> worst;

    graph.topK(5, top);
    graph.worstK(5, worst);

    CHECK(top.size() == 5);
    CHECK(worst.size() == 5);

    std::printf("\n==============================\n");
    std::printf("2025-%02d PM2.5 ranking\n", month);
    std::printf("==============================\n");

    printRanking("Top 5 (lowest PM2.5)", top);
    std::printf("\n");
    printRanking("Worst 5 (highest PM2.5)", worst);
}

int main() {
    std::printf("[StationGraph - 장데니스 테스트]\n");

    testMonth(2, 28);
    testMonth(5, 31);
    testMonth(8, 31);
    testMonth(11, 30);

    if (failed == 0) {
        std::printf("\n>> StationGraph tests passed!\n");
        return 0;
    }

    std::printf("\n>> %d test(s) failed\n", failed);
    return 1;
}