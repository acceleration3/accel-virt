#!/usr/bin/env python3
"""The OpenCL samples (samples/opencl) on the host and in the guest, side by side.

Runs the same program twice - natively on the host through its own OpenCL, and
in the running guest through the Khronos loader, avcl.dll, the device and the
executor - and writes an HTML report: each sample's output from both (a picture,
a buffer, or both), how they differ, and how fast each ran.

    tools/opencl_report.py --work build-guest-render -o build-samples/opencl-report
    tools/opencl_report.py --host-only -o build-samples/opencl-report

The guest must already be running a build of this tree (run_guest_suite.py
--keep), so that its avcl.dll is the one these samples were written against.
Both runs compile the same OpenCL C on the same host GPU, so a picture or a
buffer that differs is a difference the driver made.
"""
import argparse
import datetime
import gzip
import html
import math
import os
import pathlib
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from d3d11_report import compare, guest_shell, host_run, png, read_frame, verdict, write_summary  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUEST_DIR = "C:\\av\\clsamples"
GUEST_EXE = GUEST_DIR + "\\avclsamples.exe"

BUFFER_MAGIC = 0x00425641  # "AVB"
ELEMENTS = {1: "I", 2: "f", 3: "i"}
# A float that differs by less than this, relative or absolute, is the same
# answer computed in another order; the same device and compiler give none.
RELATIVE_TOLERANCE = 1e-5
ABSOLUTE_TOLERANCE = 1e-6


def build(args):
    targets = ["avclsamples_host"]
    if args.loopback:
        targets.append("avclsamples_loopback")
    elif not args.host_only:
        targets.append("avtest")
    for target in targets:
        subprocess.run(["cmake", "--build", str(args.build), "--target", target], check=True,
                       stdout=subprocess.DEVNULL)


def run_guest(args, out):
    """The guest's run, and everything it wrote brought back."""
    work = args.work
    guest = args.build / "guest"
    guest_shell(work, "ps", f"Remove-Item -Recurse -Force {GUEST_DIR} -ErrorAction SilentlyContinue; "
                            f"New-Item -ItemType Directory -Force {GUEST_DIR}\\out | Out-Null")
    # The loader beside the program, where Windows looks first: the run is the
    # cross-built Khronos loader's, finding avcl.dll through the adapter.
    guest_shell(work, "push", str(guest / "OpenCL.dll"), GUEST_DIR + "\\OpenCL.dll")
    guest_shell(work, "push", str(guest / "avclsamples.exe"), GUEST_EXE)
    print("guest: running", flush=True)
    only = f" --only {args.only}" if args.only else ""
    command = (f"{GUEST_EXE} --out {GUEST_DIR}\\out --frames {args.frames} --warmup {args.warmup}{only} "
               f"> {GUEST_DIR}\\run.txt 2>&1")
    # The user's interactive session: with more than one avgpu adapter the ICD
    # takes the one the primary display is on, and the agent's session has no
    # display to ask - there it would take whichever the kernel lists first.
    output = guest_shell(work, "ps", f"""
Unregister-ScheduledTask -TaskName avclsamples -Confirm:$false -ErrorAction SilentlyContinue
$a = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument '/c cd /d {GUEST_DIR} && {command}'
$p = New-ScheduledTaskPrincipal -UserId '{args.guest_user}' -LogonType Interactive -RunLevel Highest
$s = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 1)
Register-ScheduledTask -TaskName avclsamples -Action $a -Principal $p -Settings $s | Out-Null
Start-ScheduledTask -TaskName avclsamples
Start-Sleep 2
while ((Get-ScheduledTask avclsamples).State -eq 'Running') {{ Start-Sleep 1 }}
'exit: ' + (Get-ScheduledTaskInfo avclsamples).LastTaskResult
Get-Content {GUEST_DIR}\\run.txt -ErrorAction SilentlyContinue""", timeout=1800)
    print(output, flush=True)
    (out / "guest.log").write_text(output)
    # Everything in one gzip-compressed stream: name length, name, size, bytes.
    bundle = f"{GUEST_DIR}\\bundle.gz"
    guest_shell(work, "ps", f"""
$out = [IO.File]::Create('{bundle}')
$zip = New-Object IO.Compression.GZipStream($out, [IO.Compression.CompressionMode]::Compress)
$writer = New-Object IO.BinaryWriter($zip)
foreach ($f in (Get-ChildItem {GUEST_DIR}\\out -File)) {{
    $name = [Text.Encoding]::UTF8.GetBytes($f.Name)
    $bytes = [IO.File]::ReadAllBytes($f.FullName)
    $writer.Write([int]$name.Length); $writer.Write($name)
    $writer.Write([int]$bytes.Length); $writer.Write($bytes)
}}
$writer.Close()""")
    local = out / "guest-bundle.gz"
    pulled = ""
    for _ in range(5):
        pulled = guest_shell(work, "pull", bundle, str(local))
        if local.exists() and local.stat().st_size != 0:
            break
    if not local.exists() or local.stat().st_size == 0:
        sys.exit(f"could not bring the guest's results back: {pulled.strip()}")
    data = gzip.decompress(local.read_bytes())
    target = out / "guest"
    target.mkdir(parents=True, exist_ok=True)
    at = 0
    while at < len(data):
        (length,) = struct.unpack_from("<i", data, at)
        name = data[at + 4:at + 4 + length].decode()
        at += 4 + length
        (size,) = struct.unpack_from("<i", data, at)
        (target / name).write_bytes(data[at + 4:at + 4 + size])
        at += 4 + size
    local.unlink()


