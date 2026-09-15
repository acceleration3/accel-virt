#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void corrupt_live_count(ObjectPool<T> &pool) { pool.live_ += 1u; }

    template <typename T>
    static void empty_the_first_chunk(ObjectPool<T> &pool)
    {
        pool.live_ -= static_cast<std::uint32_t>(__builtin_popcountll(pool.chunks_->used_));
        pool.chunks_->used_ = 0u;
    }

    template <typename T>
    static std::uint32_t chunks(const ObjectPool<T> &pool)
    {
        std::uint32_t count = 0u;
        for (const typename ObjectPool<T>::Chunk *chunk = pool.chunks_; chunk != nullptr; chunk = chunk->next_) count++;
        return count;
    }
};

}  // namespace av

using av::Irql;
using av::MockPlatform;
using av::ObjectPool;
using av::ObjectPoolProbe;
using av::Status;
using av::Violation;

namespace {

class Tracked {
public:
    Tracked(int &destroyed, std::uint32_t value) : destroyed_(destroyed), value_(value) {}
    ~Tracked() { destroyed_++; }
    Tracked(const Tracked &) = delete;
    Tracked &operator=(const Tracked &) = delete;
    std::uint32_t value() const { return value_; }

private:
    int          &destroyed_;
    std::uint32_t value_;
};

}  // namespace

TEST(object_pool_create, constructs_with_the_arguments_at_a_stable_address)
{ MockPlatform platform; int destroyed = 0;
  { ObjectPool<Tracked> pool(platform);
    Tracked *object = nullptr;
    CHECK_STATUS(pool.create(object, destroyed, 7u), Status::ok);
    CHECK(pool.invariants_hold());
    CHECK(object != nullptr);
    CHECK_EQ(object->value(), 7u);
    CHECK(pool.find(object) == object);
    CHECK_EQ(pool.live(), 1u); }
  CHECK_EQ(destroyed, 1);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(object_pool_create, fills_a_chunk_then_takes_another)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *objects[ObjectPool<Tracked>::chunk_slots + 1u] = {};
  for (std::uint32_t i = 0u; i <= ObjectPool<Tracked>::chunk_slots; i++) {
      CHECK_STATUS(pool.create(objects[i], destroyed, i), Status::ok);
      CHECK(pool.invariants_hold());
  }
  CHECK_EQ(ObjectPoolProbe::chunks(pool), 2u);
  CHECK_EQ(pool.live(), ObjectPool<Tracked>::chunk_slots + 1u);
  for (std::uint32_t i = 0u; i <= ObjectPool<Tracked>::chunk_slots; i++) CHECK_EQ(objects[i]->value(), i); }

TEST(object_pool_create, propagates_allocation_failure_and_changes_nothing)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  platform.fail_allocation_at(1u);
  Tracked *object = nullptr;
  CHECK_STATUS(pool.create(object, destroyed, 1u), Status::no_memory);
  CHECK(object == nullptr);
  CHECK_EQ(pool.live(), 0u);
  CHECK(pool.invariants_hold()); }

TEST(object_pool_create, is_legal_up_to_apc)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *object = nullptr;
  platform.set_irql(Irql::dispatch);
  static_cast<void>(pool.create(object, destroyed, 1u));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::allocation_at_dispatch)) {}
  pool.destroy_all(); }

TEST(object_pool_destroy, runs_the_destructor_and_reuses_the_slot)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *first = nullptr, *second = nullptr, *third = nullptr;
  CHECK_STATUS(pool.create(first, destroyed, 1u), Status::ok);
  CHECK_STATUS(pool.create(second, destroyed, 2u), Status::ok);
  CHECK_STATUS(pool.destroy(first), Status::ok);
  CHECK(pool.invariants_hold());
  CHECK_EQ(destroyed, 1);
  CHECK(pool.find(first) == nullptr);
  CHECK_STATUS(pool.create(third, destroyed, 3u), Status::ok);
  CHECK(third == first);
  CHECK(pool.invariants_hold()); }

