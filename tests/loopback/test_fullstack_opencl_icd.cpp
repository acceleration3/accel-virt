/* The OpenCL ICD on this machine's GPU (feature 052, T031, T040, T051):
 * FakeClLoader drives the ICD's entries as the Khronos loader would - through
 * the dispatch table in each object's first word, never by name - and the ICD
 * root, over DdiKmt, carries every command through the real driver and device
 * core to the executor library, loaded from its file and bound to the host GPU
 * that runs the most sub-APIs. A host without OpenCL has nothing here to
 * judge, and the test returns without checking.
 *
 * The watcher's thread is ScriptedThread rather than a real one. Everything
 * below the ICD here - MockPlatform, the recording locks, the driver's own
 * core and the device core - is single-threaded by construction, and the
 * recording locks report a second owner as a violation, so a real thread
 * turning the watcher's loop would be a race against the very rig that is
 * meant to be judging. Stepped instead, a turn is something the test can state
 * exactly: after this call, this many turns, with this answer. The one place a
 * second thread is the point - a user event set from off the application's
 * thread - uses a real one, and hands the ICD over rather than sharing it. */
#include "device/abi/av_executor.h"
#include "harness/executor/executor_library_file.hpp"
#include "harness/executor/widest_host_gpu.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_d3d11_bridge.hpp"
#include "harness/icd/scripted_d3d_adapter_source.hpp"
#include "harness/icd/scripted_gl_share_source.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "guest/probe/shaders/vector_add_spv.hpp"
#include "icd/opencl/d3d11_resource.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include "cl_protocol_ids.h"

#include <CL/cl_ext.h>

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

/* The executor library opens every host API when it loads, and a real OpenCL
 * platform, VkDevice and EGL display allocate from driver modules that never
 * give everything back. The host's OpenCL drivers are named one by one - the
 * Khronos loader itself, Mesa's rusticl over gallium and LLVM, Intel's two,
 * NVIDIA's - as the other full-stack tests name them, rather than leak
 * detection turned off, so a leak of this repository's own still fails. */
extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:libRusticlOpenCL\n"
           "leak:rusticl\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libigc\n"
           "leak:libnvidia-opencl\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libglapi\n"
           "leak:libvulkan\n"
           "leak:libnvidia\n"
           "leak:nvidia_icd\n"
           "leak:libvulkan_intel\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libz\n"
           "leak:libLLVM\n"
           "leak:libgallium\n"
           "leak:libav_executor\n";
}

/* An ABI boundary C requires: a signal handler is handed a number and nothing
 * else, so there is nowhere to put a `this`. */
extern "C" void opencl_icd_gave_up(int);
extern "C" void opencl_icd_gave_up(int)
{
    static const char message[] = "test_fullstack_opencl_icd: a call did not return\n";
    (void)!write(2, message, sizeof message - 1u);
    _exit(1);
}

using av::DdiKmt;
using av::ExecutorLibraryFile;
using av::FakeClLoader;
using av::FullStackRig;
using av::LockTrace;
using av::MockPlatform;
using av::RecordingLog;
using av::RecordingThreadLock;
using av::ScriptedThread;
using av::WidestHostGpu;
using av::WireReader;
using av::WireWriter;
using av::clicd::ClIcdRoot;

namespace {

constexpr std::uint32_t elements = 64u;
constexpr std::size_t   vector_bytes = elements * sizeof(float);
constexpr std::size_t   device_name_bytes = 256u;
/* Long enough for a cold compile of the kernel on any host driver, short
 * enough that a wait that will never end is reported in a person's lifetime. */
constexpr unsigned      patience_seconds = 120u;

/* The smallest kernel that proves a GPU ran something: every element of the
 * output is the sum of two inputs, so an output that was never written, or
 * written by the wrong work item, shows. The guest's own ClProbe builds this
 * one, and the loopback runs the same source. */
constexpr const char *vector_add =
    "__kernel void vector_add(__global const float *a, __global const float *b,\n"
    "                        __global float *c)\n"
    "{\n"
    "    const int i = get_global_id(0);\n"
    "    c[i] = a[i] + b[i];\n"
    "}\n";

/* A wait that would otherwise hang ends the process with a failure instead.
 * ctest reports a non-zero exit; a hang reports nothing until someone
 * notices. */
class Deadline {
public:
    explicit Deadline(unsigned seconds)
    {
        struct sigaction watchdog = {};
        watchdog.sa_handler = &opencl_icd_gave_up;
        static_cast<void>(sigaction(SIGALRM, &watchdog, nullptr));
        static_cast<void>(alarm(seconds));
    }
    ~Deadline() { static_cast<void>(alarm(0u)); }
    Deadline(const Deadline &) = delete;
    Deadline &operator=(const Deadline &) = delete;
};

/* The ICD as one guest process holds it: its own memory, locks, log, the
 * thread its event watcher runs on, and the loader that found it. */
class ClProcess {
public:
    explicit ClProcess(DdiKmt &kmt)
        : lock_(trace_, 'H'), send_lock_(trace_, 'S'), objects_lock_(trace_, 'O'), blocks_lock_(trace_, 'B'),
          root_(memory_, kmt, lock_, send_lock_, objects_lock_, blocks_lock_, log_, watcher_, gl_share_, d3d_adapters_,
                d3d11_, av::clicd::cl_icd_binding),
          loader_(root_)
    {
    }

    ClIcdRoot           &root() { return root_; }
    RecordingLog        &log() { return log_; }
    RecordingThreadLock &lock() { return lock_; }
    RecordingThreadLock &send_lock() { return send_lock_; }
    FakeClLoader        &loader() { return loader_; }
    ScriptedThread      &watcher() { return watcher_; }
    av::ScriptedD3dAdapterSource &d3d_adapters() { return d3d_adapters_; }
    av::ScriptedD3d11Bridge      &d3d11() { return d3d11_; }

private:
    MockPlatform        memory_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    RecordingThreadLock send_lock_;
    RecordingThreadLock objects_lock_;
    RecordingThreadLock blocks_lock_;
    RecordingLog        log_;
    ScriptedThread      watcher_;
    av::ScriptedGlShareSource gl_share_;
    av::ScriptedD3dAdapterSource d3d_adapters_;
    av::ScriptedD3d11Bridge      d3d11_;
    ClIcdRoot           root_;
    FakeClLoader        loader_;
};

/* What the loader dispatches through: the table in the object's first word. */
const cl_icd_dispatch *table_of(const void *object)
{
    return object != nullptr ? *reinterpret_cast<const cl_icd_dispatch *const *>(object) : nullptr;
}

/* CL_DEVICE_NAME as the executor answers it with no guest in between: the
 * host GPU's own name, which the ICD must hand the application unchanged. The
 * three frames are the ones the ICD's own device query sends - a platform, a
 * device of it, then the name - encoded here by hand because nothing but a
 * transport can run the generated client. */
class HostClDevice {
public:
    HostClDevice(const AvExecutorOps &ops, const std::uint8_t *uuid)
        : ops_(ops), domain_(ops.open_domain(ops.opaque, uuid)), executor_(nullptr)
    {
        const AvContextOptions options = {0u, 0u, 0u, 0u, domain_};
        executor_ = ops.create(ops.opaque, AV_EXECUTOR_SUB_API_OPENCL, &options, nullptr);
    }
    ~HostClDevice()
    {
        if (executor_ != nullptr) ops_.destroy(ops_.opaque, executor_);
        if (domain_ != 0u) ops_.close_domain(ops_.opaque, domain_);
    }
    HostClDevice(const HostClDevice &) = delete;
    HostClDevice &operator=(const HostClDevice &) = delete;

    /* False when this host answered no platform, no device or no name. */
    bool name(char *into, std::size_t room)
    {
        if (executor_ == nullptr) return false;
        std::uint64_t platform = 0u;
        if (!one_handle(av::clproto::kCmd_clGetPlatformIDs, 0u, CL_DEVICE_TYPE_GPU, platform)) {
            return false;
        }
        std::uint64_t device = 0u;
        if (!one_handle(av::clproto::kCmd_clGetDeviceIDs, platform, CL_DEVICE_TYPE_GPU, device)) {
            return false;
        }
        return text(device, CL_DEVICE_NAME, into, room);
    }

private:
    /* clGetPlatformIDs and clGetDeviceIDs differ by the two fields in front:
     * the device query names a platform and a type, the platform query names
     * neither. Both answer a count, one handle and a count again. */
    bool one_handle(std::uint32_t command, std::uint64_t platform, cl_device_type type,
                    std::uint64_t &answer)
    {
        std::uint8_t frame[64];
        WireWriter   ask(frame, sizeof frame);
        if (command == av::clproto::kCmd_clGetDeviceIDs) {
            ask.write_pod<std::uint64_t>(platform);
            ask.write_varint<std::uint64_t>(type);
        }
        ask.write_varint<std::uint32_t>(1u);
        ask.write_pod<std::uint8_t>(1u);
        ask.write_pod<std::uint8_t>(1u);

        std::uint8_t reply[512] = {};
        std::size_t  length = 0u;
        if (ops_.execute(ops_.opaque, executor_, command, frame, ask.offset(), reply, sizeof reply,
                         &length) != AV_EXECUTED) {
            return false;
        }
        WireReader   read(reply, length);
        std::int64_t answered = 0;
        std::uint64_t count = 0u, listed = 0u;
        if (!read.read_varint(answered) || answered != CL_SUCCESS) return false;
        if (!read.read_varint(count) || count != 1u) return false;
        if (!read.read_pod(answer) || answer == 0u) return false;
        return read.read_varint(listed) && listed == 1u;
    }

