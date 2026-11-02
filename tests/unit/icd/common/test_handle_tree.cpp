#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/handle_tree.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;

namespace {
class Graveyard {
public:
    static constexpr std::uint32_t capacity = 256u;

    void bury(const void *node)
    {
        if (count_ < capacity) at_[count_++] = node;
    }
    bool holds(const void *node) const
    {
        for (std::uint32_t index = 0u; index < count_; index++)
            if (at_[index] == node) return true;
        return false;
    }
    void walked_into_one() { walked_ = true; }
    bool walked() const { return walked_; }
    std::uint32_t count() const { return count_; }

private:
    const void   *at_[capacity] = {};
    std::uint32_t count_ = 0u;
    bool          walked_ = false;
};

class Node {
public:
    enum class Kind : std::uint8_t { root, middle, leaf };

    Node(Graveyard &graveyard, Kind kind, Node *parent) : graveyard_(graveyard), kind_(kind), parent_(parent) {}
    ~Node() { graveyard_.bury(this); }
    Node(const Node &) = delete;
    Node &operator=(const Node &) = delete;

    Kind  kind() const { return kind_; }
    Node *parent() const { return parent_; }
    void  adopt(Node *parent) { parent_ = parent; }

    bool descends_from(const Node &other) const
    {
        for (const Node *above = parent_; above != nullptr; above = above->parent_) {
            if (graveyard_.holds(above)) {
                graveyard_.walked_into_one();
                return false;
            }
            if (above == &other) return true;
        }
        return false;
    }

private:
    Graveyard &graveyard_;
    Kind       kind_;
    Node      *parent_;
};

using Tree = av::icd::HandleTree<Node>;

class Forest {
public:
    Forest() : platform_(), graveyard_(), tree_(platform_) {}

    Tree         &tree() { return tree_; }
    Graveyard    &graveyard() { return graveyard_; }
    MockPlatform &platform() { return platform_; }

    Node *make(Node::Kind kind, Node *parent = nullptr)
    {
        Node *made = nullptr;
        CHECK_STATUS(tree_.make(made, graveyard_, kind, parent), Status::ok);
        CHECK(made != nullptr);
        return made;
    }

private:
    MockPlatform platform_;
    Graveyard    graveyard_;
    Tree         tree_;
};

}

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void miscount(ObjectPool<T> &pool) { pool.live_ += 1u; }
};

}

namespace av::icd {

struct HandleTreeProbe {
    template <typename N>
    static ObjectPool<N> &pool(HandleTree<N> &tree) { return tree.pool_; }
};

}

TEST(handle_tree, an_empty_tree_holds_and_believes_nothing)
{ Forest forest;
  CHECK_EQ(forest.tree().live(), 0u);
  CHECK(!forest.tree().holds(nullptr));
  CHECK(forest.tree().made(nullptr, Node::Kind::root) == nullptr);
  int elsewhere = 0;
  CHECK(forest.tree().made(&elsewhere, Node::Kind::root) == nullptr);
  CHECK(forest.tree().invariants_hold()); }

TEST(handle_tree, a_node_it_made_is_the_one_it_answers_for_and_only_as_its_kind)
{ Forest      forest;
  Node *const root = forest.make(Node::Kind::root);
  CHECK(forest.tree().holds(root));
  CHECK(forest.tree().made(root, Node::Kind::root) == root);
  CHECK(forest.tree().made(root, Node::Kind::middle) == nullptr);
  CHECK_EQ(forest.tree().live(), 1u);
  std::uint32_t visited = 0u;
  forest.tree().nodes().for_each([&visited](const Node &) { visited++; });
  CHECK_EQ(visited, 1u);
  CHECK(forest.tree().invariants_hold()); }

