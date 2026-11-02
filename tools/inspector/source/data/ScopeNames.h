#ifndef DATA_SCOPE_NAMES_H
#define DATA_SCOPE_NAMES_H

#include "containers/HashMap.h"
#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Types.h"

namespace inspector {

class ScopeNames {
public:
    static u32 Hash(StringView name) noexcept;
    static StringView DefaultRoot() noexcept;

    usize LoadTree(StringView root);
    void AddSource(StringView text);

    String Find(u32 id) const;
    usize GetSize() const noexcept { return m_names.GetSize(); }

private:
    void AddDirectory(const String& path);

    HashMap<u32, String> m_names;
};

}

#endif