    bool text(std::uint64_t device, cl_device_info wanted, char *into, std::size_t room)
    {
        std::uint8_t frame[64];
        WireWriter   ask(frame, sizeof frame);
        ask.write_pod<std::uint64_t>(device);
        ask.write_varint<std::uint64_t>(wanted);
        ask.write_varint<std::uint64_t>(room - 1u);
        ask.write_pod<std::uint8_t>(1u);
        ask.write_pod<std::uint8_t>(1u);

        std::uint8_t reply[1024] = {};
        std::size_t  length = 0u;
        if (ops_.execute(ops_.opaque, executor_, av::clproto::kCmd_clGetDeviceInfo, frame,
                         ask.offset(), reply, sizeof reply, &length) != AV_EXECUTED) {
            return false;
        }
        WireReader    read(reply, length);
        std::int64_t  answered = 0;
        std::uint64_t bytes = 0u;
        if (!read.read_varint(answered) || answered != CL_SUCCESS) return false;
        if (!read.read_varint(bytes) || bytes == 0u || bytes > room - 1u) return false;
        if (!read.read(into, static_cast<std::size_t>(bytes))) return false;
        into[bytes] = '\0';
        return true;
    }

    const AvExecutorOps &ops_;
    std::uint32_t        domain_;
    AvExecutor          *executor_;
};

/* The probe's US1 steps as one object, in ClProbe's own order: a device, a
 * context and a queue on it, three buffers with the inputs written, the vector
 * add built and its arguments set. What every test below starts from, and what
 * releases it all again. */
class ClVectorAdd {
public:
    explicit ClVectorAdd(FakeClLoader &loader)
        : loader_(loader), device_(nullptr), context_(nullptr), queue_(nullptr), program_(nullptr),
          kernel_(nullptr), buffers_{}, first_{}, second_{}, sum_{}
    {
        for (std::uint32_t index = 0u; index < elements; ++index) {
            first_[index] = static_cast<float>(index);
            /* Distinct per element and distinct from both inputs, so a kernel
             * that wrote one constant everywhere, or transposed its work-item
             * index, fails the sum rather than passing it. */
            second_[index] = static_cast<float>(2u * index + 1u);
            sum_[index] = first_[index] + second_[index];
        }
    }

    ~ClVectorAdd()
    {
        if (kernel_ != nullptr) CHECK_EQ(table_of(kernel_)->clReleaseKernel(kernel_), CL_SUCCESS);
        if (program_ != nullptr) CHECK_EQ(table_of(program_)->clReleaseProgram(program_), CL_SUCCESS);
        for (std::uint32_t index = 0u; index < 3u; ++index) {
            if (buffers_[index] == nullptr) continue;
            CHECK_EQ(table_of(buffers_[index])->clReleaseMemObject(buffers_[index]), CL_SUCCESS);
        }
        if (queue_ != nullptr) CHECK_EQ(table_of(queue_)->clReleaseCommandQueue(queue_), CL_SUCCESS);
        if (context_ != nullptr) CHECK_EQ(loader_.release_context(context_), CL_SUCCESS);
    }

    ClVectorAdd(const ClVectorAdd &) = delete;
    ClVectorAdd &operator=(const ClVectorAdd &) = delete;

    /* Each step judged as the probe's report judges it. False stops the test
     * rather than dereferencing what was not made. */
    bool make()
    {
        cl_uint devices = 0u;
        CHECK_EQ(loader_.device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices), CL_SUCCESS);
        CHECK_EQ(devices, 1u);
        if (device_ == nullptr) return false;

        cl_int error = CL_SUCCESS;
        context_ = loader_.create_context(device_, &error);
        CHECK_EQ(error, CL_SUCCESS);
        if (context_ == nullptr) return false;
        queue_ = loader_.create_queue(context_, device_, &error);
        CHECK_EQ(error, CL_SUCCESS);
        if (queue_ == nullptr) return false;

        /* The first is the one the mapping case maps, and it asks for host
         * memory the way an application that intends to map does - which is
         * what takes it down the arena path, where the device makes its
         * cl_mem over the very bytes the guest is handed (T033). The other
         * two are ordinary, so both paths are live in one run. */
        const cl_mem_flags flags[3] = {CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, CL_MEM_READ_ONLY,
                                       CL_MEM_READ_WRITE};
        for (std::uint32_t index = 0u; index < 3u; ++index) {
            buffers_[index] = loader_.create_buffer(context_, flags[index], vector_bytes, &error);
            CHECK_EQ(error, CL_SUCCESS);
            if (buffers_[index] == nullptr) return false;
        }
        if (!write(0u, first_) || !write(1u, second_)) return false;

        program_ = loader_.create_program(context_, vector_add, &error);
        CHECK_EQ(error, CL_SUCCESS);
        if (program_ == nullptr) return false;
        const cl_int built = loader_.build_program(program_, nullptr);
        CHECK_EQ(built, CL_SUCCESS);
        if (built != CL_SUCCESS) return false;
        kernel_ = loader_.create_kernel(program_, "vector_add", &error);
        CHECK_EQ(error, CL_SUCCESS);
        if (kernel_ == nullptr) return false;
        for (std::uint32_t index = 0u; index < 3u; ++index) {
            CHECK_EQ(table_of(kernel_)->clSetKernelArg(kernel_, index, sizeof(cl_mem),
                                                       &buffers_[index]),
                     CL_SUCCESS);
        }
        return true;
    }

    /* Writes `from` into buffer `index`, blocking, as the probe does.
     *
     * The blocking write is the interesting one: the ICD answers it by
     * enqueueing without blocking and asking the host for the event's status
     * in a loop, so it owes the queue a flush of its own - a host driver that
     * starts nothing until a flush would otherwise never complete the event
     * and this call would never return. That flush is T050's, and this is
     * what proves it: no clFlush here, and a Deadline rather than a poll that
     * sits. */
    bool write(std::uint32_t index, const float *from)
    {
        const cl_int written = table_of(queue_)->clEnqueueWriteBuffer(
            queue_, buffers_[index], CL_TRUE, 0u, vector_bytes, from, 0u, nullptr, nullptr);
        CHECK_EQ(written, CL_SUCCESS);
        return written == CL_SUCCESS;
    }

    cl_int enqueue(cl_event *event)
    {
        const std::size_t global = elements;
        return table_of(queue_)->clEnqueueNDRangeKernel(queue_, kernel_, 1u, nullptr, &global,
                                                        nullptr, 0u, nullptr, event);
    }

    cl_int flush() { return table_of(queue_)->clFlush(queue_); }

    cl_int finish() { return table_of(queue_)->clFinish(queue_); }

    /* Pushes the queue, waits for `event`, and gives it back.
     *
     * The flush is the application's own and stays here: clWaitForEvents is
     * the one wait the ICD does not flush for, because OpenCL puts that on
     * the application and flushing anyway would hide the bug in an
     * application that forgot. */
    cl_int settle(cl_event event)
    {
        if (event == nullptr) return CL_INVALID_EVENT;
        CHECK_EQ(flush(), CL_SUCCESS);
        const cl_int waited = table_of(event)->clWaitForEvents(1u, &event);
        CHECK_EQ(waited, CL_SUCCESS);
        CHECK_EQ(table_of(event)->clReleaseEvent(event), CL_SUCCESS);
        return waited;
    }

    cl_int read(float *into, cl_bool blocking, cl_event *event)
    {
        return table_of(queue_)->clEnqueueReadBuffer(queue_, buffers_[2], blocking, 0u,
                                                     vector_bytes, into, 0u, nullptr, event);
    }

    /* How many of `read` are not what the inputs add to. Exactly, not nearly:
     * a sum of whole numbers this size is exact in a float, so anything else
     * is the wrong answer rather than a rounding. */
    std::uint32_t wrong(const float *read) const
    {
        std::uint32_t count = 0u;
        for (std::uint32_t index = 0u; index < elements; ++index) {
            if (read[index] != sum_[index]) ++count;
        }
        return count;
    }

    /* What the sum would be if buffer 0 held `replaced` instead of its own
     * input, which is what a mapped write has to show up as. */
    void sum_with(const float *replaced, float *into) const
    {
        for (std::uint32_t index = 0u; index < elements; ++index) {
            into[index] = replaced[index] + second_[index];
        }
    }

    cl_device_id     device() const { return device_; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }
    cl_program       program() const { return program_; }
    cl_kernel        kernel() const { return kernel_; }

