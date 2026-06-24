#!/usr/bin/env python3
"""
oatpp Benchmark Suite — local web dashboard with live-updating results.

Usage:
  python3 run-benchmark.py -m MODE [-s SCENARIO ...] [-p] [-l]

Mode selection (-m / --mode):
  sync     synchronous server
  async    asynchronous (coroutine) server

Scenario selection (-s / --scenario) can be:
  - Exact name       : -s "Hello World"
  - Partial match    : -s json        (matches "JSON (small)", "JSON (large)")
  - Zero-based index : -s 0
  - Lua filename     : -s hello.lua
  - Multiple         : -s 0 1 hello.lua json

  -p / --perf   record perf profile and generate flamegraph SVG
  -l / --list   show available scenarios and exit
"""
import sys
sys.dont_write_bytecode = True
import os, subprocess, re, json, time, threading, csv, traceback, argparse, shutil, shlex
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler

BUILD_DIR  = os.environ.get("BUILD_DIR", "build-benchmark")
PORT       = int(os.environ.get("PORT", "8000"))
DURATION   = os.environ.get("DURATION", "10s")
CONN       = os.environ.get("CONNECTIONS", "100")
THREADS    = os.environ.get("THREADS", "4")
HTTP_PORT  = 8080
HERE       = os.path.dirname(os.path.abspath(__file__))
ROOT       = os.path.dirname(os.path.dirname(HERE))

PERF_DATA  = os.path.join(ROOT, "benchmark", "results", "perf.data")
PERF_PID   = 0

FLAMEGRAPH_DIR = os.path.join(ROOT, "benchmark", "results", "FlameGraph")

# ---------------------------------------------------------------------------
# Parse args
# ---------------------------------------------------------------------------
parser = argparse.ArgumentParser(description="oatpp Benchmark Suite")
parser.add_argument("-m", "--mode", default="sync", choices=["sync", "async"],
                    help="Server mode: 'sync' or 'async' (default: sync)")
parser.add_argument("-s", "--scenario", nargs="+", default=[],
                    help="Scenario selector(s)")
parser.add_argument("-l", "--list", action="store_true",
                    help="List all available scenarios and exit")
parser.add_argument("-p", "--perf", action="store_true",
                    help="Record perf profile and generate flamegraph SVG")
args = parser.parse_args()

SERVER_TYPE = args.mode
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

if args.list:
    print(f"Available scenarios for '{SERVER_TYPE}' server:\n")
    for i, (name, lua) in enumerate(SCENARIOS):
        print(f"  [{i:2d}]  {name:<20s}  ({lua})")
    print(f"\nTotal: {len(SCENARIOS)} scenarios")
    sys.exit(0)


def match_scenario(selector, scenarios):
    if not selector:
        return scenarios
    try:
        idx = int(selector)
        if 0 <= idx < len(scenarios):
            return [scenarios[idx]]
        print(f"Warning: index {idx} out of range (0-{len(scenarios)-1}), ignored.", flush=True)
        return []
    except ValueError:
        pass
    if selector.lower().endswith(".lua"):
        for s in scenarios:
            if s[1] == selector:
                return [s]
        print(f"Warning: no scenario with lua file '{selector}'.", flush=True)
        return []
    selector_lower = selector.lower()
    matches = [s for s in scenarios if selector_lower in s[0].lower()]
    if not matches:
        print(f"Warning: no scenario matching '{selector}'.", flush=True)
        return []
    return matches


# --- perf / flamegraph helpers -----------------------------------------------

def check_perf_prerequisites():
    if not shutil.which("perf"):
        return "perf not found in PATH"
    try:
        with open("/proc/sys/kernel/perf_event_paranoid") as f:
            val = int(f.read().strip())
        if val > 2:
            return (f"/proc/sys/kernel/perf_event_paranoid = {val} (too restrictive). "
                    "Try: echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid")
    except Exception:
        pass
    return None


