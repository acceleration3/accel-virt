#!/usr/bin/env python3
"""The D3D12 samples (samples/d3d12) on the host and in the guest, side by side.

Runs the same program twice - natively on the host through vkd3d-proton, and
in the running guest through the system d3d12.dll, rvkumd.dll's OpenAdapter12,
the device and the executor - and writes an HTML report: each sample's frame
from both, the difference between them, and how fast each rendered.

    tools/fetch_dxc.sh                                   # once: Microsoft's DXC
    tools/d3d12_report.py --work build-guest-render -o build-samples12/report

The guest compiles the HLSL to DXIL with Microsoft's dxcompiler.dll (pushed
beside the program with dxil.dll, which signs it) and the host runs that same
DXIL, so a difference in the pictures is a difference in rendering. The guest
must already be running (run_guest_suite.py --keep).

With no guest, the host half runs alone: --no-guest --wine compiles the
shaders by running the guest's program under wine, with the same dxcompiler,
and renders them on the host.

    tools/d3d12_report.py --no-guest --wine -o build-samples12/report

--layer DIR puts DIR's d3d12.dll and d3d12core.dll (vkd3d-proton's Windows
build) beside the program in the guest, so D3D12 reaches the host through the
guest's Vulkan driver instead of rvkumd.dll: the route feature 047's probes
take, for comparing the two.
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
from d3d11_report import cell, compare, guest_shell, metrics, png, read_frame, verdict, write_summary  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUEST_DIR = "C:\\rvk\\samples12"
GUEST_EXE = GUEST_DIR + "\\rvkd3d12samples.exe"
DXC = ROOT / "third_party" / "dxc"
DXC_FILES = ("dxcompiler.dll", "dxil.dll")
# dxcompiler.dll links the Visual C++ runtime, which a clean Windows install
# lacks: without it the samples find no compiler beside them.
VC_REDIST = ROOT / "third_party" / "vcredist" / "vc_redist.x64.exe"
VC_REDIST_URL = "https://aka.ms/vs/17/release/vc_redist.x64.exe"
GUEST_EXE_LOCAL = ROOT / "build" / "guest" / "rvkd3d12samples.exe"
HOST_EXE = ROOT / "build" / "rvkd3d12samples_host"


def build():
    subprocess.run([str(ROOT / "tools" / "fetch_dxc.sh")], check=True, stdout=subprocess.DEVNULL)
    for target in ("rvktest", "rvkd3d12samples_host"):
        subprocess.run(["cmake", "--build", str(ROOT / "build"), "--target", target], check=True,
                       stdout=subprocess.DEVNULL)


def sample_arguments(args, out, shaders):
    return ["--out", out, "--shaders", shaders, "--frames", str(args.frames), "--warmup", str(args.warmup),
            "--width", str(args.width), "--height", str(args.height)] + (["--only", args.only] if args.only else [])


def ensure_vc_runtime(work):
    """Microsoft's Visual C++ runtime in the guest, installed quietly when it
    is not there: the guest disk is a fresh overlay each boot."""
    present = guest_shell(work, "ps", "Test-Path C:\\Windows\\System32\\vcruntime140_1.dll").strip()
    if present.endswith("True"):
        return
    if not VC_REDIST.exists():
        VC_REDIST.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["curl", "-sSL", "-o", str(VC_REDIST), VC_REDIST_URL], check=True, timeout=600)
    guest_shell(work, "push", str(VC_REDIST), "C:\\rvk\\vc_redist.x64.exe")
    print("guest: installing the Visual C++ runtime", flush=True)
    guest_shell(work, "ps", "Start-Process -Wait C:\\rvk\\vc_redist.x64.exe -ArgumentList '/install','/quiet','/norestart'",
                timeout=600)


def run_guest(args, out):
    """The guest's run, and everything it wrote brought back."""
    work = args.work
    guest_shell(work, "ps", f"Remove-Item -Recurse -Force {GUEST_DIR} -ErrorAction SilentlyContinue; "
                            f"New-Item -ItemType Directory -Force {GUEST_DIR}\\out, {GUEST_DIR}\\shaders | Out-Null")
    guest_shell(work, "push", str(GUEST_EXE_LOCAL), GUEST_EXE)
    ensure_vc_runtime(work)
    for name in DXC_FILES:
        guest_shell(work, "push", str(DXC / name), f"{GUEST_DIR}\\{name}")
    if args.layer:
        for name in ("d3d12.dll", "d3d12core.dll"):
            guest_shell(work, "push", str(args.layer / name), f"{GUEST_DIR}\\{name}")
    print("guest: rendering", flush=True)
    # The user's interactive session, where the D3D runtime loads the user-mode driver.
    command = (f"/c cd /d {GUEST_DIR} && {GUEST_EXE} "
               + " ".join(sample_arguments(args, f"{GUEST_DIR}\\out", f"{GUEST_DIR}\\shaders"))
               + f" > {GUEST_DIR}\\run.txt 2>&1")
    output = guest_shell(work, "ps", f"""
Unregister-ScheduledTask -TaskName rvksamples12 -Confirm:$false -ErrorAction SilentlyContinue
$a = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument '{command}'
$p = New-ScheduledTaskPrincipal -UserId '{args.guest_user}' -LogonType Interactive -RunLevel Highest
$s = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 1)
Register-ScheduledTask -TaskName rvksamples12 -Action $a -Principal $p -Settings $s | Out-Null
Start-ScheduledTask -TaskName rvksamples12
Start-Sleep 2
while ((Get-ScheduledTask rvksamples12).State -eq 'Running') {{ Start-Sleep 1 }}
'exit: ' + (Get-ScheduledTaskInfo rvksamples12).LastTaskResult
Get-Content {GUEST_DIR}\\run.txt -ErrorAction SilentlyContinue""", timeout=1800)
    print(output, flush=True)
    (out / "guest.log").write_text(output)
    # Everything in one gzip-compressed tar-like bundle: file name, length, bytes.
    bundle = f"{GUEST_DIR}\\bundle.gz"
    guest_shell(work, "ps", f"""
$out = [IO.File]::Create('{bundle}')
$zip = New-Object IO.Compression.GZipStream($out, [IO.Compression.CompressionMode]::Compress)
$writer = New-Object IO.BinaryWriter($zip)
foreach ($f in (Get-ChildItem {GUEST_DIR}\\out, {GUEST_DIR}\\shaders -File)) {{
    $name = [Text.Encoding]::UTF8.GetBytes($f.Directory.Name + '/' + $f.Name)
    $bytes = [IO.File]::ReadAllBytes($f.FullName)
    $writer.Write([int]$name.Length); $writer.Write($name)
    $writer.Write([int]$bytes.Length); $writer.Write($bytes)
}}
$writer.Close()""")
    local = out / "guest-bundle.gz"
    pulled = ""
    for attempt in range(5):
        pulled = guest_shell(work, "pull", bundle, str(local))
        if local.exists() and local.stat().st_size != 0:
            break
    if not local.exists() or local.stat().st_size == 0:
        sys.exit(f"could not bring the guest's results back: {pulled.strip()}")
    data = gzip.decompress(local.read_bytes())
    at = 0
    while at < len(data):
        (length,) = struct.unpack_from("<i", data, at)
        name = data[at + 4:at + 4 + length].decode()
        at += 4 + length
        (size,) = struct.unpack_from("<i", data, at)
        folder, _, file = name.partition("/")
        target = out / ("guest" if folder == "out" else "shaders") / file
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data[at + 4:at + 4 + size])
        at += 4 + size
    local.unlink()