    /* How many elements of the sum `kernel` gets wrong, run over the three
     * buffers with the output cleared first - so a kernel that never ran is
     * every element wrong rather than the last run's right answer. `extra`,
     * when not null, is a fourth argument of eight bytes. */
    std::uint32_t wrong_from(cl_kernel kernel, const std::uint64_t *extra = nullptr)
    {
        if (kernel == nullptr) return elements;
        for (cl_uint index = 0u; index < 3u; ++index) {
            if (table_of(kernel)->clSetKernelArg(kernel, index, sizeof(cl_mem), &buffers_[index]) != CL_SUCCESS)
                return elements;
        }
        if (extra != nullptr && table_of(kernel)->clSetKernelArg(kernel, 3u, sizeof *extra, extra) != CL_SUCCESS)
            return elements;
        const float cleared[elements] = {};
        if (table_of(queue_)->clEnqueueWriteBuffer(queue_, buffers_[2], CL_TRUE, 0u, vector_bytes, cleared, 0u,
                                                   nullptr, nullptr) != CL_SUCCESS)
            return elements;
        const std::size_t global = elements;
        if (table_of(queue_)->clEnqueueNDRangeKernel(queue_, kernel, 1u, nullptr, &global, nullptr, 0u, nullptr,
                                                     nullptr) != CL_SUCCESS)
            return elements;
        float read_back[elements] = {};
        if (read(read_back, CL_TRUE, nullptr) != CL_SUCCESS) return elements;
        std::uint32_t count = 0u;
        const float   offset = extra != nullptr ? static_cast<float>(*extra) : 0.0f;
        for (std::uint32_t index = 0u; index < elements; ++index) {
            if (read_back[index] != sum_[index] + offset) ++count;
        }
        return count;
    }
    cl_mem           input() const { return buffers_[0]; }
    cl_mem           output() const { return buffers_[2]; }

private:
    FakeClLoader    &loader_;
    cl_device_id     device_;
    cl_context       context_;
    cl_command_queue queue_;
    cl_program       program_;
    cl_kernel        kernel_;
    cl_mem           buffers_[3];
    float            first_[elements];
    float            second_[elements];
    float            sum_[elements];
};

/* The host GPU is found once, before the executor library opens its own
 * displays: finding it opens and terminates the same ones, and a display
 * terminated under the library stays terminated for it. */
const WidestHostGpu &widest_gpu()
{
    static const WidestHostGpu gpu;
    return gpu;
}

const ExecutorLibraryFile &shared_library()
{
    static_cast<void>(widest_gpu());
    static const ExecutorLibraryFile library(AV_EXECUTOR_LIBRARY);
    return library;
}

/* A user event set from off the application's thread, and what sets it. The
 * handover is a join rather than an overlap: the ICD's locks here record a
 * second owner rather than blocking one, and the driver and device core below
 * it are the single-threaded rig, so what this proves is that the ICD holds
 * nothing thread-local - not that two threads may be inside it at once, which
 * on Windows the real SRWLOCKs answer for. */
class GateSetter {
public:
    explicit GateSetter(cl_event gate) : gate_(gate), thread_(), answered_(CL_INVALID_EVENT) {}

    /* Runs the set on a thread of its own and waits for it. False when the
     * thread could not be started, which is not this test's subject. */
    bool set_from_another_thread();

    /* What the set answered, on the thread that made it. */
    cl_int answered() const { return answered_; }
    void   record(cl_int answered) { answered_ = answered; }
    cl_event gate() const { return gate_; }

private:
    cl_event  gate_;
    pthread_t thread_;
    cl_int    answered_;
};

/* An ABI boundary C requires: pthread hands a thread one pointer and there is
 * nowhere to put a `this`. */
void *set_the_gate(void *self)
{
    GateSetter &setter = *static_cast<GateSetter *>(self);
    setter.record(table_of(setter.gate())->clSetUserEventStatus(setter.gate(), CL_COMPLETE));
    return nullptr;
}

bool GateSetter::set_from_another_thread()
{
    if (pthread_create(&thread_, nullptr, &set_the_gate, this) != 0) return false;
    return pthread_join(thread_, nullptr) == 0;
}

/* A callback that completes a user event of its own, which is the third way
 * US3 says a gate may be set: from inside the ICD's own delivery. It records
 * what it found the ICD's two locks in while it ran, because "with no ICD
 * lock held" is the thing that keeps a callback from deadlocking. */
class GateFromCallback {
public:
    GateFromCallback(cl_event gate, RecordingThreadLock &lock, RecordingThreadLock &send_lock)
        : gate_(gate), lock_(lock), send_lock_(send_lock), calls_(0u), status_(CL_QUEUED),
          answered_(CL_INVALID_EVENT), held_(true)
    {
    }

    void arrived(cl_int status)
    {
        ++calls_;
        status_ = status;
        held_ = lock_.held() || send_lock_.held();
        /* Straight back into the ICD, from the watcher's own thread: a
         * callback run under either lock would deadlock here. */
        answered_ = table_of(gate_)->clSetUserEventStatus(gate_, CL_COMPLETE);
    }

    std::uint32_t calls() const { return calls_; }
    cl_int        status() const { return status_; }
    cl_int        answered() const { return answered_; }
    bool          under_a_lock() const { return held_; }

private:
    cl_event             gate_;
    RecordingThreadLock &lock_;
    RecordingThreadLock &send_lock_;
    std::uint32_t        calls_;
    cl_int               status_;
    cl_int               answered_;
    bool                 held_;
};

/* An ABI boundary C requires: clSetEventCallback takes a bare function and one
 * argument, so the object is the argument and this forwards into it. */
void CL_CALLBACK callback_arrived(cl_event, cl_int status, void *user_data)
{
    static_cast<GateFromCallback *>(user_data)->arrived(status);
}

}  // namespace

/* T031, US1: the loader's own seven steps find this ICD, its one device is the
 * avgpu adapter's by LUID and vendor and the host GPU's by name - the same
 * bytes the executor answers with no guest in between - and a kernel built
 * from source on it adds two vectors the guest wrote and reads them back. */
TEST(fullstack_opencl_icd, the_loader_finds_the_device_the_executor_names_and_runs_a_kernel_on_it)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) {
      std::fprintf(stderr, "host runs no OpenCL; nothing to judge\n");
      return;
  }
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  char expected_name[device_name_bytes] = {};
  { HostClDevice host(*library.ops(), gpu.uuid());
    CHECK(host.name(expected_name, sizeof expected_name)); }
  std::fprintf(stderr, "host CL device: %s\n", expected_name);

  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    /* Steps 1 to 7 of contracts/icd_interface.md, which is what a loader does
     * before an application has asked for anything. */
    CHECK(loader.load() == FakeClLoader::Load::kept);
    CHECK_EQ(loader.platforms(), 1u);
    CHECK(std::strcmp(loader.suffix(), "AV") == 0);

    ClVectorAdd run(loader);
    if (!run.make()) return;

    /* The name is the host's, byte for byte: an ICD that answered its own
     * would be telling an application it is on a device that does not exist. */
    char   name[device_name_bytes] = {};
    size_t bytes = 0u;
    CHECK_EQ(loader.device_info(run.device(), CL_DEVICE_NAME, sizeof name, name, &bytes),
             CL_SUCCESS);
    CHECK(std::strcmp(name, expected_name) == 0);
    CHECK_EQ(bytes, std::strlen(expected_name) + 1u);

    /* And the identity is the avgpu adapter's, not the host GPU's. */
    cl_uint vendor = 0u;
    CHECK_EQ(loader.device_info(run.device(), CL_DEVICE_VENDOR_ID, sizeof vendor, &vendor, nullptr),
             CL_SUCCESS);
    CHECK_EQ(vendor, 0x1AF4u);
    cl_uchar luid[CL_LUID_SIZE_KHR] = {};
    CHECK_EQ(loader.device_info(run.device(), CL_DEVICE_LUID_KHR, sizeof luid, luid, nullptr),
             CL_SUCCESS);
    std::uint64_t named = 0u;
    std::memcpy(&named, luid, sizeof named);
    CHECK_EQ(named, DdiKmt::rig_luid);
    /* Capped at what this ICD implements, whatever the host answers. */
    char version[64] = {};
    CHECK_EQ(loader.device_info(run.device(), CL_DEVICE_VERSION, sizeof version, version, nullptr),
             CL_SUCCESS);
    CHECK(std::strncmp(version, "OpenCL 3.0", 10u) == 0);

    cl_event ran = nullptr;
    CHECK_EQ(run.enqueue(&ran), CL_SUCCESS);
    CHECK_EQ(run.settle(ran), CL_SUCCESS);
    float read[elements];
    std::memset(read, 0xEE, sizeof read);
    cl_event delivered = nullptr;
    CHECK_EQ(run.read(read, CL_FALSE, &delivered), CL_SUCCESS);
    CHECK_EQ(run.settle(delivered), CL_SUCCESS);
    CHECK_EQ(run.wrong(read), 0u);
    CHECK(process.root().invariants_hold());
    for (std::uint32_t index = 0u; index < process.log().lines(); index++)
        std::fprintf(stderr, "%s\n", process.log().line(index)); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

/* T040, US2: a buffer mapped, written through and given back is what the
 * kernel then reads - the whole point of a map, and the one thing a staged
 * mapping that never wrote back would fail. Then shared memory: an allocation
 * that survives a map, a write into the mirror, an unmap and being a kernel's
 * argument. What it deliberately does not assert is that the kernel sees what
 * was written into the mirror: no command on this wire carries bytes between
 * guest memory and a host SVM address (research 052 §7.3), the ICD says so
 * once per allocation, and asserting otherwise would be asserting a thing the
 * feature records as not built. */