def run_loopback(args, out):
    """The guest's path without a guest: avcl's core, the driver, the device
    core and the executor library in one host process (the L4 rig), in the
    VM's column. What it shows is whether the ICD carries every sample right;
    its speed is the sanitized single-process rig's, not a VM's."""
    print("loopback: running", flush=True)
    shutil.rmtree(out / "guest", ignore_errors=True)
    (out / "guest").mkdir(parents=True)
    command = [str(args.build / "avclsamples_loopback"), "--out", str(out / "guest"), "--frames", str(args.frames),
               "--warmup", str(args.warmup)]
    if args.only:
        command += ["--only", args.only]
    environment = dict(os.environ, MESA_SHADER_CACHE_DISABLE="true", VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING="1")
    done = subprocess.run(command, capture_output=True, text=True, timeout=3600, env=environment)
    (out / "guest.log").write_text(done.stdout + done.stderr)
    print(done.stdout, flush=True)


def run_host(args, out):
    print("host: running", flush=True)
    shutil.rmtree(out / "host", ignore_errors=True)
    (out / "host").mkdir(parents=True)
    command = [str(args.build / "avclsamples_host"), "--out", str(out / "host"), "--frames", str(args.frames),
               "--warmup", str(args.warmup), "--device", str(args.device)]
    if args.only:
        command += ["--only", args.only]
    text = host_run(command)
    (out / "host.log").write_text(text)
    print(text, flush=True)


def metrics(path):
    rows = {}
    if not path.exists():
        return rows
    for line in path.read_text().splitlines()[1:]:
        fields = line.split("\t")
        if len(fields) == 8:
            rows[fields[0]] = {"status": fields[1], "init_ms": float(fields[2]), "iterations": int(fields[3]),
                               "ms": float(fields[4]), "ips": float(fields[5]), "rate": float(fields[6]),
                               "unit": fields[7]}
    return rows


def read_buffer(path):
    data = path.read_bytes()
    magic, kind, count = struct.unpack_from("<III", data, 0)
    if magic != BUFFER_MAGIC or kind not in ELEMENTS or len(data) != 12 + 4 * count:
        raise ValueError(f"{path}: not a buffer")
    return kind, struct.unpack_from(f"<{count}{ELEMENTS[kind]}", data, 12)


def compare_buffers(host, guest):
    """Integers must be equal; floats equal within the tolerance, NaN to NaN."""
    kind, a = host
    guest_kind, b = guest
    stats = {"kind": {1: "u32", 2: "f32", 3: "i32"}[kind], "count": len(a), "exact": 0, "differing": 0,
             "max_abs": 0.0, "max_rel": 0.0, "first": None}
    if guest_kind != kind or len(b) != len(a):
        stats["differing"] = max(len(a), len(b))
        stats["shape"] = f"host {len(a)} x {ELEMENTS[kind]}, VM {len(b)} x {ELEMENTS.get(guest_kind, '?')}"
        return stats
    for index, (x, y) in enumerate(zip(a, b)):
        if x == y or (kind == 2 and math.isnan(x) and math.isnan(y)):
            stats["exact"] += 1
            continue
        if kind == 2:
            difference = abs(x - y)
            relative = difference / abs(x) if x != 0 else math.inf
            stats["max_abs"] = max(stats["max_abs"], difference)
            stats["max_rel"] = max(stats["max_rel"], relative)
            if difference <= ABSOLUTE_TOLERANCE or relative <= RELATIVE_TOLERANCE:
                continue
        stats["differing"] += 1
        if stats["first"] is None:
            stats["first"] = (index, x, y)
    return stats


