#include "data/ScopeNames.h"

#include "data/TraceTable.h"
#include "text/Format.h"

#include <dirent.h>
#include <string.h>

namespace inspector {
namespace {

bool IsSource(const char* name) {
    const StringView view(name);
    return view.EndsWith(".cpp") || view.EndsWith(".hpp") || view.EndsWith(".h") || view.EndsWith(".c");
}

}

StringView ScopeNames::DefaultRoot() noexcept {
    return AV_SOURCE_DIR;
}

u32 ScopeNames::Hash(StringView name) noexcept {
    u32 id = 2166136261u;
    for (const char c : name) {
        id = (id ^ static_cast<u8>(c)) * 16777619u;
    }
    return id;
}

usize ScopeNames::LoadTree(StringView root) {
    m_names.Clear();
    AddDirectory(String(root));
    return m_names.GetSize();
}

void ScopeNames::AddDirectory(const String& path) {
    DIR* dir = opendir(path.CStr());
    if (dir == nullptr) {
        return;
    }
    while (const dirent* entry = readdir(dir)) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        String child = Format("{}/{}", path, entry->d_name);
        if (entry->d_type == DT_DIR) {
            AddDirectory(child);
        } else if (IsSource(entry->d_name)) {
            String why;
            if (Optional<DynamicArray<u8>> bytes = ReadWholeFile(child, why)) {
                AddSource(StringView(reinterpret_cast<const char*>(bytes->GetBuffer()), bytes->GetSize()));
            }
        }
    }
    closedir(dir);
}

void ScopeNames::AddSource(StringView text) {
    constexpr StringView kMark = "AV_SCOPE(\"";
    for (usize at = text.Find(kMark); at != StringView::Npos; at = text.Find(kMark, at + 1)) {
        const usize start = at + kMark.GetSize();
        const usize end = text.Find('"', start);
        if (end == StringView::Npos) {
            return;
        }
        const StringView name = text.Substr(start, end - start);
        m_names.InsertOrAssign(Hash(name), String(name));
    }
}

String ScopeNames::Find(u32 id) const {
    if (const String* name = m_names.Find(id)) {
        return *name;
    }
    return Format("?{:08x}", id);
}

}
