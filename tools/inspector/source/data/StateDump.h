#ifndef DATA_STATE_DUMP_H
#define DATA_STATE_DUMP_H

// One state dump (docs/trace.md, "State dumps"): one object per line, `path key=value ...`, where
// the path is name/index pairs (`module/0/device/0/contexts/0/context/193`). Parsed into a tree:
// each object's parent is its path less its last pair. A parent the dump never wrote a line for is
// made, with no fields, so every object is reachable from a root.
//
// A binary with nothing to give answers one line starting `error `, kept as the dump's error.

#include "containers/DynamicArray.h"
#include "containers/HashMap.h"
#include "containers/String.h"
#include "containers/StringView.h"
#include "core/Optional.h"
#include "core/Types.h"

namespace inspector {

struct StateField {
    String key;
    String value;
};

struct StateNode {
    String path;
    String kind;    // the name of the last pair: "context"
    String index;   // its index: "193"
    usize parent = StringView::Npos;
    DynamicArray<usize> children;
    DynamicArray<StateField> fields;

    const StateField* Field(StringView key) const noexcept;
    StringView Value(StringView key) const noexcept; // empty when absent
    Optional<i64> Integer(StringView key) const noexcept;
    bool Flag(StringView key) const noexcept; // "true"
};

class StateDump {
public:
    static StateDump Parse(StringView text);

    const DynamicArray<StateNode>& GetNodes() const noexcept { return m_nodes; }
    const DynamicArray<usize>& GetRoots() const noexcept { return m_roots; }
    const StateNode* Find(StringView path) const noexcept;
    StringView GetError() const noexcept { return m_error.GetView(); }
    bool IsEmpty() const noexcept { return m_nodes.IsEmpty(); }

    // Every node whose kind is `kind`, in dump order.
    DynamicArray<const StateNode*> OfKind(StringView kind) const;
    // The nearest ancestor of `node` whose kind is `kind`, or null.
    const StateNode* Ancestor(const StateNode& node, StringView kind) const noexcept;

private:
    usize NodeFor(StringView path);

    DynamicArray<StateNode> m_nodes;
    DynamicArray<usize> m_roots;
    HashMap<String, usize> m_by_path;
    String m_error;
};

} // namespace inspector

#endif
