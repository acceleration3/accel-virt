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
 * Each node is found by its address through a KeyedSlots index, so judging a
 * pointer is one probe however many objects an application holds. The index
 * also links every node to its parent and to its children, in the order they
 * were made, so a node's children are found without a walk of every node, and
 * a node is destroyed with everything made through it, at any depth, each
 * after everything made through it - at a cost of the subtree, not the tree.
 *
 * `Node` has a `Kind` enumeration ordered from the roots down, `kind()` and
 * `parent()`. The tree takes no lock: its owner serializes it. */
#ifndef AV_ICD_HANDLE_TREE_HPP
#define AV_ICD_HANDLE_TREE_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::icd {

struct HandleTreeProbe;

/* Where one node sits in the tree, kept under its address. The links are
 * slots of the same index. A type of its own so the tree's members are
 * complete (tools/header_scan.py reads a class's members only to its first
 * nested type). */
template <typename Node>
struct HandleTreePlace {
    Node    *node_;
    uint32_t parent_;
    uint32_t first_child_;
    uint32_t last_child_;
    uint32_t next_;
    uint32_t previous_;
};

template <typename Node>
class HandleTree {
public:
    using Kind = typename Node::Kind;

    explicit HandleTree(BlockSource &memory);
    HandleTree(const HandleTree &) = delete;
    HandleTree &operator=(const HandleTree &) = delete;

    /* One more node, constructed from `arguments`, the last of its parent's
     * children; no_memory when the pool refuses. Whether its parent fits is the
     * caller's to ask first. */
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
    /* The `index`th child of `kind` made through `parent`, in the order they
     * were made; null past the last, and for a parent this tree does not
     * hold. */
    Node                *child(const Node &parent, Kind kind, uint32_t index) const;
    /* Every child made through `parent` to `visit(child)`, in the order they
     * were made; nothing for a parent this tree does not hold. A visitor that
     * destroys is a mistake. */
    template <typename Visitor>
    void                 for_children(const Node &parent, Visitor visit) const;
    /* `node` and every node made through it, at any depth, to `visit(node)`;
     * nothing for a node this tree does not hold. A visitor that destroys is
     * a mistake. */
    template <typename Visitor>
    void                 for_subtree(const Node &node, Visitor visit) const;
    /* `node` and every node made through it, at any depth, destroyed, each
     * after every node made through it; how many went, 0 for a node this tree
     * does not hold. */
    uint32_t             destroy(Node &node);
    uint32_t             live() const;
    /* What it holds, to visit; a visitor that destroys is a mistake. */
    const ObjectPool<Node> &nodes() const;

    /* Every node is indexed at its address, its parent is one this tree holds
     * and their links agree, and the pool is sound. */
    bool invariants_hold() const;

private:
    friend struct HandleTreeProbe;

    using Place = HandleTreePlace<Node>;
    static constexpr uint32_t no_slot = KeyedSlots<uint64_t, Place>::no_slot;

    uint64_t key_of(const void *pointer) const;
    /* The slot `node` is indexed in, or no_slot. */
    uint32_t slot_of(const Node *node) const;
    /* The place in `slot`, which the caller knows is indexed. */
    Place   &place(uint32_t slot) const;
    /* The node in `slot` taken out of its parent's children. */
    void     unlink(uint32_t slot);
    /* Whether the children linked under `slot` agree with it, each counted
     * into `children`. */
    bool     children_agree(uint32_t slot, uint32_t &children) const;

    ObjectPool<Node>                    pool_;
    /* Every node by its address, with its links. Mutable because a lookup is
     * how a const question reaches a place, and the index answers a place it
     * may change only to an index it may change. */
    mutable KeyedSlots<uint64_t, Place> index_;
};

template <typename Node>
HandleTree<Node>::HandleTree(BlockSource &memory) : pool_(memory), index_(memory, Pool::paged)
{
    AV_SCOPE("HandleTree::HandleTree");
}

template <typename Node>
template <typename... Arguments>
Status HandleTree<Node>::make(Node *&made, Arguments &&...arguments)
{
    AV_SCOPE("HandleTree::make");
    made = nullptr;
    /* Room to index the node first, so a node made is always found. */
    const Status room = index_.reserve(index_.count() + 1u);
    if (room != Status::ok) return room;
    const Status created = pool_.create(made, static_cast<Arguments &&>(arguments)...);
    if (created != Status::ok) return created;
    const uint32_t parent = slot_of(made->parent());
    uint32_t       slot = 0u;
    static_cast<void>(index_.insert(key_of(made), Place{made, parent, no_slot, no_slot, no_slot, no_slot}, slot));
    if (parent == no_slot) return Status::ok;
    Place &above = place(parent);
    place(slot).previous_ = above.last_child_;
    if (above.last_child_ != no_slot)
        place(above.last_child_).next_ = slot;
    else
        above.first_child_ = slot;
    above.last_child_ = slot;
    return Status::ok;
}