TEST(fullstack_opencl_icd, a_mapped_write_reaches_the_kernel_and_shared_memory_is_an_argument)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);
    ClVectorAdd run(loader);
    if (!run.make()) return;
    const cl_icd_dispatch *const table = table_of(run.queue());

    /* Every element replaced through the mapping, by values the run's own
     * inputs never hold, so a kernel reading the old bytes shows. */
    float replaced[elements];
    for (std::uint32_t index = 0u; index < elements; ++index)
        replaced[index] = static_cast<float>(1000u + index);

    cl_int error = CL_INVALID_VALUE;
    void *const mapped = table->clEnqueueMapBuffer(run.queue(), run.input(), CL_TRUE, CL_MAP_WRITE,
                                                   0u, vector_bytes, 0u, nullptr, nullptr, &error);
    CHECK_EQ(error, CL_SUCCESS);
    CHECK(mapped != nullptr);
    if (mapped == nullptr) return;
    std::memcpy(mapped, replaced, sizeof replaced);
    /* A pointer that is not the one the map answered is refused, and the
     * mapping survives being asked. */
    CHECK_EQ(table->clEnqueueUnmapMemObject(run.queue(), run.input(),
                                            static_cast<std::uint8_t *>(mapped) + 4u, 0u, nullptr,
                                            nullptr),
             CL_INVALID_VALUE);
    const cl_int given_back =
        table->clEnqueueUnmapMemObject(run.queue(), run.input(), mapped, 0u, nullptr, nullptr);
    CHECK_EQ(given_back, CL_SUCCESS);
    /* A failed unmap has taken the context with it, so every later call would
     * fail for that reason rather than its own. */
    if (given_back != CL_SUCCESS) return;

    cl_event ran = nullptr;
    CHECK_EQ(run.enqueue(&ran), CL_SUCCESS);
    CHECK_EQ(run.settle(ran), CL_SUCCESS);
    float read[elements];
    std::memset(read, 0xEE, sizeof read);
    cl_event delivered = nullptr;
    CHECK_EQ(run.read(read, CL_FALSE, &delivered), CL_SUCCESS);
    CHECK_EQ(run.settle(delivered), CL_SUCCESS);
    float wanted[elements] = {};
    run.sum_with(replaced, wanted);
    std::uint32_t wrong = 0u;
    for (std::uint32_t index = 0u; index < elements; ++index)
        if (read[index] != wanted[index]) ++wrong;
    CHECK_EQ(wrong, 0u);
    /* And they are the mapped bytes rather than the buffer's original input,
     * which a map that never wrote back would have left: 0 + 1. */
    CHECK(read[0] != 1.0f);

    /* Shared memory, where the device offers it. A device that offers none is
     * not a failure; it is a device without it, and the run says so. */
    cl_device_svm_capabilities svm = 0u;
    CHECK_EQ(loader.device_info(run.device(), CL_DEVICE_SVM_CAPABILITIES, sizeof svm, &svm, nullptr),
             CL_SUCCESS);
    if (svm == 0u) {
        std::fprintf(stderr, "host device offers no shared memory; nothing to judge\n");
        CHECK(process.root().invariants_hold());
        return;
    }
    CHECK_EQ(svm, static_cast<cl_device_svm_capabilities>(CL_DEVICE_SVM_COARSE_GRAIN_BUFFER));
    void *const shared = table->clSVMAlloc(run.context(), CL_MEM_READ_WRITE, vector_bytes, 0u);
    CHECK(shared != nullptr);
    if (shared == nullptr) return;
    CHECK_EQ(table->clEnqueueSVMMap(run.queue(), CL_TRUE, CL_MAP_WRITE, shared, vector_bytes, 0u,
                                    nullptr, nullptr),
             CL_SUCCESS);
    std::memcpy(shared, replaced, sizeof replaced);
    CHECK_EQ(table->clEnqueueSVMUnmap(run.queue(), shared, 0u, nullptr, nullptr), CL_SUCCESS);
    /* The address the application holds is the mirror; what crosses the wire
     * is the host's, and an address inside no allocation never reaches it. */
    CHECK_EQ(table->clSetKernelArgSVMPointer(run.kernel(), 2u, shared), CL_SUCCESS);
    CHECK_EQ(table->clSetKernelArgSVMPointer(run.kernel(), 2u,
                                             static_cast<std::uint8_t *>(shared) + vector_bytes),
             CL_INVALID_ARG_VALUE);
    /* The output buffer goes back into the slot this borrowed, so the release
     * below frees a kernel whose arguments are the objects it was made with. */
    const cl_mem output = run.output();
    CHECK_EQ(table->clSetKernelArg(run.kernel(), 2u, sizeof(cl_mem), &output), CL_SUCCESS);
    table->clSVMFree(run.context(), shared);
    /* Said once, rather than pretended: the mirror and the host's view of a
     * coarse-grained allocation are not synchronised on this wire. */
    CHECK_EQ(process.log().holding("not synchronised"), 1u);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

/* T051, US3, scenario 1: a non-blocking read the application does not wait
 * for, gated on a user event the same guest thread completes afterwards. The
 * enqueue has to come back with that gate still outstanding - an ICD that made
 * the transfer blocking, or that waited on the gate's host event inside the
 * enqueue, would never reach the line that sets it. The deadline is what turns
 * that from a hang into a failure. */
