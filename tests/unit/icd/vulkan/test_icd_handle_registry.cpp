#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "core/support/object_pool.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/device_memory.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/handle_registry.hpp"
#include "icd/vulkan/loader_handle.hpp"

#include <cstddef>
#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::icd::Connection;
using av::icd::HandleRegistry;
using av::icd::LoaderHandle;
using Kind = av::icd::LoaderHandle::Kind;

namespace av::icd {

struct LoaderHandleProbe {
    static std::size_t loader_data_offset() { return offsetof(LoaderHandle, loader_data_); }
    static void        overwrite_loader_data(LoaderHandle &handle, void *table) { handle.loader_data_ = table; }
    static void        set_parent(LoaderHandle &handle, LoaderHandle *parent) { handle.parent_ = parent; }
    static void        set_host(LoaderHandle &handle, std::uint64_t host) { handle.host_ = host; }
    static void        set_kind(LoaderHandle &handle, Kind kind) { handle.kind_ = kind; }
    static ObjectPool<CommandBatch> *forget_pool(LoaderHandle &handle)
    {
        ObjectPool<CommandBatch> *const pool = handle.batches_;
        handle.batches_ = nullptr;
        return pool;
    }
    static void restore_pool(LoaderHandle &handle, ObjectPool<CommandBatch> *pool) { handle.batches_ = pool; }
    static ObjectPool<DeviceMemory> *forget_memories(LoaderHandle &handle)
    {
        ObjectPool<DeviceMemory> *const pool = handle.memories_;
        handle.memories_ = nullptr;
        return pool;
    }
    static void restore_memories(LoaderHandle &handle, ObjectPool<DeviceMemory> *pool) { handle.memories_ = pool; }
};

struct HandleTreeProbe {
    template <typename Node>
    static ObjectPool<Node> &pool(HandleTree<Node> &tree) { return tree.pool_; }
};

struct HandleRegistryProbe {
    static ObjectPool<LoaderHandle> &pool(HandleRegistry &registry) { return HandleTreeProbe::pool(registry.tree_); }
};

}

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void miscount(ObjectPool<T> &pool, std::int32_t by) { pool.live_ += static_cast<std::uint32_t>(by); }
};

}

using av::ObjectPoolProbe;
using av::icd::HandleRegistryProbe;
using av::icd::LoaderHandleProbe;

namespace {
int first_token = 0;
int second_token = 0;
Connection *const first = reinterpret_cast<Connection *>(&first_token);
Connection *const second = reinterpret_cast<Connection *>(&second_token);

constexpr std::uint64_t instance_host = 0x1100u;
constexpr std::uint64_t physical_device_host = 0x2200u;
constexpr std::uint64_t device_host = 0x3300u;

class Tree {
public:
    explicit Tree(HandleRegistry &registry) : registry_(registry)
    {
        CHECK_STATUS(registry_.wrap(Kind::instance, instance_host, first, nullptr, instance), Status::ok);
        CHECK(registry_.invariants_hold());
        CHECK_STATUS(registry_.wrap(Kind::physical_device, physical_device_host, first, instance, physical_device),
                     Status::ok);
        CHECK(registry_.invariants_hold());
        CHECK_STATUS(registry_.wrap(Kind::device, device_host, first, physical_device, device), Status::ok);
        CHECK(registry_.invariants_hold());
        CHECK_STATUS(registry_.wrap(Kind::queue, 0x4400u, first, device, queue), Status::ok);
        CHECK(registry_.invariants_hold());
        CHECK_STATUS(registry_.wrap(Kind::command_buffer, 0x5500u, first, device, command_buffer), Status::ok);
        CHECK(registry_.invariants_hold());
    }

    LoaderHandle *instance = nullptr;
    LoaderHandle *physical_device = nullptr;
    LoaderHandle *device = nullptr;
    LoaderHandle *queue = nullptr;
    LoaderHandle *command_buffer = nullptr;

private:
    HandleRegistry &registry_;
};

}

TEST(loader_handle, keeps_the_loader_word_first_holding_the_magic)
{ CHECK_EQ(LoaderHandleProbe::loader_data_offset(), 0u);
  CHECK_EQ(LoaderHandle::loader_magic, 0x01CDC0DEu);
  int token = 0;
  LoaderHandle handle(Kind::instance, instance_host, reinterpret_cast<Connection *>(&token), nullptr);
  void *first_word = *static_cast<void **>(static_cast<void *>(&handle));
  CHECK(first_word == reinterpret_cast<void *>(LoaderHandle::loader_magic));
  CHECK(handle.invariants_hold()); }

