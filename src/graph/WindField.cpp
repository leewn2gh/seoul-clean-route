// 담당: 김재겸
#include "graph/WindField.h"
#include <cmath>

namespace {
const double PI = 3.14159265358979323846;
const double M_PER_LAT = 111000.0;     // buildGrid와 같은 값
const double WIND_CELL = 300.0;        // 바람 격자 간격 목표 (m)
const double NUDGE_SIGMA = 1000.0;     // 관측소 영향 반경 σ (m)
const double NUDGE_TAU = 120.0;        // 관측 바람으로 당기는 시간 상수 (s)
const double SPIN_DT = 60.0;           // 바람장 수렴 단계 시간 간격 (s)
const int SPIN_STEPS = 60;             // 수렴 단계 반복 (60 × 60s = 1시간)
const int JACOBI = 40;                 // 투영 때 압력 Jacobi 반복 (이전 압력에서 시작)
const double PM_DT = 60.0;             // 미세먼지 이동 시간 간격 (s)

double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }

// 바람 격자 한 장에 대한 계산 (Stable Fluid의 이류·투영)
struct Solver {
    int wr, wc;
    double hx, hy;   // 바람 격자 칸 크기 (m)
    int id(int i, int j) const { return j * wc + i; }

    double bilerp(const DynamicArray<double>& f, double x, double y) const {
        x = clampd(x, 0.0, wc - 1.0001);
        y = clampd(y, 0.0, wr - 1.0001);
        int i = (int)x, j = (int)y;
        double tx = x - i, ty = y - j;
        return f[id(i, j)] * (1 - tx) * (1 - ty) + f[id(i + 1, j)] * tx * (1 - ty)
             + f[id(i, j + 1)] * (1 - tx) * ty + f[id(i + 1, j + 1)] * tx * ty;
    }

    bool edge(int i, int j) const { return i == 0 || j == 0 || i == wc - 1 || j == wr - 1; }

    // 가장자리 바람은 배경 바람(관측 보간)으로 고정
    void boundary(DynamicArray<double>& u, DynamicArray<double>& v,
                  const DynamicArray<double>& ub, const DynamicArray<double>& vb) const {
        for (int j = 0; j < wr; ++j) for (int i = 0; i < wc; ++i)
            if (edge(i, j)) { u[id(i, j)] = ub[id(i, j)]; v[id(i, j)] = vb[id(i, j)]; }
    }

    // 발산 제거: ∇²p = ∇·u 를 Jacobi로 풀고 u -= ∇p (가장자리 압력 0)
    void project(DynamicArray<double>& u, DynamicArray<double>& v, DynamicArray<double>& p,
                 DynamicArray<double>& pt, DynamicArray<double>& div) const {
        double ax = 1.0 / (hx * hx), ay = 1.0 / (hy * hy), den = 2 * ax + 2 * ay;
        for (int j = 1; j < wr - 1; ++j) for (int i = 1; i < wc - 1; ++i)
            div[id(i, j)] = (u[id(i + 1, j)] - u[id(i - 1, j)]) / (2 * hx) + (v[id(i, j + 1)] - v[id(i, j - 1)]) / (2 * hy);
        for (int it = 0; it < JACOBI; ++it) {
            for (int j = 1; j < wr - 1; ++j) for (int i = 1; i < wc - 1; ++i)
                pt[id(i, j)] = ((p[id(i - 1, j)] + p[id(i + 1, j)]) * ax + (p[id(i, j - 1)] + p[id(i, j + 1)]) * ay
                               - div[id(i, j)]) / den;
            p.swap(pt);   // 가장자리는 두 배열 모두 0으로 유지됨
        }
        for (int j = 1; j < wr - 1; ++j) for (int i = 1; i < wc - 1; ++i) {
            u[id(i, j)] -= (p[id(i + 1, j)] - p[id(i - 1, j)]) / (2 * hx);
            v[id(i, j)] -= (p[id(i, j + 1)] - p[id(i, j - 1)]) / (2 * hy);
        }
    }
};
}  // namespace