template <typename Node>
uint64_t HandleTree<Node>::key_of(const void *pointer) const
{
    AV_SCOPE("HandleTree::key_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer));
}

template <typename Node>
uint32_t HandleTree<Node>::slot_of(const Node *node) const
{
    AV_SCOPE("HandleTree::slot_of");
    return node != nullptr ? index_.slot_of(key_of(node)) : no_slot;
}

template <typename Node>
typename HandleTree<Node>::Place &HandleTree<Node>::place(uint32_t slot) const
{
    AV_SCOPE("HandleTree::place");
    return *index_.at(slot);
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
    return slot_of(node) != no_slot;
}

template <typename Node>
Node *HandleTree<Node>::made(const void *pointer, Kind kind) const
{
    AV_SCOPE("HandleTree::made");
    if (pointer == nullptr) return nullptr;
    const Place *const found = index_.find(key_of(pointer));
    return found != nullptr && found->node_->kind() == kind ? found->node_ : nullptr;
}

template <typename Node>
Node *HandleTree<Node>::child(const Node &parent, Kind kind, uint32_t index) const
{
    AV_SCOPE("HandleTree::child");
    const uint32_t above = slot_of(&parent);
    if (above == no_slot) return nullptr;
    for (uint32_t at = place(above).first_child_; at != no_slot; at = place(at).next_) {
        Node *const node = place(at).node_;
        if (node->kind() == kind && index-- == 0u) return node;
    }
    return nullptr;
}

template <typename Node>
template <typename Visitor>
void HandleTree<Node>::for_children(const Node &parent, Visitor visit) const
{
    AV_SCOPE("HandleTree::for_children");
    const uint32_t above = slot_of(&parent);
    if (above == no_slot) return;
    for (uint32_t at = place(above).first_child_; at != no_slot; at = place(at).next_)
        visit(static_cast<const Node &>(*place(at).node_));
}

template <typename Node>
template <typename Visitor>
void HandleTree<Node>::for_subtree(const Node &node, Visitor visit) const
{
    AV_SCOPE("HandleTree::for_subtree");
    const uint32_t top = slot_of(&node);
    /* Down to a node's first child where it has one; else on to the next
     * child of the nearest node at or above it, short of the top, that has
     * one. */
    for (uint32_t at = top; at != no_slot;) {
        visit(static_cast<const Node &>(*place(at).node_));
        if (place(at).first_child_ != no_slot) {
            at = place(at).first_child_;
            continue;
        }
        while (at != top && place(at).next_ == no_slot) at = place(at).parent_;
        at = at == top ? no_slot : place(at).next_;
    }
}

template <typename Node>
void HandleTree<Node>::unlink(uint32_t slot)
{
    AV_SCOPE("HandleTree::unlink");
    const Place &going = place(slot);
    if (going.parent_ == no_slot) return;
    Place &above = place(going.parent_);
    if (going.previous_ != no_slot)
        place(going.previous_).next_ = going.next_;
    else
        above.first_child_ = going.next_;
    if (going.next_ != no_slot)
        place(going.next_).previous_ = going.previous_;
    else
        above.last_child_ = going.previous_;
}

template <typename Node>
uint32_t HandleTree<Node>::destroy(Node &node)
{
    AV_SCOPE("HandleTree::destroy");
    const uint32_t top = slot_of(&node);
    if (top == no_slot) return 0u;
    uint32_t gone = 0u;
    /* A node goes once it has no children left: down to a leaf, then back up
     * to its parent, whose next child is then its first. */
    for (uint32_t at = top;;) {
        while (place(at).first_child_ != no_slot) at = place(at).first_child_;
        const uint32_t above = place(at).parent_;
        Node *const    going = place(at).node_;
        unlink(at);
        index_.erase_slot(at);
        /* Indexed, so the pool made it and cannot refuse it. */
        static_cast<void>(pool_.destroy(going));
        gone++;
        if (at == top) return gone;
        at = above;
    }
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
bool HandleTree<Node>::children_agree(uint32_t slot, uint32_t &children) const
{
    AV_SCOPE("HandleTree::children_agree");
    const auto parent_kind = static_cast<uint32_t>(place(slot).node_->kind());
    uint32_t   previous = no_slot;
    for (uint32_t at = place(slot).first_child_; at != no_slot; at = place(at).next_) {
        const Place *const below = index_.at(at);
        if (below == nullptr || below->parent_ != slot || below->previous_ != previous) return false;
        if (static_cast<uint32_t>(below->node_->kind()) <= parent_kind) return false;
        /* More children than nodes is a loop in the links. */
        if (++children > index_.count()) return false;
        previous = at;
    }
    return place(slot).last_child_ == previous;
}

template <typename Node>
bool HandleTree<Node>::invariants_hold() const
{
    AV_SCOPE("HandleTree::invariants_hold");
    if (!pool_.invariants_hold() || !index_.invariants_hold() || index_.count() != pool_.live()) return false;
    uint32_t   linked = 0u;
    uint32_t   roots = 0u;
    const bool placed = pool_.all_of([&](const Node &node) {
        const uint32_t slot = slot_of(&node);
        if (slot == no_slot || place(slot).node_ != &node) return false;
        const uint32_t parent = slot_of(node.parent());
        if (place(slot).parent_ != parent || (node.parent() != nullptr && parent == no_slot)) return false;
        if (parent == no_slot) roots++;
        return children_agree(slot, linked);
    });
    /* Every node but a root is linked under its parent, and only once. */
    return placed && linked + roots == pool_.live();
}

}  // namespace av::icd

#endif  // AV_ICD_HANDLE_TREE_HPP
