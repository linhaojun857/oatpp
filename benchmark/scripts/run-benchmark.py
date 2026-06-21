#!/usr/bin/env python3
"""
oatpp Benchmark Suite — local web dashboard with live-updating results.

Usage:
  python3 run-benchmark.py [sync|async]

Opens http://localhost:8080 — frontend polls /api/state every 500ms.
"""
import os, sys, subprocess, re, json, time, threading, csv, traceback
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler

PORT       = int(os.environ.get("PORT", "8000"))
DURATION   = os.environ.get("DURATION", "10s")
CONN       = os.environ.get("CONNECTIONS", "100")
THREADS    = os.environ.get("THREADS", "4")
HTTP_PORT  = 8080
HERE       = os.path.dirname(os.path.abspath(__file__))
ROOT       = os.path.dirname(os.path.dirname(HERE))

SERVER_TYPE = sys.argv[1] if len(sys.argv) > 1 else "sync"
if SERVER_TYPE not in ("sync", "async"):
    print(f"Usage: {sys.argv[0]} [sync|async]")
    sys.exit(1)

BENCH_BINARY = f"benchmark-{SERVER_TYPE}"

SCENARIOS = [
    ("Hello World",      "hello.lua"),
    ("JSON (small)",     "json_small.lua"),
    ("JSON (large)",     "json_large.lua"),
    ("Path Param",       "path_param.lua"),
    ("Query Params",     "query_params.lua"),
    ("Echo Body",        "echo.lua"),
    ("Multi Headers",    "headers.lua"),
    ("Mixed Types",      "mixed_payload.lua"),
    ("Nested JSON",      "nested_json.lua"),
    ("Array (x100)",     "array_response.lua"),
]

state_lock = threading.Lock()
state = {
    "status": "starting",
    "current": "",
    "pass": 0,
    "results": [],
    "csv": "",
}

# --- wrk helpers ------------------------------------------------------------

def to_ms(v):
    if not v or v == "N/A": return 0.0
    v = v.strip()
    if v.endswith("us"): return float(v[:-2]) / 1000.0
    if v.endswith("ms"): return float(v[:-2])
    if v.endswith("s"):  return float(v[:-1]) * 1000.0
    return float(v)


def run_wrk(lua_file):
    p = os.path.join(HERE, lua_file)
    cmd = ["wrk", f"-t{THREADS}", f"-c{CONN}", f"-d{DURATION}",
           "-s", p, "--latency", f"http://localhost:{PORT}"]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=300).stdout
    except Exception:
        return None
    m = re.search(r"Requests/sec:\s+([\d.]+)", out)
    if not m: return None
    rps = float(m.group(1))
    m = re.search(r"^\s+Latency\s+([\d.]+[um]?s)", out, re.MULTILINE)
    if not m: return None
    lat = to_ms(m.group(1))
    m = re.search(r"99%\s+([\d.]+[um]?s)", out)
    p99 = to_ms(m.group(1)) if m else 0.0
    return rps, lat, p99


# --- benchmark runner (background thread) -----------------------------------