def compile_with_wine(args, out):
    """The guest's program under wine, --compile: DXIL from the same dxcompiler
    the guest runs, written where the host reads it."""
    print("wine: compiling the shaders", flush=True)
    staging = out / "wine"
    shutil.rmtree(staging, ignore_errors=True)
    (staging / "out").mkdir(parents=True)
    (out / "shaders").mkdir(parents=True, exist_ok=True)
    shutil.copy2(GUEST_EXE_LOCAL, staging)
    for name in DXC_FILES:
        shutil.copy2(DXC / name, staging)
    command = ["wine", str(staging / GUEST_EXE_LOCAL.name), "--compile",
               "--out", f"Z:{(staging / 'out').resolve()}", "--shaders", f"Z:{(out / 'shaders').resolve()}"]
    if args.only:
        command += ["--only", args.only]
    done = subprocess.run(command, capture_output=True, text=True, timeout=900,
                          env=dict(os.environ, WINEDEBUG="-all"))
    (out / "wine.log").write_text(done.stdout + done.stderr)
    failed = [line for line in (staging / "out" / "metrics.tsv").read_text().splitlines()[1:]
              if "\tcompiled\t" not in line] if (staging / "out" / "metrics.tsv").exists() else ["no metrics"]
    if failed:
        print("wine: not every sample compiled:\n  " + "\n  ".join(failed), flush=True)


def run_host(args, out):
    print("host: rendering", flush=True)
    shutil.rmtree(out / "host", ignore_errors=True)
    (out / "host").mkdir(parents=True)
    environment = dict(os.environ, RVK_D3D_PATH=str(args.d3d_runtime))
    done = subprocess.run([str(HOST_EXE)] + sample_arguments(args, str(out / "host"), str(out / "shaders")),
                          capture_output=True, text=True, env=environment, timeout=1800)
    (out / "host.log").write_text(done.stdout + done.stderr)


def report(args, out):
    host = metrics(out / "host" / "metrics.tsv")
    guest = metrics(out / "guest" / "metrics.tsv")
    names = list(dict.fromkeys(list(guest) + list(host)))
    images = out / "img"
    images.mkdir(exist_ok=True)
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
    write_summary(out, "D3D12", rows)


