#include "icd/opencl/cl_icd_root.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/sliced_wait.hpp"
#include "icd/opencl/cl_image_region.hpp"
#include "icd/opencl/cl_transfer_chunks.hpp"
#include "icd/opencl/mapped_region.hpp"
#include "icd/opencl/unanswered_transport.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

namespace {

void report_error(cl_int *error, cl_int value)
{
    AV_SCOPE("report_error");
    if (error != nullptr) *error = value;
}

constexpr size_t fill_colour_bytes = 16u;

}

cl_int ClIcdRoot::settle(ClientTransport &wire, ClObject &queue, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::settle");
    GuestClContext *const context = queue.context();
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*context, waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    const uint32_t kept = waits.blocks() ? waits.count() : without_unset_user_events(wire, waits, listed);
    if (kept == 0u) return drain_unless_idle(wire, queue);
    const cl_int drained = drain(wire, queue.wire(), context->wakes(), listed, kept);
    if (drained == CL_SUCCESS) drained_queue(queue);
    return drained;
}

uint32_t ClIcdRoot::without_unset_user_events(ClientTransport &wire, const ClWaitList &waits, uint64_t *listed)
{
    AV_SCOPE("ClIcdRoot::without_unset_user_events");
    uint32_t kept = 0u;
    for (cl_uint index = 0u; index < waits.count(); ++index) {
        ClObject *const named = judged(waits.events()[index], ClObject::Kind::event);
        const bool      user = named != nullptr && named->parent() != nullptr &&
                          named->parent()->kind() == ClObject::Kind::context;
        cl_int status = CL_COMPLETE;
        if (user && av::clproto::av_clGetEventInfo(wire, listed[index], CL_EVENT_COMMAND_EXECUTION_STATUS,
                                                   sizeof status, &status, nullptr) == CL_SUCCESS &&
            status > CL_COMPLETE) {
            refusals_.note_ran_ahead_of_user_event();
            continue;
        }
        listed[kept++] = listed[index];
    }
    return kept;
}

void ClIcdRoot::hand_over(ClientTransport &wire, ClObject &queue, uint64_t produced, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::hand_over");
    if (event != nullptr) {
        *event = static_cast<cl_event>(wrap(wire, ClObject::Kind::event, produced, queue.context(), &queue, nullptr));
        return;
    }
    static_cast<void>(av::clproto::av_clReleaseEvent(wire, produced));
}

cl_int ClIcdRoot::enqueue_task(cl_command_queue queue, cl_kernel kernel, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_task");
    const size_t one = 1u;
    return enqueue_kernel(queue, kernel, 1u, nullptr, &one, &one, event, waits);
}

