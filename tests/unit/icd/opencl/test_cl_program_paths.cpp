#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "icd/opencl/cl_program_notify.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <CL/cl_ext.h>

#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;
using av::clicd::ClProgramNotify;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_program = 0x6700u;
constexpr std::uint64_t host_kernel = 0x6800u;

const cl_icd_dispatch *table_of(const void *object)
{
    return object != nullptr ? *reinterpret_cast<const cl_icd_dispatch *const *>(object) : nullptr;
}

bool carries(const av::MockKmt &kmt, std::uint32_t index, std::uint64_t value)
{
    const std::uint8_t *const frame = kmt.command_bytes(index);
    if (frame == nullptr) return false;
    const std::uint32_t size = kmt.command_size(index) < av::MockKmt::journal_bytes ? kmt.command_size(index)
                                                                                     : av::MockKmt::journal_bytes;
    for (std::uint32_t at = 0u; at + sizeof value <= size; at++)
        if (std::memcmp(frame + at, &value, sizeof value) == 0) return true;
    return false;
}

class Programs {
public:
    explicit Programs(ClIcdRig &rig) : rig_(rig), device_(nullptr), context_(nullptr), program_(nullptr)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().blob_import(0u, 0x77u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS) return;
        rig_.host().blob_import(0u, 0x78u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_handle(host_context, CL_SUCCESS);
        context_ = rig_.loader().create_context(device_, nullptr);
        if (context_ == nullptr) return;
        rig_.host().cl_handle(host_program, CL_SUCCESS);
        program_ = rig_.loader().create_program(context_, "__kernel void k(void){}", nullptr);
    }

    bool         made() const { return program_ != nullptr; }
    cl_device_id device() const { return device_; }
    cl_context   context() const { return context_; }
    cl_program   program() const { return program_; }

private:
    ClIcdRig    &rig_;
    cl_device_id device_;
    cl_context   context_;
    cl_program   program_;
};

class Told {
public:
    void heard(cl_program program)
    {
        ++calls;
        last = program;
    }
    std::uint32_t calls = 0u;
    cl_program    last = nullptr;
};

void CL_CALLBACK told(cl_program program, void *user_data) { static_cast<Told *>(user_data)->heard(program); }

}

TEST(cl_program_paths, a_program_notify_runs_once_for_a_build_that_ran_and_never_without_a_callback)
{ Told heard;
  const ClProgramNotify none;
  CHECK(none.valid());
  none.tell(reinterpret_cast<cl_program>(&heard));
  const ClProgramNotify data_alone(nullptr, &heard);
  CHECK(!data_alone.valid());
  const ClProgramNotify both(&told, &heard);
  CHECK(both.valid());
  both.tell(nullptr);
  CHECK_EQ(heard.calls, 0u);
  both.tell(reinterpret_cast<cl_program>(&heard));
  CHECK_EQ(heard.calls, 1u);
  CHECK(heard.last == reinterpret_cast<cl_program>(&heard)); }

