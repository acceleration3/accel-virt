#ifndef DATA_SCOPE_NAMES_H
#define DATA_SCOPE_NAMES_H

// The names behind a trace table's name ids: every AV_SCOPE("...") literal in the accel-virt source
// tree, hashed as ScopeName hashes it (FNV-1a 32 over the name's bytes, src/core/trace/scope_name.hpp).
// Two names with one id are a build error in accel-virt (scope_marks), so the map is exact for the
// tree the binaries were built from.

#include "containers/HashMap.h"
#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Types.h"

namespace inspector {

class ScopeNames {
public:
    static u32 Hash(StringView name) noexcept;
    // The accel-virt src/ the inspector was built beside.
    static StringView DefaultRoot() noexcept;

    // Every name under `root`, recursively; what was found replaces what was there.
    usize LoadTree(StringView root);
    // The names in `text`, as one source file's contents.
    void AddSource(StringView text);

    // The name of `id`; "?xxxxxxxx" when none is known.
    String Find(u32 id) const;
    usize GetSize() const noexcept { return m_names.GetSize(); }

private:
    void AddDirectory(const String& path);

    HashMap<u32, String> m_names;
};

} // namespace inspector

#endif