def write_html(args, out, rows, ratios):
    geomean = math.exp(sum(math.log(r) for r in ratios) / len(ratios)) if ratios else None
    matched = sum(1 for r in rows if r["stats"] and verdict(r["stats"])[1] == "good")
    host_ok = sum(1 for r in rows if r["host"] and r["host"]["status"] == "ok")
    host_fps = [r["host"]["fps"] for r in rows if r["host"] and r["host"]["fps"] > 0]
    host_geomean = math.exp(sum(math.log(f) for f in host_fps) / len(host_fps)) if host_fps else None
    gpu = args.gpu_name
    vm_route = ("vkd3d-proton's d3d12.dll &rarr; rvkvk.dll &rarr; virtio-gpu-rvk &rarr; the host's Vulkan"
                if args.layer else "d3d12.dll &rarr; rvkumd &rarr; virtio-gpu-rvk &rarr; vkd3d-proton")
    parts = [f"""<!doctype html><meta charset="utf-8"><title>D3D12 samples: host vs VM</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
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
<h1>D3D12 samples: host vs VM</h1>
<div class="muted">{html.escape(datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))} &middot; {args.width}&times;{args.height}
&middot; {args.warmup} warm-up + blocks of {args.frames} timed frames, off-screen &middot; host: vkd3d-proton native on {html.escape(gpu)}
&middot; VM: {vm_route} &middot; shaders compiled once to DXIL by dxcompiler {"under wine on the host" if args.wine else "in the guest"}</div>
<div class="summary">
<div class="stat"><span class="muted">samples</span><b>{len(rows)}</b></div>
<div class="stat"><span class="muted">rendered on the host</span><b>{host_ok} / {len(rows)}</b></div>
<div class="stat"><span class="muted">host fps (geomean)</span><b>{cell(host_geomean, "", 0)}</b></div>
<div class="stat"><span class="muted">pictures matching</span><b>{matched} / {len(rows)}</b></div>
<div class="stat"><span class="muted">VM speed vs host (geomean)</span><b>{cell(geomean * 100 if geomean else None, "%", 1)}</b></div>
</div>"""]
    for r in rows:
        name, h, g, stats = r["name"], r["host"], r["guest"], r["stats"]
        if stats:
            label, tone = verdict(stats)
        else:
            label, tone = ("missing", "bad")
        ratio = g["fps"] / h["fps"] if h and g and h["fps"] > 0 else None
        parts.append(f"""<section class="sample"><h2>{html.escape(name)} <span class="tag {tone}">{label}</span>
<span class="muted" style="font-size:13px">VM at {cell(ratio * 100 if ratio else None, "%", 1)} of host speed</span></h2>
<div class="images">
<figure><img src="img/{name}-host.png" alt="host"><figcaption>host (vkd3d-proton native)</figcaption></figure>
<figure><img src="img/{name}-vm.png" alt="vm"><figcaption>VM ({vm_route})</figcaption></figure>
<figure><img src="img/{name}-diff.png" alt="difference"><figcaption>difference, largest channel &times;8</figcaption></figure>
</div><div class="wrap"><table>
<tr><th></th><th>fps</th><th>ms / frame</th><th>setup ms</th><th>status</th></tr>
<tr><td>host</td><td>{cell(h and h["fps"], "", 0)}</td><td>{cell(h and h["ms"] / max(h["frames"], 1), "", 3)}</td><td>{cell(h and h["init_ms"], "", 1)}</td><td>{html.escape(h["status"]) if h else "&ndash;"}</td></tr>
<tr><td>VM</td><td>{cell(g and g["fps"], "", 0)}</td><td>{cell(g and g["ms"] / max(g["frames"], 1), "", 3)}</td><td>{cell(g and g["init_ms"], "", 1)}</td><td>{html.escape(g["status"]) if g else "&ndash;"}</td></tr>
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
    parser.add_argument("-o", "--out", type=pathlib.Path, default=ROOT / "build-samples12" / "report")
    parser.add_argument("--frames", type=int, default=3000)
    parser.add_argument("--warmup", type=int, default=60)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--only", help="one sample, by name")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--no-guest", action="store_true", help="reuse the guest results already in the output")
    parser.add_argument("--wine", action="store_true",
                        help="compile the shaders with the guest's program under wine, not in the guest")
    parser.add_argument("--layer", type=pathlib.Path,
                        help="a directory holding vkd3d-proton's Windows d3d12.dll and d3d12core.dll, run beside "
                             "the program in the guest instead of the system runtime")
    parser.add_argument("--d3d-runtime", type=pathlib.Path, default=ROOT / "build-d3d" / "lib",
                        help="where tools/build_d3d_runtime.sh installed vkd3d-proton (RVK_D3D_PATH)")
    parser.add_argument("--gpu-name", default="the host GPU")
    parser.add_argument("--guest-user", default="win11\\winuser", help="the guest's logged-on user")
    args = parser.parse_args()
    if not args.no_build:
        build()
    args.out.mkdir(parents=True, exist_ok=True)
    if not args.no_guest:
        for stale in ("guest", "shaders", "img"):
            shutil.rmtree(args.out / stale, ignore_errors=True)
        run_guest(args, args.out)
    if args.wine:
        compile_with_wine(args, args.out)
    run_host(args, args.out)
    report(args, args.out)


if __name__ == "__main__":
    main()