TEST(loader_handle, answers_what_it_was_made_with_after_the_loader_overwrites_its_first_word)
{ LoaderHandle parent(Kind::instance, instance_host, first, nullptr);
  LoaderHandle handle(Kind::physical_device, physical_device_host, first, &parent);
  int table = 0;
  LoaderHandleProbe::overwrite_loader_data(handle, &table);
  CHECK(handle.kind() == Kind::physical_device);
  CHECK_EQ(handle.host(), physical_device_host);
  CHECK(handle.connection() == first);
  CHECK(handle.parent() == &parent);
  CHECK(handle.names(Kind::physical_device, physical_device_host, first));
  CHECK(!handle.names(Kind::device, physical_device_host, first));
  CHECK(!handle.names(Kind::physical_device, physical_device_host + 1u, first));
  CHECK(!handle.names(Kind::physical_device, physical_device_host, second));
  CHECK(handle.invariants_hold()); }

TEST(loader_handle, descends_from_every_parent_above_it_and_nothing_else)
{ LoaderHandle instance(Kind::instance, instance_host, first, nullptr);
  LoaderHandle physical_device(Kind::physical_device, physical_device_host, first, &instance);
  LoaderHandle device(Kind::device, device_host, first, &physical_device);
  LoaderHandle other(Kind::instance, instance_host + 1u, first, nullptr);
  CHECK(device.descends_from(physical_device));
  CHECK(device.descends_from(instance));
  CHECK(!device.descends_from(device));
  CHECK(!device.descends_from(other));
  CHECK(!instance.descends_from(instance)); }

TEST(loader_handle, invariants_refuse_what_the_registry_never_builds)
{ LoaderHandle instance(Kind::instance, instance_host, first, nullptr);
  LoaderHandle other_instance(Kind::instance, instance_host, second, nullptr);
  LoaderHandle device(Kind::device, device_host, first, &instance);
  CHECK(device.invariants_hold());
  LoaderHandleProbe::set_parent(device, nullptr);
  CHECK(!device.invariants_hold());
  LoaderHandleProbe::set_parent(device, &other_instance);
  CHECK(!device.invariants_hold());
  LoaderHandle queue(Kind::queue, 0x4400u, first, &device);
  LoaderHandleProbe::set_parent(device, &instance);
  CHECK(queue.invariants_hold());
  LoaderHandleProbe::set_kind(queue, Kind::physical_device);
  CHECK(!queue.invariants_hold());
  LoaderHandleProbe::set_kind(queue, Kind::device);
  CHECK(!queue.invariants_hold());
  LoaderHandleProbe::set_parent(instance, &other_instance);
  CHECK(!instance.invariants_hold());
  LoaderHandleProbe::set_parent(instance, nullptr);
  LoaderHandleProbe::set_host(instance, 0u);
  CHECK(!instance.invariants_hold());
  LoaderHandle orphan(Kind::instance, instance_host, nullptr, nullptr);
  CHECK(!orphan.invariants_hold()); }

TEST(handle_registry, wraps_a_handle_the_loader_can_dispatch_through)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  LoaderHandle *instance = nullptr;
  CHECK_STATUS(registry.wrap(Kind::instance, instance_host, first, nullptr, instance), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(instance != nullptr);
  CHECK(*static_cast<void **>(static_cast<void *>(instance)) == reinterpret_cast<void *>(LoaderHandle::loader_magic));
  CHECK_EQ(instance->host(), instance_host);
  CHECK(instance->connection() == first);
  CHECK_EQ(registry.live(), 1u); }

