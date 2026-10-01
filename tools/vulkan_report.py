#!/usr/bin/env python3
"""The Vulkan samples (samples/vulkan) on the host and in the guest, side by side.

Runs the same examples twice - natively on the host through its Vulkan driver,
and in the running guest through the Khronos loader, avvk.dll, the device and
the executor - and writes an HTML report like tools/d3d11_report.py's: each
sample's frame from both, the difference between them, and how fast each
rendered.

    tools/vulkan_report.py --work build-guest-render -o build-vulkan-samples/report
    tools/vulkan_report.py --no-guest -o build-vulkan-samples/report    # the host half alone

Every sample runs in a process of its own, so one that fails or hangs is a
row in the report and not the end of the run. The guest must already be
running (run_guest_suite.py --keep).
"""
import argparse
import datetime
import html
import math
import os
import pathlib
import shutil
import subprocess
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from d3d11_report import STYLE, compare, guest_shell, png, read_frame, verdict, write_summary  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
DATA = ROOT / "third_party" / "vulkan-samples" / "Vulkan-Samples" / "data"
HOST_EXE = ROOT / "build-vulkan-samples" / "host" / "avvksamples"
GUEST_BUILD = ROOT / "build-vulkan-samples" / "guest" / "avvksamples.exe"
LOADER = ROOT / "third_party" / "vulkan-runtime" / "vulkan-1.dll"
GUEST_DIR = "C:\\av\\vksamples"
GUEST_EXE = GUEST_DIR + "\\avvksamples.exe"
# The runner's own watchdog ends a stuck sample first; this is for one that
# cannot even do that.
SAMPLE_TIMEOUT = 600
FIELDS = 6
# What makes a sample's two pictures differ that is the sample's own doing.
NOTES = {
    "computeparticles": "Differs between two runs on one machine: upstream's compute shader runs 16 invocations "
                        "per particle (local_size_y 16 over a one-dimensional dispatch), racing on its position.",
    "multithreading": "Splits 256 objects over std::thread::hardware_concurrency() threads, so the scene and the "
                      "work follow each machine's core count.",
}


def build(args):
    targets = ["host"] if args.no_guest else ["all"]
    for target in targets:
        subprocess.run([str(ROOT / "tools" / "build_vulkan_samples.sh"), target, f"-j{args.jobs}"], check=True,
                       stdout=subprocess.DEVNULL)


def sample_names(args):
    if args.only:
        return args.only.split(",")
    listed = subprocess.run([str(HOST_EXE), "--list"], capture_output=True, text=True, check=True)
    return listed.stdout.split()


def runner_arguments(args):
    return ["--frames", str(args.frames), "--warmup", str(args.warmup), "--width", str(args.width),
            "--height", str(args.height)]


def result_lines(text):
    """The runner's per-sample lines - name, status, init ms, frames, ms, fps - out of a log."""
    rows = {}
    for line in text.splitlines():
        fields = line.split("\t")
        if len(fields) != FIELDS:
            continue
        try:
            rows[fields[0]] = {"status": fields[1], "init_ms": float(fields[2]), "frames": int(fields[3]),
                               "ms": float(fields[4]), "fps": float(fields[5])}
        except ValueError:
            continue
    return rows


def write_metrics(path, names, rows, exits):
    """metrics.tsv as the D3D11 runner writes it, with a row for every sample that ended without one."""
    lines = ["sample\tstatus\tinit_ms\tframes\tms\tfps"]
    for name in names:
        row = rows.get(name) or {"status": f"crashed ({exits.get(name, '?')})", "init_ms": 0.0, "frames": 0,
                                 "ms": 0.0, "fps": 0.0}
        lines.append(f"{name}\t{row['status']}\t{row['init_ms']:.3f}\t{row['frames']}\t{row['ms']:.3f}\t{row['fps']:.2f}")
    path.write_text("\n".join(lines) + "\n")


def run_host(args, out, names):
    print("host: rendering", flush=True)
    target = out / "host"
    shutil.rmtree(target, ignore_errors=True)
    target.mkdir(parents=True)
    log, rows, exits = [], {}, {}
    for name in names:
        command = [str(HOST_EXE), "--only", name, "--out", str(target), "--data", str(DATA) + "/",
                   *runner_arguments(args)]
        try:
            done = subprocess.run(command, capture_output=True, text=True, timeout=SAMPLE_TIMEOUT)
            text, exits[name] = done.stdout + done.stderr, done.returncode
        except subprocess.TimeoutExpired as expired:
            partial = expired.stdout or ""
            text = partial.decode(errors="replace") if isinstance(partial, bytes) else partial
            exits[name] = "timeout"
        log.append(f"== {name} (exit {exits[name]})\n{text}")
        found = result_lines(text)
        rows.update(found)
        row = found.get(name)
        print(f"host: {name}: {row['status'] + ', ' + format(row['fps'], '.0f') + ' fps' if row else 'no result'}",
              flush=True)
    (out / "host.log").write_text("\n".join(log))
    write_metrics(target / "metrics.tsv", names, rows, exits)