TEST(fullstack_opencl_icd, a_non_blocking_read_gated_on_a_user_event_this_thread_sets_completes)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);
    ClVectorAdd run(loader);
    if (!run.make()) return;

    cl_int error = CL_INVALID_VALUE;
    cl_event gate = loader.create_user_event(run.context(), &error);
    CHECK_EQ(error, CL_SUCCESS);
    CHECK(gate != nullptr);
    if (gate == nullptr) return;

    CHECK_EQ(run.enqueue(nullptr), CL_SUCCESS);
    float read[elements];
    std::memset(read, 0xEE, sizeof read);
    cl_event delivered = nullptr;
    CHECK_EQ(run.read(read, CL_FALSE, &delivered), CL_SUCCESS);
    CHECK(delivered != nullptr);
    CHECK_EQ(run.flush(), CL_SUCCESS);

    /* The enqueue came back, and the gate it is nominally waiting on has not
     * been set: nothing in the guest is blocked on the application. */
    cl_int status = CL_COMPLETE;
    CHECK_EQ(loader.event_info(gate, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status,
                               nullptr),
             CL_SUCCESS);
    CHECK_EQ(status, CL_SUBMITTED);

    CHECK_EQ(loader.set_user_event_status(gate, CL_COMPLETE), CL_SUCCESS);
    CHECK_EQ(loader.wait_for_events(1u, &gate), CL_SUCCESS);
    CHECK_EQ(loader.wait_for_events(1u, &delivered), CL_SUCCESS);
    status = CL_QUEUED;
    CHECK_EQ(loader.event_info(delivered, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status,
                               nullptr),
             CL_SUCCESS);
    CHECK_EQ(status, CL_COMPLETE);
    CHECK_EQ(run.wrong(read), 0u);

    CHECK_EQ(loader.release_event(delivered), CL_SUCCESS);
    CHECK_EQ(loader.release_event(gate), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

/* T051, US3, scenario 2: the same, with the gate set from a thread that is not
 * the application's. What would hang is an ICD holding its transport's lock
 * across the wait, or keeping any of the state a set touches on the thread
 * that made it. */
TEST(fullstack_opencl_icd, a_non_blocking_read_gated_on_a_user_event_another_thread_sets_completes)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);
    ClVectorAdd run(loader);
    if (!run.make()) return;

    cl_int error = CL_INVALID_VALUE;
    cl_event gate = loader.create_user_event(run.context(), &error);
    CHECK_EQ(error, CL_SUCCESS);
    if (gate == nullptr) return;

    CHECK_EQ(run.enqueue(nullptr), CL_SUCCESS);
    float read[elements];
    std::memset(read, 0xEE, sizeof read);
    cl_event delivered = nullptr;
    CHECK_EQ(run.read(read, CL_FALSE, &delivered), CL_SUCCESS);
    CHECK_EQ(run.flush(), CL_SUCCESS);

    GateSetter setter(gate);
    CHECK(setter.set_from_another_thread());
    CHECK_EQ(setter.answered(), CL_SUCCESS);
    cl_int status = CL_QUEUED;
    CHECK_EQ(loader.event_info(gate, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status,
                               nullptr),
             CL_SUCCESS);
    CHECK_EQ(status, CL_COMPLETE);

    CHECK_EQ(loader.wait_for_events(1u, &gate), CL_SUCCESS);
    CHECK_EQ(loader.wait_for_events(1u, &delivered), CL_SUCCESS);
    CHECK_EQ(run.wrong(read), 0u);

    CHECK_EQ(loader.release_event(delivered), CL_SUCCESS);
    CHECK_EQ(loader.release_event(gate), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

/* T051, US3, scenario 3: the gate is set from inside a callback, which is the
 * one path where the ICD is re-entered from its own watcher. A callback run
 * under either ICD lock would deadlock on a real SRWLOCK; here the lock trace
 * says outright that none was held while it ran. */
TEST(fullstack_opencl_icd, a_user_event_set_from_inside_a_callback_releases_the_wait)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);
    ClVectorAdd run(loader);
    if (!run.make()) return;

    cl_int error = CL_INVALID_VALUE;
    cl_event gate = loader.create_user_event(run.context(), &error);
    CHECK_EQ(error, CL_SUCCESS);
    if (gate == nullptr) return;

    /* The work whose completion the callback hangs off. */
    cl_event ran = nullptr;
    CHECK_EQ(run.enqueue(&ran), CL_SUCCESS);
    CHECK(ran != nullptr);
    float read[elements];
    std::memset(read, 0xEE, sizeof read);
    cl_event delivered = nullptr;
    CHECK_EQ(run.read(read, CL_FALSE, &delivered), CL_SUCCESS);

    GateFromCallback watched(gate, process.lock(), process.send_lock());
    CHECK_EQ(loader.set_event_callback(delivered, CL_COMPLETE, &callback_arrived, &watched),
             CL_SUCCESS);
    CHECK_EQ(watched.calls(), 0u);
    /* The work is pushed and reached before the watcher is turned, so that a
     * turn that runs no callback is the ICD's answer rather than a race. */
    CHECK_EQ(run.flush(), CL_SUCCESS);
    CHECK_EQ(loader.wait_for_events(1u, &delivered), CL_SUCCESS);
    /* The watcher's own thread, turned by the test: one turn delivers the one
     * registration, and the thread goes on - it rests rather than ends, so the
     * next registration finds it. */
    CHECK(process.watcher().step());
    CHECK_EQ(process.watcher().turns(), 1u);
    CHECK_EQ(watched.calls(), 1u);
    CHECK_EQ(watched.status(), CL_COMPLETE);
    CHECK_EQ(watched.answered(), CL_SUCCESS);
    /* Neither the ICD's lock nor its transport's was held while the callback
     * ran, which is what lets the callback call back in at all. */
    CHECK(!watched.under_a_lock());

    /* Only where the callback ran: a wait on a gate nothing set would hang
     * instead of reporting the callback that did not arrive. */
    if (watched.calls() == 1u) CHECK_EQ(loader.wait_for_events(1u, &gate), CL_SUCCESS);
    CHECK_EQ(run.wrong(read), 0u);

    CHECK_EQ(loader.release_event(ran), CL_SUCCESS);
    CHECK_EQ(loader.release_event(delivered), CL_SUCCESS);
    CHECK_EQ(loader.release_event(gate), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

/* T065, US2: the read an application asks for blocking. Every other read here
 * is the deferred kind, whose bytes are fetched once the marker says the queue
 * has reached them; a blocking read is answered inside the one call, so it is
 * the only read whose bytes the host must already have written when it
 * serialises the reply. This is the guest probe's own sequence - write, run,
 * clFinish, clEnqueueReadBuffer with CL_TRUE - and a device asked to read
 * without blocking answers it with whatever its decode arena last held. */
TEST(fullstack_opencl_icd, a_blocking_read_answers_with_the_bytes_the_kernel_wrote)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) {
      std::fprintf(stderr, "host runs no OpenCL; nothing to judge\n");
      return;
  }
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;

  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);

    ClVectorAdd run(loader);
    if (!run.make()) return;

    CHECK_EQ(run.enqueue(nullptr), CL_SUCCESS);
    CHECK_EQ(run.finish(), CL_SUCCESS);
    float read[elements];
    std::memset(read, 0xEE, sizeof read);
    /* No event: an application that blocks wants the bytes, not a handle. */
    CHECK_EQ(run.read(read, CL_TRUE, nullptr), CL_SUCCESS);
    CHECK_EQ(run.wrong(read), 0u);

    /* And once more with an event, because the two answer the read through
     * different halves of the call and only one of them was ever run. */
    std::memset(read, 0xEE, sizeof read);
    cl_event delivered = nullptr;
    CHECK_EQ(run.read(read, CL_TRUE, &delivered), CL_SUCCESS);
    CHECK_EQ(run.wrong(read), 0u);
    CHECK(delivered != nullptr);
    if (delivered != nullptr) CHECK_EQ(loader.release_event(delivered), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

/* cl_khr_d3d11_sharing on the host's own OpenCL (docs/d3d11-sharing.md): a
 * context made from a Direct3D 11 device, a buffer resource whose bytes a
 * kernel adds one to between acquire and release, and a texture whose rows -
 * further apart in the mapping than in the image - come back where they went
 * in. The Direct3D side is the tests' bridge; everything OpenCL is the host's. */
TEST(fullstack_opencl_icd, a_d3d11_resource_s_bytes_reach_the_kernel_and_come_back_changed)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) {
      std::fprintf(stderr, "host runs no OpenCL; nothing to judge\n");
      return;
  }
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;

  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);
    cl_device_id device = nullptr;
    cl_uint      devices = 0u;
    CHECK_EQ(loader.device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices), CL_SUCCESS);
    if (device == nullptr) return;
    /* The Direct3D device is on the adapter the device says it is on. */
    std::uint64_t luid = 0u;
    CHECK_EQ(loader.device_info(device, CL_DEVICE_LUID_KHR, sizeof luid, &luid, nullptr), CL_SUCCESS);
    int d3d_device = 0, buffer_resource = 0, texture_resource = 0;
    process.d3d_adapters().name_device(&d3d_device, luid);
    process.d3d11().name_device(&d3d_device);
    constexpr std::size_t bytes = 256u;
    std::uint8_t          buffer_bytes[bytes];
    for (std::size_t index = 0u; index < bytes; ++index) buffer_bytes[index] = static_cast<std::uint8_t>(index);
    process.d3d11().name_resource(&buffer_resource, &d3d_device,
                                  av::clicd::D3d11Resource(av::clicd::D3d11Resource::Kind::buffer, 0u, bytes, 0u, 0u,
                                                           0u, 1u, 1u, 1u),
                                  buffer_bytes, bytes, bytes, bytes);
    /* 8x4 R8G8B8A8_UNORM texels, 32 bytes a row, mapped 64 bytes apart. */
    constexpr std::size_t pitch = 64u, rows = 4u, row_bytes = 32u;
    std::uint8_t          texels[pitch * rows] = {};
    for (std::size_t row = 0u; row < rows; ++row)
        for (std::size_t at = 0u; at < row_bytes; ++at) texels[row * pitch + at] = static_cast<std::uint8_t>(row * 40u + at);
    std::uint8_t original[pitch * rows];
    std::memcpy(original, texels, sizeof texels);
    process.d3d11().name_resource(&texture_resource, &d3d_device,
                                  av::clicd::D3d11Resource(av::clicd::D3d11Resource::Kind::texture2d, 28u, 0u, 8u, 4u,
                                                           1u, 1u, 1u, 1u),
                                  texels, sizeof texels, pitch, pitch * rows);

    const cl_context_properties properties[] = {
        static_cast<cl_context_properties>(ClIcdRoot::d3d11_context_device),
        reinterpret_cast<cl_context_properties>(&d3d_device), 0};
    cl_int           error = CL_INVALID_VALUE;
    const cl_context context = table_of(device)->clCreateContext(properties, 1u, &device, nullptr, nullptr, &error);
    CHECK_EQ(error, CL_SUCCESS);
    if (context == nullptr) return;
    const cl_command_queue queue = loader.create_queue(context, device, &error);
    using FromBuffer = cl_mem(CL_API_CALL *)(cl_context, cl_mem_flags, void *, cl_int *);
    using FromTexture = cl_mem(CL_API_CALL *)(cl_context, cl_mem_flags, void *, cl_uint, cl_int *);
    using Objects = cl_int(CL_API_CALL *)(cl_command_queue, cl_uint, const cl_mem *, cl_uint, const cl_event *,
                                          cl_event *);
    const cl_mem buffer = reinterpret_cast<FromBuffer>(table_of(context)->clCreateFromD3D11BufferKHR)(
        context, CL_MEM_READ_WRITE, &buffer_resource, &error);
    CHECK_EQ(error, CL_SUCCESS);
    const cl_mem image = reinterpret_cast<FromTexture>(table_of(context)->clCreateFromD3D11Texture2DKHR)(
        context, CL_MEM_READ_WRITE, &texture_resource, 0u, &error);
    CHECK_EQ(error, CL_SUCCESS);
    if (buffer == nullptr || image == nullptr || queue == nullptr) return;

    const cl_program program = loader.create_program(
        context, "__kernel void one_more(__global uchar *bytes) { bytes[get_global_id(0)] += 1; }", &error);
    CHECK_EQ(loader.build_program(program, nullptr), CL_SUCCESS);
    const cl_kernel kernel = loader.create_kernel(program, "one_more", &error);
    CHECK_EQ(table_of(kernel)->clSetKernelArg(kernel, 0u, sizeof buffer, &buffer), CL_SUCCESS);

    const cl_mem both[2] = {buffer, image};
    CHECK_EQ(reinterpret_cast<Objects>(table_of(queue)->clEnqueueAcquireD3D11ObjectsKHR)(queue, 2u, both, 0u, nullptr,
                                                                                         nullptr),
             CL_SUCCESS);
    const std::size_t global = bytes;
    CHECK_EQ(table_of(queue)->clEnqueueNDRangeKernel(queue, kernel, 1u, nullptr, &global, nullptr, 0u, nullptr,
                                                     nullptr),
             CL_SUCCESS);
    std::memset(texels, 0xEE, sizeof texels);
    CHECK_EQ(reinterpret_cast<Objects>(table_of(queue)->clEnqueueReleaseD3D11ObjectsKHR)(queue, 2u, both, 0u, nullptr,
                                                                                         nullptr),
             CL_SUCCESS);
    std::uint32_t wrong = 0u;
    for (std::size_t index = 0u; index < bytes; ++index)
        if (buffer_bytes[index] != static_cast<std::uint8_t>(index + 1u)) ++wrong;
    CHECK_EQ(wrong, 0u);
    for (std::size_t row = 0u; row < rows; ++row)
        CHECK_EQ(std::memcmp(texels + row * pitch, original + row * pitch, row_bytes), 0);

    CHECK_EQ(table_of(kernel)->clReleaseKernel(kernel), CL_SUCCESS);
    CHECK_EQ(table_of(program)->clReleaseProgram(program), CL_SUCCESS);
    CHECK_EQ(loader.release_mem(buffer), CL_SUCCESS);
    CHECK_EQ(loader.release_mem(image), CL_SUCCESS);
    CHECK_EQ(loader.release_queue(queue), CL_SUCCESS);
    CHECK_EQ(loader.release_context(context), CL_SUCCESS);
    CHECK_EQ(process.d3d11().references(&buffer_resource), 0);
    CHECK_EQ(process.d3d11().references(&d3d_device), 0);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

