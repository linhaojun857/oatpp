#!/bin/bash
# =============================================================================
# Verify all 10 benchmark endpoints (sync + async)
# Usage: ./verify.sh [sync|async]
# =============================================================================
set -uo pipefail

RED='\033[31m'; GRN='\033[32m'; CYN='\033[36m'; DIM='\033[2m'; RST='\033[0m'
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

SERVER="${1:-}"
if [[ "$SERVER" != "sync" && "$SERVER" != "async" ]]; then
    echo "Usage: $0 [sync|async]"
    exit 1
fi

BENCH_BINARY="benchmark-${SERVER}"
PORT=8000
H="http://localhost:${PORT}"

ok() { echo -e "  ${GRN}PASS${RST}  $1  ->  $2"; PASS=$((PASS+1)); }
no() { echo -e "  ${RED}FAIL${RST}  $1  ->  $2  ${DIM}(expected: $3)${RST}"; FAIL=$((FAIL+1)); }

echo ""
echo -e "  ${CYN}oatpp Benchmark Endpoint Verification${RST}"
echo -e "  ${DIM}server: ${SERVER}${RST}"
echo ""

# ---- Build -----------------------------------------------------------------
echo -e "  ${DIM}Building...${RST}"
cd "$PROJECT_ROOT"
cmake -S . -B build -DOATPP_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release \
    -DOATPP_BUILD_TESTS=OFF > /dev/null 2>&1
cmake --build build --target "$BENCH_BINARY" -j"$(nproc)" > /dev/null 2>&1

# ---- Start server -----------------------------------------------------------
echo -e "  ${DIM}Starting ${SERVER} server...${RST}"
lsof -ti ":${PORT}" | xargs kill -9 2>/dev/null; true
sleep 0.3
"$PROJECT_ROOT/build/benchmark/$BENCH_BINARY" "$PORT" > /dev/null 2>&1 &
SERVER_PID=$!

for i in $(seq 1 15); do
    sleep 0.2
    kill -0 "$SERVER_PID" 2>/dev/null || { echo -e "  ${RED}Server died${RST}"; exit 1; }
    curl -s "${H}/hello" > /dev/null 2>&1 && break
done
echo ""

# ---- Verify -----------------------------------------------------------------
PASS=0; FAIL=0

# 1. hello
out=$(curl -s "${H}/hello")
echo "$out" | grep -q "Hello, Benchmark World" && ok "hello           " "$out" || no "hello           " "$out" "Hello, Benchmark World"

# 2. json_small
out=$(curl -s -X POST "${H}/json" -H "Content-Type: application/json" \
  -d '{"message":"hi","value":42,"number":3.14}')
echo "$out" | grep -q '"message":"hi"' && ok "json_small      " "$out" || no "json_small      " "$out" "message"

# 3. json_large
out=$(curl -s -X POST "${H}/json/large" -H "Content-Type: application/json" \
  -d '{"field1":"test","field2":1,"field3":1.0,"field4":true,"field5":"a","field6":"b","field7":"c","field8":1,"field9":2,"field10":3.0,"field11":[1,2],"field12":["x"],"field13":1.0,"field14":999,"field15":false}')
echo "$out" | grep -q '"field1":"test"' && ok "json_large      " "$out" || no "json_large      " "$out" "field1"

# 4. path_param
out=$(curl -s "${H}/params/benchmark-42")
echo "$out" | grep -q '"message":"param=benchmark-42"' && ok "path_param      " "$out" || no "path_param      " "$out" "param=benchmark-42"

# 5. query_params
out=$(curl -s "${H}/queries?name=foo&age=25")
echo "$out" | grep -q '"name=foo&age=25"' && ok "query_params    " "$out" || no "query_params    " "$out" "name=foo"

# 6. echo
out=$(curl -s -X POST "${H}/echo" -H "Content-Type: text/plain" -d "hello-body")
echo "$out" | grep -q "hello-body" && ok "echo            " "$out" || no "echo            " "$out" "hello-body"

# 7. headers
out=$(curl -s "${H}/headers" \
  -H "X-Header-1: h1val" \
  -H "X-Header-2: 42" \
  -H "X-Header-3: 3.14")
echo "$out" | grep -q '"message":"h1val"' && ok "headers         " "$out" || no "headers         " "$out" "h1val"

# 8. mixed_payload
out=$(curl -s -X POST "${H}/mixed" -H "Content-Type: application/json" \
  -d '{"name":"John","age":30,"score":95.5,"active":true,"tags":["a","b"],"favoriteColor":"BLUE"}')
echo "$out" | grep -q '"name":"John"' && ok "mixed_payload   " "$out" || no "mixed_payload   " "$out" "name"

# 9. nested_json
out=$(curl -s -X POST "${H}/nested" -H "Content-Type: application/json" \
  -d '{"title":"Root","nested":{"name":"Mid","child":{"value":"Deep","count":7}}}')
echo "$out" | grep -q '"title":"Root"' && ok "nested_json     " "$out" || no "nested_json     " "$out" "title"

# 10. array_response
out=$(curl -s "${H}/array")
count=$(echo "$out" | python3 -c "import sys,json;print(len(json.load(sys.stdin)))" 2>/dev/null || echo 0)
[[ "$count" == "100" ]] && ok "array_response  " "items=$count" || no "array_response  " "items=$count" "100"

# ---- Stop server -----------------------------------------------------------
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

echo ""
echo -e "  ${GRN}${PASS} passed${RST}  ${RED}${FAIL} failed${RST}"
echo ""
