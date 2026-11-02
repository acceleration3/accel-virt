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
constexpr unsigned      patience_seconds = 120u;

constexpr const char *vector_add =
    "__kernel void vector_add(__global const float *a, __global const float *b,\n"
    "                        __global float *c)\n"
    "{\n"
    "    const int i = get_global_id(0);\n"
    "    c[i] = a[i] + b[i];\n"
    "}\n";

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

const cl_icd_dispatch *table_of(const void *object)
{
    return object != nullptr ? *reinterpret_cast<const cl_icd_dispatch *const *>(object) : nullptr;
}

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

class ClVectorAdd {
public:
    explicit ClVectorAdd(FakeClLoader &loader)
        : loader_(loader), device_(nullptr), context_(nullptr), queue_(nullptr), program_(nullptr),
          kernel_(nullptr), buffers_{}, first_{}, second_{}, sum_{}
    {
        for (std::uint32_t index = 0u; index < elements; ++index) {
            first_[index] = static_cast<float>(index);
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

    std::uint32_t wrong(const float *read) const
    {
        std::uint32_t count = 0u;
        for (std::uint32_t index = 0u; index < elements; ++index) {
            if (read[index] != sum_[index]) ++count;
        }
        return count;
    }

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

class GateSetter {
public:
    explicit GateSetter(cl_event gate) : gate_(gate), thread_(), answered_(CL_INVALID_EVENT) {}

    bool set_from_another_thread();

    cl_int answered() const { return answered_; }
    void   record(cl_int answered) { answered_ = answered; }
    cl_event gate() const { return gate_; }

private:
    cl_event  gate_;
    pthread_t thread_;
    cl_int    answered_;
};

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

void CL_CALLBACK callback_arrived(cl_event, cl_int status, void *user_data)
{
    static_cast<GateFromCallback *>(user_data)->arrived(status);
}

}

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
    CHECK(loader.load() == FakeClLoader::Load::kept);
    CHECK_EQ(loader.platforms(), 1u);
    CHECK(std::strcmp(loader.suffix(), "AV") == 0);

    ClVectorAdd run(loader);
    if (!run.make()) return;

    char   name[device_name_bytes] = {};
    size_t bytes = 0u;
    CHECK_EQ(loader.device_info(run.device(), CL_DEVICE_NAME, sizeof name, name, &bytes),
             CL_SUCCESS);
    CHECK(std::strcmp(name, expected_name) == 0);
    CHECK_EQ(bytes, std::strlen(expected_name) + 1u);

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
    CHECK_EQ(table->clEnqueueUnmapMemObject(run.queue(), run.input(),
                                            static_cast<std::uint8_t *>(mapped) + 4u, 0u, nullptr,
                                            nullptr),
             CL_INVALID_VALUE);
    const cl_int given_back =
        table->clEnqueueUnmapMemObject(run.queue(), run.input(), mapped, 0u, nullptr, nullptr);
    CHECK_EQ(given_back, CL_SUCCESS);
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
    CHECK(read[0] != 1.0f);

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
    CHECK_EQ(table->clSetKernelArgSVMPointer(run.kernel(), 2u, shared), CL_SUCCESS);
    CHECK_EQ(table->clSetKernelArgSVMPointer(run.kernel(), 2u,
                                             static_cast<std::uint8_t *>(shared) + vector_bytes),
             CL_INVALID_ARG_VALUE);
    const cl_mem output = run.output();
    CHECK_EQ(table->clSetKernelArg(run.kernel(), 2u, sizeof(cl_mem), &output), CL_SUCCESS);
    table->clSVMFree(run.context(), shared);
    CHECK_EQ(process.log().holding("not synchronised"), 1u);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

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
    CHECK_EQ(run.flush(), CL_SUCCESS);
    CHECK_EQ(loader.wait_for_events(1u, &delivered), CL_SUCCESS);
    CHECK(process.watcher().step());
    CHECK_EQ(process.watcher().turns(), 1u);
    CHECK_EQ(watched.calls(), 1u);
    CHECK_EQ(watched.status(), CL_COMPLETE);
    CHECK_EQ(watched.answered(), CL_SUCCESS);
    CHECK(!watched.under_a_lock());

    if (watched.calls() == 1u) CHECK_EQ(loader.wait_for_events(1u, &gate), CL_SUCCESS);
    CHECK_EQ(run.wrong(read), 0u);

    CHECK_EQ(loader.release_event(ran), CL_SUCCESS);
    CHECK_EQ(loader.release_event(delivered), CL_SUCCESS);
    CHECK_EQ(loader.release_event(gate), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

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
    CHECK_EQ(run.read(read, CL_TRUE, nullptr), CL_SUCCESS);
    CHECK_EQ(run.wrong(read), 0u);

    std::memset(read, 0xEE, sizeof read);
    cl_event delivered = nullptr;
    CHECK_EQ(run.read(read, CL_TRUE, &delivered), CL_SUCCESS);
    CHECK_EQ(run.wrong(read), 0u);
    CHECK(delivered != nullptr);
    if (delivered != nullptr) CHECK_EQ(loader.release_event(delivered), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

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

}

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

cl_int from_binary(cl_context context, cl_device_id device, const unsigned char *bytes, std::size_t size,
                   cl_int &status, cl_program &made)
{
    cl_int error = CL_SUCCESS;
    status = CL_SUCCESS;
    made = table_of(context)->clCreateProgramWithBinary(context, 1u, &device, &size, &bytes, &status, &error);
    return error;
}

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

void CL_CALLBACK build_heard(cl_program program, void *user_data)
{
    static_cast<BuildNotices *>(user_data)->heard(program);
}

}

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

    cl_int    error = CL_INVALID_VALUE;
    cl_kernel clone = table_of(kernel)->clCloneKernel(kernel, &error);
    CHECK_EQ(error, CL_SUCCESS);
    CHECK_EQ(run.wrong_from(clone), 0u);

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
    cl_program again = nullptr;
    CHECK_EQ(from_binary(run.context(), run.device(), bytes, binary.size(), status, again), CL_SUCCESS);

    if (again != nullptr) CHECK_EQ(table_of(again)->clReleaseProgram(again), CL_SUCCESS);
    if (clone != nullptr) CHECK_EQ(table_of(clone)->clReleaseKernel(clone), CL_SUCCESS);
    CHECK_EQ(table_of(kernel)->clReleaseKernel(kernel), CL_SUCCESS);
    CHECK_EQ(table_of(loaded)->clReleaseProgram(loaded), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

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
    static char extensions[8192] = {};
    CHECK_EQ(loader.device_info(run.device(), CL_DEVICE_EXTENSIONS, sizeof extensions, extensions, nullptr),
             CL_SUCCESS);
    CHECK(std::strstr(extensions, "cl_khr_il_program") != nullptr);
    CHECK(process.root().extension_function("clCreateProgramWithILKHR") ==
          reinterpret_cast<void *>(table_of(run.context())->clCreateProgramWithIL));
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

namespace {
class ExtensionRun {
public:
    explicit ExtensionRun(FakeClLoader &loader)
        : loader_(loader), device_(nullptr), context_(nullptr), queue_(nullptr), program_(nullptr), kernel_(nullptr),
          extensions_{}
    {
    }
    ~ExtensionRun()
    {
        if (kernel_ != nullptr) CHECK_EQ(table_of(kernel_)->clReleaseKernel(kernel_), CL_SUCCESS);
        if (program_ != nullptr) CHECK_EQ(table_of(program_)->clReleaseProgram(program_), CL_SUCCESS);
        if (queue_ != nullptr) CHECK_EQ(loader_.release_queue(queue_), CL_SUCCESS);
        if (context_ != nullptr) CHECK_EQ(loader_.release_context(context_), CL_SUCCESS);
    }
    ExtensionRun(const ExtensionRun &) = delete;
    ExtensionRun &operator=(const ExtensionRun &) = delete;

    bool offers(const char *extension)
    {
        if (device_ == nullptr) {
            cl_uint devices = 0u;
            CHECK_EQ(loader_.device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices), CL_SUCCESS);
            if (device_ == nullptr) return false;
            CHECK_EQ(loader_.device_info(device_, CL_DEVICE_EXTENSIONS, sizeof extensions_, extensions_, nullptr),
                     CL_SUCCESS);
        }
        if (!named(extensions_, extension)) {
            std::fprintf(stderr, "the host GPU has no %s; nothing to judge\n", extension);
            return false;
        }
        cl_int error = CL_INVALID_VALUE;
        if (context_ == nullptr) context_ = loader_.create_context(device_, &error);
        if (context_ == nullptr) return false;
        if (queue_ == nullptr) queue_ = loader_.create_queue(context_, device_, &error);
        return queue_ != nullptr;
    }

    bool kernel(const char *source, const char *name)
    {
        cl_int error = CL_INVALID_VALUE;
        program_ = loader_.create_program(context_, source, &error);
        CHECK_EQ(error, CL_SUCCESS);
        if (program_ == nullptr) return false;
        const cl_int built = loader_.build_program(program_, nullptr);
        CHECK_EQ(built, CL_SUCCESS);
        if (built != CL_SUCCESS) return false;
        kernel_ = loader_.create_kernel(program_, name, &error);
        CHECK_EQ(error, CL_SUCCESS);
        return kernel_ != nullptr;
    }

    cl_int run(std::size_t global, std::size_t local)
    {
        const cl_int ran = table_of(queue_)->clEnqueueNDRangeKernel(queue_, kernel_, 1u, nullptr, &global,
                                                                    local != 0u ? &local : nullptr, 0u, nullptr,
                                                                    nullptr);
        return ran == CL_SUCCESS ? loader_.finish(queue_) : ran;
    }

    cl_mem buffer(std::size_t bytes, const void *from)
    {
        cl_int       error = CL_INVALID_VALUE;
        const cl_mem made = loader_.create_buffer(context_, CL_MEM_READ_WRITE, bytes, &error);
        CHECK_EQ(error, CL_SUCCESS);
        if (made != nullptr && from != nullptr)
            CHECK_EQ(table_of(queue_)->clEnqueueWriteBuffer(queue_, made, CL_TRUE, 0u, bytes, from, 0u, nullptr,
                                                            nullptr),
                     CL_SUCCESS);
        return made;
    }

    cl_int read(cl_mem from, std::size_t bytes, void *into)
    {
        return table_of(queue_)->clEnqueueReadBuffer(queue_, from, CL_TRUE, 0u, bytes, into, 0u, nullptr, nullptr);
    }

    cl_int argument(cl_uint index, std::size_t bytes, const void *value)
    {
        return table_of(kernel_)->clSetKernelArg(kernel_, index, bytes, value);
    }

    cl_device_id     device() const { return device_; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }
    FakeClLoader    &loader() { return loader_; }

private:
    bool named(const char *list, const char *wanted) const
    {
        const std::size_t length = std::strlen(wanted);
        for (const char *at = std::strstr(list, wanted); at != nullptr; at = std::strstr(at + 1, wanted)) {
            const bool starts = at == list || at[-1] == ' ';
            const bool ends = at[length] == '\0' || at[length] == ' ';
            if (starts && ends) return true;
        }
        return false;
    }

    FakeClLoader    &loader_;
    cl_device_id     device_;
    cl_context       context_;
    cl_command_queue queue_;
    cl_program       program_;
    cl_kernel        kernel_;
    char             extensions_[4096];
};

bool runs_opencl(const WidestHostGpu &gpu)
{
    if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENCL) != 0u) return true;
    std::fprintf(stderr, "host runs no OpenCL; nothing to judge\n");
    return false;
}

constexpr std::uint32_t extension_items = 64u;

std::uint32_t wrong_in_precision(ExtensionRun &run, const char *source)
{
    if (!run.kernel(source, "precise")) return extension_items;
    float input[extension_items] = {};
    for (std::uint32_t index = 0u; index < extension_items; ++index) input[index] = static_cast<float>(index);
    const cl_mem in = run.buffer(sizeof input, input);
    const cl_mem out = run.buffer(sizeof input, nullptr);
    CHECK_EQ(run.argument(0u, sizeof in, &in), CL_SUCCESS);
    CHECK_EQ(run.argument(1u, sizeof out, &out), CL_SUCCESS);
    CHECK_EQ(run.run(extension_items, 0u), CL_SUCCESS);
    float read[extension_items] = {};
    CHECK_EQ(run.read(out, sizeof read, read), CL_SUCCESS);
    std::uint32_t wrong = 0u;
    for (std::uint32_t index = 0u; index < extension_items; ++index)
        if (read[index] != static_cast<float>(2u * index + 1u)) ++wrong;
    CHECK_EQ(run.loader().release_mem(in), CL_SUCCESS);
    CHECK_EQ(run.loader().release_mem(out), CL_SUCCESS);
    return wrong;
}

}