TEST(object_pool_destroy, releases_a_chunk_that_empties)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *objects[ObjectPool<Tracked>::chunk_slots + 1u] = {};
  for (std::uint32_t i = 0u; i <= ObjectPool<Tracked>::chunk_slots; i++)
      CHECK_STATUS(pool.create(objects[i], destroyed, i), Status::ok);
  CHECK_STATUS(pool.destroy(objects[ObjectPool<Tracked>::chunk_slots]), Status::ok);
  CHECK_EQ(ObjectPoolProbe::chunks(pool), 1u);
  CHECK_EQ(platform.live_allocations(), 1u);
  CHECK_STATUS(pool.destroy(objects[0]), Status::ok);
  CHECK_EQ(ObjectPoolProbe::chunks(pool), 1u);
  CHECK(pool.invariants_hold()); }

TEST(object_pool_destroy, refuses_what_is_not_a_live_object_of_this_pool)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform), other(platform);
  Tracked *object = nullptr, *foreign = nullptr;
  CHECK_STATUS(pool.create(object, destroyed, 1u), Status::ok);
  CHECK_STATUS(other.create(foreign, destroyed, 2u), Status::ok);
  Tracked local(destroyed, 3u);
  CHECK_STATUS(pool.destroy(nullptr), Status::invalid_parameter);
  CHECK_STATUS(pool.destroy(foreign), Status::invalid_parameter);
  CHECK_STATUS(pool.destroy(&local), Status::invalid_parameter);
  CHECK_STATUS(pool.destroy(object), Status::ok);
  CHECK_STATUS(pool.destroy(object), Status::invalid_parameter);
  CHECK_EQ(destroyed, 1);
  CHECK(pool.invariants_hold()); }

TEST(object_pool_destroy, releases_an_emptied_chunk_behind_another)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *objects[ObjectPool<Tracked>::chunk_slots + 1u] = {};
  for (std::uint32_t i = 0u; i <= ObjectPool<Tracked>::chunk_slots; i++)
      CHECK_STATUS(pool.create(objects[i], destroyed, i), Status::ok);
  for (std::uint32_t i = 0u; i < ObjectPool<Tracked>::chunk_slots; i++) {
      CHECK_STATUS(pool.destroy(objects[i]), Status::ok);
      CHECK(pool.invariants_hold());
  }
  CHECK_EQ(ObjectPoolProbe::chunks(pool), 1u);
  CHECK(pool.find(objects[ObjectPool<Tracked>::chunk_slots]) == objects[ObjectPool<Tracked>::chunk_slots]); }

TEST(object_pool_find, rejects_an_address_inside_an_object_or_between_slots)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *first = nullptr, *second = nullptr;
  CHECK_STATUS(pool.create(first, destroyed, 1u), Status::ok);
  CHECK_STATUS(pool.create(second, destroyed, 2u), Status::ok);
  const auto *bytes = reinterpret_cast<unsigned char *>(first);
  CHECK(pool.find(const_cast<unsigned char *>(bytes) + 1) == nullptr);
  CHECK(pool.find(reinterpret_cast<Tracked *>(reinterpret_cast<unsigned char *>(second) +
                                              sizeof(Tracked) * (ObjectPool<Tracked>::chunk_slots - 1u))) == nullptr);
  CHECK(pool.find(reinterpret_cast<unsigned char *>(first) - sizeof(Tracked)) == nullptr);
  CHECK(pool.find(second) == second); }

