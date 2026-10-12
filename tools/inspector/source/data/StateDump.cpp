#include "data/StateDump.h"

#include "text/NumberConversion.h"

namespace inspector {
namespace {

// The path less its last name/index pair; empty for a root ("adapter/0").
StringView ParentPath(StringView path) {
    const usize index_slash = path.RFind('/');
    if (index_slash == StringView::Npos) {
        return {};
    }
    const usize name_slash = path.Substr(0, index_slash).RFind('/');
    return name_slash == StringView::Npos ? StringView{} : path.Substr(0, name_slash);
}

} // namespace

const StateField* StateNode::Field(StringView key) const noexcept {
    for (const StateField& field : fields) {
        if (field.key.GetView() == key) {
            return &field;
        }
    }
    return nullptr;
}

StringView StateNode::Value(StringView key) const noexcept {
    const StateField* field = Field(key);
    return field != nullptr ? field->value.GetView() : StringView{};
}

Optional<i64> StateNode::Integer(StringView key) const noexcept {
    const StringView value = Value(key);
    if (value.StartsWith("0x")) {
        return ParseInt<i64>(value, 0);
    }
    return ParseInt<i64>(value);
}

bool StateNode::Flag(StringView key) const noexcept {
    return Value(key) == "true";
}

usize StateDump::NodeFor(StringView path) {
    if (const usize* found = m_by_path.Find(path)) {
        return *found;
    }
    const StringView parent_path = ParentPath(path);
    const usize parent = parent_path.IsEmpty() ? StringView::Npos : NodeFor(parent_path);
    StateNode node;
    node.path = String(path);
    const usize index_slash = path.RFind('/');
    if (index_slash == StringView::Npos) {
        node.kind = String(path);
    } else {
        const usize name_slash = path.Substr(0, index_slash).RFind('/');
        const usize kind_start = name_slash == StringView::Npos ? 0 : name_slash + 1;
        node.kind = String(path.Substr(kind_start, index_slash - kind_start));
        node.index = String(path.Substr(index_slash + 1));
    }
    node.parent = parent;
    const usize at = m_nodes.GetSize();
    m_nodes.PushBack(RValueCast(node));
    m_by_path.Insert(String(path), at);
    if (parent == StringView::Npos) {
        m_roots.PushBack(at);
    } else {
        m_nodes[parent].children.PushBack(at);
    }
    return at;
}

StateDump StateDump::Parse(StringView text) {
    StateDump dump;
    usize line_start = 0;
    while (line_start < text.GetSize()) {
        usize line_end = text.Find('\n', line_start);
        if (line_end == StringView::Npos) {
            line_end = text.GetSize();
        }
        const StringView line = text.Substr(line_start, line_end - line_start).Trim();
        line_start = line_end + 1;
        if (line.IsEmpty()) {
            continue;
        }
        if (line.StartsWith("error ")) {
            dump.m_error = String(line.Substr(6));
            continue;
        }
        const usize space = line.Find(' ');
        const StringView path = space == StringView::Npos ? line : line.Substr(0, space);
        const usize node = dump.NodeFor(path);
        StringView rest = space == StringView::Npos ? StringView{} : line.Substr(space + 1);
        while (!rest.IsEmpty()) {
            const usize next = rest.Find(' ');
            const StringView pair = next == StringView::Npos ? rest : rest.Substr(0, next);
            rest = next == StringView::Npos ? StringView{} : rest.Substr(next + 1);
            const usize equals = pair.Find('=');
            StateField field;
            field.key = String(equals == StringView::Npos ? pair : pair.Substr(0, equals));
            field.value = String(equals == StringView::Npos ? StringView{} : pair.Substr(equals + 1));
            dump.m_nodes[node].fields.PushBack(RValueCast(field));
        }
    }
    return dump;
}

const StateNode* StateDump::Find(StringView path) const noexcept {
    const usize* at = m_by_path.Find(path);
    return at != nullptr ? &m_nodes[*at] : nullptr;
}

DynamicArray<const StateNode*> StateDump::OfKind(StringView kind) const {
    DynamicArray<const StateNode*> found;
    for (const StateNode& node : m_nodes) {
        if (node.kind.GetView() == kind) {
            found.PushBack(&node);
        }
    }
    return found;
}

const StateNode* StateDump::Ancestor(const StateNode& node, StringView kind) const noexcept {
    for (usize at = node.parent; at != StringView::Npos; at = m_nodes[at].parent) {
        if (m_nodes[at].kind.GetView() == kind) {
            return &m_nodes[at];
        }
    }
    return nullptr;
}

} // namespace inspector
