#!/usr/bin/env python3
"""The OpenGL samples (samples/opengl) on the host and in the guest, side by side.

Runs the same program twice - natively on the host through the host's own
OpenGL driver (EGL), and in the running guest through the system opengl32.dll,
rvkgl.dll, the device and the executor - and writes an HTML report: each
sample's frame from both, the difference between them, and how fast each
rendered.

    tools/opengl_report.py --work build-guest-render -o build-samples/gl-report

Each machine compiles the GLSL with the driver in front of it, and the host's
and the executor's are the same driver on the same GPU, so a difference in the
pictures is a difference in what reached it. The guest must already be running
(run_guest_suite.py --keep) with this tree's driver package installed.

With no VM, `--loopback` puts the samples through the same ICD, driver and
device core on the host instead (build/rvkglsamples_loopback, no guest in
between), which is the ICD's path less the VM:

    tools/opengl_report.py --loopback -o build-samples/gl-report

Add --kernel-submits to keep every batch on the kernel's path, the process
never publishing its own ring tail - the other of the two ways a batch reaches
the device, to compare them.
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

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from d3d11_report import compare, guest_shell, metrics, png, read_frame, verdict, write_summary  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUEST_DIR = "C:\\rvk\\glsamples"
GUEST_EXE = GUEST_DIR + "\\rvkglsamples.exe"
BUILD_JOBS = "8"


def build(args):
    targets = ["rvkglsamples_host"] + (["rvkglsamples_loopback"] if args.loopback else ["rvktest"])
    for target in targets:
        subprocess.run(["cmake", "--build", str(ROOT / "build"), "--target", target, "-j", BUILD_JOBS], check=True,
                       stdout=subprocess.DEVNULL)


def sample_arguments(args, out):
    arguments = ["--out", str(out), "--frames", str(args.frames), "--warmup", str(args.warmup),
                 "--width", str(args.width), "--height", str(args.height)]
    if args.only:
        arguments += ["--only", args.only]
    return arguments


def run_guest(args, out):
    """The guest's run, and everything it wrote brought back."""
    work = args.work
    guest_shell(work, "ps", f"Remove-Item -Recurse -Force {GUEST_DIR} -ErrorAction SilentlyContinue; "
                            f"New-Item -ItemType Directory -Force {GUEST_DIR}\\out | Out-Null")
    guest_shell(work, "push", str(ROOT / "build" / "guest" / "rvkglsamples.exe"), GUEST_EXE)
    print("guest: rendering", flush=True)
    # The user's interactive session: opengl32 loads the display driver's ICD
    # for a window on the adapter's desktop.
    only = f" --only {args.only}" if args.only else ""
    command = (f"/c {GUEST_EXE} --out {GUEST_DIR}\\out --frames {args.frames} --warmup {args.warmup} "
               f"--width {args.width} --height {args.height}{only} > {GUEST_DIR}\\run.txt 2>&1")
    output = guest_shell(work, "ps", f"""
Unregister-ScheduledTask -TaskName rvkglsamples -Confirm:$false -ErrorAction SilentlyContinue
$a = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument '{command}'
$p = New-ScheduledTaskPrincipal -UserId '{args.guest_user}' -LogonType Interactive -RunLevel Highest
$s = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 1)
Register-ScheduledTask -TaskName rvkglsamples -Action $a -Principal $p -Settings $s | Out-Null
Start-ScheduledTask -TaskName rvkglsamples
Start-Sleep 2
while ((Get-ScheduledTask rvkglsamples).State -eq 'Running') {{ Start-Sleep 1 }}
'exit: ' + (Get-ScheduledTaskInfo rvkglsamples).LastTaskResult
Get-Content {GUEST_DIR}\\run.txt -ErrorAction SilentlyContinue""", timeout=1800)
    print(output, flush=True)
    (out / "guest.log").write_text(output)
    # Everything in one gzip-compressed bundle: file name, length, bytes.
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
    """The ICD's path on the host: the guest column, less the VM."""
    print("loopback: rendering", flush=True)
    target = out / "guest"
    target.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, MESA_SHADER_CACHE_DISABLE="true", ASAN_OPTIONS="detect_leaks=0")
    submits = ["--kernel-submits"] if args.kernel_submits else []
    done = subprocess.run([str(ROOT / "build" / "rvkglsamples_loopback"), *submits, *sample_arguments(args, target)],
                          capture_output=True, text=True, env=environment, timeout=3600)
    (out / "guest.log").write_text(done.stdout + done.stderr)
    print(done.stdout, flush=True)


