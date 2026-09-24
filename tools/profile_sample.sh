#!/usr/bin/env bash
# Where one D3D11 sample's frames go, in the guest and on the host, in one step.
#
#   tools/profile_sample.sh <sample> [work] [seconds]
#
# The held guest must be running with --perfetto (run_guest_suite.py --keep
# --perfetto): QEMU then records its scopes. This arms the guest's processes to
# record theirs, starts the sample from C:\rvk\samples (rvkd3d11samples.exe, as
# tools/d3d11_report.py pushes it) for long enough, captures `seconds` of both
# sides stopped at one instant (tools/perfetto_trace.py --guest) and prints
# tools/frame_breakdown.py's account of it. The trace stays in build-perfetto/
# for ui.perfetto.dev.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
sample=${1:?usage: profile_sample.sh <sample> [work] [seconds]}
work=${2:-$root/build-guest-render}
seconds=${3:-0.3}
shell=(python3 -B "$root/tools/guest/guest_shell.py" --work "$work" --agent-timeout 30)
pid=$(pgrep -f "qemu-system-x86_64.*$(basename "$work")" | head -1)
[ -n "$pid" ] || { echo "no QEMU for $work" >&2; exit 1; }
trace="$root/build-perfetto/$sample-$(date +%H%M%S).pftrace"

timeout 60 python3 -B "$root/tools/perfetto_trace.py" "$pid" --guest "$work" --arm 2097152
# As the logged-on user, through the task scheduler: the agent's own session
# has no desktop to make a device on. Frames enough to outlast the capture.
timeout 60 "${shell[@]}" ps "
Unregister-ScheduledTask -TaskName rvkprofile -Confirm:\$false -ErrorAction SilentlyContinue
\$a = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument '/c C:\\rvk\\samples\\rvkd3d11samples.exe --only $sample --frames 400000 --out C:\\rvk\\samples\\out --shaders C:\\rvk\\samples\\shaders > C:\\rvk\\samples\\profile.txt 2>&1'
\$p = New-ScheduledTaskPrincipal -UserId 'win11\\winuser' -LogonType Interactive -RunLevel Highest
Register-ScheduledTask -TaskName rvkprofile -Action \$a -Principal \$p | Out-Null
Start-ScheduledTask -TaskName rvkprofile" >/dev/null
# Past the sample's start and its warm-up, into the timed frames.
deadline=$((SECONDS + 60))
until timeout 20 "${shell[@]}" ps "Get-Process rvkd3d11samples -ErrorAction SilentlyContinue | ForEach-Object { [int]\$_.CPU }" 2>/dev/null | grep -qE '^[3-9]|^[1-9][0-9]'; do
    [ $SECONDS -lt $deadline ] || { echo "the sample did not get going" >&2; exit 1; }
    sleep 1
done
timeout 600 python3 -B "$root/tools/perfetto_trace.py" "$pid" --guest "$work" -o "$trace" --seconds "$seconds"
timeout 60 "${shell[@]}" ps "Stop-Process -Name rvkd3d11samples -ErrorAction SilentlyContinue" >/dev/null || true
timeout 900 python3 -B "$root/tools/frame_breakdown.py" "$trace"
