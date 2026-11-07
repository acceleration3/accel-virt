#include "bench_timer.hpp"
#include "executor/d3d/d3d12_descriptor_holds.hpp"
#include "executor/d3d/d3d12_object_holds.hpp"
#include "executor/vulkan/vulkan_destroyer.hpp"
#include "executor/vulkan/vulkan_guard.hpp"
#include "executor/vulkan/vulkan_progress.hpp"
#include "harness/executor/counted_object.hpp"
#include "malloc_blocks.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using av::BenchTimer;
using av::CountedObject;
using av::D3d12DescriptorHolds;
using av::D3d12ObjectHolds;
using av::VulkanGuard;
using av::vkproto::HandleKind;

namespace {

[[noreturn]] void die(int line)
{
    std::fprintf(stderr, "bench failed at line %d\n", line);
    std::exit(1);
}

constexpr std::uint64_t device = 0x1000u;
constexpr std::uint64_t queue = 0x2000u;

class NullDestroyer final : public av::VulkanDestroyer {
public:
    void destroy(av::VulkanDestroyKind, std::uint64_t, std::uint64_t, std::uint64_t) override {}
};

class CompletedProgress final : public av::VulkanProgress {
public:
    std::uint64_t marked() const override { return marked_; }
    std::uint64_t reached(std::uint64_t) override { return reached_; }
    std::uint64_t reserve(std::uint64_t) override { return ++marked_; }
    void          commit(std::uint64_t mark, std::uint64_t) override { reached_ = mark; }
    void          cancel(std::uint64_t) override {}
    bool          wait_idle(std::uint64_t) override { return true; }
    bool          wait_marks(std::uint64_t, const std::uint64_t *, std::uint32_t) override { return true; }
    bool          drain(std::uint64_t, av::TeardownBudget &) override { return true; }
    void          begin_poll() override {}
    bool          lost() const override { return false; }

private:
    std::uint64_t marked_ = 0u;
    std::uint64_t reached_ = 0u;
};

struct VulkanRig {
    av::MallocBlocks platform;
    NullDestroyer    destroyer;
    CompletedProgress progress;
    VulkanGuard      guard{platform, destroyer, progress};
};

void vulkan_descriptors(const BenchTimer &timer)
{
    constexpr std::uint32_t sets = 64u;
    constexpr std::uint32_t per_set = 512u;
    constexpr std::uint32_t objects = 4096u;
    VulkanRig rig;
    for (std::uint32_t set = 0u; set < sets; set++)
        for (std::uint32_t at = 0u; at < per_set; at++)
            if (!rig.guard.bound(1000u + set, 0u, at, 10000u + at, 0u, device)) die(__LINE__);
    const std::uint64_t writes = std::uint64_t{sets} * per_set;
    std::uint32_t       salt = 1u;
    const double write_ns = timer.nanoseconds_per_operation(writes * 4u, [&] {
        for (std::uint32_t pass = 0u; pass < 4u; pass++, salt++)
            for (std::uint32_t set = 0u; set < sets; set++)
                for (std::uint32_t at = 0u; at < per_set; at++)
                    if (!rig.guard.bound(1000u + set, 0u, at, 10000u + ((at + salt * 7u + set) % objects), 0u, device))
                        die(__LINE__);
    });
    timer.report("vulkan descriptor write (replacing)", writes * 4u, write_ns);
    auto fill_fixed = [&] {
        for (std::uint32_t set = 0u; set < sets; set++)
            for (std::uint32_t at = 0u; at < per_set; at++)
                if (!rig.guard.bound(1000u + set, 0u, at, 10000u + at, 0u, device)) die(__LINE__);
    };
    fill_fixed();
    const double same_ns = timer.nanoseconds_per_operation(writes * 4u, [&] {
        for (std::uint32_t pass = 0u; pass < 4u; pass++) fill_fixed();
    });
    timer.report("vulkan descriptor write (same object again)", writes * 4u, same_ns);
    VkCopyDescriptorSet copy = {};
    copy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    copy.descriptorCount = per_set;
    const std::uint64_t copied = std::uint64_t{sets - 1u} * per_set * 4u;
    const double copy_ns = timer.nanoseconds_per_operation(copied, [&] {
        for (std::uint32_t pass = 0u; pass < 4u; pass++)
            for (std::uint32_t set = 1u; set < sets; set++) {
                copy.srcSet = reinterpret_cast<VkDescriptorSet>(std::uintptr_t{1000u});
                copy.dstSet = reinterpret_cast<VkDescriptorSet>(std::uintptr_t{1000u + set});
                if (!rig.guard.copied(&copy, 1u, device)) die(__LINE__);
            }
    });
    timer.report("vulkan descriptor copy (per descriptor)", copied, copy_ns);
}

void vulkan_commands(const BenchTimer &timer)
{
    constexpr std::uint32_t buffers = 256u;
    VulkanRig rig;
    const std::uint64_t commands = 200000u;
    const double each = timer.nanoseconds_per_operation(commands, [&] {
        for (std::uint64_t at = 0u; at < commands; at++) {
            const std::uint64_t cb = 100u + (at / 64u) % buffers;
            rig.guard.begin_frame(true);
            rig.guard.reap();
            if (!rig.guard.reference(HandleKind::command_buffer, cb, 50u, device)) die(__LINE__);
            if (!rig.guard.reference(HandleKind::buffer, 5000u + at % 97u, 0u, device)) die(__LINE__);
            if (!rig.guard.reference(HandleKind::image, 6000u + at % 89u, 0u, device)) die(__LINE__);
            if (!rig.guard.reference(HandleKind::pipeline, 7000u + at % 13u, 0u, device)) die(__LINE__);
        }
    });
    timer.report("vkCmd* with 3 handles (guard only)", commands, each);
}

void vulkan_submit(const BenchTimer &timer)
{
    VulkanRig rig;
    VkCommandBuffer buffers[4];
    for (std::uint32_t at = 0u; at < 4u; at++) {
        buffers[at] = reinterpret_cast<VkCommandBuffer>(std::uintptr_t{100u + at});
        rig.guard.begin_frame(true);
        if (!rig.guard.reference(HandleKind::command_buffer, 100u + at, 50u, device)) die(__LINE__);
        for (std::uint32_t held = 0u; held < 24u; held++)
            if (!rig.guard.reference(HandleKind::buffer, 5000u + at * 64u + held, 0u, device)) die(__LINE__);
    }
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 4u;
    submit.pCommandBuffers = buffers;
    const std::uint64_t submits = 100000u;
    const double each = timer.nanoseconds_per_operation(submits, [&] {
        for (std::uint64_t at = 0u; at < submits; at++) {
            const std::uint64_t mark = rig.guard.reserve(device);
            if (mark == 0u) die(__LINE__);
            rig.guard.submitted(mark, device, queue, &submit, 1u);
            rig.guard.end_batch();
        }
    });
    timer.report("vkQueueSubmit (guard only, 4 cbs of 24 refs)", submits, each);
}

void vulkan_pool_reset(const BenchTimer &timer)
{
    constexpr std::uint32_t sets = 50000u;
    constexpr std::uint64_t pool = 900u;
    VulkanRig *rig = nullptr;
    const double each = timer.nanoseconds_per_operation_after(
        sets,
        [&] {
            delete rig;
            rig = new VulkanRig;
            rig->guard.begin_frame(true);
            if (!rig->guard.reference(HandleKind::command_buffer, 100u, 50u, device)) die(__LINE__);
            for (std::uint32_t at = 0u; at < sets; at++) {
                if (!rig->guard.reference(HandleKind::descriptor_set, 20000u + at, pool, device)) die(__LINE__);
                for (std::uint32_t binding = 0u; binding < 3u; binding++)
                    if (!rig->guard.bound(20000u + at, binding, 0u, 5000u + binding, 0u, device)) die(__LINE__);
            }
            const std::uint64_t mark = rig->guard.reserve(device);
            VkCommandBuffer     buffer = reinterpret_cast<VkCommandBuffer>(std::uintptr_t{100u});
            VkSubmitInfo        submit = {};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1u;
            submit.pCommandBuffers = &buffer;
            rig->guard.submitted(mark, device, queue, &submit, 1u);
        },
        [&] {
            for (std::uint32_t at = 0u; at < sets; at++) {
                static_cast<void>(rig->guard.mark_of(20000u + at, device));
                static_cast<void>(rig->guard.release(20000u + at, device));
            }
            rig->guard.orphan_children(pool, device);
        });
    delete rig;
    timer.report("vkResetDescriptorPool, 50k sets x3 descriptors", sets, each);
}

void d3d12_holds(const BenchTimer &timer, std::uint32_t only_slots)
{
    constexpr std::uint32_t heap = 7u;
    constexpr std::uint32_t resources = 1024u;
    constexpr std::uint32_t small = 4096u;
    constexpr std::uint32_t large = 1048576u;
    std::vector<CountedObject> objects(resources);
    for (const std::uint32_t slots : {small, large}) {
        if (only_slots != 0u && slots != only_slots) continue;
        av::MallocBlocks platform;
        D3d12ObjectHolds holds(platform);
        D3d12DescriptorHolds descriptors(platform, holds);
        std::uint64_t        reached = 0u;
        auto                 reach = [&reached]() { return reached; };
        for (std::uint32_t at = 0u; at < slots; at++)
            if (descriptors.note(heap, at, D3d12DescriptorHolds::View{&objects[at % resources], nullptr, nullptr, nullptr}, 0u,
                                 reach) != av::Status::ok)
                die(__LINE__);
        std::uint32_t salt = 1u;
        const std::uint64_t views = slots < large ? slots * 16u : 100000u;
        char                name[96];
        const double        view_ns = timer.nanoseconds_per_operation(views, [&] {
            for (std::uint64_t at = 0u; at < views; at++)
                if (descriptors.note(heap, static_cast<std::uint32_t>(at % slots),
                                     D3d12DescriptorHolds::View{&objects[(at + salt * 13u) % resources], nullptr, nullptr, nullptr},
                                     1u, reach) != av::Status::ok)
                    die(__LINE__);
            salt++;
        });
        std::snprintf(name, sizeof name, "d3d12 CreateView, %u-slot heap", slots);
        timer.report(name, views, view_ns);
        const std::uint32_t chunk = 256u;
        const std::uint64_t copies = std::uint64_t{slots / chunk} / 2u * chunk;
        const double        copy_ns = timer.nanoseconds_per_operation(copies, [&] {
            for (std::uint32_t at = 0u; at + chunk <= slots / 2u; at += chunk)
                if (descriptors.copy(heap, slots / 2u + at, heap, at, chunk, 1u, reach) != av::Status::ok) die(__LINE__);
        });
        std::snprintf(name, sizeof name, "d3d12 CopyDescriptors, %u-slot heap (per descriptor)", slots);
        timer.report(name, copies, copy_ns);
        if (slots <= small) {
            descriptors.release_all();
            holds.release_all();
        }
    }
}

void d3d12_destroy_heap(const BenchTimer &timer)
{
    constexpr std::uint32_t heap = 9u;
    constexpr std::uint32_t slots = 32768u;
    constexpr std::uint32_t resources = 512u;
    std::vector<CountedObject> objects(resources);
    CountedObject              heap_object;
    av::MallocBlocks           platform;
    D3d12ObjectHolds           holds(platform);
    D3d12DescriptorHolds       descriptors(platform, holds);
    auto                       reach = []() { return std::uint64_t{0}; };
    const double each = timer.nanoseconds_per_operation_after(
        slots,
        [&] {
            for (std::uint32_t at = 0u; at < slots; at++)
                if (descriptors.note(heap, at, D3d12DescriptorHolds::View{&objects[at % resources], nullptr, nullptr, nullptr}, 0u,
                                     reach) != av::Status::ok)
                    die(__LINE__);
        },
        [&] {
            if (descriptors.destroy_heap(heap, &heap_object, 1u, reach) != av::Status::ok) die(__LINE__);
        });
    timer.report("d3d12 heap destroy, 32768 slots (per descriptor)", slots, each);
    holds.release_all();
}

void d3d12_retain(const BenchTimer &timer)
{
    constexpr std::uint64_t list = 11u;
    constexpr std::uint32_t resources = 48u;
    av::MallocBlocks platform;
    D3d12ObjectHolds holds(platform);
    std::vector<CountedObject> objects(resources);
    const std::uint64_t        retains = 100000u;
    const double each = timer.nanoseconds_per_operation(retains, [&] {
        for (std::uint64_t at = 0u; at < retains; at++) {
            if (holds.hold(list, &objects[at % resources], nullptr) != av::Status::ok) die(__LINE__);
            if (at % 4096u == 4095u) {
                holds.submitted(list, at);
                if (holds.release_list(list, at) != av::Status::ok) die(__LINE__);
            }
        }
    });
    timer.report("d3d12 Set*/draw retain (hold, 48 distinct)", retains, each);
    holds.release_all();
}

}

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const BenchTimer timer(5u);
    const char      *only = argc > 1 ? argv[1] : "";
    auto             wanted = [only](const char *name) { return only[0] == '\0' || std::strcmp(only, name) == 0; };
    if (wanted("descriptors")) vulkan_descriptors(timer);
    if (wanted("commands")) vulkan_commands(timer);
    if (wanted("submit")) vulkan_submit(timer);
    if (wanted("pool")) vulkan_pool_reset(timer);
    if (wanted("d3d12")) d3d12_holds(timer, 0u);
    if (only[0] != '\0' && std::strcmp(only, "d3d12small") == 0) d3d12_holds(timer, 4096u);
    if (only[0] != '\0' && std::strcmp(only, "d3d12large") == 0) d3d12_holds(timer, 1048576u);
    if (wanted("destroy")) d3d12_destroy_heap(timer);
    if (wanted("retain")) d3d12_retain(timer);
    return 0;
}
