#include "guest/probe/cl_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/probe/cl_entry.hpp"
#include "guest/probe/shaders/vector_add_spv.hpp"
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

/* A 4 KiB comment, a string of its own many times over in the long program:
 * the probe has no heap for a megabyte of source, and the API takes many
 * strings as one program. */
constexpr size_t padding_bytes = 4096u;

struct Padding {
    char text[padding_bytes + 1u];
};

constexpr Padding padding_text()
{
    constexpr char line[] = "/* avclprobe: a program longer than every room on its way to the host compiler */\n";
    constexpr size_t line_bytes = sizeof line - 1u;
    /* Whole comments, then blanks: a comment cut short would run on into the
     * kernel's neighbour, and the last one off the end of the program. */
    constexpr size_t whole = (padding_bytes - 1u) / line_bytes * line_bytes;
    Padding padding{};
    for (size_t at = 0u; at < padding_bytes; at++) padding.text[at] = at < whole ? line[at % line_bytes] : ' ';
    padding.text[padding_bytes - 1u] = '\n';
    padding.text[padding_bytes] = '\0';
    return padding;
}

constexpr Padding padding = padding_text();

/* The link step's two halves: a helper alone, and the vector add calling it
 * through a header program it includes by name. */
constexpr const char *added_helper = "float added(float a, float b) { return a + b; }\n";
constexpr const char *added_header = "float added(float a, float b);\n";
constexpr const char *added_header_name = "added.h";
constexpr const char *added_kernel =
    "#include \"added.h\"\n"
    "__kernel void vector_add(__global const float *a, __global const float *b,\n"
    "                        __global float *c)\n"
    "{\n"
    "    const int i = get_global_id(0);\n"
    "    c[i] = added(a[i], b[i]);\n"
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
    device_il_version = 0x105Bu,
};
enum : int32_t { invalid_binary = -42, invalid_operation = -59 };
enum : uint64_t { mem_read_only = 4u, mem_write_only = 2u, mem_read_write = 1u };
enum : uint64_t { map_read = 1u, map_write = 2u };
enum : int32_t { complete = 0 };

}  // namespace

/* An ABI boundary C requires: clSetEventCallback takes a bare function and one
 * argument, so the probe is the argument and this forwards straight into it. */
void AV_RUNTIME_CALL callback_ran(void *, int32_t status, void *user_data)
{
    AV_SCOPE("callback_ran");
    static_cast<ClProbe *>(user_data)->callback_arrived(status);
}

