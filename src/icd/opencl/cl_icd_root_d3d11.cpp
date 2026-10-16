/* ClIcdRoot's cl_khr_d3d11_sharing (docs/d3d11-sharing.md), and the device's
 * retain and release, which an application sharing with Direct3D 11 makes. */
#include "icd/opencl/cl_icd_root.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/sliced_wait.hpp"
#include "icd/opencl/d3d_adapter_source.hpp"
#include "icd/opencl/cl_d3d11_download.hpp"
#include "icd/opencl/cl_d3d11_upload.hpp"
#include "icd/opencl/d3d11_bridge.hpp"
#include "icd/opencl/d3d11_object.hpp"
#include "proto/d3d/dxgi_format.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"

namespace av::clicd {

namespace {

/* The three flags an object over a Direct3D 11 resource may be made with. */
constexpr cl_mem_flags access_flags = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY;

void report_d3d11_error(cl_int *error, cl_int value)
{
    AV_SCOPE("report_d3d11_error");
    if (error != nullptr) *error = value;
}

}  // namespace

cl_int ClIcdRoot::device_reference(cl_device_id device)
{
    AV_SCOPE("ClIcdRoot::device_reference");
    return judged(device, ClObject::Kind::device) != nullptr ? CL_SUCCESS : CL_INVALID_DEVICE;
}

cl_int ClIcdRoot::device_ids_from_d3d11(cl_platform_id platform, cl_uint source, void *object, cl_uint set,
                                        cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices)
{
    AV_SCOPE("ClIcdRoot::device_ids_from_d3d11");
    if (!platform_.names_this(platform)) return CL_INVALID_PLATFORM;
    if ((source != d3d11_device_source && source != d3d11_adapter_source) ||
        (set != d3d11_preferred_devices && set != d3d11_all_devices) ||
        (devices == nullptr && num_devices == nullptr) || (devices != nullptr && num_entries == 0u))
        return CL_INVALID_VALUE;
    /* An ID3D11Device is an IDXGIDevice too, which is what the device's
     * adapter is asked through. */
    const uint64_t luid =
        source == d3d11_device_source ? d3d_adapters_.device_luid(object) : d3d_adapters_.adapter_luid(object);
    return device_ids_for_luid(luid, devices, num_devices);
}

cl_int ClIcdRoot::d3d11_device_for(const ClPropertyList &list, const ClDevice &on, void *&device)
{
    AV_SCOPE("ClIcdRoot::d3d11_device_for");
    device = nullptr;
    /* A null device names none, which the extension allows. */
    uint64_t named = 0u;
    if (!list.names(static_cast<uint64_t>(d3d11_context_device), named) || named == 0u) return CL_SUCCESS;
    void *const candidate = reinterpret_cast<void *>(static_cast<uintptr_t>(named));
    if (!d3d11_.device(candidate)) return invalid_d3d11_device;
    /* A device on another adapter is one whose resources this device cannot
     * copy: its commands run on another GPU. */
    if (d3d_adapters_.device_luid(candidate) != on.luid()) return invalid_d3d11_device;
    device = candidate;
    return CL_SUCCESS;
}

cl_mem ClIcdRoot::create_from_d3d11_buffer(cl_context context, cl_mem_flags flags, void *resource, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_from_d3d11_buffer");
    GuestClContext *owner = nullptr;
    D3d11Resource   described;
    const cl_int    judged_resource =
        d3d11_resource(context, flags, resource, 0u, D3d11Resource::Kind::buffer, owner, described);
    if (judged_resource != CL_SUCCESS) {
        report_d3d11_error(error, judged_resource);
        return nullptr;
    }
    cl_int       made_error = CL_SUCCESS;
    const cl_mem made =
        create_buffer(context, flags, static_cast<size_t>(described.bytes()), nullptr, nullptr, &made_error);
    if (made == nullptr) {
        report_d3d11_error(error, made_error);
        return nullptr;
    }
    return d3d11_object(made, *owner, resource, 0u, described, flags, error);
}

cl_mem ClIcdRoot::create_from_d3d11_texture(cl_context context, cl_mem_flags flags, void *resource,
                                            cl_uint subresource, D3d11Resource::Kind kind, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_from_d3d11_texture");
    GuestClContext *owner = nullptr;
    D3d11Resource   described;
    const cl_int    judged_resource = d3d11_resource(context, flags, resource, subresource, kind, owner, described);
    if (judged_resource != CL_SUCCESS) {
        report_d3d11_error(error, judged_resource);
        return nullptr;
    }
    const DxgiFormat mapped(described.format());
    if (mapped.cl_order() == 0u) {
        report_d3d11_error(error, CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
        return nullptr;
    }
    void        *twin = nullptr;
    /* A twin is opened on the host as the image it is only where the host
     * views its format as one; any other is opened as bytes there. */
    const cl_mem twinned = kind == D3d11Resource::Kind::texture2d && mapped.cl_image_on_host()
                               ? d3d11_twin(context, *owner, resource, subresource, described, twin)
                               : nullptr;
    if (twinned != nullptr) {
        const cl_mem kept = d3d11_object(twinned, *owner, resource, subresource, described, flags, error);
        if (kept == nullptr) {
            d3d11_.let_go(twin);
            return nullptr;
        }
        lock_.acquire();
        owner->d3d11().find(kept)->pair(twin);
        lock_.release();
        return kept;
    }
    const cl_image_format format = {static_cast<cl_channel_order>(mapped.cl_order()),
                                     static_cast<cl_channel_type>(mapped.cl_type())};
    cl_image_desc         desc = {};
    desc.image_type = kind == D3d11Resource::Kind::texture3d ? CL_MEM_OBJECT_IMAGE3D : CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = described.width_of(subresource);
    desc.image_height = described.height_of(subresource);
    desc.image_depth = kind == D3d11Resource::Kind::texture3d ? described.depth_of(subresource) : 0u;
    cl_int       made_error = CL_SUCCESS;
    const cl_mem made = create_image(context, flags, &format, &desc, nullptr, &made_error);
    if (made == nullptr) {
        report_d3d11_error(error, made_error);
        return nullptr;
    }
    return d3d11_object(made, *owner, resource, subresource, described, flags, error);
}

/* The size the open names is the one avumd published the twin as: its tight
 * rows, whole pages (KernelSurface). */
cl_mem ClIcdRoot::d3d11_twin(cl_context context, GuestClContext &owner, void *resource, uint32_t subresource,
                             const D3d11Resource &described, void *&twin)
{
    AV_SCOPE("ClIcdRoot::d3d11_twin");
    twin = nullptr;
    uint64_t    handle = 0u;
    void *const made = d3d11_.twin(owner.d3d11().device(), resource, subresource, handle);
    if (made == nullptr) return nullptr;
    ClObject *const        held = judged(context, ClObject::Kind::context);
    ClientTransport *const wire = held != nullptr ? wire_of(*held) : nullptr;
    const DxgiFormatLayout layout(described.format());
    const uint64_t         bytes =
        uint64_t{described.width_of(subresource)} * layout.element_bytes() * described.height_of(subresource);
    uint64_t     opened = 0u;
    bool         closed = false;
    const cl_int taken = wire != nullptr ? owner.external().open_own(*wire, owner.guest_device(), handle, bytes,
                                                                     opened, closed)
                                         : CL_OUT_OF_RESOURCES;
    if (!closed) d3d11_.close_handle(handle);
    ClObject *object = nullptr;
    if (taken == CL_SUCCESS && objects_.make(ClObject::Kind::mem, opened, &owner, held, object) != Status::ok) {
        static_cast<void>(owner.external().give_back(*wire, opened));
        object = nullptr;
    }
    if (object == nullptr) {
        d3d11_.let_go(made);
        return nullptr;
    }
    twin = made;
    return static_cast<cl_mem>(static_cast<void *>(object));
}

cl_int ClIcdRoot::d3d11_resource(cl_context context, cl_mem_flags flags, void *resource, uint32_t subresource,
                                 D3d11Resource::Kind kind, GuestClContext *&owner, D3d11Resource &described)
{
    AV_SCOPE("ClIcdRoot::d3d11_resource");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) return CL_INVALID_CONTEXT;
    /* Every object of a context has its context. */
    owner = context_of(*held);
    if (owner->d3d11().device() == nullptr) return CL_INVALID_CONTEXT;
    if ((flags & ~access_flags) != 0u) return CL_INVALID_VALUE;
    if (!d3d11_.describe(resource, owner->d3d11().device(), described) || described.kind() != kind ||
        described.multisampled())
        return invalid_d3d11_resource;
    if (!described.holds(subresource)) return CL_INVALID_VALUE;
    lock_.acquire();
    const bool made = owner->d3d11().made_over(resource, subresource);
    lock_.release();
    return made ? invalid_d3d11_resource : CL_SUCCESS;
}

cl_mem ClIcdRoot::d3d11_object(cl_mem made, GuestClContext &owner, void *resource, uint32_t subresource,
                               const D3d11Resource &described, cl_mem_flags flags, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::d3d11_object");
    lock_.acquire();
    const Status kept = owner.d3d11().keep(made, resource, subresource, described, flags);
    lock_.release();
    if (kept != Status::ok) {
        static_cast<void>(release(made, ClObject::Kind::mem));
        report_d3d11_error(error, CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }
    d3d11_.hold(resource);
    report_d3d11_error(error, CL_SUCCESS);
    return made;
}

cl_int ClIcdRoot::enqueue_acquire_d3d11(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                        const cl_event *events, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_acquire_d3d11");
    ClObject        *held = nullptr;
    GuestClContext  *context = nullptr;
    ClientTransport *wire = nullptr;
    const cl_int     ready = d3d11_queue(queue, waits, events, held, context, wire);
    if (ready != CL_SUCCESS) return ready;
    lock_.acquire();
    const cl_int taken = context->d3d11().take(objects, count);
    lock_.release();
    if (taken != CL_SUCCESS) return taken;
    cl_int done = d3d11_copy_in(queue, *context, objects, count);
    if (done == CL_SUCCESS) done = marker_event(*wire, *held, event);
    /* An acquire that failed leaves every object Direct3D's, to be acquired
     * again. */
    if (done != CL_SUCCESS) {
        lock_.acquire();
        context->d3d11().untake(objects, count);
        lock_.release();
    }
    return done;
}

cl_int ClIcdRoot::enqueue_release_d3d11(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                        const cl_event *events, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_release_d3d11");
    ClObject        *held = nullptr;
    GuestClContext  *context = nullptr;
    ClientTransport *wire = nullptr;
    const cl_int     ready = d3d11_queue(queue, waits, events, held, context, wire);
    if (ready != CL_SUCCESS) return ready;
    lock_.acquire();
    const cl_int given = context->d3d11().give(objects, count);
    lock_.release();
    if (given != CL_SUCCESS) return given;
    /* Every command that used the objects has run before a byte is copied
     * back: the copy is what Direct3D sees next. */
    cl_int done = finish(queue);
    if (done == CL_SUCCESS) done = d3d11_copy_out(queue, *context, objects, count);
    if (done == CL_SUCCESS) done = marker_event(*wire, *held, event);
    /* A release that failed leaves every object OpenCL's, to be released
     * again. */
    if (done != CL_SUCCESS) {
        lock_.acquire();
        context->d3d11().ungive(objects, count);
        lock_.release();
    }
    return done;
}

cl_int ClIcdRoot::d3d11_queue(cl_command_queue queue, cl_uint waits, const cl_event *events, ClObject *&held,
                              GuestClContext *&context, ClientTransport *&wire)
{
    AV_SCOPE("ClIcdRoot::d3d11_queue");
    held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    /* Every object of a context has its context. */
    context = context_of(*held);
    if (context->d3d11().device() == nullptr) return CL_INVALID_CONTEXT;
    ClHostNumbers handles_room(memory_);
    uint64_t     *handles = nullptr;
    const cl_int listed = wait_list(*context, waits, events, handles_room, handles);
    if (listed != CL_SUCCESS) return listed;
    wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    if (waits == 0u) return CL_SUCCESS;
    return waits_.until_complete(*wire, handles, waits, icd::SlicedWait::forever, context->wakes());
}

cl_int ClIcdRoot::d3d11_copy_in(cl_command_queue queue, GuestClContext &context, const cl_mem *objects,
                                cl_uint count)
{
    AV_SCOPE("ClIcdRoot::d3d11_copy_in");
    for (cl_uint index = 0u; index < count; ++index) {
        lock_.acquire();
        const D3d11Object *const object = context.d3d11().find(objects[index]);
        lock_.release();
        /* OpenCL only writes this one, and is never shown its bytes. */
        if (!object->reads_resource()) continue;
        if (object->twin() != nullptr) {
            const cl_int moved =
                d3d11_.to_twin(context.d3d11().device(), object->resource(), object->subresource(), object->twin());
            if (moved != CL_SUCCESS) return moved;
            continue;
        }
        ClD3d11Upload rows(*this, queue, objects[index], *object);
        const cl_int  copied = d3d11_.read(context.d3d11().device(), object->resource(), object->subresource(), rows);
        if (copied != CL_SUCCESS) return copied;
    }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::d3d11_copy_out(cl_command_queue queue, GuestClContext &context, const cl_mem *objects,
                                 cl_uint count)
{
    AV_SCOPE("ClIcdRoot::d3d11_copy_out");
    for (cl_uint index = 0u; index < count; ++index) {
        lock_.acquire();
        const D3d11Object *const object = context.d3d11().find(objects[index]);
        lock_.release();
        /* OpenCL only read this one, and changed nothing to copy back. */
        if (!object->writes_resource()) continue;
        if (object->twin() != nullptr) {
            const cl_int moved = d3d11_.from_twin(context.d3d11().device(), object->twin(), object->resource(),
                                                  object->subresource());
            if (moved != CL_SUCCESS) return moved;
            continue;
        }
        ClD3d11Download rows(*this, queue, objects[index], *object);
        const cl_int copied = d3d11_.write(context.d3d11().device(), object->resource(), object->subresource(), rows);
        if (copied != CL_SUCCESS) return copied;
    }
    return CL_SUCCESS;
}

D3d11Object *ClIcdRoot::d3d11_object_of(ClObject &mem)
{
    AV_SCOPE("ClIcdRoot::d3d11_object_of");
    GuestClContext *const owner = context_of(mem);
    lock_.acquire();
    D3d11Object *const object = owner->d3d11().find(&mem);
    lock_.release();
    return object;
}

}  // namespace av::clicd