def ensure_kernel_profiling():
    """Ensure kernel call chain profiling is available.

    perf record -k requires perf_event_paranoid <= 1.
    If it's > 1, try to lower it with sudo.
    Returns True if kernel profiling is available (or was just enabled).
    """
    try:
        with open("/proc/sys/kernel/perf_event_paranoid") as f:
            val = int(f.read().strip())
    except Exception:
        return False

    if val <= 1:
        return True

    # Try to lower it
    print(f"perf_event_paranoid = {val}, lowering to 1 for kernel profiling...", flush=True)
    try:
        p = subprocess.run(
            ["sudo", "tee", "/proc/sys/kernel/perf_event_paranoid"],
            input="1\n", capture_output=True, text=True, timeout=5
        )
        if p.returncode == 0:
            print("  perf_event_paranoid set to 1", flush=True)
            return True
    except Exception:
        pass

    print("  WARNING: cannot lower perf_event_paranoid (need sudo). "
          "System calls won't appear in flamegraph.", flush=True)
    print("  Run: echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid", flush=True)
    return False


def resolve_lib_debuginfo(server_proc):
    """Ensure system library debug symbols are available for perf.

    Perf automatically looks in /usr/lib/debug/ for debug symbols.
    This function installs the necessary -dbg packages if missing.
    """
    binary = os.path.join(ROOT, BUILD_DIR, "benchmark", BENCH_BINARY)
    if not os.path.isfile(binary):
        return

    # Find shared libs used by the server binary
    try:
        ldd_out = subprocess.run(["ldd", binary], capture_output=True, text=True, timeout=10).stdout
    except Exception:
        return

    lib_paths = []
    for line in ldd_out.splitlines():
        parts = line.strip().split()
        if len(parts) >= 3 and parts[1] == "=>" and parts[2] != "not":
            lib_paths.append(parts[2])

    if not lib_paths:
        return

    _try_install_dbg_packages(lib_paths)


def _try_install_dbg_packages(lib_paths):
    """Auto-install debug symbol packages via apt.

    Perf automatically picks up symbols from /usr/lib/debug/ after installation.
    Only attempts packages that actually exist in the apt repository.
    """
    if not shutil.which("apt-get"):
        print("  Install debug symbols manually: sudo apt install libc6-dbg libstdc++6-XX-dbg",
              flush=True)
        return

    pkgs = set()
    for lp in lib_paths:
        bn = os.path.basename(lp)
        if "libc." in bn or "libm." in bn:
            pkgs.add("libc6-dbg")
        elif "libstdc++" in bn:
            gcc_ver_out = subprocess.run(
                ["gcc", "--version"], capture_output=True, text=True
            ).stdout
            # gcc --version formats:
            #   "gcc (Ubuntu 11.4.0-...) 11.4.0"
            #   "gcc version 11.4.0"
            gcc_m = re.search(r'gcc.*?(\d+)\.\d+\.\d+', gcc_ver_out)
            if gcc_m:
                # Ubuntu package: libstdc++6-11-dbg
                pkg_name = f"libstdc++6-{gcc_m.group(1)}-dbg"
                # Verify it exists in apt cache before adding
                check = subprocess.run(["apt-cache", "show", pkg_name],
                                       capture_output=True, check=False)
                if check.returncode == 0:
                    pkgs.add(pkg_name)

    if not pkgs:
        return

    # Check which packages are already installed
    missing = []
    for pkg in sorted(pkgs):
        p = subprocess.run(["dpkg", "-s", pkg], capture_output=True, check=False)
        if p.returncode != 0:
            missing.append(pkg)

    if not missing:
        print("  Debug symbol packages already installed.", flush=True)
        return

    print(f"  Installing debug symbol packages: {' '.join(missing)}", flush=True)
    # Let sudo interact with the user's real terminal for password
    subprocess.run(
        ["sudo", "apt-get", "install", "-y"] + missing,
        stdin=sys.stdin, stdout=sys.stdout, stderr=sys.stderr, check=False
    )


def start_perf(pid):
    global PERF_PID
    PERF_PID = pid
    if os.path.exists(PERF_DATA):
        os.remove(PERF_DATA)
    # --call-graph dwarf,4096: DWARF unwinding, 4KB stack dump (faster decode)
    # -F 99: Brendan Gregg's standard rate — enough samples, manageable data
    cmd = ["perf", "record", "--call-graph", "dwarf,4096",
           "-F", "99", "-p", str(pid), "-o", PERF_DATA,
           "--", "sleep", "86400"]
    log = os.path.join(ROOT, "benchmark", "results", "perf-record.log")
    os.makedirs(os.path.dirname(log), exist_ok=True)
    os.makedirs(os.path.dirname(PERF_DATA), exist_ok=True)
    with open(log, "w") as lf:
        proc = subprocess.Popen(cmd, stdout=lf, stderr=lf)
    print(f"perf record started (pid={proc.pid}), logging to {log}", flush=True)
    return proc


