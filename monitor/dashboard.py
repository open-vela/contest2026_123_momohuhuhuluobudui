#!/usr/bin/env python3
"""Serve a dependency-free local AgentGuard event dashboard."""

from __future__ import annotations

import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from event_stats import EVENT_LABELS, default_log_path, load_events, summarize_events

DASHBOARD_HTML = """<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AgentGuard 事件面板</title>
<style>
:root{color-scheme:dark;--bg:#09111d;--panel:#111f31;--line:#27415e;
--blue:#5ec8ff;--green:#65e6a5;--orange:#ffbc66;--pink:#ff75c3}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 20% 0,
#163652,var(--bg) 42%);font:16px system-ui,sans-serif;color:#eef7ff}
main{max-width:980px;margin:auto;padding:36px 20px}.title{display:flex;align-items:end;
justify-content:space-between;gap:20px}.title h1{margin:0;font-size:34px}.muted{color:#91a9bd}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:16px;margin:28px 0}
.card{background:linear-gradient(145deg,#15283d,#0d1928);border:1px solid var(--line);
border-radius:18px;padding:22px;box-shadow:0 12px 35px #0005}.card b{font-size:38px;
display:block;margin:6px 0}.today{font-size:14px;color:#a8bfd2}.sit b{color:var(--orange)}
.ack b{color:var(--green)}.privacy b{color:var(--pink)}table{width:100%;border-collapse:collapse}
th,td{text-align:left;padding:12px;border-bottom:1px solid var(--line)}
.status{display:inline-block;width:9px;height:9px;border-radius:50%;background:var(--green);
box-shadow:0 0 10px var(--green);margin-right:8px}@media(max-width:650px){.grid{grid-template-columns:1fr}}
</style></head><body><main><div class="title"><div><h1>AgentGuard</h1>
<div class="muted">桌面健康与隐私事件面板</div></div><div><span class="status"></span>
<span id="updated">连接中</span></div></div><section class="grid">
<div class="card sit">久坐提醒<b id="sit">-</b><span class="today" id="sitToday"></span></div>
<div class="card ack">用户确认<b id="ack">-</b><span class="today" id="ackToday"></span></div>
<div class="card privacy">隐私触发<b id="privacy">-</b><span class="today" id="privacyToday"></span></div>
</section><div class="card"><h2>最近事件</h2><table><thead><tr><th>时间</th><th>事件</th></tr></thead>
<tbody id="events"></tbody></table><p class="muted" id="summary"></p></div></main>
<script>
const $=id=>document.getElementById(id);const esc=s=>String(s).replace(/[&<>"']/g,
c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function show(data){const s=data.stats;
$('sit').textContent=s.sedentary_alerts;$('ack').textContent=s.acknowledgements;
$('privacy').textContent=s.privacy_triggers;$('sitToday').textContent='今日 '+s.today_sedentary_alerts;
$('ackToday').textContent='今日 '+s.today_acknowledgements;$('privacyToday').textContent='今日 '+s.today_privacy_triggers;
$('events').innerHTML=data.recent.map(e=>'<tr><td>'+esc(e.received_at)+'</td><td>'+esc(e.label)+'</td></tr>').join('')||
'<tr><td colspan="2" class="muted">尚无带时间戳的事件</td></tr>';
$('summary').textContent='有效事件 '+s.total_events+' · 今日 '+s.today_total+' · 损坏记录 '+s.malformed_lines;
$('updated').textContent='已更新 '+new Date().toLocaleTimeString();}
async function refresh(){try{show(await(await fetch('/api/stats',{cache:'no-store'})).json())}
catch(e){$('updated').textContent='等待服务'}}refresh();setInterval(refresh,2000);
</script></body></html>"""


def build_payload(log_path: Path) -> dict:
    try:
        events, malformed = load_events(log_path)
    except FileNotFoundError:
        events, malformed = [], 0
    stats = summarize_events(events, malformed)
    recent = []
    for payload in reversed(events):
        timestamp = payload.get("received_at") or payload.get("timestamp")
        if not timestamp:
            continue
        event = payload["event"]
        recent.append({
            "received_at": timestamp,
            "event": event,
            "label": EVENT_LABELS.get(event, event),
        })
        if len(recent) == 10:
            break
    return {"stats": stats, "recent": recent}


def make_handler(log_path: Path):
    class DashboardHandler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:
            if self.path == "/":
                self._send(200, "text/html; charset=utf-8",
                           DASHBOARD_HTML.encode("utf-8"))
            elif self.path == "/api/stats":
                body = json.dumps(build_payload(log_path),
                                  ensure_ascii=False).encode("utf-8")
                self._send(200, "application/json; charset=utf-8", body)
            else:
                self._send(404, "text/plain; charset=utf-8", b"not found")

        def _send(self, status: int, content_type: str, body: bytes) -> None:
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt: str, *args) -> None:
            pass

    return DashboardHandler


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, default=default_log_path())
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    if args.host not in {"127.0.0.1", "localhost", "::1"}:
        parser.error("--host must be a loopback address")
    return args


def main() -> int:
    args = parse_args()
    server = ThreadingHTTPServer((args.host, args.port),
                                 make_handler(args.log.expanduser()))
    print(f"AgentGuard 统计面板: http://{args.host}:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