cl_int ClIcdRoot::enqueue_event_marker(cl_command_queue queue, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_event_marker");
    if (judged(queue, ClObject::Kind::command_queue) == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (event == nullptr) return CL_INVALID_VALUE;
    return enqueue_marker(queue, 0u, nullptr, event);
}

cl_int ClIcdRoot::enqueue_wait_for_events(cl_command_queue queue, cl_uint count, const cl_event *events)
{
    AV_SCOPE("ClIcdRoot::enqueue_wait_for_events");
    if (judged(queue, ClObject::Kind::command_queue) == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (count == 0u || events == nullptr) return CL_INVALID_VALUE;
    return enqueue_barrier(queue, count, events, nullptr);
}

cl_int ClIcdRoot::enqueue_copy_image_to_buffer(cl_command_queue queue, cl_mem image, cl_mem buffer,
                                               const size_t *origin, const size_t *region, size_t offset,
                                               cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_copy_image_to_buffer");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const source = judged(image, ClObject::Kind::mem);
    ClObject *const target = judged(buffer, ClObject::Kind::mem);
    if (source == nullptr || target == nullptr) return CL_INVALID_MEM_OBJECT;
    if (source->context() != held->context() || target->context() != held->context()) return CL_INVALID_CONTEXT;
    if (origin == nullptr || region == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t                 produced = 0u;
    icd::UnansweredTransport unanswered(*wire);
    const cl_int             copied = av::clproto::av_clEnqueueCopyImageToBuffer(
        event != nullptr ? *wire : unanswered, held->wire(), source->wire(), target->wire(), origin, region, offset,
        waits.count(), waits.count() != 0u ? listed : nullptr, event != nullptr ? &produced : nullptr);
    if (copied == CL_SUCCESS && event != nullptr) hand_over(*wire, *held, produced, event);
    return copied;
}

cl_int ClIcdRoot::enqueue_copy_buffer_to_image(cl_command_queue queue, cl_mem buffer, cl_mem image, size_t offset,
                                               const size_t *origin, const size_t *region, cl_event *event,
                                               const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_copy_buffer_to_image");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const source = judged(buffer, ClObject::Kind::mem);
    ClObject *const target = judged(image, ClObject::Kind::mem);
    if (source == nullptr || target == nullptr) return CL_INVALID_MEM_OBJECT;
    if (source->context() != held->context() || target->context() != held->context()) return CL_INVALID_CONTEXT;
    if (origin == nullptr || region == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t                 produced = 0u;
    icd::UnansweredTransport unanswered(*wire);
    const cl_int             copied = av::clproto::av_clEnqueueCopyBufferToImage(
        event != nullptr ? *wire : unanswered, held->wire(), source->wire(), target->wire(), offset, origin, region,
        waits.count(), waits.count() != 0u ? listed : nullptr, event != nullptr ? &produced : nullptr);
    if (copied == CL_SUCCESS && event != nullptr) hand_over(*wire, *held, produced, event);
    return copied;
}

cl_int ClIcdRoot::enqueue_fill_image(cl_command_queue queue, cl_mem image, const void *colour, const size_t *origin,
                                     const size_t *region, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_fill_image");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(image, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (named->context() != held->context()) return CL_INVALID_CONTEXT;
    if (colour == nullptr || origin == nullptr || region == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t                 produced = 0u;
    icd::UnansweredTransport unanswered(*wire);
    const cl_int             filled = av::clproto::av_clEnqueueFillImage(
        event != nullptr ? *wire : unanswered, held->wire(), named->wire(), colour, fill_colour_bytes, origin, region,
        waits.count(), waits.count() != 0u ? listed : nullptr, event != nullptr ? &produced : nullptr);
    if (filled == CL_SUCCESS && event != nullptr) hand_over(*wire, *held, produced, event);
    return filled;
}

cl_int ClIcdRoot::enqueue_migrate(cl_command_queue queue, cl_uint count, const cl_mem *objects,
                                  cl_mem_migration_flags flags, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_migrate");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (count == 0u || objects == nullptr) return CL_INVALID_VALUE;
    ClHostNumbers   named_room(memory_);
    uint64_t *const named = named_room.room(count);
    if (named == nullptr) return CL_OUT_OF_HOST_MEMORY;
    for (cl_uint index = 0u; index < count; ++index) {
        ClObject *const object = judged(objects[index], ClObject::Kind::mem);
        if (object == nullptr) return CL_INVALID_MEM_OBJECT;
        if (object->context() != held->context()) return CL_INVALID_CONTEXT;
        named[index] = object->wire();
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t                 produced = 0u;
    icd::UnansweredTransport unanswered(*wire);
    const cl_int             moved = av::clproto::av_clEnqueueMigrateMemObjects(
        event != nullptr ? *wire : unanswered, held->wire(), count, named, flags, waits.count(),
        waits.count() != 0u ? listed : nullptr, event != nullptr ? &produced : nullptr);
    if (moved == CL_SUCCESS && event != nullptr) hand_over(*wire, *held, produced, event);
    return moved;
}

cl_mem ClIcdRoot::create_sub_buffer(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type type, const void *info,
                                    cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_sub_buffer");
    ClObject *const held = judged(buffer, ClObject::Kind::mem);
    if (held == nullptr || held->sliced_from() != nullptr) {
        report_error(error, CL_INVALID_MEM_OBJECT);
        return nullptr;
    }
    if (type != CL_BUFFER_CREATE_TYPE_REGION || info == nullptr) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    const cl_buffer_region region = *static_cast<const cl_buffer_region *>(info);
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t made = av::clproto::av_clCreateSubBuffer(*wire, held->wire(), flags, type, &region, &host_error);
    if (made == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    ClObject *const context = held->nearest(ClObject::Kind::context);
    void *const     wrapped = wrap(*wire, ClObject::Kind::mem, made, held->context(), context, error);
    if (wrapped == nullptr) return nullptr;
    ClObject *const sub = static_cast<ClObject *>(wrapped);
    {
        const ThreadLockHold locked(lock_);
        held->retain();
        sub->slice_of(*held);
    }
    if (held->backed()) sub->back_with(held->backing_offset() + region.origin, region.size);
    if (held->application_memory() != nullptr) sub->use_application_memory(held->application_memory() + region.origin);
    if (held->lent() != nullptr) sub->lend(*held->lent());
    return static_cast<cl_mem>(wrapped);
}

void *ClIcdRoot::map_image(cl_command_queue queue, cl_mem image, cl_bool blocking, cl_map_flags flags,
                           const size_t *origin, const size_t *region, size_t *row_pitch, size_t *slice_pitch,
                           cl_event *event, cl_int *error, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::map_image");
    static_cast<void>(blocking);
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) {
        report_error(error, CL_INVALID_COMMAND_QUEUE);
        return nullptr;
    }
    ClObject *const named = judged(image, ClObject::Kind::mem);
    if (named == nullptr) {
        report_error(error, CL_INVALID_MEM_OBJECT);
        return nullptr;
    }
    if (origin == nullptr || region == nullptr || row_pitch == nullptr) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    GuestClContext *const  context = held->context();
    ClientTransport *const wire = context != nullptr ? context->wire() : nullptr;
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_mem_object_type type = 0u;
    if (host_mem_info(*wire, *named, CL_MEM_TYPE, sizeof type, &type, nullptr) != CL_SUCCESS ||
        type == CL_MEM_OBJECT_BUFFER || type == CL_MEM_OBJECT_PIPE) {
        report_error(error, CL_INVALID_MEM_OBJECT);
        return nullptr;
    }
    const bool sliced = type == CL_MEM_OBJECT_IMAGE3D || type == CL_MEM_OBJECT_IMAGE2D_ARRAY ||
                        type == CL_MEM_OBJECT_IMAGE1D_ARRAY;
    if (sliced && slice_pitch == nullptr) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    const ClImageRegion layout(origin, region, 0u, 0u, element_bytes(*wire, named->wire()));
    if (!layout.valid()) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    const cl_int settled = settle(*wire, *held, waits);
    if (settled != CL_SUCCESS) {
        report_error(error, settled);
        return nullptr;
    }
    MappedRegion *const taken =
        context->take_image_mapping(named->wire(), origin, region, layout.element_bytes(), flags);
    if (taken == nullptr) {
        refusals_.note_map("clEnqueueMapImage: no slot or no staging block for a map", layout.slice_pitch(),
                           context->mappings());
        report_error(error, CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }
    if ((flags & CL_MAP_WRITE_INVALIDATE_REGION) == 0u) {
        ClTransferChunks chunks(*wire, held->wire(), context->staging());
        const cl_int     read = chunks.read_image(named->wire(), layout, taken->at());
        if (read != CL_SUCCESS) {
            context->give_back_mapping(*taken);
            report_error(error, read);
            return nullptr;
        }
    }
    *row_pitch = layout.row_pitch();
    if (slice_pitch != nullptr)
        *slice_pitch = type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? layout.row_pitch() : sliced ? layout.slice_pitch() : 0u;
    uint8_t *const at = taken->at();
    const cl_int   marked = marker_event(*wire, *held, event);
    report_error(error, marked);
    return at;
}

cl_int ClIcdRoot::unmap_image(ClientTransport &wire, ClObject &queue, uint64_t image, MappedRegion &region,
                              cl_event *event)
{
    AV_SCOPE("ClIcdRoot::unmap_image");
    GuestClContext *const context = queue.context();
    cl_int                written = CL_SUCCESS;
    if (region.writes_back()) {
        written = drain_unless_idle(wire, queue);
        if (written == CL_SUCCESS) {
            ClTransferChunks chunks(wire, queue.wire(), context->staging());
            written = chunks.write_image(image, region.image_layout(), region.at());
        }
    }
    context->give_back_mapping(region);
    if (written != CL_SUCCESS) return written;
    return marker_event(wire, queue, event);
}

cl_mem ClIcdRoot::create_image_with_properties(cl_context context, const cl_mem_properties *properties,
                                               cl_mem_flags flags, const cl_image_format *format,
                                               const cl_image_desc *desc, void *host_pointer, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_image_with_properties");
    ClPropertyList list;
    const cl_int   judged_list = judge(properties, nullptr, 0u, list);
    if (judged_list != CL_SUCCESS) {
        report_error(error, judged_list);
        return nullptr;
    }
    return create_image(context, flags, format, desc, host_pointer, error);
}

cl_mem ClIcdRoot::create_pipe(cl_context context, cl_mem_flags flags, cl_uint packet_bytes, cl_uint packets,
                              const cl_pipe_properties *properties, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_pipe");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    if (properties != nullptr && properties[0] != 0) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t made = av::clproto::av_clCreatePipe(*wire, held->wire(), flags, packet_bytes, packets, nullptr,
                                                       &host_error);
    if (made == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return static_cast<cl_mem>(wrap(*wire, ClObject::Kind::mem, made, held->context(), held, error));
}

cl_int ClIcdRoot::pipe_info(cl_mem pipe, cl_pipe_info name, size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::pipe_info");
    ClObject *const held = judged(pipe, ClObject::Kind::mem);
    if (held == nullptr) return CL_INVALID_MEM_OBJECT;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetPipeInfo(*wire, held->wire(), name, value_bytes, value, size_ret);
}

cl_int ClIcdRoot::svm_from_guest(ClientTransport &wire, ClObject &queue, uint64_t to, const void *from, size_t bytes)
{
    AV_SCOPE("ClIcdRoot::svm_from_guest");
    GuestClContext *const context = queue.context();
    ClObject *const       owner = queue.nearest(ClObject::Kind::context);
    cl_int                made = CL_SUCCESS;
    const uint64_t        through =
        av::clproto::av_clCreateBuffer(wire, owner->wire(), CL_MEM_READ_WRITE, bytes, nullptr, &made);
    if (through == 0u) return made != CL_SUCCESS ? made : CL_OUT_OF_RESOURCES;
    ClTransferChunks chunks(wire, queue.wire(), context->staging());
    cl_int           moved = chunks.write(through, 0u, bytes, from);
    if (moved == CL_SUCCESS) {
        cl_int         map_error = CL_SUCCESS;
        const uint64_t mapped = av::clproto::av_clEnqueueMapBuffer(wire, queue.wire(), through, CL_TRUE, CL_MAP_READ,
                                                                   0u, bytes, 0u, nullptr, nullptr, &map_error);
        moved = mapped != 0u ? CL_SUCCESS : (map_error != CL_SUCCESS ? map_error : CL_MAP_FAILURE);
        if (mapped != 0u) {
            moved = av::clproto::av_clEnqueueSVMMemcpy(wire, queue.wire(), CL_FALSE, to, mapped, bytes, 0u, nullptr,
                                                       nullptr);
            const cl_int unmapped = av::clproto::av_clEnqueueUnmapMemObject(wire, queue.wire(), through, mapped, 0u,
                                                                            nullptr, nullptr);
            if (moved == CL_SUCCESS) moved = unmapped;
        }
    }
    give_back_to_host(wire, ClObject::Kind::mem, through);
    queue.busy();
    return moved;
}

cl_int ClIcdRoot::svm_to_guest(ClientTransport &wire, ClObject &queue, void *to, uint64_t from, size_t bytes)
{
    AV_SCOPE("ClIcdRoot::svm_to_guest");
    GuestClContext *const context = queue.context();
    ClObject *const       owner = queue.nearest(ClObject::Kind::context);
    cl_int                made = CL_SUCCESS;
    const uint64_t        through =
        av::clproto::av_clCreateBuffer(wire, owner->wire(), CL_MEM_READ_WRITE, bytes, nullptr, &made);
    if (through == 0u) return made != CL_SUCCESS ? made : CL_OUT_OF_RESOURCES;
    cl_int         map_error = CL_SUCCESS;
    const uint64_t mapped = av::clproto::av_clEnqueueMapBuffer(wire, queue.wire(), through, CL_TRUE,
                                                               CL_MAP_WRITE_INVALIDATE_REGION, 0u, bytes, 0u, nullptr,
                                                               nullptr, &map_error);
    cl_int moved = mapped != 0u ? CL_SUCCESS : (map_error != CL_SUCCESS ? map_error : CL_MAP_FAILURE);
    if (mapped != 0u) {
        moved = av::clproto::av_clEnqueueSVMMemcpy(wire, queue.wire(), CL_FALSE, mapped, from, bytes, 0u, nullptr,
                                                   nullptr);
        const cl_int unmapped =
            av::clproto::av_clEnqueueUnmapMemObject(wire, queue.wire(), through, mapped, 0u, nullptr, nullptr);
        if (moved == CL_SUCCESS) moved = unmapped;
    }
    queue.busy();
    if (moved == CL_SUCCESS) moved = drain_unless_idle(wire, queue);
    if (moved == CL_SUCCESS) {
        ClTransferChunks chunks(wire, queue.wire(), context->staging());
        moved = chunks.read(through, 0u, bytes, to);
    }
    give_back_to_host(wire, ClObject::Kind::mem, through);
    return moved;
}

cl_int ClIcdRoot::svm_memcpy(cl_command_queue queue, cl_bool blocking, void *to, const void *from, size_t bytes,
                             cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::svm_memcpy");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (to == nullptr || from == nullptr) return CL_INVALID_VALUE;
    if (bytes == 0u) return enqueue_marker(queue, waits.count(), waits.events(), event);
    const uintptr_t target = reinterpret_cast<uintptr_t>(to);
    const uintptr_t source = reinterpret_cast<uintptr_t>(from);
    if ((target <= source && source - target < bytes) || (source < target && target - source < bytes))
        return CL_MEM_COPY_OVERLAP;
    GuestClContext *const  context = held->context();
    ClientTransport *const wire = context != nullptr ? context->wire() : nullptr;
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*context, waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    const SvmAllocation *const into_shared = context->svm().covering(to);
    const SvmAllocation *const from_shared = context->svm().covering(from);
    if (into_shared != nullptr && context->svm().covering(static_cast<uint8_t *>(to) + bytes - 1u) != into_shared)
        return CL_INVALID_VALUE;
    if (from_shared != nullptr &&
        context->svm().covering(static_cast<const uint8_t *>(from) + bytes - 1u) != from_shared)
        return CL_INVALID_VALUE;
    if (into_shared != nullptr && from_shared != nullptr) {
        held->busy();
        uint64_t     produced = 0u;
        const bool   wanted = event != nullptr || blocking == CL_TRUE;
        const cl_int copied = av::clproto::av_clEnqueueSVMMemcpy(
            *wire, held->wire(), CL_FALSE, context->svm().host_address_of(to), context->svm().host_address_of(from),
            bytes, waits.count(), waits.count() != 0u ? listed : nullptr, wanted ? &produced : nullptr);
        if (copied != CL_SUCCESS || !wanted) return copied;
        cl_int waited = CL_SUCCESS;
        if (blocking == CL_TRUE) {
            static_cast<void>(av::clproto::av_clFlush(*wire, held->wire()));
            waited = waits_.until_complete(*wire, &produced, 1u, icd::SlicedWait::forever, context->wakes());
        }
        hand_over(*wire, *held, produced, event);
        return waited;
    }
    const cl_int settled = settle(*wire, *held, waits);
    if (settled != CL_SUCCESS) return settled;
    cl_int moved = CL_SUCCESS;
    if (into_shared != nullptr) {
        moved = svm_from_guest(*wire, *held, context->svm().host_address_of(to), from, bytes);
    } else if (from_shared != nullptr) {
        moved = svm_to_guest(*wire, *held, to, context->svm().host_address_of(from), bytes);
    } else {
        __builtin_memcpy(to, from, bytes);
    }
    if (moved != CL_SUCCESS) return moved;
    return marker_event(*wire, *held, event);
}

cl_int ClIcdRoot::svm_mem_fill(cl_command_queue queue, void *pointer, const void *pattern, size_t pattern_bytes,
                               size_t bytes, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::svm_mem_fill");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    GuestClContext *const context = held->context();
    if (context == nullptr) return CL_INVALID_CONTEXT;
    if (pattern == nullptr || pattern_bytes == 0u || bytes == 0u || bytes % pattern_bytes != 0u)
        return CL_INVALID_VALUE;
    const SvmAllocation *const filled = context->svm().covering(pointer);
    if (filled == nullptr || context->svm().covering(static_cast<uint8_t *>(pointer) + bytes - 1u) != filled)
        return CL_INVALID_VALUE;
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*context, waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t                 produced = 0u;
    icd::UnansweredTransport unanswered(*wire);
    const cl_int             done = av::clproto::av_clEnqueueSVMMemFill(
        event != nullptr ? *wire : unanswered, held->wire(), context->svm().host_address_of(pointer), pattern,
        pattern_bytes, bytes, waits.count(), waits.count() != 0u ? listed : nullptr,
        event != nullptr ? &produced : nullptr);
    if (done == CL_SUCCESS && event != nullptr) hand_over(*wire, *held, produced, event);
    return done;
}

cl_int ClIcdRoot::enqueue_svm_free(cl_command_queue queue, cl_uint count, void *pointers[], SvmFreed freed,
                                   void *user_data, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_svm_free");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    GuestClContext *const context = held->context();
    if (context == nullptr) return CL_INVALID_CONTEXT;
    if (count == 0u || pointers == nullptr) return CL_INVALID_VALUE;
    for (cl_uint index = 0u; index < count; ++index) {
        if (pointers[index] == nullptr) return CL_INVALID_VALUE;
    }
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int settled = settle(*wire, *held, waits);
    if (settled != CL_SUCCESS) return settled;
    if (freed != nullptr) {
        freed(static_cast<cl_command_queue>(static_cast<void *>(held)), count, pointers, user_data);
    } else {
        const cl_context owner = reinterpret_cast<cl_context>(held->nearest(ClObject::Kind::context));
        for (cl_uint index = 0u; index < count; ++index) svm_free(owner, pointers[index]);
    }
    return marker_event(*wire, *held, event);
}

cl_int ClIcdRoot::enqueue_svm_migrate(cl_command_queue queue, cl_uint count, const void **pointers,
                                      cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_svm_migrate");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    GuestClContext *const context = held->context();
    if (context == nullptr) return CL_INVALID_CONTEXT;
    if (count == 0u || pointers == nullptr) return CL_INVALID_VALUE;
    for (cl_uint index = 0u; index < count; ++index) {
        if (context->svm().covering(pointers[index]) == nullptr) return CL_INVALID_VALUE;
    }
    return enqueue_marker(queue, waits.count(), waits.events(), event);
}

}