TEST(handle_tree, a_parent_fits_only_when_held_and_of_a_lower_kind)
{ Forest      forest;
  Node *const root = forest.make(Node::Kind::root);
  Node *const middle = forest.make(Node::Kind::middle, root);
  CHECK(forest.tree().parent_fits(Node::Kind::middle, root));
  CHECK(forest.tree().parent_fits(Node::Kind::leaf, root));
  CHECK(forest.tree().parent_fits(Node::Kind::leaf, middle));
  CHECK(!forest.tree().parent_fits(Node::Kind::middle, middle));
  CHECK(!forest.tree().parent_fits(Node::Kind::root, middle));
  CHECK(!forest.tree().parent_fits(Node::Kind::middle, nullptr));
  Node outside(forest.graveyard(), Node::Kind::root, nullptr);
  CHECK(!forest.tree().parent_fits(Node::Kind::middle, &outside)); }

TEST(handle_tree, a_subtree_goes_deepest_kind_first_and_no_walk_reaches_a_destroyed_node)
{ Forest      forest;
  Node *const root = forest.make(Node::Kind::root);
  Node *const middle = forest.make(Node::Kind::middle, root);
  static_cast<void>(forest.make(Node::Kind::leaf, middle));
  static_cast<void>(forest.make(Node::Kind::leaf, middle));
  CHECK(forest.tree().invariants_hold());
  CHECK_EQ(forest.tree().destroy(*root), 4u);
  CHECK(!forest.graveyard().walked());
  CHECK_EQ(forest.graveyard().count(), 4u);
  CHECK_EQ(forest.tree().live(), 0u);
  CHECK(forest.tree().invariants_hold()); }

TEST(handle_tree, a_destroy_leaves_every_other_branch)
{ Forest      forest;
  Node *const root = forest.make(Node::Kind::root);
  Node *const kept = forest.make(Node::Kind::middle, root);
  Node *const gone = forest.make(Node::Kind::middle, root);
  Node *const kept_leaf = forest.make(Node::Kind::leaf, kept);
  static_cast<void>(forest.make(Node::Kind::leaf, gone));
  Node *const other = forest.make(Node::Kind::root);
  CHECK_EQ(forest.tree().destroy(*gone), 2u);
  CHECK(forest.tree().holds(root));
  CHECK(forest.tree().holds(kept));
  CHECK(forest.tree().holds(kept_leaf));
  CHECK(forest.tree().holds(other));
  CHECK(!forest.tree().holds(gone));
  CHECK(forest.tree().made(gone, Node::Kind::middle) == nullptr);
  CHECK_EQ(forest.tree().live(), 4u);
  CHECK(!forest.graveyard().walked());
  CHECK(forest.tree().invariants_hold()); }

TEST(handle_tree, destroying_what_it_does_not_hold_destroys_nothing)
{ Forest forest;
  static_cast<void>(forest.make(Node::Kind::root));
  Node outside(forest.graveyard(), Node::Kind::root, nullptr);
  CHECK_EQ(forest.tree().destroy(outside), 0u);
  CHECK_EQ(forest.tree().live(), 1u); }

TEST(handle_tree, a_pool_out_of_memory_makes_nothing)
{ Forest forest;
  forest.platform().fail_allocation_at(1u);
  Node *made = reinterpret_cast<Node *>(std::uintptr_t{0x10});
  CHECK_STATUS(forest.tree().make(made, forest.graveyard(), Node::Kind::root, nullptr), Status::no_memory);
  CHECK(made == nullptr);
  CHECK_EQ(forest.tree().live(), 0u);
  CHECK(forest.tree().invariants_hold()); }

TEST(handle_tree, the_invariant_catches_a_lost_parent_and_an_unsound_pool)
{ Forest      forest;
  Node *const root = forest.make(Node::Kind::root);
  Node *const middle = forest.make(Node::Kind::middle, root);
  Node        outside(forest.graveyard(), Node::Kind::root, nullptr);
  middle->adopt(&outside);
  CHECK(!forest.tree().invariants_hold());
  middle->adopt(root);
  CHECK(forest.tree().invariants_hold());
  av::ObjectPoolProbe::miscount(av::icd::HandleTreeProbe::pool(forest.tree()));
  CHECK(!forest.tree().invariants_hold()); }