def stop_perf(perf_proc):
    perf_proc.terminate()
    try:
        perf_proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        perf_proc.kill()
    print("perf record stopped.", flush=True)


def generate_flamegraph(label=""):
    """Generate interactive flamegraph SVG from perf.data using FlameGraph tools.

    Pipeline: perf script | stackcollapse-perf.pl | flamegraph.pl
    """
    if not os.path.isfile(PERF_DATA):
        print("WARNING: perf.data not found, skipping flamegraph.", flush=True)
        return None

    out_dir = os.path.join(ROOT, "benchmark", "results")
    os.makedirs(out_dir, exist_ok=True)
    ts = datetime.now().strftime("%Y%m%d-%H%M%S")
    suffix = f"-{label}" if label else ""
    svg_path = os.path.join(out_dir, f"flamegraph-{SERVER_TYPE}{suffix}-{ts}.svg")

    stackcollapse = os.path.join(FLAMEGRAPH_DIR, "stackcollapse-perf.pl")
    flamegraph = os.path.join(FLAMEGRAPH_DIR, "flamegraph.pl")

    if not os.path.isfile(stackcollapse) or not os.path.isfile(flamegraph):
        print(f"WARNING: FlameGraph tools not found in {FLAMEGRAPH_DIR}", flush=True)
        return None

    print("Running perf script | stackcollapse-perf.pl | flamegraph.pl...", flush=True)
    try:
        # Shell pipe: all three processes run concurrently, output → file directly
        # --no-inline: skip inline expansion (350× faster, preferred for flamegraphs)
        pipe_cmd = (
            f"perf script --no-inline -i {shlex.quote(PERF_DATA)}"
            f" | perl {shlex.quote(stackcollapse)}"
            f" | perl {shlex.quote(flamegraph)}"
            f" --title {shlex.quote(f'oatpp {SERVER_TYPE} flamegraph')}"
            f" --width 2000 --minwidth 0.3"
            f" > {shlex.quote(svg_path)}"
        )
        p = subprocess.run(pipe_cmd, shell=True, capture_output=True, text=True, timeout=60)
        if p.returncode != 0:
            print(f"Pipeline failed: {p.stderr[:300]}", flush=True)
            return None

    except Exception as e:
        print(f"Flamegraph generation failed: {e}", flush=True)
        traceback.print_exc()
        return None

    with state_lock:
        state["flamegraph"] = svg_path
    print(f"Flamegraph written to: {svg_path}", flush=True)
    return svg_path


# ---------------------------------------------------------------------------
# Scenario selection
# ---------------------------------------------------------------------------

selected = []
seen = set()
for sel in args.scenario:
    for s in match_scenario(sel, SCENARIOS):
        if s not in seen:
            seen.add(s)
            selected.append(s)

if not selected:
    print("No scenarios selected — running ALL.", flush=True)
    selected = list(SCENARIOS)

print(f"Selected {len(selected)}/{len(SCENARIOS)} scenarios: "
      + ", ".join(f'"{s[0]}"' for s in selected), flush=True)
SCENARIOS = selected