def run_host(args, out):
    print("host: rendering", flush=True)
    shutil.rmtree(out / "host", ignore_errors=True)
    (out / "host").mkdir(parents=True)
    arguments = sample_arguments(args, out / "host")
    if args.gpu:
        arguments += ["--gpu", args.gpu]
    done = subprocess.run([str(ROOT / "build" / "rvkglsamples_host"), *arguments], capture_output=True, text=True,
                          timeout=1800)
    (out / "host.log").write_text(done.stdout + done.stderr)
    print(done.stdout, flush=True)


def renderer(log):
    """The `renderer:` line a run printed, or nothing."""
    if not log.exists():
        return None
    for line in log.read_text(errors="replace").splitlines():
        if line.startswith("renderer: "):
            return line[len("renderer: "):].strip()
    return None


def report(args, out):
    host = metrics(out / "host" / "metrics.tsv")
    guest = metrics(out / "guest" / "metrics.tsv")
    names = list(dict.fromkeys(list(host) + list(guest)))
    images = out / "img"
    shutil.rmtree(images, ignore_errors=True)
    images.mkdir()
    rows = []
    ratios = []
    for name in names:
        h, g = host.get(name), guest.get(name)
        entry = {"name": name, "host": h, "guest": g, "stats": None}
        host_frame, guest_frame = out / "host" / f"{name}.bgra", out / "guest" / f"{name}.bgra"
        if host_frame.exists():
            png(images / f"{name}-host.png", *read_frame(host_frame))
        if guest_frame.exists():
            png(images / f"{name}-vm.png", *read_frame(guest_frame))
        if host_frame.exists() and guest_frame.exists():
            print(f"compare: {name}", flush=True)
            entry["stats"], heat = compare(read_frame(host_frame), read_frame(guest_frame))
            png(images / f"{name}-diff.png", *heat)
        if h and g and h["fps"] > 0 and g["fps"] > 0:
            ratios.append(g["fps"] / h["fps"])
        rows.append(entry)
    write_html(args, out, rows, ratios)
    write_summary(out, "OpenGL", rows)


def cell(value, unit="", decimals=1):
    return "&ndash;" if value is None else f"{value:,.{decimals}f}{unit}"


