#include "device/context/device_context.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "device/context/d3d_ordering.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/support/text_buffer.hpp"
#include "core/support/scope_exit.hpp"
#include "device/abi/entry_points.hpp"
#include "device/fence/device_pulse.hpp"
#include "device/fence/fence_window.hpp"
#include "device/host/stdio_log.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/context/ring_window.hpp"
#include "device/threading/work_item.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/submission/reply_header.hpp"

#include <stdio.h>

namespace av {

DeviceContext::DeviceContext(Host &host, BlobTable &blobs, HostBlobOwners &owners, FenceInterrupts &interrupts,
                             const ExecutorLibrary &library,
                             ExecutorShares &shares, WireRecorder &recorder, D3dOrdering &ordering,
                             DevicePulse &pulse, uint32_t id, const char *name)
    : ordering_(ordering), pulse_(pulse), recorder_(recorder), id_(id), label_(name), binding_(host, blobs), fences_(host, interrupts, recorder, id),
      imports_(host, owners, executor_, recorder, id),
      executor_(host, library, shares, id), staging_(host),
      batches_(host, executor_, imports_, staging_, fences_, recorder, id),
      ring_(host, fences_, batches_), signal_(host, worker_, fences_), worker_(host, ring_)
      {
          AV_SCOPE("DeviceContext::DeviceContext");
      }

DeviceContext::~DeviceContext()
{
    AV_SCOPE("DeviceContext::~DeviceContext");
    ordering_.remove(ring_);
    pulse_.drop(fences_);
}

Status DeviceContext::start()
{
    AV_SCOPE("DeviceContext::start");
    worker_.connect_signal(signal_);
    return worker_.start();
}

Status DeviceContext::post(const WorkItem &item)
{
    AV_SCOPE("DeviceContext::post");
    return worker_.post(item);
}

Status DeviceContext::refused(const char *step, Status status) const
{
    AV_SCOPE("DeviceContext::refused");
    StdioLog  sink(stderr);
    DriverLog lines(sink, "virtio-gpu-av: ");
    LogLine(lines)
        .text("context ")
        .number(id_)
        .text(" setup refused at ")
        .text(step)
        .text(", status ")
        .number(static_cast<uint32_t>(status))
        .write();
    return status;
}

Status DeviceContext::bind(const ContextSetup &setup, const CapsetInfo &capset)
{
    AV_SCOPE("DeviceContext::bind");
    Status status = binding_.bind(setup, capset);
    if (status != Status::ok) return refused("binding", status);
    ScopeExit unbind([this] { binding_.unbind(); });
    ScopeExit withdraw([this] {
        ring_.withdraw();
        fences_.withdraw();
        batches_.withdraw();
        staging_.release();
    });
    if (fences_.prepare(binding_.fence_window(), setup.doorbell()) != Status::ok ||
        ring_.prepare(binding_.ring_window()) != Status::ok)
        return refused("windows", Status::invalid_parameter);
    const BlobWindow reply = binding_.reply_window();
    const uint64_t   answer_bytes = reply.length() != 0u ? reply.length() - ReplyHeader::encoded_size : 0u;
    status = staging_.allocate(uint64_t{1} << setup.ring_order(), answer_bytes);
    if (status != Status::ok) return refused("staging", status);
    executor_.configure(setup.sub_api(), capset.gpu().uuid(), setup.options());
    executor_.signals_through(signal_);
    batches_.prepare(reply);
    status = worker_.connect_doorbell(setup.doorbell());
    if (status != Status::ok) return refused("doorbell", status);
    ScopeExit disconnect([this] { worker_.disconnect_doorbell(); });
    status = pulse_.carry(fences_);
    if (status != Status::ok) return refused("pulse", status);
    ScopeExit uncarry([this] { pulse_.drop(fences_); });
    status = worker_.post(WorkItem(&AvRingAttach, nullptr, &ring_));
    if (status != Status::ok) return refused("attach", status);
    uncarry.dismiss();
    disconnect.dismiss();
    withdraw.dismiss();
    unbind.dismiss();
    if (setup.sub_api() == SubApi::d3d) {
        ring_.order_with(ordering_);
        ordering_.add(ring_, worker_);
    }
    recorder_.record_context_setup(id_, setup);
    return Status::ok;
}

const ContextBinding &DeviceContext::binding() const
{
    AV_SCOPE("DeviceContext::binding");
    return binding_;
}

Status DeviceContext::place_memory(uint64_t size, uint32_t request, AvHostMemory &memory)
{
    AV_SCOPE("DeviceContext::place_memory");
    if (!binding_.bound()) return Status::invalid_state;
    return executor_.place(size, request, memory);
}

uint32_t DeviceContext::id() const
{
    AV_SCOPE("DeviceContext::id");
    return id_;
}

bool DeviceContext::running() const
{
    AV_SCOPE("DeviceContext::running");
    return worker_.running();
}

void DeviceContext::report(StateWriter &writer)
{
    AV_SCOPE("DeviceContext::report");
    writer.open("context", id_);
    writer.text("name", label_.text());
    writer.flag("bound", binding_.bound());
    if (binding_.bound()) {
        char       version[24];
        TextBuffer text(version, sizeof version);
        text.append_decimal(binding_.version().major_version());
        text.append(".");
        text.append_decimal(binding_.version().minor_version());
        writer.number("sub_api", static_cast<uint32_t>(binding_.sub_api()));
        writer.text("version", text.text());
        writer.number("ring", binding_.ring_resource());
        writer.number("ring_order", binding_.ring_order());
        writer.number("fence", binding_.fence_resource());
        writer.number("doorbell", binding_.doorbell());
        writer.number("reply", binding_.reply_resource());
    }
    writer.flag("attached", ring_.attached());
    writer.flag("faulted", ring_.faulted());
    writer.number("head", ring_.head());
    writer.number("last_fence", ring_.last_fence());
    writer.number("batches", ring_.batches());
    writer.flag("executor", executor_.created());
    worker_.report(writer);
    writer.close();
}

}