def data_archive(out):
    """The examples' data directory as one file: the guest agent moves files, not trees."""
    archive = out / "vksamples-data.zip"
    if archive.exists() and archive.stat().st_mtime >= DATA.stat().st_mtime:
        return archive
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as bundle:
        for path in sorted(DATA.rglob("*")):
            if path.is_file():
                bundle.write(path, path.relative_to(DATA).as_posix())
    return archive


def run_guest(args, out, names):
    """The guest's run - each sample its own process, from one batch file - and its results brought back."""
    work = args.work
    guest_shell(work, "ps", f"Remove-Item -Recurse -Force {GUEST_DIR} -ErrorAction SilentlyContinue; "
                            f"New-Item -ItemType Directory -Force {GUEST_DIR}\\out, {GUEST_DIR}\\data | Out-Null")
    guest_shell(work, "push", str(GUEST_BUILD), GUEST_EXE)
    # The pinned loader beside the program: the application directory is searched first.
    guest_shell(work, "push", str(LOADER), GUEST_DIR + "\\vulkan-1.dll")
    print("guest: pushing the examples' data", flush=True)
    guest_shell(work, "push", str(data_archive(out)), GUEST_DIR + "\\data.zip", timeout=3600)
    guest_shell(work, "ps", f"Expand-Archive -Force {GUEST_DIR}\\data.zip {GUEST_DIR}\\data; "
                            f"Remove-Item {GUEST_DIR}\\data.zip", timeout=900)
    arguments = " ".join(runner_arguments(args))
    batch = ["@echo off"]
    for name in names:
        batch.append(f"{GUEST_EXE} --only {name} --out {GUEST_DIR}\\out --data {GUEST_DIR}\\data\\ {arguments} "
                     f">> {GUEST_DIR}\\run.txt 2>&1")
        batch.append(f"echo == {name} (exit %errorlevel%) >> {GUEST_DIR}\\run.txt")
    guest_shell(work, "ps", f"Set-Content -Encoding ascii {GUEST_DIR}\\run.cmd @'\n" + "\r\n".join(batch) + "\n'@")
    print("guest: rendering", flush=True)
    # The user's interactive session, where the loader finds the display adapter's ICD.
    output = guest_shell(work, "ps", f"""
Unregister-ScheduledTask -TaskName avvksamples -Confirm:$false -ErrorAction SilentlyContinue
$a = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument '/c {GUEST_DIR}\\run.cmd'
$p = New-ScheduledTaskPrincipal -UserId '{args.guest_user}' -LogonType Interactive -RunLevel Highest
$s = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 2)
Register-ScheduledTask -TaskName avvksamples -Action $a -Principal $p -Settings $s | Out-Null
Start-ScheduledTask -TaskName avvksamples
Start-Sleep 2
while ((Get-ScheduledTask avvksamples).State -eq 'Running') {{ Start-Sleep 1 }}
'exit: ' + (Get-ScheduledTaskInfo avvksamples).LastTaskResult
Get-Content {GUEST_DIR}\\run.txt -ErrorAction SilentlyContinue""", timeout=7200)
    print(output, flush=True)
    (out / "guest.log").write_text(output)
    bring_back(work, out)
    rows = result_lines(output)
    exits = {}
    for line in output.splitlines():
        if line.startswith("== ") and "(exit " in line:
            name, _, code = line[3:].partition(" (exit ")
            exits[name] = code.rstrip(")")
    write_metrics(out / "guest" / "metrics.tsv", names, rows, exits)


def bring_back(work, out):
    """The guest's frames in one zip, pulled and unpacked into out/guest."""
    bundle = f"{GUEST_DIR}\\frames.zip"
    guest_shell(work, "ps", f"Remove-Item {bundle} -ErrorAction SilentlyContinue; "
                            f"Compress-Archive -Path {GUEST_DIR}\\out\\* -DestinationPath {bundle} -CompressionLevel Fastest",
                timeout=900)
    local = out / "guest-frames.zip"
    pulled = ""
    for _ in range(5):
        pulled = guest_shell(work, "pull", bundle, str(local))
        if local.exists() and local.stat().st_size != 0:
            break
    target = out / "guest"
    shutil.rmtree(target, ignore_errors=True)
    target.mkdir(parents=True)
    if not local.exists() or local.stat().st_size == 0:
        print(f"could not bring the guest's frames back: {pulled.strip()}", flush=True)
        return
    with zipfile.ZipFile(local) as frames:
        frames.extractall(target)
    local.unlink()


def metrics(path):
    return result_lines(path.read_text()) if path.exists() else {}


def report(args, out, names):
    host = metrics(out / "host" / "metrics.tsv")
    guest = metrics(out / "guest" / "metrics.tsv")
    images = out / "img"
    shutil.rmtree(images, ignore_errors=True)
    images.mkdir()
    rows, ratios = [], []
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
    write_summary(out, "Vulkan", rows)


def cell(value, unit="", decimals=1):
    return "&ndash;" if value is None else f"{value:,.{decimals}f}{unit}"


