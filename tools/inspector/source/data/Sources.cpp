#include "data/Sources.h"

#include "text/Format.h"
#include "text/NumberConversion.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>

namespace inspector {
namespace {

// "<module>-<pid>" into its two halves; the module may itself hold dashes.
bool SplitName(StringView rest, String& module, u32& pid) {
    const usize dash = rest.RFind('-');
    if (dash == StringView::Npos || dash == 0) {
        return false;
    }
    const Optional<u32> parsed = ParseInt<u32>(rest.Substr(dash + 1));
    if (!parsed) {
        return false;
    }
    module = String(rest.Substr(0, dash));
    pid = *parsed;
    return true;
}

Source& SourceFor(DynamicArray<Source>& sources, const String& module, u32 pid) {
    for (Source& source : sources) {
        if (source.pid == pid && source.module == module) {
            return source;
        }
    }
    Source& made = sources.EmplaceBack();
    made.module = module;
    made.pid = pid;
    made.alive = ProcessAlive(pid);
    return made;
}

} // namespace

String Source::Key() const {
    return Format("{}/{}", module, pid);
}

bool ProcessAlive(u32 pid) {
    return kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
}

DynamicArray<Source> ScanSources(StringView directory) {
    DynamicArray<Source> sources;
    const String dir_path(directory);
    DIR* dir = opendir(dir_path.CStr());
    if (dir == nullptr) {
        return sources;
    }
    while (const dirent* entry = readdir(dir)) {
        const StringView name(entry->d_name);
        String module;
        u32 pid = 0;
        if (name.StartsWith("av-trace-") && SplitName(name.Substr(9), module, pid)) {
            SourceFor(sources, module, pid).trace_path = Format("{}/{}", directory, name);
        } else if (name.StartsWith("av-state-") && name.EndsWith(".sock") &&
                   SplitName(name.Substr(9, name.GetSize() - 9 - 5), module, pid)) {
            SourceFor(sources, module, pid).state_path = Format("{}/{}", directory, name);
        }
    }
    closedir(dir);
    // Insertion sort: a host runs a handful of these.
    for (usize i = 1; i < sources.GetSize(); ++i) {
        for (usize j = i; j > 0; --j) {
            const Source& a = sources[j - 1];
            const Source& b = sources[j];
            const bool ordered = a.pid < b.pid || (a.pid == b.pid && a.module.GetView().Compare(b.module.GetView()) <= 0);
            if (ordered) {
                break;
            }
            Swap(sources[j - 1], sources[j]);
        }
    }
    return sources;
}

} // namespace inspector
