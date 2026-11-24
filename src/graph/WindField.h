// 담당: 김재겸
// 바람 기반 미세먼지 예측 (Stable Fluid)
// S-DoT 바람 센서의 풍향·풍속으로 격자 바람장을 만들고, 그 바람을 따라 격자 미세먼지를 옮겨 몇 분 뒤 농도를 예측합니다.
// 프로토타입: prototype/stable_fluid (임의 바람) → 여기서는 실제 관측 바람 사용
#pragma once
#include "common/Types.h"

// grid의 미세먼지를 stations의 바람(windDir, windSpeed가 0 이상인 관측소)을 따라 minutes분 동안 옮긴 결과를 out에 저장
// out은 grid를 복사한 뒤 pm25만 바뀜. 바람 값이 있는 관측소가 없으면 out = grid (그대로)
void predictPm(const Grid& grid, const DynamicArray<Station>& stations, int minutes, Grid& out);

// predictPm 안에서 쓰는 바람장. 같은 바람으로 여러 번 예측할 때(1·2·3시간 뒤) 직접 쓰면 바람장을 한 번만 만듦
class WindField {
public:
    // grid와 같은 범위에 바람장을 만듦 (바람 격자는 약 300m 간격)
    // 1) 관측 바람을 역거리 가중 보간 → 2) 관측소 주변(1km)을 관측값 쪽으로 당기며 이류·투영을 반복해 비압축 바람장으로 수렴
    // 바람 값이 있는 관측소가 하나도 없으면 false
    bool build(const Grid& grid, const DynamicArray<Station>& stations);

    // pm(grid와 같은 칸 수)을 minutes분 동안 바람을 따라 옮김 (semi-Lagrangian, 1분 간격)
    // 음수(값 없음) 칸은 그대로 두고, 주변 값을 가져올 때도 뺌. 격자 밖에서 들어오는 공기는 가장자리 값과 같다고 봄
    void advect(DynamicArray<double>& pm, int minutes) const;

    // 격자 칸 위치의 바람 (u: 동쪽 +, v: 북쪽 +, m/s)
    void windAt(int cell, double& u, double& v) const;

    // 관측소 위치에서 계산한 바람과 관측 바람의 평균 차이 (풍향 도, 풍속 m/s)
    void fitError(double& dirDeg, double& speed) const;

    int stationCount() const { return obsX_.size(); }

private:
    // 미세먼지 격자 정보
    int rows_ = 0, cols_ = 0;
    double hx_ = 0.0, hy_ = 0.0;      // 칸 한 변 길이 (m), 동서·남북
    // 바람 격자: 미세먼지 격자 ds_ × ds_ 칸을 한 칸으로 묶음
    int ds_ = 1, wr_ = 0, wc_ = 0;
    DynamicArray<double> u_, v_;      // 바람 격자 칸별 바람 (m/s)
    // 관측소 (바람 격자 좌표, 칸 중심 = 정수)
    DynamicArray<double> obsX_, obsY_, obsU_, obsV_;

    double sample(const DynamicArray<double>& f, double x, double y) const;   // 바람 격자 쌍선형 보간
};
