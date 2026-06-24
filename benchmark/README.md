# oatpp Benchmark

HTTP benchmark with 10 scenarios, sync + async servers, live-updating web dashboard, and flamegraph profiling.

## Quick Start

```bash
# Sync server, all scenarios
./benchmark/scripts/run-benchmark.sh -m sync

# Async server, single scenario by name
./benchmark/scripts/run-benchmark.sh -m async -s "Hello World"

# With flamegraph profiling
./benchmark/scripts/run-benchmark.sh -m async -s json -p

# List available scenarios
./benchmark/scripts/run-benchmark.sh -m async -l
```

Opens `http://localhost:8080` — results stream in as benchmarks run. Benchmark results are saved to `benchmark/results/results-*.csv`.

## Flamegraph

Use `-p / --perf` to record `perf.data` and generate an interactive flamegraph SVG on the fly. The flamegraph opens fullscreen with a toolbar and live search.

| Pipeline | Description |
|----------|-------------|
| `perf record --call-graph dwarf -F 99` | DWARF-based unwinding — clean stacks, zero `[unknown]` frames |
| `perf script --no-inline` | Fast decode (skips inline expansion, preferred for flamegraphs) |
| `stackcollapse-perf.pl \| flamegraph.pl` | Brendan Gregg's standard toolchain |

FlameGraph tools are auto-downloaded from GitHub if not found in `benchmark/results/FlameGraph/`.

## Prerequisites

- C++17 compiler, CMake 3.20+
- Python 3.8+
- wrk (`sudo apt install wrk` / `brew install wrk`)
- perf (Linux kernel tool, for `-p` flamegraph mode)

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

## CLI Reference

```
run-benchmark.sh -m MODE [-s SCENARIO ...] [-p] [-l]

  -m MODE      Server mode: sync | async
  -s SELECTOR  Scenario: exact name, partial match, index, or .lua filename
  -p           Record perf profile and generate flamegraph SVG
  -l           List available scenarios and exit
```

## Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `DURATION` | `2s` | wrk duration per scenario |
| `CONNECTIONS` | `1000` | wrk connections |
| `THREADS` | `10` | wrk threads |
| `PORT` | `8000` | oatpp server port |

## Manual Usage

```bash
# Build (Release)
cmake -S . -B build-benchmark -DOATPP_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-benchmark --target benchmark-sync -j$(nproc)

# Build (RelWithDebInfo — for perf profiling with symbols)
cmake -S . -B build-benchmark -DOATPP_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOATPP_USE_JSON_FAST_SERIALIZER=ON -DOATPP_USE_JSON_FAST_DESERIALIZER=ON -DOATPP_BUILD_TESTS=OFF
cmake --build build-benchmark --target benchmark-async -j$(nproc)

# Start server
./build-benchmark/benchmark/benchmark-sync 8000

# Run wrk
wrk -t4 -c100 -d30s -s benchmark/scripts/hello.lua --latency http://localhost:8000

# Profile with perf
perf record --call-graph dwarf -F 99 -p $(pgrep benchmark-async) -- sleep 30
perf script --no-inline | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
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
├── results/                  # gitignored; auto-created
│   ├── FlameGraph/           # flamegraph.pl + stackcollapse-perf.pl
│   ├── results-*.csv
│   └── flamegraph-*.svg
└── scripts/
    ├── run-benchmark.sh      # Shell wrapper
    ├── run-benchmark.py      # Python dashboard + runner
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