bool WindField::build(const Grid& grid, const DynamicArray<Station>& stations) {
    rows_ = grid.rows; cols_ = grid.cols;
    double midLat = grid.minLat + grid.rows * grid.cellLat / 2.0;
    hy_ = grid.cellLat * M_PER_LAT;
    hx_ = grid.cellLng * M_PER_LAT * std::cos(midLat * PI / 180.0);
    u_.clear(); v_.clear();
    obsX_.clear(); obsY_.clear(); obsU_.clear(); obsV_.clear();
    if (rows_ <= 0 || cols_ <= 0 || hx_ <= 0 || hy_ <= 0) return false;

    // 바람 격자 크기: 미세먼지 칸 ds × ds개를 묶어 약 300m. 너무 작아지면 묶지 않음
    ds_ = (int)std::lround(WIND_CELL / hy_);
    if (ds_ < 1) ds_ = 1;
    wc_ = (cols_ + ds_ - 1) / ds_;
    wr_ = (rows_ + ds_ - 1) / ds_;
    if (wc_ < 3 || wr_ < 3) { ds_ = 1; wc_ = cols_; wr_ = rows_; }
    double off = (ds_ - 1) / 2.0;   // 바람 칸 중심의 미세먼지 칸 좌표 = I × ds + off

    // 관측소: 기상 풍향은 '불어오는 방향'이라 부호 반대. u = -s·sin(θ), v = -s·cos(θ)
    for (int s = 0; s < stations.size(); ++s) {
        const Station& st = stations[s];
        if (st.windDir < 0 || st.windSpeed < 0) continue;
        double c = (st.lng - grid.minLng) / grid.cellLng - 0.5;   // 미세먼지 칸 좌표 (칸 중심 = 정수)
        double r = (st.lat - grid.minLat) / grid.cellLat - 0.5;
        if (c < -0.5 || c > cols_ - 0.5 || r < -0.5 || r > rows_ - 0.5) continue;   // 격자 밖
        double th = st.windDir * PI / 180.0;
        obsX_.push_back((c - off) / ds_);
        obsY_.push_back((r - off) / ds_);
        obsU_.push_back(-st.windSpeed * std::sin(th));
        obsV_.push_back(-st.windSpeed * std::cos(th));
    }
    int ns = obsX_.size();
    if (ns == 0) return false;

    Solver S{wr_, wc_, hx_ * ds_, hy_ * ds_};
    int n = wr_ * wc_;
    DynamicArray<double> ub, vb, ut, vt, a, u1, v1, p, pt, div;
    DynamicArray<double>* all[] = {&ub, &vb, &ut, &vt, &a, &u_, &v_, &u1, &v1, &p, &pt, &div};
    for (DynamicArray<double>* f : all) f->resize(n, 0.0);

    // 배경 바람: 역거리 가중 보간 1/(d² + 칸²). 외력 목표: 관측소 주변 가우시안 가중 평균
    double soft = S.hx * S.hx;
    for (int j = 0; j < wr_; ++j) for (int i = 0; i < wc_; ++i) {
        double w = 0, wu = 0, wv = 0, g = 0, gu = 0, gv = 0;
        for (int s = 0; s < ns; ++s) {
            double dx = (i - obsX_[s]) * S.hx, dy = (j - obsY_[s]) * S.hy;
            double d2 = dx * dx + dy * dy;
            double iw = 1.0 / (d2 + soft);
            w += iw; wu += iw * obsU_[s]; wv += iw * obsV_[s];
            double gg = std::exp(-d2 / (2 * NUDGE_SIGMA * NUDGE_SIGMA));
            g += gg; gu += gg * obsU_[s]; gv += gg * obsV_[s];
        }
        int k = S.id(i, j);
        ub[k] = wu / w; vb[k] = wv / w;
        ut[k] = g > 1e-9 ? gu / g : ub[k];
        vt[k] = g > 1e-9 ? gv / g : vb[k];
        a[k] = g < 1.0 ? g : 1.0;
        u_[k] = ub[k]; v_[k] = vb[k];
    }
    if (wr_ < 3 || wc_ < 3) return true;   // 투영할 안쪽 칸이 없음 → 보간한 바람 그대로

    for (int r = 0; r < 5; ++r) S.project(u_, v_, p, pt, div);   // 처음 바람장을 비압축성으로

    // 수렴 단계: 외력(관측 쪽으로 당김) → 이류 → 투영 반복. 관측 바람은 시각 t 값으로 고정
    double rate = SPIN_DT / NUDGE_TAU;
    for (int step = 0; step < SPIN_STEPS; ++step) {
        for (int k = 0; k < n; ++k) {
            u_[k] += rate * a[k] * (ut[k] - u_[k]);
            v_[k] += rate * a[k] * (vt[k] - v_[k]);
        }
        for (int j = 1; j < wr_ - 1; ++j) for (int i = 1; i < wc_ - 1; ++i) {
            int k = S.id(i, j);
            double x = i - SPIN_DT * u_[k] / S.hx, y = j - SPIN_DT * v_[k] / S.hy;
            u1[k] = S.bilerp(u_, x, y);
            v1[k] = S.bilerp(v_, x, y);
        }
        S.boundary(u1, v1, ub, vb);
        S.project(u1, v1, p, pt, div);
        S.boundary(u1, v1, ub, vb);
        u_.swap(u1); v_.swap(v1);
    }
    return true;
}