TEST(fullstack_opencl_icd, a_kernel_computes_in_half_precision_where_the_host_has_it)
{ const WidestHostGpu &gpu = widest_gpu();
  if (!runs_opencl(gpu)) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    CHECK(process.loader().load() == FakeClLoader::Load::kept);
    ExtensionRun run(process.loader());
    if (!run.offers("cl_khr_fp16")) return;
    CHECK_EQ(wrong_in_precision(run, "#pragma OPENCL EXTENSION cl_khr_fp16 : enable\n"
                                     "__kernel void precise(__global const float *in, __global float *out)\n"
                                     "{ size_t i = get_global_id(0); half h = (half)in[i];\n"
                                     "  out[i] = (float)(h * (half)2.0f + (half)1.0f); }\n"),
             0u); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(fullstack_opencl_icd, a_kernel_computes_in_double_precision_where_the_host_has_it)
{ const WidestHostGpu &gpu = widest_gpu();
  if (!runs_opencl(gpu)) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    CHECK(process.loader().load() == FakeClLoader::Load::kept);
    ExtensionRun run(process.loader());
    if (!run.offers("cl_khr_fp64")) return;
    CHECK_EQ(wrong_in_precision(run, "#pragma OPENCL EXTENSION cl_khr_fp64 : enable\n"
                                     "__kernel void precise(__global const float *in, __global float *out)\n"
                                     "{ size_t i = get_global_id(0); double d = (double)in[i];\n"
                                     "  out[i] = (float)(d * 2.0 + 1.0); }\n"),
             0u); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(fullstack_opencl_icd, a_kernel_s_32_bit_atomics_on_global_and_local_memory_all_land)
{ const WidestHostGpu &gpu = widest_gpu();
  if (!runs_opencl(gpu)) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    CHECK(process.loader().load() == FakeClLoader::Load::kept);
    ExtensionRun run(process.loader());
    if (!run.offers("cl_khr_global_int32_base_atomics") || !run.offers("cl_khr_global_int32_extended_atomics") ||
        !run.offers("cl_khr_local_int32_base_atomics") || !run.offers("cl_khr_local_int32_extended_atomics"))
        return;
    CHECK(run.kernel("#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable\n"
                     "#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable\n"
                     "#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable\n"
                     "#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable\n"
                     "__kernel void counts(__global int *g, __local int *l)\n"
                     "{ if (get_local_id(0) == 0) { l[0] = 0; l[1] = 0; }\n"
                     "  barrier(CLK_LOCAL_MEM_FENCE);\n"
                     "  atom_inc(&l[0]); atom_max(&l[1], (int)get_local_id(0));\n"
                     "  barrier(CLK_LOCAL_MEM_FENCE);\n"
                     "  if (get_local_id(0) == 0) { atom_add(&g[0], l[0]); atom_max(&g[1], l[1]);\n"
                     "                              atom_or(&g[2], 1 << get_group_id(0)); }\n"
                     "  atom_inc(&g[3]); }\n",
                     "counts"));
    const std::int32_t cleared[4] = {};
    const cl_mem       counts = run.buffer(sizeof cleared, cleared);
    CHECK_EQ(run.argument(0u, sizeof counts, &counts), CL_SUCCESS);
    CHECK_EQ(run.argument(1u, 2u * sizeof(std::int32_t), nullptr), CL_SUCCESS);
    constexpr std::size_t local = 16u;
    CHECK_EQ(run.run(extension_items, local), CL_SUCCESS);
    std::int32_t read[4] = {};
    CHECK_EQ(run.read(counts, sizeof read, read), CL_SUCCESS);
    CHECK_EQ(read[0], static_cast<std::int32_t>(extension_items));
    CHECK_EQ(read[1], static_cast<std::int32_t>(local - 1u));
    CHECK_EQ(read[2], (1 << (extension_items / local)) - 1);
    CHECK_EQ(read[3], static_cast<std::int32_t>(extension_items));
    CHECK_EQ(run.loader().release_mem(counts), CL_SUCCESS); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(fullstack_opencl_icd, a_kernel_s_64_bit_atomics_all_land_where_the_host_has_them)
{ const WidestHostGpu &gpu = widest_gpu();
  if (!runs_opencl(gpu)) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    CHECK(process.loader().load() == FakeClLoader::Load::kept);
    ExtensionRun run(process.loader());
    if (!run.offers("cl_khr_int64_base_atomics") || !run.offers("cl_khr_int64_extended_atomics")) return;
    CHECK(run.kernel("#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable\n"
                     "#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics : enable\n"
                     "__kernel void counts(__global long *g)\n"
                     "{ long i = (long)get_global_id(0);\n"
                     "  atom_add(&g[0], (1L << 33) + i); atom_max(&g[1], i << 40); }\n",
                     "counts"));
    const std::int64_t cleared[2] = {};
    const cl_mem       counts = run.buffer(sizeof cleared, cleared);
    CHECK_EQ(run.argument(0u, sizeof counts, &counts), CL_SUCCESS);
    CHECK_EQ(run.run(extension_items, 0u), CL_SUCCESS);
    std::int64_t read[2] = {};
    CHECK_EQ(run.read(counts, sizeof read, read), CL_SUCCESS);
    const std::int64_t items = extension_items;
    CHECK_EQ(read[0], items * (std::int64_t{1} << 33) + items * (items - 1) / 2);
    CHECK_EQ(read[1], (items - 1) << 40);
    CHECK_EQ(run.loader().release_mem(counts), CL_SUCCESS); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(fullstack_opencl_icd, a_kernel_writes_a_3d_image_and_its_texels_come_back)
{ const WidestHostGpu &gpu = widest_gpu();
  if (!runs_opencl(gpu)) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    CHECK(process.loader().load() == FakeClLoader::Load::kept);
    ExtensionRun run(process.loader());
    if (!run.offers("cl_khr_3d_image_writes")) return;
    CHECK(run.kernel("#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable\n"
                     "__kernel void fill(__write_only image3d_t image)\n"
                     "{ int i = get_global_id(0); int4 at = (int4)(i % 4, (i / 4) % 4, i / 16, 0);\n"
                     "  write_imageui(image, at, (uint4)(at.x, at.y, at.z, 200)); }\n",
                     "fill"));
    const cl_image_format format = {CL_RGBA, CL_UNSIGNED_INT8};
    cl_image_desc         desc = {};
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_width = 4u;
    desc.image_height = 4u;
    desc.image_depth = 4u;
    cl_int error = CL_INVALID_VALUE;
    const cl_mem image = table_of(run.context())->clCreateImage(run.context(), CL_MEM_WRITE_ONLY, &format, &desc,
                                                                nullptr, &error);
    CHECK_EQ(error, CL_SUCCESS);
    if (image == nullptr) return;
    CHECK_EQ(run.argument(0u, sizeof image, &image), CL_SUCCESS);
    CHECK_EQ(run.run(extension_items, 0u), CL_SUCCESS);
    std::uint8_t      texels[extension_items * 4u] = {};
    const std::size_t origin[3] = {0u, 0u, 0u};
    const std::size_t region[3] = {4u, 4u, 4u};
    CHECK_EQ(table_of(run.queue())->clEnqueueReadImage(run.queue(), image, CL_TRUE, origin, region, 0u, 0u, texels,
                                                       0u, nullptr, nullptr),
             CL_SUCCESS);
    std::uint32_t wrong = 0u;
    for (std::uint32_t index = 0u; index < extension_items; ++index) {
        const std::uint8_t *const texel = &texels[index * 4u];
        if (texel[0] != index % 4u || texel[1] != (index / 4u) % 4u || texel[2] != index / 16u || texel[3] != 200u)
            ++wrong;
    }
    CHECK_EQ(wrong, 0u);
    CHECK_EQ(run.loader().release_mem(image), CL_SUCCESS); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(fullstack_opencl_icd, a_2d_image_over_a_buffer_reads_the_buffer_s_bytes)
{ const WidestHostGpu &gpu = widest_gpu();
  if (!runs_opencl(gpu)) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    CHECK(process.loader().load() == FakeClLoader::Load::kept);
    ExtensionRun run(process.loader());
    if (!run.offers("cl_khr_image2d_from_buffer")) return;
    cl_uint pitch_alignment = 0u;
    CHECK_EQ(run.loader().device_info(run.device(), CL_DEVICE_IMAGE_PITCH_ALIGNMENT, sizeof pitch_alignment,
                                      &pitch_alignment, nullptr),
             CL_SUCCESS);
    constexpr std::size_t width = 16u;
    constexpr std::size_t height = 4u;
    const std::size_t     aligned = pitch_alignment != 0u ? pitch_alignment : 1u;
    const std::size_t     pitch = (width + aligned - 1u) / aligned * aligned * 4u;
    std::uint8_t          bytes[4096] = {};
    CHECK(pitch * height <= sizeof bytes);
    for (std::size_t y = 0u; y < height; ++y)
        for (std::size_t x = 0u; x < width * 4u; ++x) bytes[y * pitch + x] = static_cast<std::uint8_t>(x + 7u * y);
    const cl_mem          backing = run.buffer(pitch * height, bytes);
    const cl_image_format format = {CL_RGBA, CL_UNSIGNED_INT8};
    cl_image_desc         desc = {};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_row_pitch = pitch;
    desc.buffer = backing;
    cl_int error = CL_INVALID_VALUE;
    const cl_mem image = table_of(run.context())->clCreateImage(run.context(), CL_MEM_READ_ONLY, &format, &desc,
                                                                nullptr, &error);
    CHECK_EQ(error, CL_SUCCESS);
    if (image == nullptr) return;
    CHECK(run.kernel("__kernel void copy(__read_only image2d_t image, __global uchar4 *out)\n"
                     "{ int i = get_global_id(0); int2 at = (int2)(i % 16, i / 16);\n"
                     "  out[i] = convert_uchar4(read_imageui(image, at)); }\n",
                     "copy"));
    const cl_mem out = run.buffer(width * height * 4u, nullptr);
    CHECK_EQ(run.argument(0u, sizeof image, &image), CL_SUCCESS);
    CHECK_EQ(run.argument(1u, sizeof out, &out), CL_SUCCESS);
    CHECK_EQ(run.run(width * height, 0u), CL_SUCCESS);
    std::uint8_t read[width * height * 4u] = {};
    CHECK_EQ(run.read(out, sizeof read, read), CL_SUCCESS);
    std::uint32_t wrong = 0u;
    for (std::size_t y = 0u; y < height; ++y)
        for (std::size_t x = 0u; x < width * 4u; ++x)
            if (read[y * width * 4u + x] != bytes[y * pitch + x]) ++wrong;
    CHECK_EQ(wrong, 0u);
    CHECK_EQ(run.loader().release_mem(out), CL_SUCCESS);
    CHECK_EQ(run.loader().release_mem(image), CL_SUCCESS);
    CHECK_EQ(run.loader().release_mem(backing), CL_SUCCESS); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(fullstack_opencl_icd, a_depth_image_a_kernel_writes_reads_back)
{ const WidestHostGpu &gpu = widest_gpu();
  if (!runs_opencl(gpu)) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  Deadline patience(patience_seconds);
  FullStackRig rig(*library.ops(), gpu.uuid());
  CHECK_EQ(rig.started(), 0);
  { ClProcess process(rig.kmt());
    CHECK(process.loader().load() == FakeClLoader::Load::kept);
    ExtensionRun run(process.loader());
    if (!run.offers("cl_khr_depth_images")) return;
    const cl_image_format format = {CL_DEPTH, CL_FLOAT};
    cl_image_desc         desc = {};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = 8u;
    desc.image_height = 8u;
    cl_int error = CL_INVALID_VALUE;
    const cl_mem image = table_of(run.context())->clCreateImage(run.context(), CL_MEM_READ_WRITE, &format, &desc,
                                                                nullptr, &error);
    CHECK_EQ(error, CL_SUCCESS);
    if (image == nullptr) return;
    CHECK(run.kernel("__kernel void depths(__write_only image2d_depth_t image)\n"
                     "{ int i = get_global_id(0);\n"
                     "  write_imagef(image, (int2)(i % 8, i / 8), (float)i / 64.0f); }\n",
                     "depths"));
    CHECK_EQ(run.argument(0u, sizeof image, &image), CL_SUCCESS);
    CHECK_EQ(run.run(extension_items, 0u), CL_SUCCESS);
    float             read[extension_items] = {};
    const std::size_t origin[3] = {0u, 0u, 0u};
    const std::size_t region[3] = {8u, 8u, 1u};
    CHECK_EQ(table_of(run.queue())->clEnqueueReadImage(run.queue(), image, CL_TRUE, origin, region, 0u, 0u, read, 0u,
                                                       nullptr, nullptr),
             CL_SUCCESS);
    std::uint32_t wrong = 0u;
    for (std::uint32_t index = 0u; index < extension_items; ++index)
        if (read[index] != static_cast<float>(index) / 64.0f) ++wrong;
    CHECK_EQ(wrong, 0u);
    CHECK_EQ(run.loader().release_mem(image), CL_SUCCESS); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(fullstack_opencl_icd, a_kernel_and_a_read_gated_on_a_user_event_run_only_once_it_is_set)
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
    const float cleared[elements] = {};
    CHECK_EQ(table->clEnqueueWriteBuffer(run.queue(), run.output(), CL_TRUE, 0u, vector_bytes, cleared, 0u,
                                         nullptr, nullptr),
             CL_SUCCESS);

    cl_int error = CL_INVALID_VALUE;
    cl_event gate = loader.create_user_event(run.context(), &error);
    CHECK_EQ(error, CL_SUCCESS);
    if (gate == nullptr) return;
    const std::size_t global = elements;
    cl_event          ran = nullptr;
    CHECK_EQ(table->clEnqueueNDRangeKernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr, 1u, &gate,
                                           &ran),
             CL_SUCCESS);
    float read[elements];
    std::memset(read, 0xEE, sizeof read);
    cl_event delivered = nullptr;
    CHECK_EQ(table->clEnqueueReadBuffer(run.queue(), run.output(), CL_FALSE, 0u, vector_bytes, read, 1u, &ran,
                                        &delivered),
             CL_SUCCESS);
    CHECK_EQ(run.flush(), CL_SUCCESS);

    cl_int status = CL_COMPLETE;
    CHECK_EQ(loader.event_info(ran, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status, nullptr),
             CL_SUCCESS);
    CHECK(status != CL_COMPLETE);
    status = CL_COMPLETE;
    CHECK_EQ(loader.event_info(delivered, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status, nullptr),
             CL_SUCCESS);
    CHECK(status != CL_COMPLETE);
    std::uint8_t untouched[sizeof read];
    std::memset(untouched, 0xEE, sizeof untouched);
    CHECK_EQ(std::memcmp(read, untouched, sizeof read), 0);

    CHECK_EQ(loader.set_user_event_status(gate, CL_COMPLETE), CL_SUCCESS);
    CHECK_EQ(loader.wait_for_events(1u, &delivered), CL_SUCCESS);
    CHECK_EQ(run.wrong(read), 0u);

    CHECK_EQ(loader.release_event(delivered), CL_SUCCESS);
    CHECK_EQ(loader.release_event(ran), CL_SUCCESS);
    CHECK_EQ(loader.release_event(gate), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

TEST(fullstack_opencl_icd, a_sub_buffer_reads_and_maps_its_region_of_the_buffer)
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
    cl_device_id device = nullptr;
    cl_uint      devices = 0u;
    CHECK_EQ(loader.device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices), CL_SUCCESS);
    if (device == nullptr) return;
    cl_int           error = CL_INVALID_VALUE;
    const cl_context context = loader.create_context(device, &error);
    if (context == nullptr) return;
    const cl_command_queue queue = loader.create_queue(context, device, &error);
    const cl_icd_dispatch *const table = table_of(queue);
    cl_uint align_bits = 0u;
    CHECK_EQ(loader.device_info(device, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof align_bits, &align_bits, nullptr),
             CL_SUCCESS);
    const std::size_t origin = align_bits / 8u;
    constexpr std::size_t region_bytes = 256u;
    const std::size_t total = origin + 2u * region_bytes;
    std::uint8_t     *filled = static_cast<std::uint8_t *>(std::malloc(total));
    for (std::size_t index = 0u; index < total; ++index) filled[index] = static_cast<std::uint8_t>(index * 7u);

    for (const cl_mem_flags flags : {static_cast<cl_mem_flags>(CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR),
                                     static_cast<cl_mem_flags>(CL_MEM_READ_WRITE)}) {
        const cl_mem buffer = loader.create_buffer(context, flags, total, &error);
        CHECK_EQ(error, CL_SUCCESS);
        if (buffer == nullptr) break;
        CHECK_EQ(table->clEnqueueWriteBuffer(queue, buffer, CL_TRUE, 0u, total, filled, 0u, nullptr, nullptr),
                 CL_SUCCESS);
        const cl_buffer_region region = {origin, region_bytes};
        const cl_mem sub = table->clCreateSubBuffer(buffer, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &region,
                                                    &error);
        CHECK_EQ(error, CL_SUCCESS);
        if (sub == nullptr) break;
        std::uint8_t read[region_bytes] = {};
        CHECK_EQ(table->clEnqueueReadBuffer(queue, sub, CL_TRUE, 0u, region_bytes, read, 0u, nullptr, nullptr),
                 CL_SUCCESS);
        CHECK_EQ(std::memcmp(read, filled + origin, region_bytes), 0);
        cl_mem associated = nullptr;
        CHECK_EQ(table->clGetMemObjectInfo(sub, CL_MEM_ASSOCIATED_MEMOBJECT, sizeof associated, &associated, nullptr),
                 CL_SUCCESS);
        CHECK(associated == buffer);

        std::uint8_t *const mapped = static_cast<std::uint8_t *>(table->clEnqueueMapBuffer(
            queue, sub, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0u, region_bytes, 0u, nullptr, nullptr, &error));
        CHECK_EQ(error, CL_SUCCESS);
        if (mapped == nullptr) break;
        CHECK_EQ(std::memcmp(mapped, filled + origin, region_bytes), 0);
        std::memset(mapped, 0x5A, region_bytes);
        CHECK_EQ(table->clEnqueueUnmapMemObject(queue, sub, mapped, 0u, nullptr, nullptr), CL_SUCCESS);
        std::uint8_t *const whole = static_cast<std::uint8_t *>(std::malloc(total));
        CHECK_EQ(table->clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0u, total, whole, 0u, nullptr, nullptr),
                 CL_SUCCESS);
        std::uint32_t wrong = 0u;
        for (std::size_t index = 0u; index < total; ++index) {
            const bool inside = index >= origin && index < origin + region_bytes;
            if (whole[index] != (inside ? 0x5Au : filled[index])) ++wrong;
        }
        CHECK_EQ(wrong, 0u);
        std::free(whole);
        CHECK_EQ(loader.release_mem(sub), CL_SUCCESS);
        CHECK_EQ(loader.release_mem(buffer), CL_SUCCESS);
    }
    std::free(filled);
    CHECK_EQ(loader.release_queue(queue), CL_SUCCESS);
    CHECK_EQ(loader.release_context(context), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(fullstack_opencl_icd, an_image_is_mapped_filled_and_copied_through_a_buffer)
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
    cl_device_id device = nullptr;
    cl_uint      devices = 0u;
    CHECK_EQ(loader.device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices), CL_SUCCESS);
    if (device == nullptr) return;
    cl_int           error = CL_INVALID_VALUE;
    const cl_context context = loader.create_context(device, &error);
    if (context == nullptr) return;
    const cl_command_queue queue = loader.create_queue(context, device, &error);
    const cl_icd_dispatch *const table = table_of(queue);
    constexpr std::size_t width = 16u, height = 8u;
    std::uint32_t texels[width * height];
    for (std::uint32_t index = 0u; index < width * height; ++index) texels[index] = 0x01000000u * index + index;
    const cl_image_format format = {CL_RGBA, CL_UNSIGNED_INT8};
    cl_image_desc         desc = {};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = width;
    desc.image_height = height;
    const cl_mem image = table->clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &format, &desc,
                                              texels, &error);
    CHECK_EQ(error, CL_SUCCESS);
    if (image == nullptr) return;

    const size_t origin[3] = {2u, 1u, 0u}, region[3] = {4u, 3u, 1u};
    std::size_t  row_pitch = 0u;
    std::uint32_t *const mapped = static_cast<std::uint32_t *>(table->clEnqueueMapImage(
        queue, image, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, origin, region, &row_pitch, nullptr, 0u, nullptr, nullptr,
        &error));
    CHECK_EQ(error, CL_SUCCESS);
    if (mapped == nullptr) return;
    CHECK_EQ(row_pitch, 4u * sizeof(std::uint32_t));
    std::uint32_t wrong = 0u;
    for (std::size_t y = 0u; y < 3u; ++y)
        for (std::size_t x = 0u; x < 4u; ++x)
            if (mapped[y * 4u + x] != texels[(y + 1u) * width + x + 2u]) ++wrong;
    CHECK_EQ(wrong, 0u);
    for (std::size_t index = 0u; index < 12u; ++index) mapped[index] = 0xC0DE0000u + static_cast<std::uint32_t>(index);
    CHECK_EQ(table->clEnqueueUnmapMemObject(queue, image, mapped, 0u, nullptr, nullptr), CL_SUCCESS);

    const cl_mem buffer = loader.create_buffer(context, CL_MEM_READ_WRITE, sizeof texels, &error);
    const size_t whole[3] = {width, height, 1u}, corner[3] = {0u, 0u, 0u};
    cl_event     copied = nullptr;
    CHECK_EQ(table->clEnqueueCopyImageToBuffer(queue, image, buffer, corner, whole, 0u, 0u, nullptr, &copied),
             CL_SUCCESS);
    std::uint32_t read[width * height] = {};
    CHECK_EQ(table->clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0u, sizeof read, read, 1u, &copied, nullptr),
             CL_SUCCESS);
    wrong = 0u;
    for (std::size_t y = 0u; y < height; ++y)
        for (std::size_t x = 0u; x < width; ++x) {
            const bool          inside = x >= 2u && x < 6u && y >= 1u && y < 4u;
            const std::uint32_t wanted = inside ? 0xC0DE0000u + static_cast<std::uint32_t>((y - 1u) * 4u + x - 2u)
                                                : texels[y * width + x];
            if (read[y * width + x] != wanted) ++wrong;
        }
    CHECK_EQ(wrong, 0u);

    const cl_uint4 colour = {{7u, 8u, 9u, 10u}};
    const size_t   square[3] = {2u, 2u, 1u};
    CHECK_EQ(table->clEnqueueFillImage(queue, image, &colour, corner, square, 0u, nullptr, nullptr), CL_SUCCESS);
    const size_t row_rest[3] = {width - 2u, 1u, 1u}, row_origin[3] = {2u, 0u, 0u};
    CHECK_EQ(table->clEnqueueCopyBufferToImage(queue, buffer, image, 2u * sizeof(std::uint32_t), row_origin, row_rest,
                                               0u, nullptr, nullptr),
             CL_SUCCESS);
    std::uint32_t row[width] = {};
    const size_t  one_row[3] = {width, 1u, 1u};
    CHECK_EQ(table->clEnqueueReadImage(queue, image, CL_TRUE, corner, one_row, 0u, 0u, row, 0u, nullptr, nullptr),
             CL_SUCCESS);
    const std::uint32_t fill = 7u | (8u << 8u) | (9u << 16u) | (10u << 24u);
    CHECK_EQ(row[0], fill);
    CHECK_EQ(row[1], fill);
    wrong = 0u;
    for (std::size_t x = 2u; x < width; ++x)
        if (row[x] != read[x]) ++wrong;
    CHECK_EQ(wrong, 0u);

    CHECK_EQ(loader.release_event(copied), CL_SUCCESS);
    CHECK_EQ(loader.release_mem(buffer), CL_SUCCESS);
    CHECK_EQ(loader.release_mem(image), CL_SUCCESS);
    CHECK_EQ(loader.release_queue(queue), CL_SUCCESS);
    CHECK_EQ(loader.release_context(context), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(fullstack_opencl_icd, shared_memory_is_copied_in_read_by_a_kernel_filled_and_copied_out)
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
    cl_device_svm_capabilities svm = 0u;
    CHECK_EQ(loader.device_info(run.device(), CL_DEVICE_SVM_CAPABILITIES, sizeof svm, &svm, nullptr), CL_SUCCESS);
    if (svm == 0u) {
        std::fprintf(stderr, "host device offers no shared memory; nothing to judge\n");
        return;
    }
    void *const shared = table->clSVMAlloc(run.context(), CL_MEM_READ_WRITE, vector_bytes, 0u);
    CHECK(shared != nullptr);
    if (shared == nullptr) return;
    float replaced[elements];
    for (std::uint32_t index = 0u; index < elements; ++index) replaced[index] = static_cast<float>(500u + index);
    CHECK_EQ(table->clEnqueueSVMMemcpy(run.queue(), CL_TRUE, shared, replaced, vector_bytes, 0u, nullptr, nullptr),
             CL_SUCCESS);
    CHECK_EQ(table->clSetKernelArgSVMPointer(run.kernel(), 0u, shared), CL_SUCCESS);
    cl_event ran = nullptr;
    CHECK_EQ(run.enqueue(&ran), CL_SUCCESS);
    float read[elements];
    std::memset(read, 0xEE, sizeof read);
    CHECK_EQ(run.read(read, CL_TRUE, nullptr), CL_SUCCESS);
    float wanted[elements] = {};
    run.sum_with(replaced, wanted);
    std::uint32_t wrong = 0u;
    for (std::uint32_t index = 0u; index < elements; ++index)
        if (read[index] != wanted[index]) ++wrong;
    CHECK_EQ(wrong, 0u);

    const float pattern = 2.5f;
    CHECK_EQ(table->clEnqueueSVMMemFill(run.queue(), shared, &pattern, sizeof pattern, vector_bytes, 1u, &ran,
                                        nullptr),
             CL_SUCCESS);
    float back[elements] = {};
    CHECK_EQ(table->clEnqueueSVMMemcpy(run.queue(), CL_TRUE, back, shared, vector_bytes, 0u, nullptr, nullptr),
             CL_SUCCESS);
    wrong = 0u;
    for (std::uint32_t index = 0u; index < elements; ++index)
        if (back[index] != pattern) ++wrong;
    CHECK_EQ(wrong, 0u);

    const cl_mem input = run.input();
    CHECK_EQ(table->clSetKernelArg(run.kernel(), 0u, sizeof(cl_mem), &input), CL_SUCCESS);
    void *freed[1] = {shared};
    cl_event gone = nullptr;
    CHECK_EQ(table->clEnqueueSVMFree(run.queue(), 1u, freed, nullptr, nullptr, 0u, nullptr, &gone), CL_SUCCESS);
    CHECK_EQ(run.flush(), CL_SUCCESS);
    CHECK_EQ(loader.wait_for_events(1u, &gone), CL_SUCCESS);
    CHECK_EQ(loader.release_event(gone), CL_SUCCESS);
    CHECK_EQ(loader.release_event(ran), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

TEST(fullstack_opencl_icd, a_non_blocking_write_gated_on_a_user_event_set_after_it_returns)
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
    cl_int         error = CL_INVALID_VALUE;
    const cl_event gate = loader.create_user_event(run.context(), &error);
    CHECK(gate != nullptr);
    if (gate == nullptr) return;
    float doubled[elements] = {};
    for (std::uint32_t index = 0u; index < elements; ++index) doubled[index] = static_cast<float>(2u * index);
    cl_event written = nullptr;
    CHECK_EQ(table_of(run.queue())->clEnqueueWriteBuffer(run.queue(), run.input(), CL_FALSE, 0u, vector_bytes, doubled,
                                                         1u, &gate, &written),
             CL_SUCCESS);
    const std::size_t global = elements;
    CHECK_EQ(table_of(run.queue())->clEnqueueNDRangeKernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr, 1u,
                                                           &gate, nullptr),
             CL_SUCCESS);
    CHECK_EQ(loader.set_user_event_status(gate, CL_COMPLETE), CL_SUCCESS);
    CHECK_EQ(run.finish(), CL_SUCCESS);
    if (written != nullptr) CHECK_EQ(loader.wait_for_events(1u, &written), CL_SUCCESS);
    float read[elements] = {};
    CHECK_EQ(run.read(read, CL_TRUE, nullptr), CL_SUCCESS);
    float expected[elements] = {};
    run.sum_with(doubled, expected);
    std::uint32_t wrong = 0u;
    for (std::uint32_t index = 0u; index < elements; ++index)
        if (read[index] != expected[index]) ++wrong;
    CHECK_EQ(wrong, 0u);
    if (written != nullptr) CHECK_EQ(loader.release_event(written), CL_SUCCESS);
    CHECK_EQ(loader.release_event(gate), CL_SUCCESS);
    CHECK(process.root().invariants_hold()); }
  CHECK_EQ(rig.kmt().open_objects(), 0u); }
