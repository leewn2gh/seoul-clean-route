CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Isrc -MMD -MP   # -MMD: 헤더가 바뀌면 그 헤더를 쓰는 파일도 다시 빌드
EMXX     = em++

SRCS = $(wildcard src/*.cpp src/*/*.cpp)
OBJS = $(patsubst src/%.cpp,build/%.o,$(SRCS))
LIB_OBJS = $(filter-out build/main.o,$(OBJS))

.PHONY: all test wasm clean

all: build/app

build/app: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

# tests/test_*.cpp 를 각각 빌드해서 실행
test: $(LIB_OBJS)
	@mkdir -p build/tests
	@for t in $(wildcard tests/test_*.cpp); do \
		name=$$(basename $$t .cpp); \
		$(CXX) $(CXXFLAGS) $$t $(LIB_OBJS) -o build/tests/$$name && ./build/tests/$$name || exit 1; \
	done

# 웹에 실어 보낼 데이터 (페이지를 열 때마다 내려받으므로 필요한 파일만)
#   osm_walk.csv(44MB) 대신 미리 계산한 보행로 칸 passable_25m.bin(약 450KB), S-DoT(월 30MB)는 넣지 않음
WASM_FILES = $(wildcard data/processed/stations.csv data/processed/air_*.csv data/processed/simple_*.csv data/processed/passable_*.bin)
WASM_DATA = $(foreach f,$(WASM_FILES),--preload-file $(f)@/data/$(notdir $(f)))

# WebAssembly 빌드 (Emscripten 필요). 웹은 src/core/Api.cpp의 api_* 함수를 부름 (main은 실행하지 않음)
wasm:
	$(EMXX) -std=c++17 -O2 -Isrc $(SRCS) -o web/app.js \
		-s MODULARIZE=1 -s EXPORT_NAME=createApp -s INVOKE_RUN=0 -s ENVIRONMENT=web \
		-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]' \
		-s ALLOW_MEMORY_GROWTH=1 \
		$(WASM_DATA)

clean:
	rm -rf build web/app.js web/app.wasm web/app.data