double WindField::sample(const DynamicArray<double>& f, double x, double y) const {
    Solver S{wr_, wc_, hx_ * ds_, hy_ * ds_};
    return S.bilerp(f, x, y);
}

void WindField::windAt(int cell, double& u, double& v) const {
    u = v = 0.0;
    if (u_.empty() || cell < 0 || cell >= rows_ * cols_) return;
    double off = (ds_ - 1) / 2.0;
    double x = (cell % cols_ - off) / ds_, y = (cell / cols_ - off) / ds_;
    u = sample(u_, x, y);
    v = sample(v_, x, y);
}

void WindField::fitError(double& dirDeg, double& speed) const {
    dirDeg = speed = 0.0;
    int nDir = 0, ns = obsX_.size();
    for (int s = 0; s < ns; ++s) {
        double mu = sample(u_, obsX_[s], obsY_[s]), mv = sample(v_, obsX_[s], obsY_[s]);
        double os = std::sqrt(obsU_[s] * obsU_[s] + obsV_[s] * obsV_[s]);
        speed += std::fabs(std::sqrt(mu * mu + mv * mv) - os);
        if (os < 0.5) continue;   // 거의 무풍이면 풍향 비교에서 뺌
        double d = std::fabs(std::atan2(mv, mu) - std::atan2(obsV_[s], obsU_[s])) * 180.0 / PI;
        if (d > 180.0) d = 360.0 - d;
        dirDeg += d; ++nDir;
    }
    if (ns > 0) speed /= ns;
    if (nDir > 0) dirDeg /= nDir;
}

void WindField::advect(DynamicArray<double>& pm, int minutes) const {
    int n = rows_ * cols_;
    if (u_.empty() || pm.size() != n || minutes <= 0) return;

    // 바람은 고정이므로 칸마다 한 스텝 동안 거슬러 올라갈 거리(칸 단위)를 미리 계산
    DynamicArray<double> bx, by, next;
    bx.resize(n); by.resize(n); next.resize(n);
    for (int k = 0; k < n; ++k) {
        double u, v;
        windAt(k, u, v);
        bx[k] = PM_DT * u / hx_;
        by[k] = PM_DT * v / hy_;
    }

    int steps = (int)std::lround(minutes * 60.0 / PM_DT);
    for (int step = 0; step < steps; ++step) {
        for (int r = 0; r < rows_; ++r) for (int c = 0; c < cols_; ++c) {
            int k = r * cols_ + c;
            if (pm[k] < 0) { next[k] = pm[k]; continue; }
            // 바람을 거슬러 올라간 위치의 값을 가져옴. 격자 밖이면 가장자리로 고정
            double x = clampd(c - bx[k], 0.0, cols_ - 1.0), y = clampd(r - by[k], 0.0, rows_ - 1.0);
            int i = (int)x, j = (int)y;
            if (i > cols_ - 2) i = cols_ - 2 < 0 ? 0 : cols_ - 2;
            if (j > rows_ - 2) j = rows_ - 2 < 0 ? 0 : rows_ - 2;
            double tx = x - i, ty = y - j;
            int i1 = i + 1 < cols_ ? i + 1 : i, j1 = j + 1 < rows_ ? j + 1 : j;
            int ks[4] = {j * cols_ + i, j * cols_ + i1, j1 * cols_ + i, j1 * cols_ + i1};
            double ws[4] = {(1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty};
            double sum = 0, wsum = 0;
            for (int q = 0; q < 4; ++q) {
                if (pm[ks[q]] < 0) continue;   // 값 없는 칸은 빼고 가중 평균
                sum += ws[q] * pm[ks[q]]; wsum += ws[q];
            }
            next[k] = wsum > 1e-12 ? sum / wsum : pm[k];
        }
        pm.swap(next);
    }
}

void predictPm(const Grid& grid, const DynamicArray<Station>& stations, int minutes, Grid& out) {
    out = grid;
    WindField wind;
    if (!wind.build(grid, stations)) return;
    wind.advect(out.pm25, minutes);
}