ClProbe::ClProbe(Kmt &kmt, GuestReport &report, ClEntry &cl)
    : steps_(kmt, report, "opencl"), report_(report), kmt_(kmt), cl_(cl), platform_(nullptr),
      device_(nullptr),
      context_(nullptr), queue_(nullptr), program_(nullptr), kernel_(nullptr), long_program_(nullptr),
      long_kernel_(nullptr), buffers_{}, kept_programs_{}, kept_kernels_{}, programs_kept_(0u), kernels_kept_(0u),
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
    long_program_step();
    binary_step();
    link_step();
    il_step();
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
    /* This driver's platform is the one whose suffix is AV. A machine may
     * have others, and the loader lists them all. */
    for (uint32_t index = 0u; index < written && index < 8u; ++index) {
        char   suffix[16] = {};
        size_t bytes = 0u;
        if (cl_.platform_text(found[index], platform_icd_suffix, suffix, sizeof suffix, bytes) != 0) {
            continue;
        }
        if (suffix[0] == 'A' && suffix[1] == 'V' && suffix[2] == '\0') {
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
    /* Both are the avgpu adapter's rather than the host GPU's. A LUID of
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
    program_ = cl_.create_program(context_, &vector_add, 1u, error);
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

void ClProbe::long_program_step()
{
    AV_SCOPE("ClProbe::long_program_step");
    /* The kernel first, so the comments that make the bulk follow it: the
     * same vector add, built afresh from a program twenty thousand times its
     * size. */
    const char *sources[long_strings + 1u] = {};
    sources[0] = vector_add;
    for (uint32_t index = 1u; index <= long_strings; ++index) sources[index] = padding.text;
    constexpr uint32_t kib = static_cast<uint32_t>(long_strings * padding_bytes / 1024u);
    int32_t            error = 0;
    long_program_ = cl_.create_program(context_, sources, long_strings + 1u, error);
    if (steps_.check_measured("long_program", long_program_ != nullptr ? Status::ok : Status::invalid_state,
                              long_program_ != nullptr ? kib : static_cast<uint32_t>(error)) != Status::ok)
        return;
    const int32_t built = cl_.build_program(long_program_, device_, nullptr);
    if (steps_.check_result("long_build", built) != Status::ok) return;
    long_kernel_ = cl_.create_kernel(long_program_, "vector_add", error);
    if (steps_.check_result("long_kernel", long_kernel_ != nullptr ? 0 : error) != Status::ok) return;
    int32_t        ran = 0;
    const uint32_t wrong = wrong_from(long_kernel_, ran);
    if (steps_.check_result("long_run", ran) != Status::ok) return;
    steps_.check_measured("long_sum", wrong == 0u ? Status::ok : Status::conflict, wrong);
}

uint32_t ClProbe::wrong_from(void *kernel, int32_t &ran)
{
    AV_SCOPE("ClProbe::wrong_from");
    ran = 0;
    for (uint32_t index = 0u; index < 3u; ++index) {
        const int32_t one = cl_.set_kernel_buffer(kernel, index, buffers_[index]);
        if (one != 0) ran = one;
    }
    /* The output cleared first, so a sum read back is this kernel's. */
    float cleared[elements] = {};
    if (ran == 0) ran = cl_.write_buffer(queue_, buffers_[2], sizeof cleared, cleared);
    if (ran == 0) ran = cl_.run_kernel(queue_, kernel, elements);
    if (ran == 0) ran = cl_.finish(queue_);
    float read[elements] = {};
    if (ran == 0) ran = cl_.read_buffer(queue_, buffers_[2], sizeof read, read);
    if (ran != 0) return elements;
    uint32_t wrong = 0u;
    for (uint32_t index = 0u; index < elements; ++index) {
        if (read[index] != sum_[index]) ++wrong;
    }
    return wrong;
}

void ClProbe::keep(void *program, void *kernel)
{
    AV_SCOPE("ClProbe::keep");
    if (program != nullptr && programs_kept_ < most_kept) kept_programs_[programs_kept_++] = program;
    if (kernel != nullptr && kernels_kept_ < most_kept) kept_kernels_[kernels_kept_++] = kernel;
}

void ClProbe::binary_step()
{
    AV_SCOPE("ClProbe::binary_step");
    size_t         bytes = 0u;
    int32_t        error = 0;
    uint8_t *const binary = cl_.program_binary(program_, bytes, error);
    if (steps_.check_measured("binary", binary != nullptr && bytes != 0u ? Status::ok : Status::invalid_state,
                              binary != nullptr ? static_cast<uint32_t>(bytes) : static_cast<uint32_t>(error)) !=
        Status::ok)
        return;
    int32_t     status = 0;
    void *const loaded = cl_.create_program_with_binary(context_, device_, binary, bytes, status, error);
    keep(loaded, nullptr);
    int32_t built = loaded != nullptr ? cl_.build_program(loaded, device_, nullptr) : (error != 0 ? error : status);
    void *kernel = built == 0 ? cl_.create_kernel(loaded, "vector_add", error) : nullptr;
    keep(nullptr, kernel);
    if (built == 0 && kernel == nullptr) built = error != 0 ? error : -1;
    if (steps_.check_result("binary_build", built) != Status::ok) return;
    int32_t        ran = 0;
    const uint32_t wrong = wrong_from(kernel, ran);
    steps_.check_measured("binary_sum", ran == 0 && wrong == 0u ? Status::ok : Status::conflict,
                          ran == 0 ? wrong : static_cast<uint32_t>(ran));
    /* Its last byte flipped, as a cache damaged on disk would have it: what
     * a driver must not run, and what the application rebuilds its source
     * for. */
    binary[bytes - 1u] = static_cast<uint8_t>(binary[bytes - 1u] ^ 0x5Au);
    void *const refused = cl_.create_program_with_binary(context_, device_, binary, bytes, status, error);
    keep(refused, nullptr);
    const bool clean = refused == nullptr && error == invalid_binary && status == invalid_binary;
    steps_.check_measured("binary_refused", clean ? Status::ok : Status::conflict, static_cast<uint32_t>(error));
}

void ClProbe::link_step()
{
    AV_SCOPE("ClProbe::link_step");
    int32_t     error = 0;
    void *const helper = cl_.create_program(context_, &added_helper, 1u, error);
    keep(helper, nullptr);
    void *const header = helper != nullptr ? cl_.create_program(context_, &added_header, 1u, error) : nullptr;
    keep(header, nullptr);
    void *const caller = header != nullptr ? cl_.create_program(context_, &added_kernel, 1u, error) : nullptr;
    keep(caller, nullptr);
    int32_t compiled = caller != nullptr ? cl_.compile_program(helper, device_, nullptr, nullptr) : error;
    if (compiled == 0) compiled = cl_.compile_program(caller, device_, header, added_header_name);
    if (steps_.check_result("compile", compiled) != Status::ok) return;
    void *const inputs[2] = {helper, caller};
    void *const linked = cl_.link_program(context_, inputs, 2u, error);
    keep(linked, nullptr);
    void *const kernel = linked != nullptr ? cl_.create_kernel(linked, "vector_add", error) : nullptr;
    keep(nullptr, kernel);
    if (steps_.check_result("link", kernel != nullptr ? 0 : (error != 0 ? error : -1)) != Status::ok) return;
    int32_t        ran = 0;
    const uint32_t wrong = wrong_from(kernel, ran);
    steps_.check_measured("link_sum", ran == 0 && wrong == 0u ? Status::ok : Status::conflict,
                          ran == 0 ? wrong : static_cast<uint32_t>(ran));
}

void ClProbe::il_step()
{
    AV_SCOPE("ClProbe::il_step");
    char   versions[256] = {};
    size_t bytes = 0u;
    bool   offered = false;
    if (cl_.device_text(device_, device_il_version, versions, sizeof versions, bytes) == 0) {
        for (size_t at = 0u; at + 6u <= sizeof versions && versions[at] != '\0' && !offered; ++at)
            offered = versions[at] == 'S' && versions[at + 1u] == 'P' && versions[at + 2u] == 'I' &&
                      versions[at + 3u] == 'R' && versions[at + 4u] == '-' && versions[at + 5u] == 'V';
    }
    report_.note("opencl", "il_version", offered ? versions : "not offered");
    int32_t     error = 0;
    void *const program = cl_.create_program_with_il(context_, vector_add_spv, sizeof vector_add_spv, error);
    keep(program, nullptr);
    if (!offered) {
        /* A device without IL refuses it as the API says it must, and the
         * run says which. */
        const bool refused = program == nullptr && error == invalid_operation;
        steps_.check_measured("il", refused ? Status::ok : Status::conflict, static_cast<uint32_t>(error));
        return;
    }
    int32_t built = program != nullptr ? cl_.build_program(program, device_, nullptr) : (error != 0 ? error : -1);
    void *kernel = built == 0 ? cl_.create_kernel(program, "vector_add", error) : nullptr;
    keep(nullptr, kernel);
    if (built == 0 && kernel == nullptr) built = error != 0 ? error : -1;
    if (steps_.check_result("il", built) != Status::ok) return;
    int32_t        ran = 0;
    const uint32_t wrong = wrong_from(kernel, ran);
    steps_.check_measured("il_sum", ran == 0 && wrong == 0u ? Status::ok : Status::conflict,
                          ran == 0 ? wrong : static_cast<uint32_t>(ran));
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
    while (kernels_kept_ != 0u) cl_.release_kernel(kept_kernels_[--kernels_kept_]);
    while (programs_kept_ != 0u) cl_.release_program(kept_programs_[--programs_kept_]);
    if (long_kernel_ != nullptr) cl_.release_kernel(long_kernel_);
    if (long_program_ != nullptr) cl_.release_program(long_program_);
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
