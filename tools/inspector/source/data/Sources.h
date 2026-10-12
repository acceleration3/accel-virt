#ifndef DATA_SOURCES_H
#define DATA_SOURCES_H

// The accel-virt binaries running on this host, as their trace tables and state sockets name them
// in /dev/shm (docs/trace.md): `av-trace-<module>-<pid>` and `av-state-<module>-<pid>.sock`. A
// process's files outlive it - a crashed QEMU's table is still readable - so each is marked alive
// or not by asking the kernel whether the pid is still there.

#include "containers/DynamicArray.h"
#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Types.h"

namespace inspector {

struct Source {
    String module;      // "device", "executor"
    u32 pid = 0;
    String trace_path;  // empty when the process left no table
    String state_path;  // empty when it serves no dump
    bool alive = false;

    String Key() const; // "device/1240628"
};

// Every source under `directory`, sorted by pid then module.
DynamicArray<Source> ScanSources(StringView directory = "/dev/shm");

bool ProcessAlive(u32 pid);

} // namespace inspector

#endif
