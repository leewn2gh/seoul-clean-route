// 방지혁 담당 데이터 모듈 테스트 파일 ( HashTable, TimeAVL, DataLoader)//
#include <cstdio>
#include <cmath>
#include <fstream>
#include "data/HashTable.h"
#include "data/TimeAVL.h"
#include "data/DataLoader.h"
#include "common/TestData.h"

static int failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf(" 실패: %s (%s:%d)\n", #cond, __FILE__, __LINE__); ++failed; } } while (0)

int main() {
    std::printf("[Data Module 테스트 시작]\n");

    // 1. HashTable 테스트
    HashTable<std::string, int> ht;
    ht.put("OA-2221", 10);
    ht.put("OA-1200", 20);

    CHECK(ht.size() == 2);
    int* val1 = ht.get("OA-2221");
    CHECK(val1 != nullptr && *val1 == 10);
    CHECK(ht.get("NONE") == nullptr);

    // 2. TimeAVL 테스트
    TimeAVL avl;
    Record r1;
    r1.time = { 2025, 2, 1, 12 };
    r1.stationIndex = 0;
    r1.pm25 = 15.5;
    avl.insert(r1);

    CHECK(avl.size() == 1);
    CHECK(avl.height() >= 1);

    DynamicArray<Record> searchOut;
    avl.rangeSearch({ 2025, 2, 1, 0 }, { 2025, 2, 1, 23 }, searchOut);
    CHECK(searchOut.size() == 1);
    CHECK(searchOut[0].pm25 == 15.5);

    // 3. DataLoader & buildGrid 테스트 (TestData 활용)
    DynamicArray<Station> fakeStations = makeTestStations();
    CHECK(fakeStations.size() == 25);

    Grid grid;
    buildGrid(fakeStations, 100.0, grid);
    CHECK(grid.rows > 0);
    CHECK(grid.cols > 0);
    CHECK(grid.pm25.size() == grid.rows * grid.cols);
    CHECK(grid.temp.size() == grid.rows * grid.cols);
    // 격자 범위는 측정소 위치와 상관없이 서울 전체 (위도 37.41~37.72, 경도 126.73~127.27)
    CHECK(grid.minLat == 37.41 && grid.minLng == 126.73);
    CHECK(grid.minLat + grid.rows * grid.cellLat >= 37.72);
    CHECK(grid.minLng + grid.cols * grid.cellLng >= 127.27);
    // passable, temp도 칸 수만큼 만들고, 중간발표에서는 모든 칸 통행 가능
    CHECK(grid.passable.size() == grid.rows * grid.cols);
    bool allOpen = true;
    for (int k = 0; k < grid.passable.size(); ++k) if (!grid.passable[k]) allOpen = false;
    CHECK(allOpen);
    // 측정소가 있는 칸의 값은 그 측정소 값과 거의 같아야 함 (칸 중심과 측정소가 최대 약 70m)
    {
        const Station& s0 = fakeStations[12];
        int r = (int)std::floor((s0.lat - grid.minLat) / grid.cellLat);
        int c = (int)std::floor((s0.lng - grid.minLng) / grid.cellLng);
        CHECK(std::fabs(grid.pm25[r * grid.cols + c] - s0.pm25) < 1.0);
        CHECK(std::fabs(grid.temp[r * grid.cols + c] - s0.temp) < 1.0);
    }

    // 4. 자치구 CSV 형식 (YYYYMMDDHH, pm10/pm25 순서, 빈칸 = 결측값) 읽기
    {
        std::ofstream st("build/tests/test_stations.csv");
        st << "code,name,lat,lng,address,note\n"
           << "111123,종로구,37.571973,127.005082,서울 종로구,\n"
           << "111274,강동구,37.544963,127.135681,서울 강동구,\n";
        st.close();

        std::ofstream air("build/tests/test_air.csv");
        air << "datetime,station_code,station_name,pm10,pm25,o3,no2,co,so2\r\n"
            << "2025020100,111123,종로구,40,20,0.01,0.02,0.4,0.003\r\n"
            << "2025020101,111123,종로구,,30,0.01,0.02,0.4,0.003\r\n"     // pm10 빈칸
            << "2025020102,111123,종로구,60,,0.01,0.02,0.4,0.003\r\n"     // pm25 빈칸
            << "2025020100,111274,강동구,,,0.01,0.02,0.4,0.003\r\n"       // 둘 다 빈칸
            << "2025030100,111123,종로구,999,999,0.01,0.02,0.4,0.003\r\n" // 기간 밖
            << "2025020100,999999,없는구,10,10,0.01,0.02,0.4,0.003\r\n";  // 없는 측정소
        air.close();
    }
    DataLoader loader;
    CHECK(loader.loadStations("build/tests/test_stations.csv"));
    CHECK(loader.stations().size() == 2);
    CHECK(loader.stations()[0].type == StationType::DISTRICT);
    CHECK(loader.stations()[1].type == StationType::DISTRICT);
    CHECK(loader.loadDistrict("build/tests/test_air.csv"));
    DynamicArray<Station> avg;
    loader.averageStations({ 2025, 2, 1, 0 }, { 2025, 2, 28, 23 }, avg);
    CHECK(avg.size() == 2);
    CHECK(std::fabs(avg[0].pm25 - 25.0) < 1e-9);   // (20 + 30) / 2, 빈칸 제외
    CHECK(std::fabs(avg[0].pm10 - 50.0) < 1e-9);   // (40 + 60) / 2, 빈칸 제외
    CHECK(avg[1].pm25 == -1.0 && avg[1].pm10 == -1.0);   // 유효한 값이 없으면 -1

    // 5. loadStations - StationType 지정 (SIMPLE, SDOT)
    {
        {
            std::ofstream stSimple("build/tests/test_simple_stations.csv");
            stSimple << "code,name,lat,lng,address,note\n"
                     << "556,556,37.562675,127.047560,사근초,\n";
            stSimple.close();

            std::ofstream stSdot("build/tests/test_sdot_stations.csv");
            stSdot << "code,name,lat,lng,address,note\n"
                   << "OC3CL200011,OC3CL200011,37.576920,127.004514,종로구,\n";
            stSdot.close();
        }

        DataLoader multiLoader;
        CHECK(multiLoader.loadStations("build/tests/test_stations.csv", StationType::DISTRICT));
        CHECK(multiLoader.loadStations("build/tests/test_simple_stations.csv", StationType::SIMPLE));
        CHECK(multiLoader.loadStations("build/tests/test_sdot_stations.csv", StationType::SDOT));

        CHECK(multiLoader.stations().size() == 4);
        CHECK(multiLoader.stations()[0].type == StationType::DISTRICT);
        CHECK(multiLoader.stations()[1].type == StationType::DISTRICT);
        CHECK(multiLoader.stations()[2].type == StationType::SIMPLE);
        CHECK(multiLoader.stations()[3].type == StationType::SDOT);
    }

    // 6. loadSimple 테스트 (datetime,station_code,station_name,pm10,pm25)
    {
        {
            std::ofstream stSimple("build/tests/test_simple_stations.csv");
            stSimple << "code,name,lat,lng,address,note\n"
                     << "556,556,37.562675,127.047560,사근초,\n"
                     << "561,561,37.633195,127.040884,신화초,\n";
            stSimple.close();

            std::ofstream dataSimple("build/tests/test_simple.csv");
            dataSimple << "datetime,station_code,station_name,pm10,pm25\r\n"
                       << "2025020101,556,556,,15.0\r\n"
                       << "2025020102,556,556,,25.0\r\n"
                       << "2025020101,561,561,,30.0\r\n";
            dataSimple.close();
        }

        DataLoader simpleLoader;
        CHECK(simpleLoader.loadStations("build/tests/test_simple_stations.csv", StationType::SIMPLE));
        CHECK(simpleLoader.loadSimple("build/tests/test_simple.csv"));

        DynamicArray<Station> simpleAvg;
        simpleLoader.averageStations({ 2025, 2, 1, 0 }, { 2025, 2, 1, 23 }, simpleAvg);
        CHECK(simpleAvg.size() == 2);
        CHECK(std::fabs(simpleAvg[0].pm25 - 20.0) < 1e-9); // (15 + 25) / 2
        CHECK(simpleAvg[0].pm10 == -1.0);                  // 빈칸은 -1
        CHECK(std::fabs(simpleAvg[1].pm25 - 30.0) < 1e-9);
        CHECK(simpleAvg[1].pm10 == -1.0);
    }

    // 7. loadSdot 및 averageStations (기온, 습도, 풍향/풍속 벡터 평균)
    {
        {
            std::ofstream stSdot("build/tests/test_sdot_stations.csv");
            stSdot << "code,name,lat,lng,address,note\n"
                   << "SDOT01,SDOT01,37.560,127.000,종로,\n"
                   << "SDOT02,SDOT02,37.570,127.010,중구,\n"
                   << "SDOT03,SDOT03,37.580,127.020,용산,\n";
            stSdot.close();

            // SDOT01: 풍향 30도(속도 2.0), 풍향 90도(속도 2.0) -> 평균 풍향 60도, 풍속 2.0 m/s
            //         기온 1.0, -3.0 -> 평균 -1.0도 (영하 기온 정상 처리 확인)
            //         습도 50, 70 -> 평균 60%
            // SDOT02: 풍향 350도(속도 1.0), 풍향 10도(속도 1.0) -> 0도 경계 교차 벡터 평균 -> 풍향 0.0도, 풍속 1.0
            //         기온 5.0, 빈칸 -> 평균 5.0 (빈칸 -999 제외)
            // SDOT03: 결측값만 존재 -> temp=-999, humidity=-1, windDir=-1, windSpeed=-1
            std::ofstream dataSdot("build/tests/test_sdot.csv");
            dataSdot << "datetime,station_code,station_name,pm10,pm25,temp,humidity,wind_dir,wind_speed,no2,o3\r\n"
                     << "2025020100,SDOT01,SDOT01,,,1.0,50,30,2.0,,\r\n"
                     << "2025020101,SDOT01,SDOT01,,,-3.0,70,90,2.0,,\r\n"
                     << "2025020100,SDOT02,SDOT02,,,5.0,80,350,1.0,,\r\n"
                     << "2025020101,SDOT02,SDOT02,,,,80,10,1.0,,\r\n"
                     << "2025020100,SDOT03,SDOT03,,,,,,,,,\r\n";
            dataSdot.close();
        }

        DataLoader sdotLoader;
        CHECK(sdotLoader.loadStations("build/tests/test_sdot_stations.csv", StationType::SDOT));
        CHECK(sdotLoader.loadSdot("build/tests/test_sdot.csv"));

        DynamicArray<Station> sdotAvg;
        sdotLoader.averageStations({ 2025, 2, 1, 0 }, { 2025, 2, 1, 23 }, sdotAvg);
        CHECK(sdotAvg.size() == 3);

        // SDOT01 검증
        CHECK(sdotAvg[0].pm10 == -1.0 && sdotAvg[0].pm25 == -1.0);
        CHECK(std::fabs(sdotAvg[0].temp - (-1.0)) < 1e-9);
        CHECK(std::fabs(sdotAvg[0].humidity - 60.0) < 1e-9);
        CHECK(std::fabs(sdotAvg[0].windSpeed - 2.0) < 1e-9);
        CHECK(std::fabs(sdotAvg[0].windDir - 60.0) < 1e-3);

        // SDOT02 검증
        CHECK(std::fabs(sdotAvg[1].temp - 5.0) < 1e-9);
        CHECK(std::fabs(sdotAvg[1].humidity - 80.0) < 1e-9);
        CHECK(std::fabs(sdotAvg[1].windSpeed - 1.0) < 1e-9);
        CHECK(std::fabs(sdotAvg[1].windDir - 0.0) < 1e-3);

        // SDOT03 검증 (모두 결측)
        CHECK(sdotAvg[2].temp == -999.0);
        CHECK(sdotAvg[2].humidity == -1.0);
        CHECK(sdotAvg[2].windSpeed == -1.0);
        CHECK(sdotAvg[2].windDir == -1.0);
    }

    // 8. buildGrid에서 grid.temp IDW (temp == -999 제외) 및 grid.pm25 IDW (pm25 < 0 제외)
    {
        DynamicArray<Station> mixedStations;

        // 관측소 1: PM2.5만 있고 기온은 결측 (-999)
        Station s1;
        s1.name = "PM_Only";
        s1.lat = 37.50;
        s1.lng = 127.00;
        s1.pm25 = 20.0;
        s1.temp = -999.0;
        mixedStations.push_back(s1);

        // 관측소 2: 기온만 있고 PM2.5는 결측 (-1)
        Station s2;
        s2.name = "Temp_Only";
        s2.lat = 37.50;
        s2.lng = 127.00;
        s2.pm25 = -1.0;
        s2.temp = 12.0;
        mixedStations.push_back(s2);

        Grid mixedGrid;
        buildGrid(mixedStations, 100.0, mixedGrid);

        int r = (int)std::floor((37.50 - mixedGrid.minLat) / mixedGrid.cellLat);
        int c = (int)std::floor((127.00 - mixedGrid.minLng) / mixedGrid.cellLng);
        int idx = r * mixedGrid.cols + c;

        // 해당 칸에서 PM2.5는 s1의 20.0, 기온은 s2의 12.0이 반영되어야 함
        CHECK(std::fabs(mixedGrid.pm25[idx] - 20.0) < 1.0);
        CHECK(std::fabs(mixedGrid.temp[idx] - 12.0) < 1.0);

        // 기온이 -999인 s1은 grid.temp 계산에 오염을 주지 않아야 함
        CHECK(mixedGrid.temp[idx] > 0.0);
    }

    std::printf(failed == 0 ? ">> 데이터 모듈 테스트 모두 통과!\n" : ">> 실패 %d개 발생\n", failed);
    return failed == 0 ? 0 : 1;
}