TEST(handle_registry, the_same_host_object_on_the_same_connection_is_the_same_handle)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  Tree tree(registry);
  LoaderHandle *again = nullptr;
  CHECK_STATUS(registry.wrap(Kind::physical_device, physical_device_host, first, tree.instance, again), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(again == tree.physical_device);
  CHECK_EQ(registry.live(), 5u);
  LoaderHandle *other = nullptr;
  CHECK_STATUS(registry.wrap(Kind::physical_device, physical_device_host + 1u, first, tree.instance, other),
               Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(other != nullptr && other != tree.physical_device);
  LoaderHandle *elsewhere = nullptr;
  CHECK_STATUS(registry.wrap(Kind::instance, instance_host, second, nullptr, elsewhere), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(elsewhere != tree.instance);
  LoaderHandle *as_device = nullptr;
  CHECK_STATUS(registry.wrap(Kind::device, physical_device_host, first, tree.instance, as_device), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(as_device != tree.physical_device);
  CHECK_EQ(registry.live(), 8u); }

TEST(handle_registry, refuses_a_handle_it_could_not_keep_its_invariants_with)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  Tree tree(registry);
  LoaderHandle *other_instance = nullptr;
  CHECK_STATUS(registry.wrap(Kind::instance, instance_host, second, nullptr, other_instance), Status::ok);
  LoaderHandle stack_instance(Kind::instance, instance_host, first, nullptr);
  struct Refusal {
      Kind          kind;
      std::uint64_t host;
      Connection   *connection;
      LoaderHandle *parent;
  };
  const Refusal refusals[] = {
      {Kind::physical_device, 0u, first, tree.instance},
      {Kind::physical_device, 0x2201u, nullptr, tree.instance},
      {Kind::instance, 0x1101u, first, tree.instance},
      {Kind::device, 0x3301u, first, nullptr},
      {Kind::device, 0x3301u, first, &stack_instance},
      {Kind::device, 0x3301u, first, tree.queue},
      {Kind::queue, 0x4401u, first, tree.queue},
      {Kind::device, 0x3301u, first, other_instance},
  };
  for (const Refusal &refusal : refusals) {
      LoaderHandle *handle = tree.instance;
      CHECK_STATUS(registry.wrap(refusal.kind, refusal.host, refusal.connection, refusal.parent, handle),
                   Status::invalid_parameter);
      CHECK(handle == nullptr);
      CHECK(registry.invariants_hold());
      CHECK_EQ(registry.live(), 6u);
  }
  CHECK_STATUS(registry.destroy(tree.instance), Status::ok);
  CHECK(registry.invariants_hold());
  LoaderHandle *handle = nullptr;
  CHECK_STATUS(registry.wrap(Kind::physical_device, 0x2201u, first, tree.instance, handle), Status::invalid_parameter);
  CHECK(handle == nullptr); }

TEST(handle_registry, made_answers_only_live_handles_of_the_kind_asked)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  Tree tree(registry);
  CHECK(registry.made(tree.device, Kind::device) == tree.device);
  CHECK(registry.made(tree.device, Kind::queue) == nullptr);
  CHECK(registry.made(tree.instance, Kind::instance) == tree.instance);
  LoaderHandle stack(Kind::device, device_host, first, tree.instance);
  CHECK(registry.made(&stack, Kind::device) == nullptr);
  CHECK(registry.made(nullptr, Kind::instance) == nullptr);
  CHECK(registry.made(static_cast<char *>(static_cast<void *>(tree.device)) + 1, Kind::device) == nullptr);
  CHECK_STATUS(registry.destroy(tree.device), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(registry.made(tree.device, Kind::device) == nullptr); }

TEST(handle_registry, instance_of_answers_the_live_instance_on_a_connection)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  CHECK(registry.instance_of(first) == nullptr);
  Tree tree(registry);
  CHECK(registry.instance_of(first) == tree.instance);
  CHECK(registry.instance_of(second) == nullptr);
  LoaderHandle *other = nullptr;
  CHECK_STATUS(registry.wrap(Kind::instance, instance_host, second, nullptr, other), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(registry.instance_of(second) == other);
  CHECK(registry.instance_of(first) == tree.instance);
  CHECK_STATUS(registry.destroy(tree.instance), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(registry.instance_of(first) == nullptr); }

TEST(handle_registry, a_device_takes_its_queues_and_command_buffers_and_nothing_above)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  Tree tree(registry);
  LoaderHandle *other_device = nullptr;
  CHECK_STATUS(registry.wrap(Kind::device, device_host + 1u, first, tree.physical_device, other_device), Status::ok);
  LoaderHandle *other_queue = nullptr;
  CHECK_STATUS(registry.wrap(Kind::queue, 0x4401u, first, other_device, other_queue), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK_STATUS(registry.destroy(tree.device), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.live(), 4u);
  CHECK(registry.made(tree.queue, Kind::queue) == nullptr);
  CHECK(registry.made(tree.command_buffer, Kind::command_buffer) == nullptr);
  CHECK(registry.made(tree.physical_device, Kind::physical_device) == tree.physical_device);
  CHECK(registry.made(other_device, Kind::device) == other_device);
  CHECK(registry.made(other_queue, Kind::queue) == other_queue);
  CHECK_STATUS(registry.destroy(tree.device), Status::invalid_parameter);
  CHECK(registry.invariants_hold()); }

TEST(handle_registry, an_instance_takes_everything_made_through_it_at_any_depth)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  Tree tree(registry);
  LoaderHandle *other = nullptr;
  CHECK_STATUS(registry.wrap(Kind::instance, instance_host, second, nullptr, other), Status::ok);
  LoaderHandle *other_device = nullptr;
  CHECK_STATUS(registry.wrap(Kind::device, device_host, second, other, other_device), Status::ok);
  CHECK_STATUS(registry.destroy(tree.instance), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.live(), 2u);
  CHECK(registry.made(other, Kind::instance) == other);
  CHECK(registry.made(other_device, Kind::device) == other_device); }

TEST(handle_registry, live_on_counts_the_handles_of_one_connection)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  CHECK_EQ(registry.live_on(first), 0u);
  Tree tree(registry);
  LoaderHandle *other = nullptr;
  CHECK_STATUS(registry.wrap(Kind::instance, instance_host, second, nullptr, other), Status::ok);
  CHECK_EQ(registry.live_on(first), 5u);
  CHECK_EQ(registry.live_on(second), 1u);
  CHECK_EQ(registry.live_on(nullptr), 0u);
  CHECK_STATUS(registry.destroy(tree.device), Status::ok);
  CHECK_EQ(registry.live_on(first), 2u); }

TEST(handle_registry, a_leaf_destroys_alone)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  Tree tree(registry);
  CHECK_STATUS(registry.destroy(tree.command_buffer), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.live(), 4u);
  CHECK(registry.made(tree.queue, Kind::queue) == tree.queue); }