def buffer_verdict(stats):
    if "shape" in stats or stats["differing"] != 0:
        return "differs", "bad"
    if stats["exact"] == stats["count"]:
        return "identical", "good"
    return "matches", "good"


def report(args, out):
    host = metrics(out / "host" / "metrics.tsv")
    guest = metrics(out / "guest" / "metrics.tsv")
    names = list(dict.fromkeys(list(host) + list(guest)))
    images = out / "img"
    images.mkdir(exist_ok=True)
    rows = []
    for name in names:
        entry = {"name": name, "host": host.get(name), "guest": guest.get(name), "image": None, "buffer": None,
                 "has_image": False}
        host_frame, guest_frame = out / "host" / f"{name}.bgra", out / "guest" / f"{name}.bgra"
        if host_frame.exists():
            png(images / f"{name}-host.png", *read_frame(host_frame))
            entry["has_image"] = True
        if guest_frame.exists():
            png(images / f"{name}-vm.png", *read_frame(guest_frame))
            entry["has_image"] = True
        if host_frame.exists() and guest_frame.exists():
            print(f"compare: {name} (image)", flush=True)
            entry["image"], heat = compare(read_frame(host_frame), read_frame(guest_frame))
            png(images / f"{name}-diff.png", *heat)
        host_buffer, guest_buffer = out / "host" / f"{name}.buf", out / "guest" / f"{name}.buf"
        if host_buffer.exists() and guest_buffer.exists():
            print(f"compare: {name} (buffer)", flush=True)
            entry["buffer"] = compare_buffers(read_buffer(host_buffer), read_buffer(guest_buffer))
        rows.append(entry)
    write_html(args, out, rows)
    write_summary(out, "OpenCL", rows, rate="ips", judge=sample_verdict)


def cell(value, unit="", decimals=1):
    return "&ndash;" if value is None else f"{value:,.{decimals}f}{unit}"


def sample_verdict(r):
    """The worst of the sample's comparisons, or what is missing."""
    h, g = r["host"], r["guest"]
    if not h or not g:
        return ("host only", "warn") if h else ("VM only", "warn")
    if h["status"] != "ok" or g["status"] != "ok":
        return (g["status"] if g["status"] != "ok" else f"host {h['status']}"), "bad"
    verdicts = []
    if r["image"]:
        verdicts.append(verdict(r["image"]))
    if r["buffer"]:
        verdicts.append(buffer_verdict(r["buffer"]))
    if not verdicts:
        return "no output", "bad"
    order = {"good": 0, "warn": 1, "bad": 2}
    return max(verdicts, key=lambda v: order[v[1]])