state_lock = threading.Lock()
state = {
    "status": "starting",
    "current": "",
    "pass": 0,
    "results": [],
    "csv": "",
    "flamegraph": "",
    "perf": bool(args.perf),
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
            run_wrk(lua_file)
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
  #perf-link{margin-bottom:20px}
  #perf-link a{color:#58a6ff;text-decoration:none;font-size:13px}
  #perf-link a.disabled{color:#8b949e;pointer-events:none}
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
<p id="perf-link" style="display:none">
  <a id="perf-anchor" href="/flamegraph" target="_blank">&#x1F525; View Flamegraph</a>
</p>
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
      var dot = document.querySelector('.dot');
      var txt = document.getElementById('status-text');
      if(s.status === 'done'){
        dot.className = 'dot done';
        txt.textContent = 'Complete.';
        if(!s.perf || s.flamegraph){
          if(pollTimer){clearInterval(pollTimer);pollTimer=null;}
        }
      } else if(s.status === 'running'){
        dot.className = 'dot running';
        txt.textContent = 'Running ' + s.current + ' · pass ' + s.pass + '/3';
      }

      var fg = document.getElementById('perf-link');
      var anchor = document.getElementById('perf-anchor');
      if(s.perf && s.status === 'done' && !s.flamegraph){
        fg.style.display = 'block';
        anchor.textContent = '⏳ Generating flamegraph...';
        anchor.className = 'disabled';
      } else if(s.perf && s.flamegraph){
        fg.style.display = 'block';
        anchor.textContent = '🔥 View Flamegraph (opens in new tab)';
        anchor.className = '';
        anchor.href = '/flamegraph';
      } else {
        fg.style.display = 'none';
      }

      (s.results||[]).forEach(function(r){
        updateRow(r.name, r.rps, r.lat, r.p99);
      });

      if(s.status === 'done' && (!s.perf || s.flamegraph)){
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
            elif self.path == "/flamegraph":
                with state_lock:
                    svg_path = state.get("flamegraph", "")
                if not svg_path or not os.path.isfile(svg_path):
                    self.send_response(404)
                    self.end_headers()
                    self.wfile.write(b"No flamegraph available yet.")
                    return
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.end_headers()
                fullscreen_html = (
                    b'<!DOCTYPE html><html><head><meta charset="UTF-8">'
                    b'<meta name="viewport" content="width=device-width,initial-scale=1.0">'
                    b'<title>Flamegraph</title>'
                    b'<style>'
                    b'*{margin:0;padding:0;box-sizing:border-box}'
                    b'html,body{width:100%;height:100%;overflow:hidden;background:#1e1e1e}'
                    b'#toolbar{display:flex;justify-content:space-between;align-items:center;'
                    b'padding:0 16px;height:40px;background:#161b22;border-bottom:1px solid #30363d}'
                    b'#toolbar .title{color:#8b949e;font-size:13px;font-family:-apple-system,sans-serif}'
                    b'#toolbar a{color:#58a6ff;text-decoration:none;font-size:13px;'
                    b'padding:4px 12px;border:1px solid #30363d;border-radius:6px;font-family:-apple-system,sans-serif}'
                    b'#toolbar a:hover{background:#30363d}'
                    b'iframe{width:100%;height:calc(100vh - 40px);border:none;display:block}'
                    b'</style></head><body>'
                    b'<div id="toolbar">'
                    b'<span class="title">\xf0\x9f\x94\xa5 Flamegraph</span>'
                    b'<div>'
                    b'<a href="/" target="_blank">\xf0\x9f\x8f\xa0 Dashboard</a>&nbsp;'
                    b'<a href="/flamegraph.svg/download">\xf0\x9f\x93\xa5 Download</a>'
                    b'</div></div>'
                    b'<iframe src="/flamegraph.svg"></iframe>'
                    b'</body></html>'
                )
                self.wfile.write(fullscreen_html)
            elif self.path == "/flamegraph.svg":
                with state_lock:
                    svg_path = state.get("flamegraph", "")
                if not svg_path or not os.path.isfile(svg_path):
                    self.send_response(404)
                    self.end_headers()
                    self.wfile.write(b"No flamegraph available yet.")
                    return
                self.send_response(200)
                self.send_header("Content-Type", "image/svg+xml")
                self.end_headers()
                with open(svg_path, "rb") as f:
                    self.wfile.write(f.read())
            elif self.path == "/flamegraph.svg/download":
                with state_lock:
                    svg_path = state.get("flamegraph", "")
                if not svg_path or not os.path.isfile(svg_path):
                    self.send_response(404)
                    self.end_headers()
                    self.wfile.write(b"No flamegraph available yet.")
                    return
                self.send_response(200)
                self.send_header("Content-Type", "image/svg+xml")
                self.send_header("Content-Disposition", "attachment")
                self.end_headers()
                with open(svg_path, "rb") as f:
                    self.wfile.write(f.read())
            elif self.path == "/favicon.ico":
                self.send_response(204)
                self.end_headers()
            else:
                self.send_response(404)
                self.end_headers()
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, format, *args):
        pass


# --- main -------------------------------------------------------------------

def ensure_dirs():
    results_dir = os.path.join(ROOT, "benchmark", "results")
    os.makedirs(results_dir, exist_ok=True)

    fg_dir = FLAMEGRAPH_DIR
    os.makedirs(fg_dir, exist_ok=True)

    if os.path.isfile(os.path.join(fg_dir, "flamegraph.pl")):
        return

    print("FlameGraph tools not found, downloading from GitHub...", flush=True)
    base_url = "https://raw.githubusercontent.com/brendangregg/FlameGraph/master"
    for fname in ("flamegraph.pl", "stackcollapse-perf.pl"):
        url = f"{base_url}/{fname}"
        dest = os.path.join(fg_dir, fname)
        try:
            subprocess.run(
                ["curl", "-sL", "--retry", "3", "--retry-delay", "2",
                 "-o", dest, url],
                check=True, timeout=30
            )
            os.chmod(dest, 0o755)
            print(f"  Downloaded {fname}", flush=True)
        except Exception as e:
            print(f"  WARNING: failed to download {fname}: {e}", flush=True)


def main():
    ensure_dirs()

    build_type = "RelWithDebInfo" if args.perf else "Release"
    print(f"Building {BENCH_BINARY}... (CMAKE_BUILD_TYPE={build_type})", flush=True)
    subprocess.run(["cmake", "-S", ROOT, "-B", BUILD_DIR,
                    "-DOATPP_BUILD_BENCHMARKS=ON", f"-DCMAKE_BUILD_TYPE={build_type}",
                    "-DOATPP_USE_JSON_FAST_SERIALIZER=ON", "-DOATPP_USE_JSON_FAST_DESERIALIZER=ON",
                    "-DOATPP_BUILD_TESTS=OFF"],
                   capture_output=True, check=False)
    r = subprocess.run(["cmake", "--build", BUILD_DIR, "--target", BENCH_BINARY,
                        "-j", str(os.cpu_count() or 4)],
                       cwd=ROOT, capture_output=True, check=False)
    if r.returncode != 0:
        print("Build failed", flush=True)
        print(r.stderr.decode(), flush=True)
        sys.exit(1)

    subprocess.run(["pkill", "-9", "-f", BENCH_BINARY], capture_output=True, check=False)
    subprocess.run(["bash", "-c", f"lsof -ti :{PORT} | xargs kill -9 2>/dev/null"],
                   capture_output=True, check=False)
    time.sleep(0.5)

    print(f"Starting {SERVER_TYPE} server on port {PORT}...", flush=True)
    server_proc = subprocess.Popen(
        [os.path.join(ROOT, BUILD_DIR, "benchmark", BENCH_BINARY), str(PORT)],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1)

    perf_proc = None
    if args.perf:
        err = check_perf_prerequisites()
        if err:
            print(f"WARNING: perf prerequisites not met: {err}", flush=True)
            print("Continuing without flamegraph.", flush=True)
        else:
            print("Resolving debug symbols for system libraries...", flush=True)
            resolve_lib_debuginfo(server_proc)
            print("Enabling kernel profiling (system calls)...", flush=True)
            ensure_kernel_profiling()
            print("Starting perf record (attach to server process)...", flush=True)
            perf_proc = start_perf(server_proc.pid)

    httpd = HTTPServer(("0.0.0.0", HTTP_PORT), Handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
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
            if perf_proc is not None and state.get("status") == "done":
                stop_perf(perf_proc)
                _perf_proc_ref = perf_proc
                perf_proc = None
                time.sleep(0.5)
                def _bg_generate():
                    import traceback as tb
                    try:
                        generate_flamegraph()
                    except Exception:
                        tb.print_exc()
                threading.Thread(target=_bg_generate, daemon=True).start()
    except KeyboardInterrupt:
        pass
    finally:
        if perf_proc is not None:
            stop_perf(perf_proc)
            generate_flamegraph()
        subprocess.run(["pkill", "-9", "-f", BENCH_BINARY], capture_output=True, check=False)
        subprocess.run(["pkill", "-9", "-f", "perf record"], capture_output=True, check=False)
        print("Stopped.", flush=True)


if __name__ == "__main__":
    main()
