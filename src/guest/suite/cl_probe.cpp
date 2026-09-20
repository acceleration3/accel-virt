#include "guest/suite/cl_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/suite/cl_entry.hpp"
#include "guest/suite/guest_report.hpp"

namespace av {

namespace {

/* The kernel the run builds, which is the smallest thing that proves a GPU
 * ran something: every element of the output is the sum of two inputs, so an
 * output that was never written, or written by the wrong work item, shows. */
constexpr const char *vector_add =
    "__kernel void vector_add(__global const float *a, __global const float *b,\n"
    "                        __global float *c)\n"
    "{\n"
    "    const int i = get_global_id(0);\n"
    "    c[i] = a[i] + b[i];\n"
    "}\n";

/* cl.h's, spelled here because this half of the probe is portable and does not
 * include the API's headers. */
enum : uint32_t {
    platform_name = 0x0902u,
    platform_icd_suffix = 0x0920u,
    device_name = 0x102Bu,
    device_vendor_id = 0x1001u,
    device_luid = 0x106Cu,
    device_svm_capabilities = 0x1053u,
};
enum : uint64_t { mem_read_only = 4u, mem_write_only = 2u, mem_read_write = 1u };
enum : uint64_t { map_read = 1u, map_write = 2u };
enum : int32_t { complete = 0 };

}  // namespace

/* An ABI boundary C requires: clSetEventCallback takes a bare function and one
 * argument, so the probe is the argument and this forwards straight into it. */
void callback_ran(void *, int32_t status, void *user_data)
{
    AV_SCOPE("callback_ran");
    static_cast<ClProbe *>(user_data)->callback_arrived(status);
}

ClProbe::ClProbe(Kmt &kmt, GuestReport &report, ClEntry &cl)
    : steps_(kmt, report, "opencl"), report_(report), kmt_(kmt), cl_(cl), platform_(nullptr),
      device_(nullptr),
      context_(nullptr), queue_(nullptr), program_(nullptr), kernel_(nullptr), buffers_{},
      first_{}, second_{}, sum_{}, callbacks_(0u), callback_status_(0)
{
    AV_SCOPE("ClProbe::ClProbe");
    for (uint32_t index = 0u; index < elements; ++index) {
        first_[index] = static_cast<float>(index);
        /* Distinct per element and distinct from both inputs, so a kernel
         * that wrote one constant everywhere, or transposed its work-item
         * index, fails the sum rather than passing it. */
        second_[index] = static_cast<float>(2u * index + 1u);
        sum_[index] = first_[index] + second_[index];
    }
}

void ClProbe::run()
{
    AV_SCOPE("ClProbe::run");
    static_cast<void>(scenarios());
    /* However far the run got, everything it made goes back: what a run
     * leaves behind is what a process leaks, and a run that stopped at the
     * build made as much as one that reached the end. */
    release_all();
}

Status ClProbe::scenarios()
{
    AV_SCOPE("ClProbe::scenarios");
    if (platform_step() != Status::ok) return Status::not_found;
    if (device_step() != Status::ok) return Status::not_found;
    luid_step();
    if (context_step() != Status::ok) return Status::not_found;
    if (queue_step() != Status::ok) return Status::not_found;
    if (program_step() != Status::ok) return Status::not_found;
    if (kernel_step() != Status::ok) return Status::not_found;
    if (read_step() != Status::ok) return Status::not_found;
    map_step();
    svm_step();
    callback_step();
    return Status::ok;
}

Status ClProbe::platform_step()
{
    AV_SCOPE("ClProbe::platform_step");
    uint32_t count = 0u;
    if (steps_.check_result("platform_count", cl_.platform_count(count)) != Status::ok) {
        return Status::not_found;
    }
    void    *found[8] = {};
    uint32_t written = 0u;
    if (steps_.check_result("platforms", cl_.platforms(found, 8u, written)) != Status::ok) {
        return Status::not_found;
    }
    /* This driver's platform is the one whose suffix is RVK. A machine may
     * have others, and the loader lists them all. */
    for (uint32_t index = 0u; index < written && index < 8u; ++index) {
        char   suffix[16] = {};
        size_t bytes = 0u;
        if (cl_.platform_text(found[index], platform_icd_suffix, suffix, sizeof suffix, bytes) != 0) {
            continue;
        }
        if (suffix[0] == 'R' && suffix[1] == 'V' && suffix[2] == 'K' && suffix[3] == '\0') {
            platform_ = found[index];
            break;
        }
    }
    return steps_.check_measured("platform", platform_ != nullptr ? Status::ok : Status::not_found,
                                 written);
}

Status ClProbe::device_step()
{
    AV_SCOPE("ClProbe::device_step");
    void    *found[4] = {};
    uint32_t written = 0u;
    if (steps_.check_result("device_ids", cl_.devices(platform_, found, 4u, written)) != Status::ok) {
        return Status::not_found;
    }
    device_ = written != 0u ? found[0] : nullptr;
    if (steps_.check_measured("device", device_ != nullptr ? Status::ok : Status::not_found,
                              written) != Status::ok) {
        return Status::not_found;
    }
    /* The name is the host GPU's, and the runner reads it: a software device
     * here means the guest never reached one. */
    char   name[128] = {};
    size_t bytes = 0u;
    if (cl_.device_text(device_, device_name, name, sizeof name, bytes) == 0) {
        report_.note("opencl", "device_name", name);
    }
    return Status::ok;
}

void ClProbe::luid_step()
{
    AV_SCOPE("ClProbe::luid_step");
    uint64_t vendor = 0u;
    uint64_t luid = 0u;
    const int32_t asked_vendor = cl_.device_number(device_, device_vendor_id, vendor);
    const int32_t asked_luid = cl_.device_number(device_, device_luid, luid);
    /* Both are the rvkgpu adapter's rather than the host GPU's. A LUID of
     * zero is what a device on no adapter would answer. */
    const bool named = asked_vendor == 0 && asked_luid == 0 && luid != 0u && vendor != 0u;
    /* Measured rather than checked: this step makes no D3DKMT call, so the
     * code a plain check would print is always zero. The LUID's low half is
     * what a reader can match against the adapter's. */
    steps_.check_measured("luid", named ? Status::ok : Status::not_found,
                          static_cast<uint32_t>(luid));
}

Status ClProbe::context_step()
{
    AV_SCOPE("ClProbe::context_step");
    int32_t error = 0;
    context_ = cl_.create_context(device_, error);
    return steps_.check_result("context", context_ != nullptr ? 0 : error);
}

Status ClProbe::queue_step()
{
    AV_SCOPE("ClProbe::queue_step");
    int32_t error = 0;
    queue_ = cl_.create_queue(context_, device_, error);
    if (steps_.check_result("queue", queue_ != nullptr ? 0 : error) != Status::ok) {
        return Status::not_found;
    }
    const size_t   bytes = sizeof(float) * elements;
    const uint64_t flags[3] = {mem_read_only, mem_read_only, mem_read_write};
    /* The first failure is what is reported. A later buffer that succeeded
     * overwrites `error` with CL_SUCCESS, which would otherwise turn a failed
     * creation into an "ok" line and leave a null buffer to be written to. */
    int32_t refused = 0;
    for (uint32_t index = 0u; index < 3u; ++index) {
        error = 0;
        buffers_[index] = cl_.create_buffer(context_, flags[index], bytes, error);
        if (buffers_[index] == nullptr && refused == 0) refused = error != 0 ? error : -1;
    }
    if (steps_.check_result("buffers", refused) != Status::ok) return Status::not_found;
    if (steps_.check_result("write", cl_.write_buffer(queue_, buffers_[0], bytes, first_)) !=
        Status::ok) {
        return Status::not_found;
    }
    return steps_.check_result("write_second",
                               cl_.write_buffer(queue_, buffers_[1], bytes, second_));
}

Status ClProbe::program_step()
{
    AV_SCOPE("ClProbe::program_step");
    int32_t error = 0;
    program_ = cl_.create_program(context_, vector_add, error);
    if (steps_.check_result("program", program_ != nullptr ? 0 : error) != Status::ok) {
        return Status::not_found;
    }
    const int32_t built = cl_.build_program(program_, device_, nullptr);
    if (built != 0) {
        /* What the compiler said, which is the only thing that makes a failed
         * build worth reading. */
        char   log[512] = {};
        size_t bytes = 0u;
        if (cl_.program_build_text(program_, device_, log, sizeof log, bytes) == 0) {
            report_.note("opencl", "build_log", log);
        }
    }
    return steps_.check_result("build", built);
}

Status ClProbe::kernel_step()
{
    AV_SCOPE("ClProbe::kernel_step");
    int32_t error = 0;
    kernel_ = cl_.create_kernel(program_, "vector_add", error);
    if (steps_.check_result("kernel", kernel_ != nullptr ? 0 : error) != Status::ok) {
        return Status::not_found;
    }
    int32_t set = 0;
    for (uint32_t index = 0u; index < 3u; ++index) {
        const int32_t one = cl_.set_kernel_buffer(kernel_, index, buffers_[index]);
        if (one != 0) set = one;
    }
    return steps_.check_result("arguments", set);
}

Status ClProbe::read_step()
{
    AV_SCOPE("ClProbe::read_step");
    if (steps_.check_result("run", cl_.run_kernel(queue_, kernel_, elements)) != Status::ok) {
        return Status::not_found;
    }
    if (steps_.check_result("finish", cl_.finish(queue_)) != Status::ok) return Status::not_found;
    float read[elements] = {};
    if (steps_.check_result("read", cl_.read_buffer(queue_, buffers_[2], sizeof read, read)) !=
        Status::ok) {
        return Status::not_found;
    }
    /* Exactly, not nearly: a sum of whole numbers this size is exact in a
     * float, so anything else is the wrong answer rather than a rounding. */
    uint32_t wrong = 0u;
    for (uint32_t index = 0u; index < elements; ++index) {
        if (read[index] != sum_[index]) ++wrong;
    }
    return steps_.check_measured("sum", wrong == 0u ? Status::ok : Status::conflict, wrong);
}

void ClProbe::map_step()
{
    AV_SCOPE("ClProbe::map_step");
    const size_t bytes = sizeof(float) * elements;
    int32_t      error = 0;
    float *const mapped =
        static_cast<float *>(cl_.map_buffer(queue_, buffers_[2], map_read, bytes, error));
    if (mapped == nullptr) {
        steps_.check_result("map", error != 0 ? error : -1);
        return;
    }
    /* What the kernel wrote, seen through the mapping rather than through a
     * read. */
    uint32_t wrong = 0u;
    for (uint32_t index = 0u; index < elements; ++index) {
        if (mapped[index] != sum_[index]) ++wrong;
    }
    steps_.check_measured("map", wrong == 0u ? Status::ok : Status::conflict, wrong);
    steps_.check_result("unmap", cl_.unmap(queue_, buffers_[2], mapped));
}

void ClProbe::svm_step()
{
    AV_SCOPE("ClProbe::svm_step");
    uint64_t capabilities = 0u;
    if (cl_.device_number(device_, device_svm_capabilities, capabilities) != 0 ||
        capabilities == 0u) {
        /* A device that offers none is not a failure; it is a device without
         * shared memory, and the run says so. */
        report_.note("opencl", "svm", "not offered");
        return;
    }
    void *const shared = cl_.svm_alloc(context_, sizeof(float) * elements);
    if (shared == nullptr) {
        steps_.check_result("svm", -1);
        return;
    }
    /* A kernel argument that is an address rather than an object: what the
     * ICD translates, and the device bounds-checks. */
    steps_.check_result("svm", cl_.set_kernel_svm(kernel_, 2u, shared));
    /* And the output buffer goes back into the slot this borrowed, so a step
     * added after this one dispatches against what every other step set. */
    static_cast<void>(cl_.set_kernel_buffer(kernel_, 2u, buffers_[2]));
    cl_.svm_free(context_, shared);
}

void ClProbe::callback_step()
{
    AV_SCOPE("ClProbe::callback_step");
    int32_t     error = 0;
    void *const event = cl_.create_user_event(context_, error);
    if (event == nullptr) {
        steps_.check_result("callback", error != 0 ? error : -1);
        return;
    }
    /* A member, not a local: a real ICD delivers on its own thread, and a
     * callback that arrives after this frame has gone would otherwise write
     * into a dead stack. */
    callbacks_ = 0u;
    callback_status_ = 0;
    if (cl_.watch_event(event, complete, &callback_ran, this) != 0) {
        steps_.check_result("callback", -1);
        cl_.release_event(event);
        return;
    }
    /* Set from this thread, which is the scenario the specification's US3
     * names: a callback on an event the same guest thread completes later
     * must not need that thread to be anywhere in particular. */
    steps_.check_result("user_event", cl_.set_user_event_status(event, complete));
    steps_.check_result("wait", cl_.wait_for_event(event));
    const uint32_t arrived = callbacks_seen();
    steps_.check_measured("callback", arrived == 1u ? Status::ok : Status::conflict, arrived);
    cl_.release_event(event);
}

/* The wait on an event says the host has reached it. It says nothing about
 * the ICD's own thread having run the callback yet - OpenCL orders neither
 * against the other, and the conformance suite waits for a callback for the
 * same reason. Judging the count the moment the wait returned would be
 * judging which of two threads got there first. */
uint32_t ClProbe::callbacks_seen() const
{
    AV_SCOPE("ClProbe::callbacks_seen");
    uint64_t       frequency = 0u;
    const uint64_t started = kmt_.performance_counter(frequency);
    const uint64_t patience = frequency * callback_seconds;
    uint32_t       arrived = callbacks_;
    while (arrived == 0u && kmt_.performance_counter(frequency) - started < patience) {
        arrived = callbacks_;
    }
    return arrived;
}

void ClProbe::release_all()
{
    AV_SCOPE("ClProbe::release_all");
    /* In reverse, as a program unwinds: what a run leaves behind is what a
     * process leaks. */
    if (kernel_ != nullptr) cl_.release_kernel(kernel_);
    if (program_ != nullptr) cl_.release_program(program_);
    for (uint32_t index = 0u; index < 3u; ++index) {
        if (buffers_[index] != nullptr) cl_.release_buffer(buffers_[index]);
    }
    if (queue_ != nullptr) cl_.release_queue(queue_);
    if (context_ != nullptr) cl_.release_context(context_);
    steps_.check("release", Status::ok);
}

void ClProbe::callback_arrived(int32_t status)
{
    AV_SCOPE("ClProbe::callback_arrived");
    /* Read and written rather than incremented: the count is volatile, and a
     * read-modify-write of one is what C++23 deprecates. Only the ICD's own
     * thread writes it, so the two halves cannot interleave with each other. */
    callbacks_ = callbacks_ + 1u;
    callback_status_ = status;
}

}  // namespace av
