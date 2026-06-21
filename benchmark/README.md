# oatpp Benchmark Suite

HTTP benchmark suite with 10 scenarios, sync + async servers, and a live-updating web dashboard.

## Quick Start

```bash
# One command — builds, starts server, opens browser with live dashboard
./benchmark/scripts/run-benchmark.sh sync

# Async version
./benchmark/scripts/run-benchmark.sh async

# Custom parameters
DURATION=30s CONNECTIONS=200 THREADS=8 ./benchmark/scripts/run-benchmark.sh sync
```

Opens `http://localhost:8080` — results stream in as benchmarks run.

## Prerequisites

- C++17 compiler, CMake 3.20+
- Python 3.8+
- wrk (`sudo apt install wrk` / `brew install wrk`)

## Scenarios

| # | Endpoint | Method | Description |
|---|----------|--------|-------------|
| 1 | `/hello` | GET | Plain text |
| 2 | `/json` | POST | Small DTO serialize + deserialize |
| 3 | `/json/large` | POST | 15-field DTO (~600B) round-trip |
| 4 | `/params/{id}` | GET | Path parameter → JSON |
| 5 | `/queries?name=X&age=Y` | GET | Query params → JSON |
| 6 | `/echo` | POST | Raw body echo |
| 7 | `/headers` | GET | Multi-header → JSON |
| 8 | `/mixed` | POST | String + Int + Bool + Vector + Enum |
| 9 | `/nested` | POST | 3-level nested JSON |
| 10 | `/array` | GET | 100-item JSON array |

## Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `DURATION` | `10s` | Wrk duration per scenario |
| `CONNECTIONS` | `100` | Wrk connections |
| `THREADS` | `4` | Wrk threads |
| `PORT` | `8000` | oatpp server port |

## Manual Usage

```bash
# Build
cmake -S . -B build -DOATPP_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target benchmark-sync -j$(nproc)

# Start server
./build/benchmark/benchmark-sync 8000

# Run wrk
wrk -t4 -c100 -d30s -s benchmark/scripts/hello.lua --latency http://localhost:8000
```

## Directory Structure

```
benchmark/
├── DTOs.hpp
├── sync/
│   ├── AppComponent.hpp      # DI components
│   ├── BenchController.hpp   # ENDPOINT macros
│   └── App.cpp               # main()
├── async/
│   ├── AppComponent.hpp      # DI + Executor
│   ├── BenchController.hpp   # ENDPOINT_ASYNC macros
│   └── App.cpp               # main()
└── scripts/
    ├── run-benchmark.sh      # Shell wrapper
    ├── run_bench.py          # Python dashboard + runner
    └── *.lua                 # 10 wrk scripts
```

## Architecture

```
AppComponent (DI registry)
    ├── ObjectMapper         (JSON)
    ├── ServerConnectionProvider (TCP)
    ├── HttpRouter
    └── ConnectionHandler    (sync: HttpConnectionHandler
                                  async: AsyncHttpConnectionHandler + Executor)
            │
            ▼
    BenchController (10 endpoints)
            │
            ▼
    server.run(condition) — Ctrl+C to stop
```