TEST(handle_registry, destroy_refuses_what_is_not_a_live_handle)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  Tree tree(registry);
  LoaderHandle stack(Kind::instance, instance_host, first, nullptr);
  CHECK_STATUS(registry.destroy(&stack), Status::invalid_parameter);
  CHECK_STATUS(registry.destroy(nullptr), Status::invalid_parameter);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.live(), 5u); }

TEST(handle_registry, destroys_a_wide_tree_spread_over_several_chunks)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  Tree tree(registry);
  constexpr std::uint32_t devices = 70u;
  LoaderHandle *made[devices] = {};
  for (std::uint32_t index = 0u; index < devices; index++) {
      CHECK_STATUS(registry.wrap(Kind::device, 0x10000u + index, first, tree.physical_device, made[index]),
                   Status::ok);
      LoaderHandle *queue = nullptr;
      CHECK_STATUS(registry.wrap(Kind::queue, 0x20000u + index, first, made[index], queue), Status::ok);
  }
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.live(), 5u + 2u * devices);
  CHECK_STATUS(registry.destroy(tree.physical_device), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.live(), 1u);
  CHECK_STATUS(registry.destroy(tree.instance), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.live(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(handle_registry, a_refused_allocation_refuses_only_the_handle_that_needed_a_chunk)
{ constexpr std::uint32_t slots = av::ObjectPool<LoaderHandle>::chunk_slots;
  constexpr std::uint32_t handles = 2u * slots + 1u;
  struct Case {
      std::uint32_t failing;
      std::uint32_t refused_index;
  };
  const Case cases[] = {{1u, slots}, {2u, 2u * slots}, {3u, 0u}};
  for (const Case &check : cases) {
      MockPlatform   platform;
      HandleRegistry registry(platform);
      LoaderHandle  *instance = nullptr;
      CHECK_STATUS(registry.wrap(Kind::instance, instance_host, first, nullptr, instance), Status::ok);
      platform.fail_allocation_at(check.failing);
      std::uint32_t refused = 0u;
      for (std::uint32_t index = 1u; index < handles + 1u; index++) {
          LoaderHandle *handle = instance;
          const Status  status = registry.wrap(Kind::physical_device, index, first, instance, handle);
          CHECK(registry.invariants_hold());
          if (status != Status::ok) {
              CHECK_STATUS(status, Status::no_memory);
              CHECK(handle == nullptr);
              CHECK_EQ(index, check.refused_index);
              refused++;
              continue;
          }
          CHECK(handle != nullptr && handle->host() == index);
      }
      CHECK_EQ(refused, check.refused_index != 0u ? 1u : 0u);
      CHECK_EQ(registry.live(), handles + 1u - refused);
      CHECK_STATUS(registry.destroy(instance), Status::ok);
      CHECK_EQ(platform.live_allocations(), 0u);
  }
}

TEST(handle_registry, invariants_see_a_dangling_parent_and_a_duplicate)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  Tree tree(registry);
  LoaderHandle stack(Kind::instance, instance_host + 9u, first, nullptr);
  LoaderHandleProbe::set_parent(*tree.physical_device, &stack);
  CHECK(!registry.invariants_hold());
  LoaderHandleProbe::set_parent(*tree.physical_device, tree.instance);
  CHECK(registry.invariants_hold());
  LoaderHandleProbe::set_host(*tree.queue, 0x5500u);
  LoaderHandleProbe::set_kind(*tree.queue, Kind::command_buffer);
  CHECK(!registry.invariants_hold());
  LoaderHandleProbe::set_kind(*tree.queue, Kind::queue);
  CHECK(registry.invariants_hold());
  LoaderHandleProbe::set_host(*tree.device, 0u);
  CHECK(!registry.invariants_hold());
  LoaderHandleProbe::set_host(*tree.device, device_host);
  CHECK(registry.invariants_hold());
  ObjectPoolProbe::miscount(HandleRegistryProbe::pool(registry), 1);
  CHECK(!registry.invariants_hold());
  ObjectPoolProbe::miscount(HandleRegistryProbe::pool(registry), -1);
  CHECK(registry.invariants_hold()); }

namespace {
class BatchSource {
public:
    BatchSource() : lock_(trace_, 'S'), log_(), connection_(kmt_, platform_, lock_, log_), batches_(platform_), memories_(platform_) {}

    MockPlatform                          &platform() { return platform_; }
    av::ObjectPool<av::icd::CommandBatch> &batches() { return batches_; }
    av::ObjectPool<av::icd::DeviceMemory> &memories() { return memories_; }

    av::icd::DeviceMemory &make_memory()
    {
        av::icd::DeviceMemory *memory = nullptr;
        CHECK_STATUS(memories_.create(memory, kmt_, connection_, log_), Status::ok);
        return *memory;
    }

    av::icd::CommandBatch &make()
    {
        av::icd::CommandBatch *batch = nullptr;
        CHECK_STATUS(batches_.create(batch, platform_, connection_), Status::ok);
        return *batch;
    }

private:
    av::MockKmt                           kmt_;
    MockPlatform                          platform_;
    av::LockTrace                         trace_;
    av::RecordingThreadLock               lock_;
    av::RecordingLog                      log_;
    Connection                            connection_;
    av::ObjectPool<av::icd::CommandBatch> batches_;
    av::ObjectPool<av::icd::DeviceMemory> memories_;
};

}

TEST(loader_handle, a_command_buffer_owns_its_batch_until_it_is_destroyed)
{ BatchSource source;
  {
      LoaderHandle instance(Kind::instance, instance_host, first, nullptr);
      LoaderHandle command_buffer(Kind::command_buffer, 0x5500u, first, &instance);
      CHECK(command_buffer.batch() == nullptr);
      av::icd::CommandBatch &batch = source.make();
      command_buffer.record_into(source.batches(), batch);
      CHECK(command_buffer.invariants_hold());
      CHECK(command_buffer.batch() == &batch);
      CHECK_EQ(source.batches().live(), 1u);
  }
  CHECK_EQ(source.batches().live(), 0u);
  CHECK_EQ(source.platform().live_allocations(), 0u); }

TEST(handle_registry, destroying_a_device_takes_its_command_buffers_batches_and_counts_them)
{ MockPlatform platform;
  BatchSource source;
  HandleRegistry registry(platform);
  Tree tree(registry);
  CHECK_EQ(registry.batches(), 0u);
  tree.command_buffer->record_into(source.batches(), source.make());
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.batches(), 1u);
  CHECK_STATUS(registry.destroy(tree.device), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.batches(), 0u);
  CHECK_EQ(source.batches().live(), 0u); }

