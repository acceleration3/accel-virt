/* The objects an ICD hands the application as pointers, each made through a
 * parent of a lower kind, and the only place a pointer from outside is
 * believed: Vulkan's dispatchable handles (HandleRegistry) and OpenCL's
 * objects (ClObjectTable).
 *
 * An application passes back whatever it likes - an object it destroyed, one of
 * another kind, a stack address, a number it made up. None of those may be
 * read through to find out, so a pointer is judged by the pool's own record of
 * what it made before anything behind it is looked at.
 *
 * A node is destroyed with everything made through it, at any depth, deepest
 * kind first. A node's parents are all of lower kinds, so every parent chain a
 * pass walks is made of nodes no pass has destroyed yet; destroying the subtree
 * in one pass would destroy a queue before the event made on it is judged, and
 * walk the event's parent into storage the pool may already have freed.
 *
 * `Node` has a `Kind` enumeration ordered from the roots down, `kind()`,
 * `parent()`, and `descends_from(other)`, true when `other` is above it. The
 * tree takes no lock: its owner serializes it. */
#ifndef AV_ICD_HANDLE_TREE_HPP
#define AV_ICD_HANDLE_TREE_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::icd {

struct HandleTreeProbe;

template <typename Node>
class HandleTree {
public:
    using Kind = typename Node::Kind;

    explicit HandleTree(BlockSource &memory);
    HandleTree(const HandleTree &) = delete;
    HandleTree &operator=(const HandleTree &) = delete;

    /* One more node, constructed from `arguments`; no_memory when the pool
     * refuses. Whether its parent fits is the caller's to ask first. */
    template <typename... Arguments>
    [[nodiscard]] Status make(Node *&made, Arguments &&...arguments);
    /* Whether a node of `kind` may be made through `parent`: one this tree
     * holds, of a lower kind. */
    bool                 parent_fits(Kind kind, const Node *parent) const;
    /* Whether `node` is one this tree holds. */
    bool                 holds(const Node *node) const;
    /* The node of `kind` at `pointer`, or null; nothing is read through a
     * pointer the pool did not make. */
    Node                *made(const void *pointer, Kind kind) const;
    /* `node` and every node made through it, at any depth, destroyed deepest
     * kind first; how many went, 0 for a node this tree does not hold. */
    uint32_t             destroy(Node &node);
    uint32_t             live() const;
    /* What it holds, to visit; a visitor that destroys is a mistake. */
    const ObjectPool<Node> &nodes() const;

    /* Every node's parent is one this tree holds, and the pool is sound. */
    bool invariants_hold() const;

private:
    friend struct HandleTreeProbe;

    uint32_t deepest_kind() const;

    ObjectPool<Node> pool_;
};

template <typename Node>
HandleTree<Node>::HandleTree(BlockSource &memory) : pool_(memory)
{
    AV_SCOPE("HandleTree::HandleTree");
}

template <typename Node>
template <typename... Arguments>
Status HandleTree<Node>::make(Node *&made, Arguments &&...arguments)
{
    AV_SCOPE("HandleTree::make");
    return pool_.create(made, static_cast<Arguments &&>(arguments)...);
}

template <typename Node>
bool HandleTree<Node>::parent_fits(Kind kind, const Node *parent) const
{
    AV_SCOPE("HandleTree::parent_fits");
    if (!holds(parent)) return false;
    return static_cast<uint32_t>(parent->kind()) < static_cast<uint32_t>(kind);
}

template <typename Node>
bool HandleTree<Node>::holds(const Node *node) const
{
    AV_SCOPE("HandleTree::holds");
    return node != nullptr && pool_.find(const_cast<Node *>(node)) != nullptr;
}

template <typename Node>
Node *HandleTree<Node>::made(const void *pointer, Kind kind) const
{
    AV_SCOPE("HandleTree::made");
    if (pointer == nullptr) return nullptr;
    Node *const node = pool_.find(const_cast<void *>(pointer));
    return node != nullptr && node->kind() == kind ? node : nullptr;
}

template <typename Node>
uint32_t HandleTree<Node>::deepest_kind() const
{
    AV_SCOPE("HandleTree::deepest_kind");
    uint32_t deepest = 0u;
    pool_.for_each([&deepest](const Node &node) {
        const auto kind = static_cast<uint32_t>(node.kind());
        if (kind > deepest) deepest = kind;
    });
    return deepest;
}

template <typename Node>
uint32_t HandleTree<Node>::destroy(Node &node)
{
    AV_SCOPE("HandleTree::destroy");
    if (!holds(&node)) return 0u;
    const uint32_t before = pool_.live();
    const auto     top = static_cast<uint32_t>(node.kind());
    for (uint32_t kind = deepest_kind(); kind > top; kind--) {
        pool_.destroy_if([&node, kind](const Node &candidate) {
            return static_cast<uint32_t>(candidate.kind()) == kind && candidate.descends_from(node);
        });
    }
    /* holds() said it is there, so the destroy cannot refuse. */
    static_cast<void>(pool_.destroy(&node));
    return before - pool_.live();
}

template <typename Node>
uint32_t HandleTree<Node>::live() const
{
    AV_SCOPE("HandleTree::live");
    return pool_.live();
}

template <typename Node>
const ObjectPool<Node> &HandleTree<Node>::nodes() const
{
    AV_SCOPE("HandleTree::nodes");
    return pool_;
}

template <typename Node>
bool HandleTree<Node>::invariants_hold() const
{
    AV_SCOPE("HandleTree::invariants_hold");
    if (!pool_.invariants_hold()) return false;
    return pool_.all_of([this](const Node &node) { return node.parent() == nullptr || holds(node.parent()); });
}

}  // namespace av::icd

#endif  // AV_ICD_HANDLE_TREE_HPP
