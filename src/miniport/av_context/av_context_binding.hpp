#ifndef AV_AV_CONTEXT_BINDING_HPP
#define AV_AV_CONTEXT_BINDING_HPP

#include "core/platform/platform.hpp"
#include "miniport/av_context/guest_context.hpp"
#include "core/support/status.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CapsetInfo;
class ContextPages;
class GuestPageList;
class RegisterWindow;
class SubmissionDescriptor;
class VirtioGpuDevice;
struct AvContextBindingProbe;

class AvContextBinding {
public:
    static constexpr uint32_t transport_resources   = 3u;
    static constexpr uint32_t resources_per_context = transport_resources;
    static constexpr uint32_t first_resource_id = 0x00100000u;
    static constexpr size_t   request_bytes = GpuRequestWriter::max_backing_request_size;

    AvContextBinding(Platform &platform, RegisterWindow &doorbells, const AvContextRecord &record, uint32_t slot,
                      uint32_t generation, uint8_t *request, size_t capacity);
    static constexpr uint32_t generations = 256u;
    AvContextBinding(const AvContextBinding &) = delete;
    AvContextBinding &operator=(const AvContextBinding &) = delete;

    [[nodiscard]] Status set_up(VirtioGpuDevice &device, const CapsetInfo &capset, const ContextPages &pages);
    [[nodiscard]] Status tear_down(VirtioGpuDevice &device);
    void                 abandon();
    [[nodiscard]] Status submit(const SubmissionDescriptor &submission);
    void                 kick();
    void                 fail();
    uint64_t             next_begin() const;
    void                 user_mode_submits(uint64_t *progress);
    void                 driver_submits();
    bool                 forget_progress(const uint64_t *progress);
    [[nodiscard]] Status serve();

    GuestContext::State state() const;
    bool                established() const;
    uint32_t            context_id() const;
    SubApi              sub_api() const;
    uint64_t            completed() const;
    uint64_t            raised() const;
    bool                faulted() const;

    bool invariants_hold() const;

private:
    void run_sequence(VirtioGpuDevice &device);

    static constexpr size_t answer_bytes  = 64u;

    GuestContext     context_;
    AvContextRecord record_;
    uint32_t         context_id_;
    uint32_t         doorbell_;
    uint32_t         first_resource_id_;
    uint8_t         *request_;
    size_t           capacity_;
    uint8_t          answer_[answer_bytes];

    friend struct AvContextBindingProbe;
};

}

#endif
