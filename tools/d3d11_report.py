#!/usr/bin/env python3
"""The D3D11 samples (samples/d3d11) on the host and in the guest, side by side.

Runs the same program twice - natively on the host through DXVK, and in the
running guest through rvkumd.dll, the device and the executor - and writes an
HTML report: each sample's frame from both, the difference between them, and
how fast each rendered.

    tools/d3d11_report.py --work build-guest-render -o build-samples/report

The guest compiles the HLSL with Microsoft's compiler and the host runs that
same bytecode, so a difference in the pictures is a difference in rendering.
The guest must already be running (run_guest_suite.py --keep).
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
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUEST_DIR = "C:\\rvk\\samples"
GUEST_EXE = GUEST_DIR + "\\rvkd3d11samples.exe"


def guest_shell(work, *arguments, timeout=900):
    command = [sys.executable, "-B", str(ROOT / "tools" / "guest" / "guest_shell.py"), "--work", str(work),
               "--timeout", str(timeout - 30), *arguments]
    done = subprocess.run(command, capture_output=True, text=True, timeout=timeout)
    return done.stdout + done.stderr


def build():
    for target in ("rvktest", "rvkd3d11samples_host"):
        subprocess.run(["cmake", "--build", str(ROOT / "build"), "--target", target], check=True,
                       stdout=subprocess.DEVNULL)


def run_guest(args, out):
    """The guest's run, and everything it wrote brought back."""
    work = args.work
    guest_shell(work, "ps", f"Remove-Item -Recurse -Force {GUEST_DIR} -ErrorAction SilentlyContinue; "
                            f"New-Item -ItemType Directory -Force {GUEST_DIR}\\out, {GUEST_DIR}\\shaders | Out-Null")
    guest_shell(work, "push", str(ROOT / "build" / "guest" / "rvkd3d11samples.exe"), GUEST_EXE)
    print("guest: rendering", flush=True)
    # The user's interactive session, where the D3D runtime loads the user-mode driver.
    command = (f"/c {GUEST_EXE} --out {GUEST_DIR}\\out --shaders {GUEST_DIR}\\shaders --frames {args.frames} "
               f"--warmup {args.warmup} --width {args.width} --height {args.height} > {GUEST_DIR}\\run.txt 2>&1")
    output = guest_shell(work, "ps", f"""
Unregister-ScheduledTask -TaskName rvksamples -Confirm:$false -ErrorAction SilentlyContinue
$a = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument '{command}'
$p = New-ScheduledTaskPrincipal -UserId '{args.guest_user}' -LogonType Interactive -RunLevel Highest
$s = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 1)
Register-ScheduledTask -TaskName rvksamples -Action $a -Principal $p -Settings $s | Out-Null
Start-ScheduledTask -TaskName rvksamples
Start-Sleep 2
while ((Get-ScheduledTask rvksamples).State -eq 'Running') {{ Start-Sleep 1 }}
'exit: ' + (Get-ScheduledTaskInfo rvksamples).LastTaskResult
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
    pulled = guest_shell(work, "pull", bundle, str(local))
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


def run_host(args, out):
    print("host: rendering", flush=True)
    shutil.rmtree(out / "host", ignore_errors=True)
    (out / "host").mkdir(parents=True)
    environment = dict(os.environ, RVK_D3D_PATH=str(ROOT / "build-d3d" / "lib"))
    done = subprocess.run([str(ROOT / "build" / "rvkd3d11samples_host"), "--out", str(out / "host"),
                           "--shaders", str(out / "shaders"), "--frames", str(args.frames),
                           "--warmup", str(args.warmup), "--width", str(args.width), "--height", str(args.height)],
                          capture_output=True, text=True, env=environment, timeout=1800)
    (out / "host.log").write_text(done.stdout + done.stderr)


def metrics(path):
    rows = {}
    if not path.exists():
        return rows
    lines = path.read_text().splitlines()
    for line in lines[1:]:
        fields = line.split("\t")
        if len(fields) == 6:
            rows[fields[0]] = {"status": fields[1], "init_ms": float(fields[2]), "frames": int(fields[3]),
                               "ms": float(fields[4]), "fps": float(fields[5])}
    return rows


def read_frame(path):
    data = path.read_bytes()
    magic, width, height = struct.unpack_from("<III", data, 0)
    if magic != 0x41524742:
        raise ValueError(f"{path}: not a frame")
    return width, height, data[12:]


def png(path, width, height, bgra):
    """RGB PNG from BGRA rows, with zlib alone."""
    rows = bytearray()
    stride = width * 4
    for y in range(height):
        row = bgra[y * stride:(y + 1) * stride]
        rgb = bytearray(width * 3)
        rgb[0::3] = row[2::4]
        rgb[1::3] = row[1::4]
        rgb[2::3] = row[0::4]
        rows += b"\x00" + rgb

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(rows), 6)) +
                     chunk(b"IEND", b""))


def compare(host, guest):
    """Per-channel differences, and a heat map of the largest channel's (x8)."""
    width, height, a = host
    _, _, b = guest
    diff = bytes(abs(x - y) for x, y in zip(a, b))
    count = width * height
    worst = bytearray(count)
    worst_b, worst_g, worst_r = diff[0::4], diff[1::4], diff[2::4]
    for i in range(count):
        m = worst_b[i]
        if worst_g[i] > m:
            m = worst_g[i]
        if worst_r[i] > m:
            m = worst_r[i]
        worst[i] = m
    changed = sum(1 for m in worst if m > 2)
    squared = sum(d * d for i, d in enumerate(diff) if i % 4 != 3)
    mse = squared / (count * 3)
    psnr = float("inf") if mse == 0 else 10 * math.log10(255 * 255 / mse)
    heat = bytearray(count * 4)
    for i, m in enumerate(worst):
        v = min(255, m * 8)
        heat[i * 4 + 0] = 0 if v else 40      # blue: background where equal
        heat[i * 4 + 1] = v // 2
        heat[i * 4 + 2] = v
        heat[i * 4 + 3] = 255
    stats = {"max": max(worst) if count else 0, "mean": sum(worst) / count if count else 0.0,
             "changed": 100.0 * changed / count if count else 0.0, "psnr": psnr}
    return stats, (width, height, bytes(heat))


def verdict(stats):
    if stats["max"] == 0:
        return "identical", "good"
    if stats["psnr"] >= 40 and stats["changed"] < 1.0:
        return "matches", "good"
    if stats["psnr"] >= 30:
        return "close", "warn"
    return "differs", "bad"


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


def cell(value, unit="", decimals=1):
    return "&ndash;" if value is None else f"{value:,.{decimals}f}{unit}"


def write_html(args, out, rows, ratios):
    geomean = math.exp(sum(math.log(r) for r in ratios) / len(ratios)) if ratios else None
    matched = sum(1 for r in rows if r["stats"] and verdict(r["stats"])[1] == "good")
    gpu = args.gpu_name
    parts = [f"""<!doctype html><meta charset="utf-8"><title>D3D11 samples: host vs VM</title>
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
<h1>D3D11 samples: host vs VM</h1>
<div class="muted">{html.escape(datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))} &middot; {args.width}&times;{args.height}
&middot; {args.warmup} warm-up + {args.frames} timed frames, off-screen &middot; host: DXVK native on {html.escape(gpu)}
&middot; VM: rvkumd &rarr; virtio-gpu-rvk &rarr; DXVK on the same GPU &middot; shaders compiled once by d3dcompiler_47 in the guest</div>
<div class="summary">
<div class="stat"><span class="muted">samples</span><b>{len(rows)}</b></div>
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
<figure><img src="img/{name}-host.png" alt="host"><figcaption>host (DXVK native)</figcaption></figure>
<figure><img src="img/{name}-vm.png" alt="vm"><figcaption>VM (rvkumd over virtio-gpu-rvk)</figcaption></figure>
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
    parser.add_argument("-o", "--out", type=pathlib.Path, default=ROOT / "build-samples" / "report")
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--warmup", type=int, default=60)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--no-guest", action="store_true", help="reuse the guest results already in the output")
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
    run_host(args, args.out)
    report(args, args.out)


if __name__ == "__main__":
    main()