def status_cell(row):
    return html.escape(row["status"]) if row else "&ndash;"


def write_html(args, out, rows, ratios):
    geomean = math.exp(sum(math.log(r) for r in ratios) / len(ratios)) if ratios else None
    matched = sum(1 for r in rows if r["stats"] and verdict(r["stats"])[1] == "good")
    host_ok = sum(1 for r in rows if r["host"] and r["host"]["status"] == "ok")
    guest_ok = sum(1 for r in rows if r["guest"] and r["guest"]["status"] == "ok")
    parts = [f"""<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Vulkan samples: host vs VM</title>
<style>
{STYLE}</style>
<h1>Vulkan samples: host vs VM</h1>
<div class="muted">{html.escape(datetime.datetime.now().strftime("%Y-%m-%d %H:%M"))} &middot; {args.width}&times;{args.height}
&middot; warm-up &ge;1 s ({args.warmup} frames at least), then blocks of {args.frames} frames each finished with vkQueueWaitIdle until 1 s, off-screen
&middot; host: native Vulkan on {html.escape(args.gpu_name)}
&middot; VM: vulkan-1.dll &rarr; avvk.dll &rarr; virtio-gpu-av &rarr; the same GPU &middot; Sascha Willems' examples (Voultapher/Vulkan-Samples), one process per sample</div>
<div class="summary">
<div class="stat"><span class="muted">samples</span><b>{len(rows)}</b></div>
<div class="stat"><span class="muted">ran on the host</span><b>{host_ok} / {len(rows)}</b></div>
<div class="stat"><span class="muted">ran in the VM</span><b>{guest_ok} / {len(rows)}</b></div>
<div class="stat"><span class="muted">pictures matching</span><b>{matched} / {len(rows)}</b></div>
<div class="stat"><span class="muted">VM speed vs host (geomean)</span><b>{cell(geomean * 100 if geomean else None, "%", 1)}</b></div>
</div>"""]
    for r in rows:
        name, h, g, stats = r["name"], r["host"], r["guest"], r["stats"]
        label, tone = verdict(stats) if stats else ("missing", "bad")
        ratio = g["fps"] / h["fps"] if h and g and h["fps"] > 0 else None
        parts.append(f"""<section class="sample"><h2>{html.escape(name)} <span class="tag {tone}">{label}</span>
<span class="muted" style="font-size:13px">VM at {cell(ratio * 100 if ratio else None, "%", 1)} of host speed</span></h2>
{f'<p class="muted">{html.escape(NOTES[name])}</p>' if name in NOTES else ""}
<div class="images">
<figure><img src="img/{name}-host.png" alt="host"><figcaption>host (native Vulkan)</figcaption></figure>
<figure><img src="img/{name}-vm.png" alt="vm"><figcaption>VM (avvk over virtio-gpu-av)</figcaption></figure>
<figure><img src="img/{name}-diff.png" alt="difference"><figcaption>difference, largest channel &times;8</figcaption></figure>
</div><div class="wrap"><table>
<tr><th></th><th>fps</th><th>ms / frame</th><th>setup ms</th><th>status</th></tr>
<tr><td>host</td><td>{cell(h and h["fps"], "", 0)}</td><td>{cell(h and h["ms"] / max(h["frames"], 1), "", 3)}</td><td>{cell(h and h["init_ms"], "", 1)}</td><td>{status_cell(h)}</td></tr>
<tr><td>VM</td><td>{cell(g and g["fps"], "", 0)}</td><td>{cell(g and g["ms"] / max(g["frames"], 1), "", 3)}</td><td>{cell(g and g["init_ms"], "", 1)}</td><td>{status_cell(g)}</td></tr>
</table>""")
        if stats:
            psnr = "&infin;" if stats["psnr"] == float("inf") else f'{stats["psnr"]:.1f} dB'
            parts.append(f"""<table><tr><th>pixels differing (&gt;2)</th><th>largest difference</th><th>mean difference</th><th>PSNR</th></tr>
<tr><td>{stats["changed"]:.3f}%</td><td>{stats["max"]}</td><td>{stats["mean"]:.4f}</td><td>{psnr}</td></tr></table>""")
        parts.append("</div></section>")
    (out / "index.html").write_text("\n".join(parts))
    print(f"report: {out / 'index.html'}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest-render", help="the running guest's work directory")
    parser.add_argument("-o", "--out", type=pathlib.Path, default=ROOT / "build-vulkan-samples" / "report")
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--warmup", type=int, default=60)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--only", help="comma-separated sample names; every sample if absent")
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--no-guest", action="store_true", help="the host half alone; guest results already in the output are kept")
    parser.add_argument("--gpu-name", default="the host GPU")
    parser.add_argument("--guest-user", default="win11\\winuser", help="the guest's logged-on user")
    args = parser.parse_args()
    if not args.no_build:
        build(args)
    args.out.mkdir(parents=True, exist_ok=True)
    names = sample_names(args)
    if not args.no_guest:
        run_guest(args, args.out, names)
    run_host(args, args.out, names)
    report(args, args.out, names)


if __name__ == "__main__":
    main()
