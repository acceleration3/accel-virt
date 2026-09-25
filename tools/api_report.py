#!/usr/bin/env python3
"""Every API's sample suite, host against VM, on one page with a tab each.

Runs the report tools of the APIs asked for - tools/d3d11_report.py,
d3d12_report.py, opengl_report.py, vulkan_report.py and opencl_report.py -
one after the other against the held guest, each into its own directory
under the output, and writes an index.html over them: an overview tab with
every API's geomean and the samples still under the goal, and a tab per API
holding that API's own report. One API failing does not stop the others; its
tab says so and keeps its log.

    tools/api_report.py                          # all of them
    tools/api_report.py --apis d3d12,opengl      # some
    tools/api_report.py --page-only              # the page again, from what is there

The guest must already be running (run_guest_suite.py --keep).
"""
import argparse
import datetime
import html
import json
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from d3d11_report import STYLE, cell  # noqa: E402

APIS = {"d3d11": "D3D11", "d3d12": "D3D12", "opengl": "OpenGL", "vulkan": "Vulkan", "opencl": "OpenCL"}
GOAL = 0.9                 # the VM's share of the host's speed every sample should reach
RUN_TIMEOUT = 3 * 3600     # one API's whole run; a hung one must not hold the rest


def run(api, args):
    """One API's report tool into <out>/<api>; its outcome for the page."""
    out = args.out / api
    out.mkdir(parents=True, exist_ok=True)
    command = [sys.executable, "-B", str(ROOT / "tools" / f"{api}_report.py"), "--work", str(args.work),
               "-o", str(out)]
    if args.no_build:
        command.append("--no-build")
    if args.no_guest:
        command.append("--no-guest")
    started = time.monotonic()
    print(f"== {APIS[api]}: {' '.join(command[2:])}", flush=True)
    log = (args.out / f"{api}.log").open("w")
    try:
        done = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=RUN_TIMEOUT)
        code = done.returncode
    except subprocess.TimeoutExpired:
        code = "timed out"
    finally:
        log.close()
    seconds = time.monotonic() - started
    print(f"   {APIS[api]}: {'ok' if code == 0 else f'failed ({code})'} in {seconds:.0f} s", flush=True)
    return {"code": code, "seconds": seconds}


def summary_of(api, out):
    path = out / api / "summary.json"
    return json.loads(path.read_text()) if path.exists() else None


def overview(apis, summaries, outcomes):
    rows, behind = [], []
    for api in apis:
        summary = summaries[api]
        outcome = outcomes.get(api)
        state = "" if outcome is None else ("ok" if outcome["code"] == 0 else f"failed ({outcome['code']})")
        if summary is None:
            rows.append(f"<tr><td>{APIS[api]}</td><td colspan=5 class=bad>no results"
                        f"{' - ' + state if state else ''}; see {api}.log</td></tr>")
            continue
        samples = summary["samples"]
        measured = [s for s in samples if s["ratio"]]
        reached = sum(1 for s in measured if s["ratio"] >= GOAL)
        pictures = sum(1 for s in samples if s["tone"] == "good")
        geomean = summary["geomean"]
        tone = "good" if measured and reached == len(samples) else "warn" if measured else "bad"
        rows.append(f"<tr><td><a href='#{api}'>{APIS[api]}</a></td><td>{len(samples)}</td>"
                    f"<td class={tone}>{reached} / {len(samples)}</td><td>{pictures} / {len(samples)}</td>"
                    f"<td>{cell(geomean * 100 if geomean else None, '%', 1)}</td>"
                    f"<td class={'good' if state == 'ok' else 'bad' if state else ''}>{html.escape(state or 'earlier')}</td></tr>")
        for sample in samples:
            if sample["ratio"] is None or sample["ratio"] < GOAL:
                behind.append((api, sample))
    parts = [f"""<div class=wrap><table><tr><th>API</th><th>samples</th><th>at {GOAL:.0%} of host or better</th>
<th>pictures match</th><th>VM speed (geomean)</th><th>this run</th></tr>{''.join(rows)}</table></div>"""]
    if behind:
        items = []
        for api, sample in sorted(behind, key=lambda entry: entry[1]["ratio"] or 0):
            unit = summaries[api]["unit"]
            reason = (f"{cell(sample['ratio'] * 100, '%', 1)} ({cell(sample['vm'], ' ' + unit, 0)} vs "
                      f"{cell(sample['host'], ' ' + unit, 0)})" if sample["ratio"] else
                      html.escape(f"VM {sample['vm_status'] or 'no result'}, host {sample['host_status'] or 'no result'}"))
            items.append(f"<tr><td>{APIS[api]}</td><td>{html.escape(sample['name'])}</td><td>{reason}</td>"
                         f"<td class={sample['tone']}>{html.escape(sample['picture'])}</td></tr>")
        parts.append(f"""<h2 style="font-size:17px;margin:26px 0 0">Under {GOAL:.0%} of the host, or not measured</h2>
<div class=wrap><table><tr><th>API</th><th>sample</th><th>VM vs host</th><th>picture</th></tr>{''.join(items)}</table></div>""")
    else:
        parts.append(f"<p class=good>Every measured sample is at {GOAL:.0%} of the host or better.</p>")
    return "\n".join(parts)


