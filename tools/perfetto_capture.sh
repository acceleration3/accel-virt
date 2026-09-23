#!/usr/bin/env bash
# One Perfetto trace of a running guest: the host kernel's scheduling of every
# QEMU thread (tracebox, which needs root for ftrace) and the device's and
# executor's scopes (RVK_PERFETTO rings), merged on one timeline.
#
#   tools/perfetto_capture.sh [seconds] [output.pftrace]
#
# QEMU must have been started with RVK_PERFETTO=1 (run_guest_suite.py --perfetto).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
seconds=${1:-5}
output=${2:-$root/build-perfetto/rvk-$(date +%H%M%S).pftrace}
tracebox=$root/build-perfetto/tracebox
if [ ! -x "$tracebox" ]; then
    mkdir -p "$root/build-perfetto"
    curl -sL -o "$tracebox" https://get.perfetto.dev/tracebox
    chmod +x "$tracebox"
fi
pid=$(pgrep -f 'qemu-system-x86_64.*virtio-gpu-rvk' | head -1)
[ -n "$pid" ] || { echo "no QEMU with virtio-gpu-rvk running" >&2; exit 1; }
system=$(mktemp --suffix=.pftrace)
trap 'rm -f "$system"' EXIT
sudo "$tracebox" -o "$system" --time "${seconds}s" \
    sched/sched_switch sched/sched_waking sched/sched_wakeup irq/irq_handler_entry irq/irq_handler_exit
sudo chown "$(id -u):$(id -g)" "$system"
python3 -B "$root/tools/perfetto_trace.py" "$pid" --merge "$system" -o "$output" --seconds "$seconds"
echo "open $output at https://ui.perfetto.dev"