/* cl_khr_byte_addressable_store, through the ICD on the host's own OpenCL:
 * each work-item stores one byte, and none disturbs its neighbours - what a
 * device without byte stores would get wrong as soon as two work-items share
 * a word. */
TEST(fullstack_opencl_icd, a_kernel_stores_single_bytes_and_leaves_their_neighbours)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) {
      std::fprintf(stderr, "host runs no OpenCL; nothing to judge\n");
      return;
  }
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;

  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);
    cl_device_id device = nullptr;
    cl_uint      devices = 0u;
    CHECK_EQ(loader.device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices), CL_SUCCESS);
    if (device == nullptr) return;
    char extensions[4096] = {};
    CHECK_EQ(loader.device_info(device, CL_DEVICE_EXTENSIONS, sizeof extensions, extensions, nullptr), CL_SUCCESS);
    CHECK(std::strstr(extensions, "cl_khr_byte_addressable_store") != nullptr);
    cl_int           error = CL_INVALID_VALUE;
    const cl_context context = loader.create_context(device, &error);
    if (context == nullptr) return;
    const cl_command_queue queue = loader.create_queue(context, device, &error);
    constexpr std::size_t  bytes = 257u;
    const cl_mem           buffer = loader.create_buffer(context, CL_MEM_READ_WRITE, bytes, &error);
    std::uint8_t           cleared[bytes] = {};
    CHECK_EQ(table_of(queue)->clEnqueueWriteBuffer(queue, buffer, CL_TRUE, 0u, bytes, cleared, 0u, nullptr, nullptr),
             CL_SUCCESS);
    const cl_program program = loader.create_program(
        context, "__kernel void one_each(__global uchar *bytes) { size_t i = get_global_id(0); bytes[i] = (uchar)(i * 7u + 1u); }",
        &error);
    CHECK_EQ(loader.build_program(program, nullptr), CL_SUCCESS);
    const cl_kernel kernel = loader.create_kernel(program, "one_each", &error);
    CHECK_EQ(table_of(kernel)->clSetKernelArg(kernel, 0u, sizeof buffer, &buffer), CL_SUCCESS);
    const std::size_t global = bytes;
    CHECK_EQ(table_of(queue)->clEnqueueNDRangeKernel(queue, kernel, 1u, nullptr, &global, nullptr, 0u, nullptr,
                                                     nullptr),
             CL_SUCCESS);
    std::uint8_t read[bytes] = {};
    CHECK_EQ(table_of(queue)->clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0u, bytes, read, 0u, nullptr, nullptr),
             CL_SUCCESS);
    std::uint32_t wrong = 0u;
    for (std::size_t index = 0u; index < bytes; ++index)
        if (read[index] != static_cast<std::uint8_t>(index * 7u + 1u)) ++wrong;
    CHECK_EQ(wrong, 0u);
    CHECK_EQ(table_of(kernel)->clReleaseKernel(kernel), CL_SUCCESS);
    CHECK_EQ(table_of(program)->clReleaseProgram(program), CL_SUCCESS);
    CHECK_EQ(loader.release_mem(buffer), CL_SUCCESS);
    CHECK_EQ(loader.release_queue(queue), CL_SUCCESS);
    CHECK_EQ(loader.release_context(context), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

namespace {

/* Real code, and more of it than every room on the way: DaVinci Resolve's
 * image pipeline builds a 293 KB source, and the ICD answered it
 * CL_OUT_OF_RESOURCES because its command outgrew the 256 KiB the transport
 * lent it. Helpers the kernel never calls make the bulk, so a compiler judges
 * all of it and the build stays quick. */
constexpr std::uint32_t long_helpers = 30000u;
constexpr std::uint32_t long_defines = 12000u;

class LongProgram {
public:
    LongProgram()
        : helpers_(static_cast<char *>(std::malloc(std::size_t{long_helpers} * 72u))),
          options_(static_cast<char *>(std::malloc(std::size_t{long_defines} * 32u + 64u))), helpers_bytes_(0u),
          options_bytes_(0u)
    {
        for (std::uint32_t index = 0u; index < long_helpers; ++index)
            helpers_bytes_ += static_cast<std::size_t>(std::snprintf(
                helpers_ + helpers_bytes_, 72u, "inline float avcl_pad_%05u(float x) { return x + %u.0f; }\n", index,
                index));
        for (std::uint32_t index = 0u; index < long_defines; ++index)
            options_bytes_ += static_cast<std::size_t>(
                std::snprintf(options_ + options_bytes_, 32u, "-D AVCL_PAD_%06u=%u ", index, index));
        options_bytes_ += static_cast<std::size_t>(
            std::snprintf(options_ + options_bytes_, 64u, "-D AVCL_LAST=%u", long_defines));
    }
    ~LongProgram()
    {
        std::free(helpers_);
        std::free(options_);
    }
    LongProgram(const LongProgram &) = delete;
    LongProgram &operator=(const LongProgram &) = delete;

    const char *helpers() const { return helpers_; }
    std::size_t helpers_bytes() const { return helpers_bytes_; }
    const char *options() const { return options_; }
    std::size_t options_bytes() const { return options_bytes_; }

    /* The kernel, as a second string: c = a + b, through a helper of the
     * long first string and two of the long options' defines. */
    static constexpr const char *kernel =
        "__kernel void long_sum(__global const float *a, __global const float *b, __global float *c)\n"
        "{\n"
        "    const int i = get_global_id(0);\n"
        "    c[i] = avcl_pad_00007(a[i] + b[i]) - 7.0f + (float)(AVCL_LAST - AVCL_PAD_011999 - 1);\n"
        "}\n";

private:
    char       *helpers_;
    char       *options_;
    std::size_t helpers_bytes_;
    std::size_t options_bytes_;
};

}  // namespace

/* A program longer than the transport's room, the batch, the ring and the
 * executor's decode block, in two strings, built with options longer than a
 * room too: it builds, its kernel runs, and the context carries on - the
 * calls before and after are the ordinary ones. */
TEST(fullstack_opencl_icd, a_program_longer_than_every_room_builds_and_runs)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) {
      std::fprintf(stderr, "host runs no OpenCL; nothing to judge\n");
      return;
  }
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;

  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);
    cl_device_id device = nullptr;
    cl_uint      devices = 0u;
    CHECK_EQ(loader.device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices), CL_SUCCESS);
    if (device == nullptr) return;
    cl_int           error = CL_INVALID_VALUE;
    const cl_context context = loader.create_context(device, &error);
    if (context == nullptr) return;
    const cl_command_queue queue = loader.create_queue(context, device, &error);

    const LongProgram long_program;
    CHECK(long_program.helpers_bytes() > (std::size_t{1} << 20) + (std::size_t{512} << 10));
    CHECK(long_program.options_bytes() > std::size_t{256} << 10);
    const char *strings[2] = {long_program.helpers(), LongProgram::kernel};
    const std::size_t lengths[2] = {long_program.helpers_bytes(), 0u};
    error = CL_INVALID_VALUE;
    const cl_program program = table_of(context)->clCreateProgramWithSource(context, 2u, strings, lengths, &error);
    CHECK_EQ(error, CL_SUCCESS);
    CHECK(program != nullptr);
    if (program == nullptr) return;
    CHECK_EQ(table_of(program)->clBuildProgram(program, 0u, nullptr, long_program.options(), nullptr, nullptr),
             CL_SUCCESS);
    std::size_t logged = 0u;
    CHECK_EQ(table_of(program)->clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0u, nullptr, &logged),
             CL_SUCCESS);
    const cl_kernel kernel = loader.create_kernel(program, "long_sum", &error);
    CHECK_EQ(error, CL_SUCCESS);
    if (kernel == nullptr) return;

    float first[elements] = {};
    float second[elements] = {};
    for (std::uint32_t index = 0u; index < elements; ++index) {
        first[index] = static_cast<float>(index);
        second[index] = static_cast<float>(3u * index + 2u);
    }
    cl_mem buffers[3] = {};
    for (cl_mem &buffer : buffers) buffer = loader.create_buffer(context, CL_MEM_READ_WRITE, vector_bytes, &error);
    CHECK_EQ(table_of(queue)->clEnqueueWriteBuffer(queue, buffers[0], CL_TRUE, 0u, vector_bytes, first, 0u, nullptr,
                                                   nullptr),
             CL_SUCCESS);
    CHECK_EQ(table_of(queue)->clEnqueueWriteBuffer(queue, buffers[1], CL_TRUE, 0u, vector_bytes, second, 0u,
                                                   nullptr, nullptr),
             CL_SUCCESS);
    for (cl_uint index = 0u; index < 3u; ++index)
        CHECK_EQ(table_of(kernel)->clSetKernelArg(kernel, index, sizeof(cl_mem), &buffers[index]), CL_SUCCESS);
    const std::size_t global = elements;
    CHECK_EQ(table_of(queue)->clEnqueueNDRangeKernel(queue, kernel, 1u, nullptr, &global, nullptr, 0u, nullptr,
                                                     nullptr),
             CL_SUCCESS);
    float read[elements] = {};
    CHECK_EQ(table_of(queue)->clEnqueueReadBuffer(queue, buffers[2], CL_TRUE, 0u, vector_bytes, read, 0u, nullptr,
                                                  nullptr),
             CL_SUCCESS);
    std::uint32_t wrong = 0u;
    for (std::uint32_t index = 0u; index < elements; ++index)
        if (read[index] != first[index] + second[index]) ++wrong;
    CHECK_EQ(wrong, 0u);

    CHECK_EQ(table_of(kernel)->clReleaseKernel(kernel), CL_SUCCESS);
    CHECK_EQ(table_of(program)->clReleaseProgram(program), CL_SUCCESS);
    for (cl_mem buffer : buffers) CHECK_EQ(loader.release_mem(buffer), CL_SUCCESS);
    CHECK_EQ(loader.release_queue(queue), CL_SUCCESS);
    CHECK_EQ(loader.release_context(context), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }


namespace {

/* What a program says of itself that is the same however it was made. */
std::uint32_t program_answers_wrong(const cl_program program, const cl_context context, const cl_device_id device)
{
    std::uint32_t wrong = 0u;
    cl_uint       devices = 0u;
    cl_device_id  named = nullptr;
    cl_context    owner = nullptr;
    cl_uint       references = 0u;
    std::size_t   kernels = 0u;
    char          names[64] = {};
    const cl_icd_dispatch &table = *table_of(program);
    if (table.clGetProgramInfo(program, CL_PROGRAM_NUM_DEVICES, sizeof devices, &devices, nullptr) != CL_SUCCESS ||
        devices != 1u)
        ++wrong;
    if (table.clGetProgramInfo(program, CL_PROGRAM_DEVICES, sizeof named, &named, nullptr) != CL_SUCCESS ||
        named != device)
        ++wrong;
    if (table.clGetProgramInfo(program, CL_PROGRAM_CONTEXT, sizeof owner, &owner, nullptr) != CL_SUCCESS ||
        owner != context)
        ++wrong;
    if (table.clGetProgramInfo(program, CL_PROGRAM_REFERENCE_COUNT, sizeof references, &references, nullptr) !=
            CL_SUCCESS ||
        references != 1u)
        ++wrong;
    if (table.clGetProgramInfo(program, CL_PROGRAM_NUM_KERNELS, sizeof kernels, &kernels, nullptr) != CL_SUCCESS ||
        kernels != 1u)
        ++wrong;
    if (table.clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, sizeof names, names, nullptr) != CL_SUCCESS ||
        std::strcmp(names, "vector_add") != 0)
        ++wrong;
    return wrong;
}

/* A program's one binary, as an application caching it fetches it: the size,
 * then the bytes into memory of its own. Empty on any refusal. */
class FetchedBinary {
public:
    explicit FetchedBinary(cl_program program) : bytes_(nullptr), size_(0u)
    {
        if (table_of(program)->clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof size_, &size_, nullptr) !=
                CL_SUCCESS ||
            size_ == 0u)
            return;
        bytes_ = static_cast<unsigned char *>(std::malloc(size_));
        std::size_t pointers = 0u;
        if (bytes_ == nullptr ||
            table_of(program)->clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof bytes_, &bytes_, &pointers) !=
                CL_SUCCESS ||
            pointers != sizeof bytes_)
            size_ = 0u;
    }
    ~FetchedBinary() { std::free(bytes_); }
    FetchedBinary(const FetchedBinary &) = delete;
    FetchedBinary &operator=(const FetchedBinary &) = delete;

    unsigned char *bytes() const { return bytes_; }
    std::size_t    size() const { return size_; }

private:
    unsigned char *bytes_;
    std::size_t    size_;
};

/* What a program made from `bytes` answers, and the status it gives them. */
cl_int from_binary(cl_context context, cl_device_id device, const unsigned char *bytes, std::size_t size,
                   cl_int &status, cl_program &made)
{
    cl_int error = CL_SUCCESS;
    status = CL_SUCCESS;
    made = table_of(context)->clCreateProgramWithBinary(context, 1u, &device, &size, &bytes, &status, &error);
    return error;
}

/* Counts pfn_notify's calls and keeps what it was told, as an application
 * waiting on a build would. */
class BuildNotices {
public:
    BuildNotices() : calls_(0u), last_(nullptr) {}
    void heard(cl_program program)
    {
        ++calls_;
        last_ = program;
    }
    std::uint32_t calls() const { return calls_; }
    cl_program    last() const { return last_; }

private:
    std::uint32_t calls_;
    cl_program    last_;
};

/* An ABI boundary C requires: pfn_notify is a bare function with one
 * argument, so the object is the argument and this forwards into it. */
void CL_CALLBACK build_heard(cl_program program, void *user_data)
{
    static_cast<BuildNotices *>(user_data)->heard(program);
}

}  // namespace

/* An application that caches binaries (DaVinci Resolve, Affinity): the
 * program's binary fetched, made into a program again, built, and its kernel
 * run - and a binary damaged in its cache, cut short or another device's is
 * CL_INVALID_BINARY, which the application answers by building its source. */