def run_benchmarks():
    try:
        time.sleep(0.5)
        for name, lua_file in SCENARIOS:
            with state_lock:
                state["current"] = name
                state["pass"] = 0
                state["status"] = "running"

            run_wrk(lua_file)  # warmup
            time.sleep(0.2)

            best = None
            for i in (1, 2, 3):
                with state_lock:
                    state["pass"] = i
                r = run_wrk(lua_file)
                if r and (best is None or r[0] > best[0]):
                    best = r
                time.sleep(0.1)

            if best is None:
                with state_lock:
                    state["results"].append({
                        "name": name, "rps": "FAILED", "lat": "-", "p99": "-", "rps_raw": 0
                    })
            else:
                rps, lat, p99 = best
                with state_lock:
                    state["results"].append({
                        "name": name,
                        "rps": f"{rps:,.0f}", "lat": f"{lat:.2f}",
                        "p99": f"{p99:.2f}", "rps_raw": rps
                    })
            time.sleep(0.3)

        with state_lock:
            state["status"] = "done"
            state["current"] = ""
            state["pass"] = 0

        # Save CSV
        out_dir = os.path.join(ROOT, "benchmark", "results")
        os.makedirs(out_dir, exist_ok=True)
        ts = datetime.now().strftime("%Y%m%d-%H%M%S")
        csv_path = os.path.join(out_dir, f"results-{SERVER_TYPE}-{ts}.csv")
        with open(csv_path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["scenario", "req_per_sec", "avg_ms", "p99_ms"])
            for r in state["results"]:
                w.writerow([r["name"], r["rps"].replace(",", ""), r["lat"], r["p99"]])
        with state_lock:
            state["csv"] = csv_path

    except Exception as e:
        traceback.print_exc()
        with state_lock:
            state["status"] = "done"
            state["current"] = f"Error: {e}"


# --- HTTP server ------------------------------------------------------------

HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>oatpp Benchmark</title>
<style>
  *{margin:0;padding:0;box-sizing:border-box}
  body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
       background:#0d1117;color:#c9d1d9;padding:40px 60px}
  h1{font-size:20px;font-weight:600;color:#f0f6fc;margin-bottom:6px}
  .meta{color:#8b949e;font-size:13px;margin-bottom:30px}
  .meta span{color:#58a6ff}
  #status{font-size:13px;color:#8b949e;margin-bottom:20px}
  .dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px}
  .dot.running{background:#3fb950;animation:pulse 1s infinite}
  .dot.done{background:#8b949e}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:0.3}}
  table{width:100%;max-width:700px;border-collapse:collapse;margin-bottom:20px}
  th{text-align:left;font-weight:500;color:#8b949e;font-size:12px;
     text-transform:uppercase;letter-spacing:.5px;padding:8px 12px;border-bottom:1px solid #21262d}
  th.num{text-align:right}
  td{padding:10px 12px;font-size:14px;border-bottom:1px solid #21262d}
  td.num{text-align:right;font-variant-numeric:tabular-nums}
  td.rps{color:#f0f6fc;font-weight:500}
  td.pending{color:#484f58}
</style>
</head>
<body>
<h1>oatpp Benchmark</h1>
<p class="meta">
  server=<span>SERVER</span> &middot;
  duration=<span>DUR</span> &middot;
  connections=<span>CONN</span> &middot;
  threads=<span>THR</span> &middot;
  port=<span>PORT</span>
</p>
<p id="status"><span class="dot running"></span> <span id="status-text">Waiting...</span></p>
<table>
<thead><tr><th>Scenario</th><th class="num">Req/sec</th><th class="num">Avg</th><th class="num">P99</th></tr></thead>
<tbody id="tbody"></tbody>
</table>
<script>
var SCENARIOS = SCENARIOS_JSON;
var pollTimer = null;

function init(){
  var tbody = document.getElementById('tbody');
  SCENARIOS.forEach(function(name){
    var tr = document.createElement('tr');
    tr.id = 'row-' + name.replace(/[^a-zA-Z0-9]/g, '');
    tr.innerHTML = '<td>' + name + '</td>' +
      '<td class="num rps pending">&mdash;</td>' +
      '<td class="num pending">&mdash;</td>' +
      '<td class="num pending">&mdash;</td>';
    tbody.appendChild(tr);
  });
  poll();
  pollTimer = setInterval(poll, 500);
}

function poll(){
  fetch('/api/state')
    .then(function(r){return r.json();})
    .then(function(s){
      // Status
      var dot = document.querySelector('.dot');
      var txt = document.getElementById('status-text');
      if(s.status === 'done'){
        dot.className = 'dot done';
        txt.textContent = 'Complete.';
        if(pollTimer){clearInterval(pollTimer);pollTimer=null;}
      } else if(s.status === 'running'){
        dot.className = 'dot running';
        txt.textContent = 'Running ' + s.current + ' · pass ' + s.pass + '/3';
      }

      // Results
      (s.results||[]).forEach(function(r){
        updateRow(r.name, r.rps, r.lat, r.p99);
      });

      // Done
      if(s.status === 'done'){
        pollTimer = null;
      }
    });
}

function updateRow(name, rps, lat, p99){
  var id = 'row-' + name.replace(/[^a-zA-Z0-9]/g, '');
  var tr = document.getElementById(id);
  if(!tr) return;
  var cells = tr.querySelectorAll('td');
  if(rps === 'FAILED'){
    cells[1].textContent = 'FAILED';
    cells[1].className = 'num rps';
    cells[2].textContent = '-';
    cells[3].textContent = '-';
    return;
  }
  cells[1].textContent = rps;
  cells[1].classList.remove('pending');
  cells[2].textContent = lat + 'ms';
  cells[2].classList.remove('pending');
  cells[3].textContent = p99 + 'ms';
  cells[3].classList.remove('pending');
}

init();
</script>
</body>
</html>"""

HTML = HTML.replace("SERVER", SERVER_TYPE)
HTML = HTML.replace("DUR", DURATION)
HTML = HTML.replace("CONN", CONN)
HTML = HTML.replace("THR", THREADS)
HTML = HTML.replace("PORT", str(PORT))
HTML = HTML.replace("SCENARIOS_JSON", json.dumps([s[0] for s in SCENARIOS]))


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        try:
            if self.path in ("/", "/index.html"):
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.end_headers()
                self.wfile.write(HTML.encode())
            elif self.path == "/api/state":
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Cache-Control", "no-cache")
                self.end_headers()
                with state_lock:
                    data = json.dumps(state, default=str)
                self.wfile.write(data.encode())
            else:
                self.send_response(404)
                self.end_headers()
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, format, *args):
        pass


# --- main -------------------------------------------------------------------

def main():
    # Build
    print(f"Building {BENCH_BINARY}...", flush=True)
    subprocess.run(["cmake", "-S", ROOT, "-B", "build",
                    "-DOATPP_BUILD_BENCHMARKS=ON", "-DCMAKE_BUILD_TYPE=Release",
                    "-DOATPP_BUILD_TESTS=OFF"],
                   capture_output=True, check=False)
    r = subprocess.run(["cmake", "--build", "build", "--target", BENCH_BINARY,
                        "-j", str(os.cpu_count() or 4)],
                       cwd=ROOT, capture_output=True, check=False)
    if r.returncode != 0:
        print("Build failed", flush=True)
        print(r.stderr.decode(), flush=True)
        sys.exit(1)

    # Kill stale
    subprocess.run(["pkill", "-9", "-f", BENCH_BINARY], capture_output=True, check=False)
    subprocess.run(["bash", "-c", f"lsof -ti :{PORT} | xargs kill -9 2>/dev/null"],
                   capture_output=True, check=False)
    time.sleep(0.5)

    # Start oatpp server
    print(f"Starting {SERVER_TYPE} server on port {PORT}...", flush=True)
    subprocess.Popen([os.path.join(ROOT, "build", "benchmark", BENCH_BINARY), str(PORT)],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1)

    # Start HTTP dashboard
    httpd = HTTPServer(("0.0.0.0", HTTP_PORT), Handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()

    # Start benchmarks
    threading.Thread(target=run_benchmarks, daemon=True).start()

    url = f"http://localhost:{HTTP_PORT}"
    print(f"\n  Dashboard: {url}\n  Press Ctrl+C to stop.\n", flush=True)

    try:
        import webbrowser
        webbrowser.open(url)
    except Exception:
        pass

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        subprocess.run(["pkill", "-9", "-f", BENCH_BINARY], capture_output=True, check=False)
        print("Stopped.", flush=True)


if __name__ == "__main__":
    main()