def write_html(args, out, rows):
    ratios = [r["guest"]["ips"] / r["host"]["ips"] for r in rows
              if r["host"] and r["guest"] and r["host"]["ips"] > 0 and r["guest"]["ips"] > 0]
    geomean = math.exp(sum(math.log(x) for x in ratios) / len(ratios)) if ratios else None
    verdicts = [sample_verdict(r) for r in rows]
    matched = sum(1 for v in verdicts if v[1] == "good")
    ok_host = sum(1 for r in rows if r["host"] and r["host"]["status"] == "ok")
    device = (out / "host" / "device.txt").read_text().strip() if (out / "host" / "device.txt").exists() else ""
    vm_device = (out / "guest" / "device.txt").read_text().strip() if (out / "guest" / "device.txt").exists() else ""
    vm = "loopback" if args.loopback else "VM"
    parts = [f"""<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>OpenCL samples: host vs {vm}</title>
<style>
:root {{ --bg:#fafaf8; --fg:#1d1d1b; --muted:#6b6b66; --card:#fff; --line:#e4e2dc; --good:#1f7a4d; --warn:#a36a00; --bad:#b3261e; }}
@media (prefers-color-scheme: dark) {{ :root {{ --bg:#161614; --fg:#ecebe6; --muted:#a19f97; --card:#1f1f1c; --line:#33322e; --good:#5cc08a; --warn:#e0a33a; --bad:#f07167; }} }}
body {{ background:var(--bg); color:var(--fg); font:14px/1.45 system-ui, sans-serif; margin:0; padding:24px max(16px, 3vw); }}
h1 {{ font-size:22px; margin:0 0 4px; }} .muted {{ color:var(--muted); }}
.summary {{ display:flex; flex-wrap:wrap; gap:12px; margin:18px 0 26px; }}
.stat {{ background:var(--card); border:1px solid var(--line); border-radius:10px; padding:12px 16px; min-width:150px; }}
.stat b {{ display:block; font-size:22px; }}
.sample {{ background:var(--card); border:1px solid var(--line); border-radius:12px; padding:16px; margin-bottom:18px; }}
.sample h2 {{ font-size:17px; margin:0 0 10px; display:flex; gap:10px; align-items:baseline; flex-wrap:wrap; }}
.tag {{ font-size:12px; padding:2px 8px; border-radius:99px; border:1px solid currentColor; }}
.good {{ color:var(--good); }} .warn {{ color:var(--warn); }} .bad {{ color:var(--bad); }}
.images {{ display:grid; grid-template-columns:repeat(auto-fit, minmax(260px, 1fr)); gap:10px; }}
figure {{ margin:0; }} figure img {{ width:100%; border-radius:6px; border:1px solid var(--line); display:block; }}
figcaption {{ font-size:12px; color:var(--muted); margin-top:4px; }}
table {{ border-collapse:collapse; margin-top:12px; font-variant-numeric:tabular-nums; }}
td, th {{ padding:4px 12px 4px 0; text-align:right; }} th:first-child, td:first-child {{ text-align:left; }}
th {{ color:var(--muted); font-weight:500; }}
.wrap {{ overflow-x:auto; }}
.overview td:nth-child(2), .overview th:nth-child(2) {{ text-align:left; }}
</style>
<h1>OpenCL samples: host vs {vm}</h1>
<div class="muted">{html.escape(datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))}
&middot; {args.warmup} warm-up iterations (at least 1 s), then blocks of {args.frames} iterations with a clFinish each, for at least 1 s
&middot; host: {html.escape(device or "the host's OpenCL")}
&middot; {vm}: {html.escape(vm_device or "not run")} ({"avcl&apos;s core, the driver, the device core and the executor in one host process" if args.loopback else "Khronos loader &rarr; avcl &rarr; virtio-gpu-av &rarr; the host&apos;s OpenCL"})</div>
<div class="summary">
<div class="stat"><span class="muted">samples</span><b>{len(rows)}</b></div>
<div class="stat"><span class="muted">correct on the host</span><b>{ok_host} / {len(rows)}</b></div>
<div class="stat"><span class="muted">{vm} output matching host</span><b>{matched} / {len(rows)}</b></div>
<div class="stat"><span class="muted">{vm} speed vs host (geomean)</span><b>{cell(geomean * 100 if geomean else None, "%", 1)}</b></div>
</div>
<div class="wrap"><table class="overview">
<tr><th>sample</th><th>verdict</th><th>host it/s</th><th>{vm} it/s</th><th>{vm} / host</th><th>host rate</th><th>{vm} rate</th></tr>"""]
    for r, (label, tone) in zip(rows, verdicts):
        h, g = r["host"], r["guest"]
        ratio = g["ips"] / h["ips"] if h and g and h["ips"] > 0 else None
        unit = (h or g)["unit"]
        parts.append(f"""<tr><td><a href="#{html.escape(r['name'])}">{html.escape(r['name'])}</a></td>
<td class="{tone}">{html.escape(label)}</td><td>{cell(h and h["ips"], "", 1)}</td><td>{cell(g and g["ips"], "", 1)}</td>
<td>{cell(ratio * 100 if ratio else None, "%", 1)}</td><td>{cell(h and h["rate"], " " + unit, 2)}</td>
<td>{cell(g and g["rate"], " " + unit, 2)}</td></tr>""")
    parts.append("</table></div>")
    for r, (label, tone) in zip(rows, verdicts):
        parts.append(sample_section(r, label, tone, vm))
    (out / "index.html").write_text("\n".join(parts))
    print(f"report: {out / 'index.html'}")