TEST(loader_handle, invariants_refuse_a_batch_on_anything_but_a_command_buffer_or_without_its_pool)
{ BatchSource source;
  LoaderHandle instance(Kind::instance, instance_host, first, nullptr);
  LoaderHandle device(Kind::device, device_host, first, &instance);
  av::icd::CommandBatch &batch = source.make();
  device.record_into(source.batches(), batch);
  CHECK(!device.invariants_hold());
  LoaderHandleProbe::set_kind(device, Kind::command_buffer);
  CHECK(device.invariants_hold());
  av::ObjectPool<av::icd::CommandBatch> *const pool = LoaderHandleProbe::forget_pool(device);
  CHECK(!device.invariants_hold());
  LoaderHandleProbe::restore_pool(device, pool);
  CHECK(device.invariants_hold()); }

TEST(loader_handle, a_device_owns_its_memory_until_it_is_destroyed)
{ BatchSource source;
  {
      LoaderHandle instance(Kind::instance, instance_host, first, nullptr);
      LoaderHandle device(Kind::device, device_host, first, &instance);
      CHECK(device.device_memory() == nullptr);
      av::icd::DeviceMemory &memory = source.make_memory();
      device.allocate_from(source.memories(), memory);
      CHECK(device.invariants_hold());
      CHECK(device.device_memory() == &memory);
      CHECK_EQ(source.memories().live(), 1u);
  }
  CHECK_EQ(source.memories().live(), 0u); }

TEST(handle_registry, destroying_an_instance_takes_its_devices_memory_and_counts_it)
{ MockPlatform platform;
  BatchSource source;
  HandleRegistry registry(platform);
  Tree tree(registry);
  CHECK_EQ(registry.memories(), 0u);
  tree.device->allocate_from(source.memories(), source.make_memory());
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.memories(), 1u);
  CHECK_EQ(registry.batches(), 0u);
  CHECK_STATUS(registry.destroy(tree.instance), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK_EQ(registry.memories(), 0u);
  CHECK_EQ(source.memories().live(), 0u); }

TEST(loader_handle, invariants_refuse_memory_on_anything_but_a_device_or_without_its_pool)
{ BatchSource source;
  LoaderHandle instance(Kind::instance, instance_host, first, nullptr);
  LoaderHandle queue(Kind::queue, 0x4400u, first, &instance);
  queue.allocate_from(source.memories(), source.make_memory());
  CHECK(!queue.invariants_hold());
  LoaderHandleProbe::set_kind(queue, Kind::device);
  CHECK(queue.invariants_hold());
  av::ObjectPool<av::icd::DeviceMemory> *const pool = LoaderHandleProbe::forget_memories(queue);
  CHECK(!queue.invariants_hold());
  LoaderHandleProbe::restore_memories(queue, pool);
  CHECK(queue.invariants_hold()); }

TEST(handle_registry, child_answers_each_live_handle_of_a_kind_made_through_a_parent_once)
{ MockPlatform platform;
  HandleRegistry registry(platform);
  Tree tree(registry);
  LoaderHandle *second_queue = nullptr;
  CHECK_STATUS(registry.wrap(Kind::queue, 0x4401u, first, tree.device, second_queue), Status::ok);
  LoaderHandle *elsewhere = nullptr;
  CHECK_STATUS(registry.wrap(Kind::queue, 0x4402u, first, tree.instance, elsewhere), Status::ok);
  CHECK(registry.invariants_hold());
  const LoaderHandle *const found[] = {registry.child(*tree.device, Kind::queue, 0u),
                                       registry.child(*tree.device, Kind::queue, 1u)};
  CHECK(found[0] != found[1]);
  CHECK(found[0] == tree.queue || found[0] == second_queue);
  CHECK(found[1] == tree.queue || found[1] == second_queue);
  CHECK(registry.child(*tree.device, Kind::queue, 2u) == nullptr);
  CHECK(registry.child(*tree.device, Kind::command_buffer, 0u) == tree.command_buffer);
  CHECK(registry.child(*tree.device, Kind::command_buffer, 1u) == nullptr);
  CHECK(registry.child(*tree.instance, Kind::queue, 0u) == elsewhere);
  CHECK(registry.child(*tree.physical_device, Kind::queue, 0u) == nullptr);
  CHECK_STATUS(registry.destroy(second_queue), Status::ok);
  CHECK(registry.invariants_hold());
  CHECK(registry.child(*tree.device, Kind::queue, 0u) == tree.queue);
  CHECK(registry.child(*tree.device, Kind::queue, 1u) == nullptr); }