TEST(fullstack_opencl_icd, a_program_s_binary_made_into_a_program_again_runs_and_a_damaged_one_is_refused)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);
    ClVectorAdd run(loader);
    if (!run.make()) return;
    CHECK_EQ(program_answers_wrong(run.program(), run.context(), run.device()), 0u);
    char        source[512] = {};
    std::size_t source_bytes = 0u;
    CHECK_EQ(table_of(run.program())->clGetProgramInfo(run.program(), CL_PROGRAM_SOURCE, sizeof source, source,
                                                       &source_bytes),
             CL_SUCCESS);
    CHECK_EQ(std::strcmp(source, vector_add), 0);

    const FetchedBinary binary(run.program());
    CHECK(binary.size() > 72u);
    if (binary.size() <= 72u) return;
    cl_int     status = CL_INVALID_VALUE;
    cl_program loaded = nullptr;
    CHECK_EQ(from_binary(run.context(), run.device(), binary.bytes(), binary.size(), status, loaded), CL_SUCCESS);
    CHECK_EQ(status, CL_SUCCESS);
    CHECK(loaded != nullptr);
    if (loaded == nullptr) return;
    BuildNotices       notices;
    const cl_device_id device = run.device();
    CHECK_EQ(table_of(loaded)->clBuildProgram(loaded, 1u, &device, nullptr, &build_heard, &notices),
             CL_SUCCESS);
    CHECK_EQ(notices.calls(), 1u);
    CHECK(notices.last() == loaded);
    cl_program_binary_type type = CL_PROGRAM_BINARY_TYPE_NONE;
    CHECK_EQ(table_of(loaded)->clGetProgramBuildInfo(loaded, run.device(), CL_PROGRAM_BINARY_TYPE, sizeof type, &type,
                                                     nullptr),
             CL_SUCCESS);
    CHECK_EQ(type, static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_EXECUTABLE));

    /* Every kernel of it at once, which is how Resolve takes them, and what
     * each says of itself. */
    cl_kernel kernel = nullptr;
    cl_uint   kernels = 0u;
    CHECK_EQ(table_of(loaded)->clCreateKernelsInProgram(loaded, 1u, &kernel, &kernels), CL_SUCCESS);
    CHECK_EQ(kernels, 1u);
    if (kernel == nullptr) return;
    char    name[32] = {};
    cl_uint arguments = 0u;
    cl_program owner = nullptr;
    CHECK_EQ(table_of(kernel)->clGetKernelInfo(kernel, CL_KERNEL_FUNCTION_NAME, sizeof name, name, nullptr),
             CL_SUCCESS);
    CHECK_EQ(std::strcmp(name, "vector_add"), 0);
    CHECK_EQ(table_of(kernel)->clGetKernelInfo(kernel, CL_KERNEL_NUM_ARGS, sizeof arguments, &arguments, nullptr),
             CL_SUCCESS);
    CHECK_EQ(arguments, 3u);
    CHECK_EQ(table_of(kernel)->clGetKernelInfo(kernel, CL_KERNEL_PROGRAM, sizeof owner, &owner, nullptr), CL_SUCCESS);
    CHECK(owner == loaded);
    cl_kernel_arg_address_qualifier address = 0u;
    CHECK_EQ(table_of(kernel)->clGetKernelArgInfo(kernel, 2u, CL_KERNEL_ARG_ADDRESS_QUALIFIER, sizeof address,
                                                  &address, nullptr),
             CL_SUCCESS);
    CHECK_EQ(address, static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_GLOBAL));
    CHECK_EQ(run.wrong_from(kernel), 0u);

    /* A clone carries the arguments set so far, and runs as its original. */
    cl_int    error = CL_INVALID_VALUE;
    cl_kernel clone = table_of(kernel)->clCloneKernel(kernel, &error);
    CHECK_EQ(error, CL_SUCCESS);
    CHECK_EQ(run.wrong_from(clone), 0u);

    /* Damaged: a byte of the payload flipped, the envelope's device changed,
     * a byte short, and bytes that were never a binary. */
    unsigned char *const bytes = binary.bytes();
    cl_program refused = nullptr;
    bytes[binary.size() - 1u] ^= 0x5Au;
    CHECK_EQ(from_binary(run.context(), run.device(), bytes, binary.size(), status, refused), CL_INVALID_BINARY);
    CHECK_EQ(status, CL_INVALID_BINARY);
    CHECK(refused == nullptr);
    bytes[binary.size() - 1u] ^= 0x5Au;
    bytes[16] ^= 0x01u;
    CHECK_EQ(from_binary(run.context(), run.device(), bytes, binary.size(), status, refused), CL_INVALID_BINARY);
    bytes[16] ^= 0x01u;
    CHECK_EQ(from_binary(run.context(), run.device(), bytes, binary.size() - 1u, status, refused), CL_INVALID_BINARY);
    static const unsigned char never[] = "not a binary any driver made";
    CHECK_EQ(from_binary(run.context(), run.device(), never, sizeof never, status, refused), CL_INVALID_BINARY);
    /* And whole again, it is taken again. */
    cl_program again = nullptr;
    CHECK_EQ(from_binary(run.context(), run.device(), bytes, binary.size(), status, again), CL_SUCCESS);

    if (again != nullptr) CHECK_EQ(table_of(again)->clReleaseProgram(again), CL_SUCCESS);
    if (clone != nullptr) CHECK_EQ(table_of(clone)->clReleaseKernel(clone), CL_SUCCESS);
    CHECK_EQ(table_of(kernel)->clReleaseKernel(kernel), CL_SUCCESS);
    CHECK_EQ(table_of(loaded)->clReleaseProgram(loaded), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

/* Separate compilation: a helper compiled alone, a kernel compiled against a
 * header program naming it, and the two linked into a program whose kernel
 * runs - each step telling its pfn_notify once it is done. */
TEST(fullstack_opencl_icd, two_programs_compiled_apart_and_linked_run_as_one)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);
    ClVectorAdd run(loader);
    if (!run.make()) return;
    cl_int error = CL_INVALID_VALUE;
    const cl_program helper = loader.create_program(run.context(), "float added(float a, float b) { return a + b; }\n",
                                                    &error);
    const cl_program declared = loader.create_program(run.context(), "float added(float a, float b);\n", &error);
    const cl_program kernel_source = loader.create_program(
        run.context(),
        "#include \"added.h\"\n"
        "__kernel void vector_add(__global const float *a, __global const float *b, __global float *c)\n"
        "{\n"
        "    const int i = get_global_id(0);\n"
        "    c[i] = added(a[i], b[i]);\n"
        "}\n",
        &error);
    CHECK(helper != nullptr && declared != nullptr && kernel_source != nullptr);
    if (helper == nullptr || declared == nullptr || kernel_source == nullptr) return;
    BuildNotices notices;
    CHECK_EQ(table_of(helper)->clCompileProgram(helper, 0u, nullptr, nullptr, 0u, nullptr, nullptr, &build_heard,
                                                &notices),
             CL_SUCCESS);
    const char        *header_name = "added.h";
    const cl_device_id device = run.device();
    CHECK_EQ(table_of(kernel_source)->clCompileProgram(kernel_source, 1u, &device, nullptr, 1u, &declared,
                                                       &header_name, &build_heard, &notices),
             CL_SUCCESS);
    CHECK_EQ(notices.calls(), 2u);
    CHECK(notices.last() == kernel_source);
    cl_program_binary_type type = CL_PROGRAM_BINARY_TYPE_NONE;
    CHECK_EQ(table_of(helper)->clGetProgramBuildInfo(helper, run.device(), CL_PROGRAM_BINARY_TYPE, sizeof type, &type,
                                                     nullptr),
             CL_SUCCESS);
    CHECK_EQ(type, static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT));

    const cl_program inputs[2] = {helper, kernel_source};
    const cl_program linked = table_of(run.context())->clLinkProgram(run.context(), 0u, nullptr, nullptr, 2u, inputs,
                                                                    &build_heard, &notices, &error);
    CHECK_EQ(error, CL_SUCCESS);
    CHECK(linked != nullptr);
    CHECK_EQ(notices.calls(), 3u);
    CHECK(notices.last() == linked);
    if (linked == nullptr) return;
    CHECK_EQ(program_answers_wrong(linked, run.context(), run.device()), 0u);
    const cl_kernel kernel = loader.create_kernel(linked, "vector_add", &error);
    CHECK_EQ(run.wrong_from(kernel), 0u);

    /* A link of nothing, and of something that is not a program. */
    CHECK(table_of(run.context())->clLinkProgram(run.context(), 0u, nullptr, nullptr, 0u, nullptr, nullptr, nullptr,
                                                 &error) == nullptr);
    CHECK_EQ(error, CL_INVALID_VALUE);
    const cl_program not_a_program[1] = {reinterpret_cast<cl_program>(run.queue())};
    CHECK(table_of(run.context())->clLinkProgram(run.context(), 0u, nullptr, nullptr, 1u, not_a_program, nullptr,
                                                 nullptr, &error) == nullptr);
    CHECK_EQ(error, CL_INVALID_PROGRAM);

    if (kernel != nullptr) CHECK_EQ(table_of(kernel)->clReleaseKernel(kernel), CL_SUCCESS);
    for (const cl_program program : {linked, kernel_source, declared, helper})
        CHECK_EQ(table_of(program)->clReleaseProgram(program), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

/* SPIR-V on a host that takes it: the probe's own module, its vector add run
 * over buffers, and offset_add's eight private bytes - which a host keeping no
 * type names for IL cannot tell from a sampler - set as the ulong they are,
 * read off the module. Its binary goes round as any program's does. */
TEST(fullstack_opencl_icd, a_program_from_spir_v_runs_its_kernels_and_says_what_it_was_made_of)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  { ClProcess process(rig.kmt());
    FakeClLoader &loader = process.loader();
    CHECK(loader.load() == FakeClLoader::Load::kept);
    ClVectorAdd run(loader);
    if (!run.make()) return;
    char versions[256] = {};
    CHECK_EQ(loader.device_info(run.device(), CL_DEVICE_IL_VERSION, sizeof versions, versions, nullptr), CL_SUCCESS);
    cl_int           error = CL_INVALID_VALUE;
    const cl_program program = table_of(run.context())->clCreateProgramWithIL(
        run.context(), av::vector_add_spv, sizeof av::vector_add_spv, &error);
    if (std::strstr(versions, "SPIR-V") == nullptr) {
        std::fprintf(stderr, "the host takes no SPIR-V (%s); nothing more to judge\n", versions);
        CHECK(program == nullptr);
        return;
    }
    CHECK_EQ(error, CL_SUCCESS);
    if (program == nullptr) return;
    CHECK_EQ(loader.build_program(program, nullptr), CL_SUCCESS);
    std::uint32_t module[sizeof av::vector_add_spv / sizeof av::vector_add_spv[0]] = {};
    std::size_t   module_bytes = 0u;
    CHECK_EQ(table_of(program)->clGetProgramInfo(program, CL_PROGRAM_IL, sizeof module, module, &module_bytes),
             CL_SUCCESS);
    CHECK_EQ(module_bytes, sizeof module);
    CHECK_EQ(std::memcmp(module, av::vector_add_spv, sizeof module), 0);

    const cl_kernel added = loader.create_kernel(program, "vector_add", &error);
    CHECK_EQ(run.wrong_from(added), 0u);
    const cl_kernel offset = loader.create_kernel(program, "offset_add", &error);
    const std::uint64_t by = 1000u;
    CHECK_EQ(run.wrong_from(offset, &by), 0u);

    if (offset != nullptr) CHECK_EQ(table_of(offset)->clReleaseKernel(offset), CL_SUCCESS);
    if (added != nullptr) CHECK_EQ(table_of(added)->clReleaseKernel(added), CL_SUCCESS);
    CHECK_EQ(table_of(program)->clReleaseProgram(program), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }
