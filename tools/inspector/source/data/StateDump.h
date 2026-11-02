#ifndef DATA_STATE_DUMP_H
#define DATA_STATE_DUMP_H

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
    String kind;
    String index;
    usize parent = StringView::Npos;
    DynamicArray<usize> children;
    DynamicArray<StateField> fields;

    const StateField* Field(StringView key) const noexcept;
    StringView Value(StringView key) const noexcept;
    Optional<i64> Integer(StringView key) const noexcept;
    bool Flag(StringView key) const noexcept;
};

class StateDump {
public:
    static StateDump Parse(StringView text);

    const DynamicArray<StateNode>& GetNodes() const noexcept { return m_nodes; }
    const DynamicArray<usize>& GetRoots() const noexcept { return m_roots; }
    const StateNode* Find(StringView path) const noexcept;
    StringView GetError() const noexcept { return m_error.GetView(); }
    bool IsEmpty() const noexcept { return m_nodes.IsEmpty(); }

    DynamicArray<const StateNode*> OfKind(StringView kind) const;
    const StateNode* Ancestor(const StateNode& node, StringView kind) const noexcept;

private:
    usize NodeFor(StringView path);

    DynamicArray<StateNode> m_nodes;
    DynamicArray<usize> m_roots;
    HashMap<String, usize> m_by_path;
    String m_error;
};

}

#endif