def write_page(args, apis, outcomes):
    summaries = {api: summary_of(api, args.out) for api in apis}
    tabs = ["<button data-tab=overview>Overview</button>"]
    panes = [f"<section id=pane-overview>{overview(apis, summaries, outcomes)}</section>"]
    for api in apis:
        summary = summaries[api]
        geomean = summary and summary["geomean"]
        badge = f" <span class=muted>{geomean * 100:.0f}%</span>" if geomean else ""
        tabs.append(f"<button data-tab={api}>{APIS[api]}{badge}</button>")
        report = args.out / api / "index.html"
        body = (f"<iframe data-src='{api}/index.html' title='{APIS[api]} report'></iframe>" if report.exists() else
                f"<p class=bad>No {APIS[api]} report; <a href='{api}.log'>{api}.log</a> says why.</p>")
        panes.append(f"<section id=pane-{api} hidden>{body}</section>")
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    page = f"""<!doctype html><meta charset="utf-8"><meta name=viewport content="width=device-width, initial-scale=1">
<title>Sample suites: host vs VM</title>
<style>{STYLE}
nav {{ display:flex; flex-wrap:wrap; gap:6px; margin:16px 0 20px; border-bottom:1px solid var(--line); }}
nav button {{ font:inherit; color:var(--muted); background:none; border:0; border-bottom:2px solid transparent;
  padding:8px 12px; cursor:pointer; }}
nav button[aria-selected=true] {{ color:var(--fg); border-bottom-color:var(--fg); font-weight:600; }}
iframe {{ width:100%; height:calc(100vh - 150px); border:1px solid var(--line); border-radius:10px; background:var(--bg); }}
a {{ color:inherit; }}
</style>
<h1>Sample suites: host vs VM</h1>
<div class=muted>{stamp} &middot; every API's samples natively on the host and in the guest; the goal is {GOAL:.0%} of the host's speed</div>
<nav>{''.join(tabs)}</nav>
{''.join(panes)}
<script>
function show(name) {{
  const known = document.getElementById('pane-' + name) ? name : 'overview';
  for (const button of document.querySelectorAll('nav button'))
    button.setAttribute('aria-selected', button.dataset.tab === known);
  for (const pane of document.querySelectorAll('section')) pane.hidden = pane.id !== 'pane-' + known;
  const frame = document.querySelector('#pane-' + known + ' iframe');
  if (frame && !frame.src) frame.src = frame.dataset.src;
}}
for (const button of document.querySelectorAll('nav button'))
  button.addEventListener('click', () => {{ location.hash = button.dataset.tab; }});
addEventListener('hashchange', () => show(location.hash.slice(1)));
show(location.hash.slice(1));
</script>
"""
    (args.out / "index.html").write_text(page)
    print(f"report: {args.out / 'index.html'}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest-render",
                        help="the running guest's work directory")
    parser.add_argument("-o", "--out", type=pathlib.Path, default=ROOT / "build-samples" / "all")
    parser.add_argument("--apis", default=",".join(APIS), help=f"comma-separated, of {', '.join(APIS)}")
    parser.add_argument("--no-build", action="store_true", help="passed to every API's tool")
    parser.add_argument("--no-guest", action="store_true", help="passed to every API's tool: the guest halves "
                                                                "already in the output are reused")
    parser.add_argument("--page-only", action="store_true", help="run nothing; write the page from what is there")
    args = parser.parse_args()
    apis = [api.strip().lower() for api in args.apis.split(",") if api.strip()]
    unknown = [api for api in apis if api not in APIS]
    if unknown:
        parser.error(f"no such API: {', '.join(unknown)}")
    args.out.mkdir(parents=True, exist_ok=True)
    outcomes = {}
    if not args.page_only:
        for api in apis:
            outcomes[api] = run(api, args)
            write_page(args, apis, outcomes)
    write_page(args, apis, outcomes)
    failed = [api for api, outcome in outcomes.items() if outcome["code"] != 0]
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