def sample_section(r, label, tone, vm):
    name, h, g = r["name"], r["host"], r["guest"]
    ratio = g["ips"] / h["ips"] if h and g and h["ips"] > 0 else None
    unit = (h or g)["unit"]
    section = [f"""<section class="sample" id="{html.escape(name)}"><h2>{html.escape(name)} <span class="tag {tone}">{html.escape(label)}</span>
<span class="muted" style="font-size:13px">{vm} at {cell(ratio * 100 if ratio else None, "%", 1)} of host speed</span></h2>"""]
    if r["has_image"]:
        section.append(f"""<div class="images">
<figure><img src="img/{name}-host.png" alt="host"><figcaption>host (native OpenCL)</figcaption></figure>
<figure><img src="img/{name}-vm.png" alt="vm"><figcaption>{vm} (avcl)</figcaption></figure>
<figure><img src="img/{name}-diff.png" alt="difference"><figcaption>difference, largest channel &times;8</figcaption></figure>
</div>""")
    section.append(f"""<div class="wrap"><table>
<tr><th></th><th>iterations / s</th><th>ms / iteration</th><th>rate</th><th>setup ms</th><th>status</th></tr>
<tr><td>host</td><td>{cell(h and h["ips"], "", 1)}</td><td>{cell(h and h["ms"] / max(h["iterations"], 1), "", 3)}</td><td>{cell(h and h["rate"], " " + unit, 2)}</td><td>{cell(h and h["init_ms"], "", 1)}</td><td>{html.escape(h["status"]) if h else "&ndash;"}</td></tr>
<tr><td>{vm}</td><td>{cell(g and g["ips"], "", 1)}</td><td>{cell(g and g["ms"] / max(g["iterations"], 1), "", 3)}</td><td>{cell(g and g["rate"], " " + unit, 2)}</td><td>{cell(g and g["init_ms"], "", 1)}</td><td>{html.escape(g["status"]) if g else "&ndash;"}</td></tr>
</table>""")
    stats = r["image"]
    if stats:
        section.append(f"""<table><tr><th>pixels differing (&gt;2)</th><th>largest difference</th><th>mean difference</th><th>PSNR</th></tr>
<tr><td>{stats["changed"]:.3f}%</td><td>{stats["max"]}</td><td>{stats["mean"]:.4f}</td><td>{"&infin;" if stats["psnr"] == float("inf") else f'{stats["psnr"]:.1f} dB'}</td></tr></table>""")
    b = r["buffer"]
    if b:
        first = "&ndash;" if not b["first"] else html.escape(f"[{b['first'][0]}] host {b['first'][1]!r}, {vm} {b['first'][2]!r}")
        section.append(f"""<table><tr><th>buffer</th><th>elements</th><th>bit-identical</th><th>differing</th><th>largest abs</th><th>largest rel</th><th>first difference</th></tr>
<tr><td>{b["kind"]}</td><td>{b["count"]:,}</td><td>{b["exact"]:,}</td><td>{b["differing"]:,}</td><td>{b["max_abs"]:.3g}</td><td>{b["max_rel"]:.3g}</td><td>{first}</td></tr></table>""")
    section.append("</div></section>")
    return "\n".join(section)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest-render",
                        help="the running guest's work directory")
    parser.add_argument("--build", type=pathlib.Path, default=ROOT / "build",
                        help="the build tree holding avclsamples_host and guest/avclsamples.exe")
    parser.add_argument("-o", "--out", type=pathlib.Path, default=ROOT / "build-samples" / "opencl-report")
    parser.add_argument("--frames", type=int, default=20, help="iterations per timed block")
    parser.add_argument("--warmup", type=int, default=10)
    parser.add_argument("--device", type=int, default=0, help="which host GPU the host run uses")
    parser.add_argument("--only", help="one sample")
    parser.add_argument("--guest-user", default="win11\\winuser", help="the guest's logged-on user")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--no-guest", action="store_true", help="reuse the guest results already in the output")
    parser.add_argument("--host-only", action="store_true", help="run nothing in the guest; report the host alone")
    parser.add_argument("--loopback", action="store_true",
                        help="no guest: run the suite through the ICD, driver, device and executor in one host "
                             "process (avclsamples_loopback) and report it in the VM's column")
    args = parser.parse_args()
    if not args.no_build:
        build(args)
    args.out.mkdir(parents=True, exist_ok=True)
    if args.loopback:
        shutil.rmtree(args.out / "img", ignore_errors=True)
        run_loopback(args, args.out)
    elif not args.no_guest and not args.host_only:
        for stale in ("guest", "img"):
            shutil.rmtree(args.out / stale, ignore_errors=True)
        run_guest(args, args.out)
    elif args.host_only:
        shutil.rmtree(args.out / "img", ignore_errors=True)
    run_host(args, args.out)
    report(args, args.out)


if __name__ == "__main__":
    main()