TEST(object_pool_find, is_legal_at_dispatch)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *object = nullptr;
  CHECK_STATUS(pool.create(object, destroyed, 1u), Status::ok);
  platform.set_irql(Irql::dispatch);
  CHECK(pool.find(object) == object);
  platform.set_irql(Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(object_pool_destroy_if, destroys_exactly_the_matching_objects)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *objects[100] = {};
  for (std::uint32_t i = 0u; i < 100u; i++) CHECK_STATUS(pool.create(objects[i], destroyed, i), Status::ok);
  pool.destroy_if([](const Tracked &tracked) { return tracked.value() % 2u == 0u; });
  CHECK(pool.invariants_hold());
  CHECK_EQ(destroyed, 50);
  CHECK_EQ(pool.live(), 50u);
  CHECK(pool.find(objects[1]) == objects[1]);
  CHECK(pool.find(objects[2]) == nullptr);
  pool.destroy_if([](const Tracked &) { return true; });
  CHECK_EQ(pool.live(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(object_pool_all_of, checks_every_live_object_and_nothing_else)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  CHECK(pool.all_of([](const Tracked &) { return false; }));
  Tracked *objects[70] = {};
  for (std::uint32_t i = 0u; i < 70u; i++) CHECK_STATUS(pool.create(objects[i], destroyed, i), Status::ok);
  CHECK(pool.all_of([](const Tracked &tracked) { return tracked.value() < 70u; }));
  CHECK(!pool.all_of([](const Tracked &tracked) { return tracked.value() != 69u; }));
  CHECK_STATUS(pool.destroy(objects[3]), Status::ok);
  CHECK(pool.all_of([](const Tracked &tracked) { return tracked.value() != 3u; })); }

TEST(object_pool_destroy_all, empties_the_pool_and_releases_every_chunk)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *object = nullptr;
  for (std::uint32_t i = 0u; i < 200u; i++) CHECK_STATUS(pool.create(object, destroyed, i), Status::ok);
  pool.destroy_all();
  CHECK(pool.invariants_hold());
  CHECK_EQ(destroyed, 200);
  CHECK_EQ(pool.live(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(object_pool_invariants, a_live_count_that_disagrees_with_the_slots_is_detected)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *object = nullptr;
  CHECK_STATUS(pool.create(object, destroyed, 1u), Status::ok);
  ObjectPoolProbe::corrupt_live_count(pool);
  CHECK(!pool.invariants_hold()); }

TEST(object_pool_invariants, a_retained_empty_chunk_is_detected)
{ MockPlatform platform; int destroyed = 0;
  ObjectPool<Tracked> pool(platform);
  Tracked *object = nullptr;
  for (std::uint32_t i = 0u; i <= ObjectPool<Tracked>::chunk_slots; i++)
      CHECK_STATUS(pool.create(object, destroyed, i), Status::ok);
  ObjectPoolProbe::empty_the_first_chunk(pool);
  CHECK(!pool.invariants_hold());
  pool.destroy_all();
  CHECK_STATUS(pool.create(object, destroyed, 2u), Status::ok);
  CHECK(pool.invariants_hold()); }

/* Feature 045: a pool takes its chunks from any block source, not only a
 * kernel platform - the Vulkan ICD keeps its dispatchable objects in one over
 * the process heap. Every chunk it asks for it gives back, and a source that
 * refuses a chunk is a pool that refuses the object. */
namespace {

class CountingSource final : public av::BlockSource {
public:
    void *allocate(std::size_t bytes, av::Pool) override
    {
        if (refuse_) return nullptr;
        allocated_++;
        return std::malloc(bytes);
    }
    void release(void *block) override
    {
        released_++;
        std::free(block);
    }
    void require_irql_at_most(Irql, const char *) override { asked_++; }

    void          refuse() { refuse_ = true; }
    std::uint32_t allocated() const { return allocated_; }
    std::uint32_t released() const { return released_; }
    std::uint32_t asked() const { return asked_; }

private:
    std::uint32_t allocated_ = 0u;
    std::uint32_t released_ = 0u;
    std::uint32_t asked_ = 0u;
    bool          refuse_ = false;
};

}  // namespace

TEST(object_pool_create, takes_its_chunks_from_any_block_source)
{ CountingSource source; int destroyed = 0;
  { ObjectPool<Tracked> pool(source);
    Tracked *objects[ObjectPool<Tracked>::chunk_slots + 1u] = {};
    for (Tracked *&object : objects) CHECK_STATUS(pool.create(object, destroyed, 7u), Status::ok);
    CHECK_EQ(source.allocated(), 2u);
    CHECK(source.asked() > 0u);
    CHECK(pool.find(objects[0]) == objects[0]);
    CHECK_STATUS(pool.destroy(objects[ObjectPool<Tracked>::chunk_slots]), Status::ok);
    CHECK_EQ(source.released(), 1u); }
  CHECK_EQ(source.released(), 2u);
  CHECK_EQ(destroyed, static_cast<int>(ObjectPool<Tracked>::chunk_slots) + 1);
  CountingSource empty;
  empty.refuse();
  ObjectPool<Tracked> refused(empty);
  Tracked *object = nullptr;
  CHECK_STATUS(refused.create(object, destroyed, 1u), Status::no_memory);
  CHECK(object == nullptr); }
