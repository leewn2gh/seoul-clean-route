// 담당: 조효근
#include <cstdio>
#include <cstdlib>
#include "route/Router.h"
#include "route/MinHeap.h"
#include "route/Stack.h"
#include "route/Queue.h"

// passable이 비어 있으면(OSM 반영 전) 모든 칸을 통행 가능으로 봄 (GridGraph와 같은 규칙)
static bool passableAt(const Grid& g, int c) {
    return g.passable.empty() || g.passable[c];
}

// 공통 경로 복원 함수 (다익스트라와 A*가 공통으로 사용)
static void reconstructRoute(const GridGraph& graph, int goal, const DynamicArray<int>& prev, Route& out) {
    Stack<int> pathStack;
    int curr = goal;
    while (curr != -1) {
        pathStack.push(curr);
        curr = prev[curr];
    }

    // 출력 데이터 초기화
    out.lats.clear();
    out.lngs.clear();
    out.pm25s.clear();
    out.distance = 0.0;
    out.exposure = 0.0;
    out.minutes = 0.0;

    const Grid& g = graph.grid();
    int prev_node = -1;

    // 스택에서 꺼내며 출발지 -> 도착지 순서로 복원 및 수치 계산
    while (!pathStack.empty()) {
        int u = pathStack.top();
        pathStack.pop();

        double lat = 0.0, lng = 0.0;
        graph.cellCenter(u, lat, lng);
        out.lats.push_back(lat);
        out.lngs.push_back(lng);

        // 칸별 pm25 값 넣기 (lats와 같은 순서, 비어있거나 음수면 기준값 35.0)
        double pm_u = (u < g.pm25.size() && g.pm25[u] >= 0.0) ? g.pm25[u] : 35.0;
        out.pm25s.push_back(pm_u);

        // 첫 번째 칸(출발지)이 아니면 거리 및 미세먼지 노출량 누적
        if (prev_node != -1) {
            double len = graph.length(prev_node, u);
            out.distance += len;

            double pm_prev = (prev_node < g.pm25.size() && g.pm25[prev_node] >= 0.0) ? g.pm25[prev_node] : 35.0;
            out.exposure += len * ((pm_prev + pm_u) / 2.0); // 거리 × 평균 미세먼지
        }
        prev_node = u;
    }

    // 시간 계산: 4.5km/h = 분당 75m 이동 (4500 / 60 = 75)
    out.minutes = out.distance / 75.0;
}

int nearestPassable(const GridGraph& graph, int c, int maxRadius) {
    int n = graph.vertexCount();
    if (c < 0 || c >= n) return -1;

    const Grid& g = graph.grid();

    // 이미 통행 가능하면 자기 자신 반환
    if (passableAt(g, c)) {
        return c;
    }

    // BFS를 위한 방문 처리 배열
    DynamicArray<bool> visited;
    visited.resize(n, false);

    Queue<int> q;
    q.enqueue(c);
    visited[c] = true;

    int startR = c / g.cols;
    int startC = c % g.cols;

    // 8방향 (상하좌우 + 대각선)
    const int dr[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
    const int dc[8] = {0, 0, -1, 1, -1, 1, -1, 1};

    // BFS 탐색
    while (!q.empty()) {
        int u = q.front();
        q.dequeue();

        // 큐에서 꺼낸 칸이 통행 가능하면 가장 가까운 칸이므로 즉시 반환
        if (passableAt(g, u)) {
            return u;
        }

        int r = u / g.cols, col = u % g.cols;
        for (int i = 0; i < 8; ++i) {
            int nr = r + dr[i], nc = col + dc[i];
            if (nr < 0 || nr >= g.rows || nc < 0 || nc >= g.cols) continue;

            // 시작 칸 c로부터 maxRadius 반경을 벗어나면 탐색하지 않음
            if (std::abs(nr - startR) > maxRadius || std::abs(nc - startC) > maxRadius) continue;

            int v = nr * g.cols + nc;
            if (!visited[v]) {
                visited[v] = true;
                q.enqueue(v);
            }
        }
    }

    return -1; // maxRadius칸 안에서 통행 가능한 칸을 찾지 못한 경우
}

bool findRoute(const GridGraph& graph, int start, int goal, const WeightOption& opt, Route& out, int* popCount) {
    int n = graph.vertexCount();
    if (start < 0 || start >= n || goal < 0 || goal >= n) {
        if (popCount != nullptr) *popCount = 0;
        return false;
    }

    double INF = 1e18;
    DynamicArray<double> dist;
    dist.resize(n, INF);
    DynamicArray<int> prev;
    prev.resize(n, -1);

    dist[start] = 0.0;

    MinHeap<int> pq;
    pq.push(0.0, start);

    DynamicArray<int> nbors;
    int pops = 0;

    // 다익스트라 메인 루프
    while (!pq.empty()) {
        MinHeap<int>::Item item = pq.pop();
        pops++;

        double d = item.priority;
        int u = item.value;

        // 목표 칸에 도달하면 탐색 종료
        if (u == goal) break;

        // 큐에서 꺼낸 거리가 현재 기록된 거리보다 크면 무시
        if (d > dist[u]) continue;

        nbors.clear();
        graph.neighbors(u, nbors);

        for (int i = 0; i < nbors.size(); ++i) {
            int v = nbors[i];
            double w = graph.weight(u, v, opt);

            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = u;
                pq.push(dist[v], v);
            }
        }
    }

    if (popCount != nullptr) {
        *popCount = pops;
    }

    // 목표지에 도달할 수 없는 경우 (경로 없음)
    if (dist[goal] == INF && start != goal) {
        return false;
    }

    reconstructRoute(graph, goal, prev, out);
    return true;
}

