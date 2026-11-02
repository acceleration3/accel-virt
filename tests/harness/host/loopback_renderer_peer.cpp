#include "harness/host/loopback_renderer_peer.hpp"

#include "harness/framework/test_registry.hpp"
#include "proto/renderer/region_message.hpp"
#include "proto/renderer/renderer_message_header.hpp"

#include <cstring>
#include <fcntl.h>

namespace av {

LoopbackRendererPeer::Process::Process(MockHost &qemu, const AvExecutorOps &executor)
    : host(), resources(qemu, executor), memory(), doorbells(), service(host, resources, memory, doorbells), regions{},
      region_count(0u)
{
    host.translate_guest_with(
        [](void *context, std::uint64_t address, std::uint64_t length) -> void * {
            return static_cast<GuestMemoryTable *>(context)->resolve(address, length);
        },
        &memory);
    host.forward_requests();
}

LoopbackRendererPeer::Process::~Process()
{
    service.shut_down();
    for (std::uint32_t index = 0u; index < region_count; index++) {
        void         *base = nullptr;
        std::uint64_t size = 0u;
        int           descriptor = -1;
        if (memory.remove(regions[index], base, size, descriptor) != Status::ok) continue;
        resources.unmap_region(base, size);
        resources.close_descriptor(descriptor);
    }
}

void LoopbackRendererPeer::Process::remember_region(const std::uint8_t *bytes, std::size_t length)
{
    RendererMessageHeader header;
    RegionMessage         region;
    if (header.decode(bytes, length) != Status::ok || header.kind() != RendererMessageHeader::Kind::region ||
        region.decode(bytes + RendererMessageHeader::encoded_size, header.body_length(), RegionMessage::descriptors) !=
            Status::ok ||
        region_count == GuestMemoryTable::max_regions)
        return;
    regions[region_count++] = region.region();
}

LoopbackRendererPeer::LoopbackRendererPeer(MockHost &qemu, const AvExecutorOps &executor)
    : qemu_(qemu), executor_(executor), process_(), answers_{}, first_(0u), queued_(0u), last_outcome_(Status::ok),
      delivered_(0u), generation_(1u), spoken_to_(false), silent_(false)
{
    if (process_.construct(qemu_, executor_) != Status::ok)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "the renderer process could not be made");
}

LoopbackRendererPeer::~LoopbackRendererPeer()
{
    process_.destroy();
}

void LoopbackRendererPeer::started()
{
    if (!spoken_to_) return;
    process_.destroy();
    if (process_.construct(qemu_, executor_) != Status::ok)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "the renderer process could not be made");
    first_ = 0u;
    queued_ = 0u;
    spoken_to_ = false;
    silent_ = false;
    generation_++;
}

void LoopbackRendererPeer::deliver(const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                                   std::uint32_t count)
{
    delivered_++;
    spoken_to_ = true;
    Process &process = process_.object();
    process.resources.adopt(descriptors, count);
    Answer     &slot = answers_[(first_ + queued_) % max_answers];
    std::size_t answer_length = 0u;
    int         answer_descriptor = -1;
    last_outcome_ = process.service.receive(bytes, length, descriptors, count, slot.bytes, sizeof slot.bytes,
                                            answer_length, answer_descriptor);
    slot.descriptor = answer_descriptor;
    if (last_outcome_ == Status::ok) process.remember_region(bytes, length);
    if (answer_length == 0u || silent_) return;
    if (queued_ == max_answers) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "the renderer's answers were never received");
        return;
    }
    slot.length = answer_length;
    queued_++;
}

bool LoopbackRendererPeer::answer(std::uint8_t *buffer, std::size_t capacity, std::size_t &length, int *descriptors,
                                  std::uint32_t capacity_descriptors, std::uint32_t &count)
{
    count = 0u;
    if (queued_ == 0u) return false;
    const Answer &next = answers_[first_];
    first_ = (first_ + 1u) % max_answers;
    queued_--;
    length = next.length;
    std::memcpy(buffer, next.bytes, next.length < capacity ? next.length : capacity);
    if (next.descriptor >= 0 && capacity_descriptors != 0u) {
        const int crossed = ::fcntl(next.descriptor, F_DUPFD_CLOEXEC, 0);
        if (crossed >= 0) descriptors[count++] = crossed;
    }
    return true;
}

void LoopbackRendererPeer::fall_silent()
{
    silent_ = true;
}

std::uint32_t LoopbackRendererPeer::generation() const
{
    return generation_;
}

Status LoopbackRendererPeer::last_outcome() const
{
    return last_outcome_;
}

std::uint32_t LoopbackRendererPeer::delivered() const
{
    return delivered_;
}

std::uint32_t LoopbackRendererPeer::queued() const
{
    return queued_;
}

RendererService &LoopbackRendererPeer::service()
{
    return process_.object().service;
}

MockRendererResources &LoopbackRendererPeer::resources()
{
    return process_.object().resources;
}

GuestMemoryTable &LoopbackRendererPeer::memory()
{
    return process_.object().memory;
}

MockHost &LoopbackRendererPeer::host()
{
    return process_.object().host;
}

}
