// 담당: 김재겸
// OSM 보행로를 격자에 반영합니다. 보행로가 지나는 칸만 통행 가능(passable = true)으로 만듭니다.
#pragma once
#include <string>
#include "common/Types.h"

// path: 보행로 선분 파일 (data/processed/osm_walk.csv, 열: lat1,lng1,lat2,lng2)
// 1) grid.passable을 칸 수만큼 모두 false로 만들고
// 2) 선분마다 칸 크기 절반 간격으로 점을 찍어 그 점이 들어간 칸을 true로 바꿈 (격자 밖 점은 건너뜀)
// 파일을 열지 못하거나 읽은 선분이 하나도 없으면 false (이때 passable은 건드리지 않음)
bool applyRoads(const std::string& path, Grid& grid);