TEST(cl_program_paths, a_build_judges_its_device_list_and_tells_its_callback_when_it_ran)
{ ClIcdRig rig;
  Programs run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_icd_dispatch &table = *table_of(run.program());
  Told heard;
  const std::uint32_t sent = rig.kmt().commands();
  cl_device_id device = run.device();
  CHECK_EQ(table.clBuildProgram(run.program(), 0u, nullptr, nullptr, nullptr, &heard), CL_INVALID_VALUE);
  CHECK_EQ(table.clBuildProgram(run.program(), 1u, nullptr, nullptr, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(table.clBuildProgram(run.program(), 0u, &device, nullptr, nullptr, nullptr), CL_INVALID_VALUE);
  cl_device_id stranger = reinterpret_cast<cl_device_id>(run.context());
  CHECK_EQ(table.clBuildProgram(run.program(), 1u, &stranger, nullptr, nullptr, nullptr), CL_INVALID_DEVICE);
  CHECK_EQ(table.clBuildProgram(reinterpret_cast<cl_program>(run.context()), 0u, nullptr, nullptr, nullptr, nullptr),
           CL_INVALID_PROGRAM);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(table.clBuildProgram(run.program(), 1u, &device, "-DX", &told, &heard), CL_SUCCESS);
  CHECK_EQ(heard.calls, 1u);
  CHECK(heard.last == run.program());
  rig.host().cl_result(CL_INVALID_OPERATION);
  CHECK_EQ(table.clBuildProgram(run.program(), 0u, nullptr, nullptr, &told, &heard), CL_INVALID_OPERATION);
  CHECK_EQ(heard.calls, 1u);
  rig.host().cl_result(CL_BUILD_PROGRAM_FAILURE);
  rig.host().cl_info(CL_SUCCESS, "", 0u);
  CHECK_EQ(table.clBuildProgram(run.program(), 0u, nullptr, nullptr, &told, &heard), CL_BUILD_PROGRAM_FAILURE);
  CHECK_EQ(heard.calls, 2u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_program_paths, a_binary_crosses_for_the_context_s_device_and_its_status_comes_back)
{ ClIcdRig rig;
  Programs run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_icd_dispatch &table = *table_of(run.context());
  const unsigned char    bytes[8] = {'b', 'i', 'n', 'a', 'r', 'y', '!', 0u};
  const unsigned char   *binary = bytes;
  std::size_t            length = sizeof bytes;
  cl_device_id           devices[2] = {run.device(), run.device()};
  cl_int                 status[2] = {7, 7};
  cl_int                 error = CL_SUCCESS;
  const std::uint32_t    sent = rig.kmt().commands();
  CHECK(table.clCreateProgramWithBinary(reinterpret_cast<cl_context>(run.program()), 1u, devices, &length, &binary,
                                        status, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(table.clCreateProgramWithBinary(run.context(), 0u, devices, &length, &binary, status, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(table.clCreateProgramWithBinary(run.context(), 1u, devices, nullptr, &binary, status, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  std::size_t none = 0u;
  CHECK(table.clCreateProgramWithBinary(run.context(), 1u, devices, &none, &binary, status, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(status[0], CL_INVALID_VALUE);
  const std::size_t lengths[2] = {sizeof bytes, sizeof bytes};
  const unsigned char *binaries[2] = {bytes, bytes};
  CHECK(table.clCreateProgramWithBinary(run.context(), 2u, devices, lengths, binaries, status, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  CHECK_EQ(rig.kmt().commands(), sent);

  rig.host().cl_from_binary(0u, CL_INVALID_BINARY, CL_INVALID_BINARY);
  CHECK(table.clCreateProgramWithBinary(run.context(), 1u, devices, &length, &binary, status, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_BINARY);
  CHECK_EQ(status[0], CL_INVALID_BINARY);
  CHECK(carries(rig.kmt(), sent, host_device));
  CHECK(carries(rig.kmt(), sent, host_context));
  rig.host().cl_from_binary(host_program + 1u, CL_SUCCESS, CL_SUCCESS);
  const cl_program loaded =
      table.clCreateProgramWithBinary(run.context(), 1u, devices, &length, &binary, status, &error);
  CHECK(loaded != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(status[0], CL_SUCCESS);
  CHECK(table_of(loaded) == table_of(run.context()));
  rig.host().cl_from_binary(0u, CL_SUCCESS, CL_SUCCESS);
  CHECK(table.clCreateProgramWithBinary(run.context(), 1u, devices, &length, &binary, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_BINARY);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_program_paths, il_and_built_in_kernels_are_the_host_s_to_take_or_refuse)
{ ClIcdRig rig;
  Programs run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_icd_dispatch &table = *table_of(run.context());
  const std::uint32_t    words[2] = {0x07230203u, 0u};
  cl_int                 error = CL_SUCCESS;
  CHECK(table.clCreateProgramWithIL(run.context(), nullptr, sizeof words, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(table.clCreateProgramWithIL(run.context(), words, 0u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(table.clCreateProgramWithIL(reinterpret_cast<cl_context>(run.program()), words, sizeof words, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  rig.host().cl_handle(0u, CL_INVALID_OPERATION);
  CHECK(table.clCreateProgramWithIL(run.context(), words, sizeof words, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_OPERATION);
  rig.host().cl_handle(host_program + 2u, CL_SUCCESS);
  CHECK(table.clCreateProgramWithIL(run.context(), words, sizeof words, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(rig.root().extension_function("clCreateProgramWithILKHR") ==
        reinterpret_cast<void *>(table.clCreateProgramWithIL));

  cl_device_id device = run.device();
  CHECK(table.clCreateProgramWithBuiltInKernels(run.context(), 1u, &device, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(table.clCreateProgramWithBuiltInKernels(run.context(), 0u, nullptr, "k", &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  cl_device_id stranger = reinterpret_cast<cl_device_id>(run.program());
  CHECK(table.clCreateProgramWithBuiltInKernels(run.context(), 1u, &stranger, "k", &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  rig.host().cl_handle(0u, CL_INVALID_VALUE);
  CHECK(table.clCreateProgramWithBuiltInKernels(run.context(), 1u, &device, "none_here", &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_program_paths, a_compile_names_its_headers_by_the_host_s_numbers_and_a_link_makes_a_program)
{ ClIcdRig rig;
  Programs run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_icd_dispatch &table = *table_of(run.program());
  const char            *name = "h.h";
  const cl_program       header = run.program();
  Told                   heard;
  CHECK_EQ(table.clCompileProgram(run.program(), 0u, nullptr, nullptr, 1u, &header, nullptr, nullptr, nullptr),
           CL_INVALID_VALUE);
  const cl_program stranger = reinterpret_cast<cl_program>(run.context());
  CHECK_EQ(table.clCompileProgram(run.program(), 0u, nullptr, nullptr, 1u, &stranger, &name, nullptr, nullptr),
           CL_INVALID_PROGRAM);
  CHECK_EQ(table.clCompileProgram(run.program(), 0u, nullptr, nullptr, 0u, &header, nullptr, nullptr, nullptr),
           CL_INVALID_VALUE);
  cl_program many[av::clicd::ClIcdRoot::max_linked + 1u] = {};
  const char *names[av::clicd::ClIcdRoot::max_linked + 1u] = {};
  for (cl_program &one : many) one = run.program();
  for (const char *&one : names) one = name;
  CHECK_EQ(table.clCompileProgram(run.program(), 0u, nullptr, nullptr, av::clicd::ClIcdRoot::max_linked + 1u, many,
                                  names, nullptr, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(table.clCompileProgram(run.program(), 0u, nullptr, nullptr, 0u, nullptr, nullptr, nullptr, &heard),
           CL_INVALID_VALUE);
  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(table.clCompileProgram(run.program(), 0u, nullptr, "-DY", 1u, &header, &name, &told, &heard), CL_SUCCESS);
  CHECK_EQ(heard.calls, 1u);
  CHECK(carries(rig.kmt(), sent, host_program));
  rig.host().cl_result(CL_COMPILE_PROGRAM_FAILURE);
  rig.host().cl_info(CL_SUCCESS, "", 0u);
  CHECK_EQ(table.clCompileProgram(run.program(), 0u, nullptr, nullptr, 0u, nullptr, nullptr, &told, &heard),
           CL_COMPILE_PROGRAM_FAILURE);
  CHECK_EQ(heard.calls, 2u);

  const cl_icd_dispatch &context_table = *table_of(run.context());
  cl_int                 error = CL_SUCCESS;
  CHECK(context_table.clLinkProgram(run.context(), 0u, nullptr, nullptr, 0u, nullptr, nullptr, nullptr, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(context_table.clLinkProgram(run.context(), 0u, nullptr, nullptr, 1u, &stranger, nullptr, nullptr, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_PROGRAM);
  CHECK(context_table.clLinkProgram(reinterpret_cast<cl_context>(run.program()), 0u, nullptr, nullptr, 1u, &header,
                                    nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  rig.host().cl_handle(host_program + 3u, CL_SUCCESS);
  const cl_program linked =
      context_table.clLinkProgram(run.context(), 0u, nullptr, "-x", 1u, &header, &told, &heard, &error);
  CHECK(linked != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(heard.calls, 3u);
  CHECK(heard.last == linked);
  rig.host().cl_handle(host_program + 4u, CL_LINK_PROGRAM_FAILURE);
  rig.host().cl_info(CL_SUCCESS, "", 0u);
  const cl_program failed =
      context_table.clLinkProgram(run.context(), 0u, nullptr, nullptr, 1u, &header, nullptr, nullptr, &error);
  CHECK(failed != nullptr);
  CHECK_EQ(error, CL_LINK_PROGRAM_FAILURE);
  rig.host().cl_handle(0u, CL_INVALID_LINKER_OPTIONS);
  CHECK(context_table.clLinkProgram(run.context(), 0u, nullptr, "-bad", 1u, &header, nullptr, nullptr, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_LINKER_OPTIONS);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_program_paths, a_program_names_this_icd_s_objects_and_fetches_its_binary_into_the_application_s_memory)
{ ClIcdRig rig;
  Programs run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_icd_dispatch &table = *table_of(run.program());
  const std::uint32_t    sent = rig.kmt().commands();
  cl_uint                count = 0u;
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_REFERENCE_COUNT, sizeof count, &count, nullptr),
           CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_NUM_DEVICES, sizeof count, &count, nullptr), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  cl_device_id device = nullptr;
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_DEVICES, sizeof device, &device, nullptr), CL_SUCCESS);
  CHECK(device == run.device());
  cl_context context = nullptr;
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_CONTEXT, sizeof context, &context, nullptr), CL_SUCCESS);
  CHECK(context == run.context());
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK_EQ(table.clGetProgramInfo(reinterpret_cast<cl_program>(run.context()), CL_PROGRAM_NUM_DEVICES, sizeof count,
                                  &count, nullptr),
           CL_INVALID_PROGRAM);

  unsigned char  into[16] = {};
  unsigned char *pointers[1] = {into};
  std::size_t    answered = 0u;
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_BINARIES, 0u, nullptr, &answered), CL_SUCCESS);
  CHECK_EQ(answered, sizeof pointers);
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_BINARIES, 1u, pointers, nullptr), CL_INVALID_VALUE);
  const std::size_t size = 6u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&size), sizeof size);
  rig.host().cl_info(CL_SUCCESS, "sealed", 6u);
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_BINARIES, sizeof pointers, pointers, &answered),
           CL_SUCCESS);
  CHECK(std::memcmp(into, "sealed", 6u) == 0);
  unsigned char *skipped[1] = {nullptr};
  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_BINARIES, sizeof skipped, skipped, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), before);
  const std::size_t empty = 0u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&empty), sizeof empty);
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_BINARIES, sizeof pointers, pointers, nullptr), CL_SUCCESS);
  const std::size_t two[2] = {6u, 6u};
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(two), sizeof two);
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_BINARIES, sizeof pointers, pointers, nullptr),
           CL_OUT_OF_RESOURCES);
  rig.host().cl_info(CL_INVALID_PROGRAM_EXECUTABLE, "", 0u);
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_BINARIES, sizeof pointers, pointers, nullptr),
           CL_INVALID_PROGRAM_EXECUTABLE);
  rig.host().cl_info(CL_SUCCESS, "k", 2u);
  char names[8] = {};
  CHECK_EQ(table.clGetProgramInfo(run.program(), CL_PROGRAM_KERNEL_NAMES, sizeof names, names, nullptr), CL_SUCCESS);
  CHECK_EQ(std::strcmp(names, "k"), 0);

  CHECK_EQ(table.clSetProgramSpecializationConstant(run.program(), 1u, 0u, nullptr), CL_INVALID_VALUE);
  rig.host().cl_result(CL_INVALID_PROGRAM);
  const std::uint32_t constant = 3u;
  CHECK_EQ(table.clSetProgramSpecializationConstant(run.program(), 1u, sizeof constant, &constant),
           CL_INVALID_PROGRAM);
  CHECK_EQ(table.clSetProgramSpecializationConstant(reinterpret_cast<cl_program>(run.context()), 1u, sizeof constant,
                                                    &constant),
           CL_INVALID_PROGRAM);
  CHECK_EQ(table.clSetProgramReleaseCallback(reinterpret_cast<cl_program>(run.context()), &told, nullptr),
           CL_INVALID_PROGRAM);
  CHECK_EQ(table.clSetProgramReleaseCallback(run.program(), nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(table.clSetProgramReleaseCallback(run.program(), &told, nullptr), CL_INVALID_OPERATION);
  cl_platform_id platform = nullptr;
  CHECK_EQ(rig.root().platform_ids(1u, &platform, nullptr), CL_SUCCESS);
  CHECK_EQ(table.clUnloadPlatformCompiler(platform), CL_SUCCESS);
  CHECK_EQ(table.clUnloadPlatformCompiler(reinterpret_cast<cl_platform_id>(run.context())), CL_INVALID_PLATFORM);
  CHECK_EQ(table.clUnloadCompiler(), CL_SUCCESS);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_program_paths, kernels_made_together_or_cloned_are_this_icd_s_and_say_whose_they_are)
{ ClIcdRig rig;
  Programs run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_icd_dispatch &table = *table_of(run.program());
  cl_kernel kernels[2] = {};
  cl_uint   made = 0u;
  CHECK_EQ(table.clCreateKernelsInProgram(run.program(), 0u, kernels, &made), CL_INVALID_VALUE);
  CHECK_EQ(table.clCreateKernelsInProgram(reinterpret_cast<cl_program>(run.context()), 2u, kernels, &made),
           CL_INVALID_PROGRAM);
  rig.host().cl_count(CL_SUCCESS, 2u);
  CHECK_EQ(table.clCreateKernelsInProgram(run.program(), 0u, nullptr, &made), CL_SUCCESS);
  CHECK_EQ(made, 2u);
  const std::uint64_t hosts[2] = {host_kernel, host_kernel + 1u};
  rig.host().cl_ids(CL_SUCCESS, hosts, 2u, 2u);
  CHECK_EQ(table.clCreateKernelsInProgram(run.program(), 2u, kernels, &made), CL_SUCCESS);
  CHECK(kernels[0] != nullptr && kernels[1] != nullptr && kernels[0] != kernels[1]);
  rig.host().cl_ids(CL_INVALID_PROGRAM_EXECUTABLE, hosts, 0u, 0u);
  cl_kernel refused[1] = {};
  CHECK_EQ(table.clCreateKernelsInProgram(run.program(), 1u, refused, nullptr), CL_INVALID_PROGRAM_EXECUTABLE);
  CHECK(refused[0] == nullptr);
  if (kernels[0] == nullptr) return;

  const cl_icd_dispatch &kernel_table = *table_of(kernels[0]);
  const std::uint32_t    sent = rig.kmt().commands();
  cl_program program = nullptr;
  CHECK_EQ(kernel_table.clGetKernelInfo(kernels[0], CL_KERNEL_PROGRAM, sizeof program, &program, nullptr),
           CL_SUCCESS);
  CHECK(program == run.program());
  cl_context context = nullptr;
  CHECK_EQ(kernel_table.clGetKernelInfo(kernels[0], CL_KERNEL_CONTEXT, sizeof context, &context, nullptr),
           CL_SUCCESS);
  CHECK(context == run.context());
  cl_uint count = 0u;
  CHECK_EQ(kernel_table.clGetKernelInfo(kernels[0], CL_KERNEL_REFERENCE_COUNT, sizeof count, &count, nullptr),
           CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_info(CL_SUCCESS, "k", 2u);
  char name[8] = {};
  CHECK_EQ(kernel_table.clGetKernelInfo(kernels[0], CL_KERNEL_FUNCTION_NAME, sizeof name, name, nullptr), CL_SUCCESS);
  CHECK_EQ(std::strcmp(name, "k"), 0);
  CHECK(carries(rig.kmt(), sent, host_kernel));
  rig.host().cl_info(CL_KERNEL_ARG_INFO_NOT_AVAILABLE, "", 0u);
  CHECK_EQ(kernel_table.clGetKernelArgInfo(kernels[0], 0u, CL_KERNEL_ARG_NAME, sizeof name, name, nullptr),
           CL_KERNEL_ARG_INFO_NOT_AVAILABLE);
  CHECK_EQ(kernel_table.clGetKernelInfo(reinterpret_cast<cl_kernel>(run.program()), CL_KERNEL_NUM_ARGS, sizeof count,
                                        &count, nullptr),
           CL_INVALID_KERNEL);

  cl_int error = CL_SUCCESS;
  rig.host().cl_handle(0u, CL_OUT_OF_RESOURCES);
  CHECK(kernel_table.clCloneKernel(kernels[0], &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(kernel_table.clCloneKernel(reinterpret_cast<cl_kernel>(run.program()), &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_KERNEL);
  rig.host().cl_handle(host_kernel + 2u, CL_SUCCESS);
  const cl_kernel clone = kernel_table.clCloneKernel(kernels[0], &error);
  CHECK(clone != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  program = nullptr;
  CHECK_EQ(kernel_table.clGetKernelInfo(clone, CL_KERNEL_PROGRAM, sizeof program, &program, nullptr), CL_SUCCESS);
  CHECK(program == run.program());

  cl_device_id stranger = reinterpret_cast<cl_device_id>(run.program());
  std::size_t  value = 0u;
  CHECK_EQ(kernel_table.clGetKernelSubGroupInfo(kernels[0], stranger, CL_KERNEL_MAX_NUM_SUB_GROUPS, 0u, nullptr,
                                                sizeof value, &value, nullptr),
           CL_INVALID_DEVICE);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&host_kernel), sizeof value);
  CHECK_EQ(kernel_table.clGetKernelSubGroupInfoKHR(kernels[0], nullptr, CL_KERNEL_MAX_NUM_SUB_GROUPS, 0u, nullptr,
                                                   sizeof value, &value, nullptr),
           CL_SUCCESS);

  cl_bool on = CL_TRUE;
  CHECK_EQ(kernel_table.clSetKernelExecInfo(kernels[0], CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, sizeof on, &on),
           CL_INVALID_OPERATION);
  CHECK_EQ(kernel_table.clSetKernelExecInfo(kernels[0], CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, 1u, &on),
           CL_INVALID_VALUE);
  rig.host().cl_result(CL_SUCCESS);
  cl_bool off = CL_FALSE;
  CHECK_EQ(kernel_table.clSetKernelExecInfo(kernels[0], CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, sizeof off, &off),
           CL_SUCCESS);
  void *outside = &value;
  CHECK_EQ(kernel_table.clSetKernelExecInfo(kernels[0], CL_KERNEL_EXEC_INFO_SVM_PTRS, sizeof outside, &outside),
           CL_INVALID_VALUE);
  CHECK_EQ(kernel_table.clSetKernelExecInfo(kernels[0], CL_KERNEL_EXEC_INFO_SVM_PTRS, 3u, &outside),
           CL_INVALID_VALUE);
  void *many[av::clicd::ClIcdRoot::max_linked + 1u] = {};
  CHECK_EQ(kernel_table.clSetKernelExecInfo(kernels[0], CL_KERNEL_EXEC_INFO_SVM_PTRS, sizeof many, many),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(kernel_table.clSetKernelExecInfo(kernels[0], 0x4203u, sizeof outside, &outside), CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_program_paths, the_root_s_own_calls_answer_as_the_entries_through_it_do)
{ ClIcdRig rig;
  Programs run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  av::clicd::ClIcdRoot &root = rig.root();
  CHECK_EQ(root.set_specialization_constant(run.program(), 0u, 0u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(root.set_program_release_callback(run.program(), &told), CL_INVALID_OPERATION);
  CHECK_EQ(root.unload_platform_compiler(reinterpret_cast<cl_platform_id>(run.program())), CL_INVALID_PLATFORM);
  cl_int error = CL_SUCCESS;
  CHECK(root.clone_kernel(reinterpret_cast<cl_kernel>(run.program()), &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_KERNEL);
  cl_uint count = 0u;
  CHECK_EQ(root.kernel_info(reinterpret_cast<cl_kernel>(run.program()), CL_KERNEL_REFERENCE_COUNT, sizeof count,
                            &count, nullptr),
           CL_INVALID_KERNEL);
  CHECK_EQ(root.set_kernel_exec_info(reinterpret_cast<cl_kernel>(run.program()), CL_KERNEL_EXEC_INFO_SVM_PTRS, 0u,
                                     nullptr),
           CL_INVALID_KERNEL);
  CHECK(root.extension_function("clCreateProgramWithILKHR") == root.dispatch().clCreateProgramWithIL); }