bool findRouteAStar(const GridGraph& graph, int start, int goal, const WeightOption& opt, Route& out, int* popCount) {
    int n = graph.vertexCount();
    if (start < 0 || start >= n || goal < 0 || goal >= n) {
        if (popCount != nullptr) *popCount = 0;
        return false;
    }

    double INF = 1e18;
    DynamicArray<double> dist;   // 출발지부터 현재 칸까지의 실제 누적 비용 (gScore)
    dist.resize(n, INF);
    DynamicArray<int> prev;
    prev.resize(n, -1);
    DynamicArray<bool> closed;   // 이미 최단 거리가 확정되어 처리된 칸 표시
    closed.resize(n, false);

    dist[start] = 0.0;

    MinHeap<int> pq;
    pq.push(graph.length(start, goal), start);

    DynamicArray<int> nbors;
    int pops = 0;

    // A* 메인 루프
    while (!pq.empty()) {
        MinHeap<int>::Item item = pq.pop();
        pops++;

        int u = item.value;

        // 이미 최적 경로가 확정된 칸이면 무시 (중복 삽입된 구버전 데이터 건너뛰기)
        if (closed[u]) continue;
        closed[u] = true;

        // 목표 칸에 도달하면 탐색 종료
        if (u == goal) break;

        nbors.clear();
        graph.neighbors(u, nbors);

        for (int i = 0; i < nbors.size(); ++i) {
            int v = nbors[i];
            if (closed[v]) continue;

            double w = graph.weight(u, v, opt);

            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = u;
                // 우선순위 = 실제 누적 비용 + 목표까지의 직선 거리 (휴리스틱)
                double priority = dist[v] + graph.length(v, goal);
                pq.push(priority, v);
            }
        }
    }

    if (popCount != nullptr) {
        *popCount = pops;
    }

    // 목표지에 도달할 수 없는 경우 (경로 없음)
    if (dist[goal] == INF && start != goal) {
        return false;
    }

    reconstructRoute(graph, goal, prev, out);
    return true;
}

bool compareRoutes(const GridGraph& graph, int start, int goal, const WeightOption& opt,
                   Route& best, Route& shortest) {
    // 1. 주어진 가중치 비중(opt)을 적용한 최적 경로 탐색
    bool foundBest = findRoute(graph, start, goal, opt, best);
    
    // 2. 미세먼지 비중을 0으로 강제 설정하여 최단 경로(거리 기준) 탐색
    WeightOption shortOpt;
    shortOpt.alpha = 0.0;
    shortOpt.beta = 0.0;
    bool foundShortest = findRoute(graph, start, goal, shortOpt, shortest);
    
    return foundBest && foundShortest;
}