#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
seconds=${1:-5}
output=${2:-$root/build-perfetto/av-$(date +%H%M%S).pftrace}
tracebox=$root/build-perfetto/tracebox
if [ ! -x "$tracebox" ]; then
    mkdir -p "$root/build-perfetto"
    curl -sL -o "$tracebox" https://get.perfetto.dev/tracebox
    chmod +x "$tracebox"
fi
pid=$(pgrep -f 'qemu-system-x86_64.*virtio-gpu-av' | head -1)
[ -n "$pid" ] || { echo "no QEMU with virtio-gpu-av running" >&2; exit 1; }
system=$(mktemp --suffix=.pftrace)
trap 'rm -f "$system"' EXIT
sudo "$tracebox" -o "$system" --time "${seconds}s" \
    sched/sched_switch sched/sched_waking sched/sched_wakeup irq/irq_handler_entry irq/irq_handler_exit
sudo chown "$(id -u):$(id -g)" "$system"
python3 -B "$root/tools/perfetto_trace.py" "$pid" --merge "$system" -o "$output" --seconds "$seconds"
echo "open $output at https://ui.perfetto.dev"