def write_html(args, out, rows, ratios):
    geomean = math.exp(sum(math.log(r) for r in ratios) / len(ratios)) if ratios else None
    matched = sum(1 for r in rows if r["stats"] and verdict(r["stats"])[1] == "good")
    other = "loopback" if args.loopback else "VM"
    other_path = ("rvkgl &rarr; driver and device core in one host process &rarr; executor"
                  if args.loopback else "opengl32 &rarr; rvkgl &rarr; virtio-gpu-rvk &rarr; executor")
    host_gl = html.escape(renderer(out / "host.log") or "the host GPU")
    guest_gl = html.escape(renderer(out / "guest.log") or "&ndash;")
    parts = [f"""<!doctype html><meta charset="utf-8"><title>OpenGL samples: host vs {other}</title>
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
</style>
<h1>OpenGL samples: host vs {other}</h1>
<div class="muted">{html.escape(datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))} &middot; {args.width}&times;{args.height}
&middot; {args.warmup} warm-up frames (and at least a second) + blocks of {args.frames} timed frames, off-screen
&middot; host: {host_gl} &middot; {other}: {other_path}, reporting {guest_gl}
&middot; GLSL compiled by the driver on each side</div>
<div class="summary">
<div class="stat"><span class="muted">samples</span><b>{len(rows)}</b></div>
<div class="stat"><span class="muted">pictures matching</span><b>{matched} / {len(rows)}</b></div>
<div class="stat"><span class="muted">{other} speed vs host (geomean)</span><b>{cell(geomean * 100 if geomean else None, "%", 1)}</b></div>
</div>"""]
    for r in rows:
        name, h, g, stats = r["name"], r["host"], r["guest"], r["stats"]
        label, tone = verdict(stats) if stats else ("missing", "bad")
        ratio = g["fps"] / h["fps"] if h and g and h["fps"] > 0 else None
        parts.append(f"""<section class="sample"><h2>{html.escape(name)} <span class="tag {tone}">{label}</span>
<span class="muted" style="font-size:13px">{other} at {cell(ratio * 100 if ratio else None, "%", 1)} of host speed</span></h2>
<div class="images">
<figure><img src="img/{name}-host.png" alt="host"><figcaption>host (native OpenGL)</figcaption></figure>
<figure><img src="img/{name}-vm.png" alt="{other}"><figcaption>{other} (rvkgl)</figcaption></figure>
<figure><img src="img/{name}-diff.png" alt="difference"><figcaption>difference, largest channel &times;8</figcaption></figure>
</div><div class="wrap"><table>
<tr><th></th><th>fps</th><th>ms / frame</th><th>setup ms</th><th>status</th></tr>
<tr><td>host</td><td>{cell(h and h["fps"], "", 0)}</td><td>{cell(h and h["ms"] / max(h["frames"], 1), "", 3)}</td><td>{cell(h and h["init_ms"], "", 1)}</td><td>{html.escape(h["status"]) if h else "&ndash;"}</td></tr>
<tr><td>{other}</td><td>{cell(g and g["fps"], "", 0)}</td><td>{cell(g and g["ms"] / max(g["frames"], 1), "", 3)}</td><td>{cell(g and g["init_ms"], "", 1)}</td><td>{html.escape(g["status"]) if g else "&ndash;"}</td></tr>
</table>""")
        if stats:
            parts.append(f"""<table><tr><th>pixels differing (&gt;2)</th><th>largest difference</th><th>mean difference</th><th>PSNR</th></tr>
<tr><td>{stats["changed"]:.3f}%</td><td>{stats["max"]}</td><td>{stats["mean"]:.4f}</td><td>{"&infin;" if stats["psnr"] == float("inf") else f'{stats["psnr"]:.1f} dB'}</td></tr></table>""")
        parts.append("</div></section>")
    (out / "index.html").write_text("\n".join(parts))
    print(f"report: {out / 'index.html'}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest-render", help="the running guest's work directory")
    parser.add_argument("-o", "--out", type=pathlib.Path, default=ROOT / "build-samples" / "gl-report")
    parser.add_argument("--frames", type=int, default=3000)
    parser.add_argument("--warmup", type=int, default=60)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--only", help="one sample, by name")
    parser.add_argument("--gpu", help="the host's GPU, by part of its GL_RENDERER (default: the one with the most memory)")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--no-guest", action="store_true", help="reuse the guest results already in the output")
    parser.add_argument("--loopback", action="store_true",
                        help="the ICD through the driver and device core on the host, instead of a VM")
    parser.add_argument("--kernel-submits", action="store_true",
                        help="with --loopback: every batch through the kernel, the process never publishing its own "
                             "ring tail")
    parser.add_argument("--guest-user", default="win11\\winuser", help="the guest's logged-on user")
    args = parser.parse_args()
    if not args.no_build:
        build(args)
    args.out.mkdir(parents=True, exist_ok=True)
    if not args.no_guest:
        shutil.rmtree(args.out / "guest", ignore_errors=True)
        if args.loopback:
            run_loopback(args, args.out)
        else:
            run_guest(args, args.out)
    run_host(args, args.out)
    report(args, args.out)


if __name__ == "__main__":
    main()
