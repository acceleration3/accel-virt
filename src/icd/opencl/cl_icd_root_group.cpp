#include "icd/opencl/cl_icd_root.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/sliced_wait.hpp"
#include "icd/opencl/cl_context_group.hpp"
#include "icd/opencl/cl_kernel_args.hpp"
#include "icd/opencl/cl_mem_flags.hpp"
#include "icd/opencl/cl_replicas.hpp"
#include "icd/opencl/cl_transfer_chunks.hpp"
#include "icd/opencl/unanswered_transport.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

namespace {

constexpr uint32_t migration_chunk_words = 131072u;
constexpr uint64_t migration_chunk_bytes = uint64_t{migration_chunk_words} * sizeof(uint64_t);

}

cl_int ClIcdRoot::group_devices(cl_uint num_devices, const cl_device_id *devices, ClDevice **members)
{
    AV_SCOPE("ClIcdRoot::group_devices");
    if (num_devices > ClContextGroup::most) return CL_INVALID_VALUE;
    for (cl_uint index = 0u; index < num_devices; ++index) {
        ClObject *const named = judged(devices[index], ClObject::Kind::device);
        ClDevice *const device = named != nullptr ? devices_.of(*named) : nullptr;
        if (device == nullptr) return CL_INVALID_DEVICE;
        for (cl_uint other = 0u; other < index; ++other)
            if (members[other] == device) return CL_INVALID_VALUE;
        members[index] = device;
    }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::open_session(ClDevice &device, uint32_t gl_share, const cl_context_properties *properties,
                               const ClContextNotify &notify, GuestClContext *&session, uint64_t &host_context)
{
    AV_SCOPE("ClIcdRoot::open_session");
    session = nullptr;
    host_context = 0u;
    GuestClContext *context = nullptr;
    Status          made =
        contexts_.create(context, device.connection(), memory_, send_lock_, blocks_lock_, lines_, shared_handles_);
    if (made == Status::ok) made = context->make(gl_share, watcher_.wake_handle());
    if (made == Status::ok) context->keep_properties(properties);
    if (made == Status::ok) context->notify_with(notify);
    if (made != Status::ok) {
        const bool exhausted = context != nullptr && context->exhausted();
        if (context != nullptr) static_cast<void>(contexts_.destroy(context));
        return exhausted                   ? CL_OUT_OF_RESOURCES
               : made == Status::no_memory ? CL_OUT_OF_HOST_MEMORY
               : gl_share != 0u            ? CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR
                                           : CL_DEVICE_NOT_AVAILABLE;
    }
    ClientTransport *const wire = context->wire();
    uint64_t               host_platform = 0u;
    cl_uint                platforms = 0u;
    uint64_t               host_device = 0u;
    cl_uint                found = 0u;
    cl_int                 host_error = CL_SUCCESS;
    uint64_t               created = 0u;
    if (wire != nullptr && av::clproto::av_clGetPlatformIDs(*wire, 1u, &host_platform, &platforms) == CL_SUCCESS &&
        av::clproto::av_clGetDeviceIDs(*wire, host_platform, CL_DEVICE_TYPE_GPU, 1u, &host_device, &found) ==
            CL_SUCCESS) {
        created = av::clproto::av_clCreateContext(*wire, nullptr, 1u, &host_device, &host_error);
    }
    if (created == 0u) {
        const bool exhausted = context->exhausted();
        static_cast<void>(contexts_.destroy(context));
        return exhausted                  ? CL_OUT_OF_RESOURCES
               : host_error != CL_SUCCESS ? host_error
                                          : CL_DEVICE_NOT_AVAILABLE;
    }
    session = context;
    host_context = created;
    return CL_SUCCESS;
}

void ClIcdRoot::retire_group(ClContextGroup &group)
{
    AV_SCOPE("ClIcdRoot::retire_group");
    GuestClContext *const primary = group.owner().context();
    for (uint32_t index = 0u; index < group.count(); ++index) {
        ClContextGroup::Member &member = group.at(index);
        ClientTransport *const  wire = member.session->wire();
        if (wire != nullptr) {
            if (member.copy_queue != 0u) give_back_to_host(*wire, ClObject::Kind::command_queue, member.copy_queue);
            if (member.done_event != 0u) give_back_to_host(*wire, ClObject::Kind::event, member.done_event);
        }
        member.session->join(nullptr);
        if (member.session == primary) continue;
        watcher_.forget_context(wire);
        static_cast<void>(contexts_.destroy(member.session));
    }
    static_cast<void>(groups_.destroy(&group));
}

ClContextGroup::Member *ClIcdRoot::member_of(ClObject &context, ClObject *device)
{
    AV_SCOPE("ClIcdRoot::member_of");
    GuestClContext *const primary = context.context();
    ClContextGroup *const group = primary != nullptr ? primary->group() : nullptr;
    ClDevice *const       named = device != nullptr ? devices_.of(*device) : nullptr;
    if (named == nullptr) return nullptr;
    return group != nullptr ? group->of(named) : nullptr;
}

uint64_t ClIcdRoot::copy_queue_of(ClContextGroup::Member &member)
{
    AV_SCOPE("ClIcdRoot::copy_queue_of");
    const ThreadLockHold locked(lock_);
    if (member.copy_queue != 0u) return member.copy_queue;
    ClientTransport *const wire = member.session->wire();
    ClObject *const        device = member.device->object();
    if (wire == nullptr || device == nullptr) return 0u;
    cl_int host_error = CL_SUCCESS;
    member.copy_queue = av::clproto::av_clCreateCommandQueueWithProperties(*wire, member.host_context, device->wire(),
                                                                          nullptr, &host_error);
    return member.copy_queue;
}

uint64_t ClIcdRoot::done_event_of(ClContextGroup::Member &member)
{
    AV_SCOPE("ClIcdRoot::done_event_of");
    const ThreadLockHold locked(lock_);
    if (member.done_event != 0u) return member.done_event;
    ClientTransport *const wire = member.session->wire();
    if (wire == nullptr) return 0u;
    cl_int         host_error = CL_SUCCESS;
    const uint64_t made = av::clproto::av_clCreateUserEvent(*wire, member.host_context, &host_error);
    if (made == 0u) return 0u;
    if (av::clproto::av_clSetUserEventStatus(*wire, made, CL_COMPLETE) != CL_SUCCESS) {
        give_back_to_host(*wire, ClObject::Kind::event, made);
        return 0u;
    }
    member.done_event = made;
    return made;
}

cl_int ClIcdRoot::bring(ClObject &queue, ClObject *first, ClObject *second)
{
    AV_SCOPE("ClIcdRoot::bring");
    GuestClContext *const target = queue.context();
    if (target == nullptr || target->group() == nullptr) return CL_SUCCESS;
    ClObject *const named[2] = {first, second};
    for (ClObject *const object : named) {
        if (object == nullptr || object->kind() != ClObject::Kind::mem) continue;
        const cl_int brought = bring_mem(queue, *object);
        if (brought != CL_SUCCESS) return brought;
    }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::bring_mem(ClObject &queue, ClObject &mem)
{
    AV_SCOPE("ClIcdRoot::bring_mem");
    GuestClContext *const target = queue.context();
    GuestClContext *const source = mem.context();
    if (source == nullptr || source->group() != target->group()) return CL_SUCCESS;
    if (source != target) {
        const cl_int moved = migrate_mem(mem, *source, *target);
        if (moved != CL_SUCCESS) return moved;
    }
    mem.note_use(queue.wire());
    return CL_SUCCESS;
}

cl_int ClIcdRoot::migrate_mem(ClObject &mem, GuestClContext &source, GuestClContext &target)
{
    AV_SCOPE("ClIcdRoot::migrate_mem");
    ClContextGroup *const   group = source.group();
    ClContextGroup::Member *const from = group->of(&source);
    ClContextGroup::Member *const to = group->of(&target);
    if (from == nullptr || to == nullptr) return CL_INVALID_CONTEXT;
    if (mem.extent().shape() != ClMemExtent::Shape::buffer || mem.sliced_from() != nullptr || mem.backed() ||
        mem.lent() != nullptr || mem.application_memory() != nullptr || mem.companion() != nullptr ||
        (mem.traits() & ClObject::mem_from_gl) != 0u)
        return CL_INVALID_OPERATION;
    ClientTransport *const wire_from = source.wire();
    ClientTransport *const wire_to = target.wire();
    if (wire_from == nullptr || wire_to == nullptr) return CL_OUT_OF_RESOURCES;
    const uint64_t bytes = mem.extent().bytes();
    if (bytes > ~size_t{0}) return CL_MEM_OBJECT_ALLOCATION_FAILURE;
    const bool     touched = (mem.traits() & ClObject::mem_touched) != 0u;
    uint64_t       read_queue = 0u;
    if (touched) {
        for (uint32_t pass = 0u; pass < ClPendingReads::max_reads; ++pass) {
            uint64_t event = 0u;
            {
                const ThreadLockHold locked(lock_);
                for (uint32_t slot = 0u; slot < ClPendingReads::max_reads && event == 0u; ++slot) {
                    const ClPendingRead *const row = reads_.at(slot);
                    if (row->held() && row->buffer() == mem.wire()) event = row->event();
                }
            }
            if (event == 0u) break;
            static_cast<void>(deliver_reads(*wire_from, source.staging(), &event, 1u));
        }
        const uint64_t last = mem.last_queue();
        if (last != 0u && drain(*wire_from, last, source.wakes(), nullptr, 0u) == CL_SUCCESS) {
            static_cast<void>(deliver_queued(*wire_from, source.staging(), last));
            read_queue = last;
        } else {
            read_queue = copy_queue_of(*from);
        }
        if (read_queue == 0u) return CL_OUT_OF_RESOURCES;
    }
    const cl_mem_flags host_flags = ClMemFlags(static_cast<cl_mem_flags>(mem.flags())).for_host() &
                                    ~static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR);
    cl_int         host_error = CL_SUCCESS;
    const uint64_t fresh =
        av::clproto::av_clCreateBuffer(*wire_to, to->host_context, host_flags, static_cast<size_t>(bytes), nullptr, &host_error);
    if (fresh == 0u) return host_error != CL_SUCCESS ? host_error : CL_MEM_OBJECT_ALLOCATION_FAILURE;
    if (touched) {
        const uint64_t write_queue = copy_queue_of(*to);
        ClHostNumbers  scratch_room(memory_);
        uint64_t *const scratch = write_queue != 0u ? scratch_room.room(migration_chunk_words) : nullptr;
        cl_int         copied = scratch != nullptr ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
        for (uint64_t offset = 0u; offset < bytes && copied == CL_SUCCESS; offset += migration_chunk_bytes) {
            const uint64_t   piece = bytes - offset < migration_chunk_bytes ? bytes - offset : migration_chunk_bytes;
            ClTransferChunks reader(*wire_from, read_queue, source.staging());
            copied = reader.read(mem.wire(), static_cast<size_t>(offset), static_cast<size_t>(piece), scratch);
            if (copied != CL_SUCCESS) break;
            ClTransferChunks writer(*wire_to, write_queue, target.staging());
            copied = writer.write(fresh, static_cast<size_t>(offset), static_cast<size_t>(piece), scratch);
        }
        if (copied != CL_SUCCESS) {
            give_back_to_host(*wire_to, ClObject::Kind::mem, fresh);
            return copied;
        }
    }
    give_back_to_host(*wire_from, ClObject::Kind::mem, mem.wire());
    mem.rehome(&target, fresh);
    return CL_SUCCESS;
}

cl_int ClIcdRoot::bring_kernel(ClObject &queue, ClObject &kernel)
{
    AV_SCOPE("ClIcdRoot::bring_kernel");
    GuestClContext *const target = queue.context();
    if (target == nullptr || target->group() == nullptr || kernel.args() == nullptr) return CL_SUCCESS;
    GuestClContext *const source = kernel.context();
    if (source == nullptr || source->group() != target->group()) return CL_SUCCESS;
    ClientTransport *const wire = target->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    if (source != target) {
        ClObject *const program = kernel.parent();
        ClientTransport *const wire_from = source->wire();
        if (program == nullptr || wire_from == nullptr) return CL_INVALID_KERNEL;
        const uint64_t replica = program->context() == target ? program->wire()
                                 : program->replicas() != nullptr ? program->replicas()->in(target)
                                                                   : 0u;
        if (replica == 0u) return CL_INVALID_PROGRAM_EXECUTABLE;
        char   name[256] = {};
        size_t named = 0u;
        const cl_int asked = av::clproto::av_clGetKernelInfo(*wire_from, kernel.wire(), CL_KERNEL_FUNCTION_NAME,
                                                             sizeof name - 1u, name, &named);
        if (asked != CL_SUCCESS) return asked;
        cl_int         host_error = CL_SUCCESS;
        const uint64_t fresh = av::clproto::av_clCreateKernel(*wire, replica, name, &host_error);
        if (fresh == 0u) return host_error != CL_SUCCESS ? host_error : CL_INVALID_PROGRAM_EXECUTABLE;
        give_back_to_host(*wire_from, ClObject::Kind::kernel, kernel.wire());
        kernel.rehome(target, fresh);
    }
    icd::UnansweredTransport unanswered(*wire);
    const ClKernelArgs      &args = *kernel.args();
    for (uint32_t position = 0u; position < args.count(); ++position) {
        const ClKernelArgs::Arg &arg = args.at(position);
        cl_int                   sent = CL_SUCCESS;
        if (arg.kind == ClKernelArgs::Kind::value) {
            sent = av::clproto::av_clSetKernelArg(unanswered, kernel.wire(), arg.index, arg.bytes, args.bytes_of(arg));
        } else if (arg.kind == ClKernelArgs::Kind::local) {
            sent = av::clproto::av_clSetKernelArg(unanswered, kernel.wire(), arg.index, arg.bytes, nullptr);
        } else {
            ClObject *object = judged(arg.object, ClObject::Kind::mem);
            uint64_t  handle = 0u;
            if (object != nullptr) {
                const cl_int brought = bring_mem(queue, *object);
                if (brought != CL_SUCCESS) return brought;
                handle = object->wire();
            } else {
                object = judged(arg.object, ClObject::Kind::sampler);
                if (object == nullptr) return CL_INVALID_ARG_VALUE;
                handle = object->context() == target ? object->wire()
                         : object->replicas() != nullptr ? object->replicas()->in(target)
                                                         : 0u;
                if (handle == 0u) return CL_INVALID_ARG_VALUE;
            }
            sent = av::clproto::av_clSetKernelArg(unanswered, kernel.wire(), arg.index, sizeof handle, &handle);
        }
        if (sent != CL_SUCCESS) return sent;
    }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::foreign_event(ClObject &queue, ClObject &event, uint64_t &handle)
{
    AV_SCOPE("ClIcdRoot::foreign_event");
    GuestClContext *const target = queue.context();
    GuestClContext *const source = event.context();
    if (source == nullptr || target == nullptr || source->group() == nullptr || source->group() != target->group())
        return CL_INVALID_EVENT_WAIT_LIST;
    const uint64_t replica = event.replicas() != nullptr ? event.replicas()->in(target) : 0u;
    if (replica != 0u) {
        handle = replica;
        return CL_SUCCESS;
    }
    ClContextGroup::Member *const to = target->group()->of(target);
    ClientTransport *const        wire = source->wire();
    const uint64_t                standing = to != nullptr ? done_event_of(*to) : 0u;
    if (wire == nullptr || standing == 0u) return CL_OUT_OF_RESOURCES;
    const uint64_t waited[1] = {event.wire()};
    const cl_int   reached = waits_.until_complete(*wire, waited, 1u, icd::SlicedWait::forever, source->wakes());
    if (reached != CL_SUCCESS) return reached;
    cl_int status = CL_COMPLETE;
    if (av::clproto::av_clGetEventInfo(*wire, event.wire(), CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status,
                                       nullptr) == CL_SUCCESS &&
        status < 0)
        return CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
    handle = standing;
    return CL_SUCCESS;
}

namespace {

struct ProgramSource {
    cl_uint        count;
    const char   **sources;
    const size_t  *lengths;
};

uint64_t make_program(ClientTransport &wire, uint64_t host_context, const void *what, cl_int &error)
{
    AV_SCOPE("make_program");
    const ProgramSource &text = *static_cast<const ProgramSource *>(what);
    return av::clproto::av_clCreateProgramWithSource(wire, host_context, text.count, text.sources, text.lengths,
                                                     &error);
}

uint64_t make_sampler(ClientTransport &wire, uint64_t host_context, const void *what, cl_int &error)
{
    AV_SCOPE("make_sampler");
    return av::clproto::av_clCreateSamplerWithProperties(wire, host_context, static_cast<const uint64_t *>(what),
                                                         &error);
}

uint64_t make_user_event(ClientTransport &wire, uint64_t host_context, const void *, cl_int &error)
{
    AV_SCOPE("make_user_event");
    return av::clproto::av_clCreateUserEvent(wire, host_context, &error);
}

}

cl_int ClIcdRoot::replicate_to_peers(ClObject &object, ClObject::Kind kind, ReplicaMaker make, const void *what)
{
    AV_SCOPE("ClIcdRoot::replicate_to_peers");
    ClContextGroup *const group = object.context()->group();
    if (group == nullptr) return CL_SUCCESS;
    ClReplicas *replicas = nullptr;
    if (replicas_.create(replicas) != Status::ok) return CL_OUT_OF_HOST_MEMORY;
    cl_int failed = CL_SUCCESS;
    for (uint32_t index = 0u; index < group->count() && failed == CL_SUCCESS; ++index) {
        ClContextGroup::Member &member = group->at(index);
        if (member.session == object.context()) continue;
        ClientTransport *const wire = member.session->wire();
        cl_int                 host_error = CL_SUCCESS;
        const uint64_t         made = wire != nullptr ? make(*wire, member.host_context, what, host_error) : 0u;
        if (made == 0u) {
            failed = host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES;
            break;
        }
        if (!replicas->add(*member.session, made)) {
            give_back_to_host(*wire, kind, made);
            failed = CL_OUT_OF_HOST_MEMORY;
        }
    }
    if (failed != CL_SUCCESS) {
        release_replicas(*replicas, kind);
        static_cast<void>(replicas_.destroy(replicas));
        return failed;
    }
    object.keep_replicas(replicas);
    return CL_SUCCESS;
}

cl_int ClIcdRoot::replicate_program(ClObject &program, cl_uint count, const char **sources, const size_t *lengths)
{
    AV_SCOPE("ClIcdRoot::replicate_program");
    const ProgramSource text = {count, sources, lengths};
    return replicate_to_peers(program, ClObject::Kind::program, &make_program, &text);
}

cl_int ClIcdRoot::replicate_sampler(ClObject &sampler, const uint64_t *properties)
{
    AV_SCOPE("ClIcdRoot::replicate_sampler");
    return replicate_to_peers(sampler, ClObject::Kind::sampler, &make_sampler, properties);
}

cl_int ClIcdRoot::replicate_user_event(ClObject &event)
{
    AV_SCOPE("ClIcdRoot::replicate_user_event");
    return replicate_to_peers(event, ClObject::Kind::event, &make_user_event, nullptr);
}

void ClIcdRoot::release_replicas(ClReplicas &replicas, ClObject::Kind kind)
{
    AV_SCOPE("ClIcdRoot::release_replicas");
    for (uint32_t index = 0u; index < replicas.count(); ++index) {
        const ClReplicas::Entry &entry = replicas.at(index);
        ClientTransport *const   wire = entry.session->wire();
        if (wire != nullptr) give_back_to_host(*wire, kind, entry.wire);
    }
}

}
