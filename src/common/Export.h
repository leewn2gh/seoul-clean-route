// 담당: 이준호
// 계산 결과를 웹에서 읽는 JSON / GeoJSON 문자열로 만듭니다.
// 문자열 함수는 웹(WebAssembly)에서 바로 돌려주고, write 함수는 같은 내용을 파일로 저장합니다(콘솔 실행용).
#pragma once
#include <string>
#include "common/Types.h"
#include "station/StationGraph.h"
#include "search/Trie.h"

// 격자를 압축 형식으로: {"rows","cols","minLat","minLng","cellLat","cellLng","step","min","max","values":[...]}
// step×step 칸을 하나로 묶어 평균(통행 가능하고 값이 있는 칸만). 값이 없는 묶음은 null
std::string gridJSON(const Grid& grid, int step = 1);

// 최적 경로와 최단 경로를 선(LineString) 두 개로. 거리·노출량·소요 시간, 칸별 미세먼지(pm25s)도 속성으로 넣음
std::string routesGeoJSON(const Route& best, const Route& shortest);

// Top / Worst 지역 순위 {"top":[{"name":..,"score":..}], "worst":[...]}
std::string rankingJSON(const DynamicArray<RankItem>& top, const DynamicArray<RankItem>& worst);

// 검색 결과 [{"name":..,"lat":..,"lng":..}]
std::string placesJSON(const DynamicArray<Place>& places);

// 칸별 미세먼지를 사각형(Polygon)으로 저장. step: step×step 칸을 하나로 묶어 저장 (파일 크기 조절)
// 묶은 칸 중 통행 가능하고 값이 있는 칸의 평균을 씀. 그런 칸이 없으면 저장하지 않음
bool writeGridGeoJSON(const Grid& grid, const std::string& path, int step = 1);

// 위 문자열 함수의 결과를 파일로 저장. 성공하면 true
bool writeTextFile(const std::string& path, const std::string& text);
bool writeRoutesGeoJSON(const Route& best, const Route& shortest, const std::string& path);
bool writeRankingJSON(const DynamicArray<RankItem>& top, const DynamicArray<RankItem>& worst,
                      const std::string& path);
