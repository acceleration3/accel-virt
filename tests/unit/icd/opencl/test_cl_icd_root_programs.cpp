#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/cl_running_rig.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/cl_program_notify.hpp"

#include <CL/cl_ext.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::ClRunningRig;
using av::clicd::ClObject;
using av::clicd::ClProgramNotify;

namespace av::clicd {
struct ClIcdRootProbe {
    static ClObject *without_a_context(ClIcdRoot &root, ClObject::Kind kind, std::uint64_t wire)
    { ClObject *made = nullptr;
      return root.objects_.make(kind, wire, nullptr, nullptr, made) == av::Status::ok ? made : nullptr; }
    static GuestClContext *context(ClIcdRoot &root, cl_context handle)
    { ClObject *const held = root.judged(handle, ClObject::Kind::context);
      return held != nullptr ? root.context_of(*held) : nullptr; }
    static void note_build_log(ClIcdRoot &root, ClientTransport &wire, std::uint64_t program, std::uint64_t device)
    { root.note_build_log(wire, program, device, nullptr); }
    static ClObject *fill_the_object_pool(ClIcdRig &rig, std::uint32_t leave_free)
    { ClObject *made = nullptr;
      ClObject *last[8] = {};
      std::uint32_t kept = 0u;
      rig.platform().fail_allocation_at(1u);
      for (std::uint64_t wire = 0x9000u; wire < 0x9400u; ++wire) {
          if (rig.root().objects_.make(ClObject::Kind::event, wire, nullptr, nullptr, made) != av::Status::ok) break;
          last[kept++ % 8u] = made;
      }
      rig.platform().fail_allocation_at(0u);
      for (std::uint32_t freed = 0u; freed < leave_free; ++freed) rig.root().objects_.destroy(*last[(kept - 1u - freed) % 8u]);
      return made; }
};
}

using av::clicd::ClIcdRootProbe;

namespace {

constexpr std::uint64_t host_other_program = 0x6710u;

void CL_CALLBACK no_notice(cl_program, void *) {}

const unsigned char binary_bytes[8] = {'b', 'i', 'n', 'a', 'r', 'y', '!', 0u};

}

TEST(cl_icd_root_programs, a_lost_context_refuses_every_program_and_kernel_entry_that_needs_the_host)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  cl_device_id          device = run.device();
  const cl_program      program = run.program();
  const std::uint32_t   words[2] = {0x07230203u, 0u};
  const unsigned char  *binary = binary_bytes;
  const std::size_t     length = sizeof binary_bytes;
  const char           *name = "h.h";
  cl_int                status = CL_SUCCESS;
  cl_int                error = CL_SUCCESS;
  cl_int                value = 0;
  std::size_t           wide = 0u;
  cl_kernel             kernels[1] = {};
  cl_uint               made = 0u;
  run.lose();
  const std::uint32_t sent = rig.kmt().commands();

  CHECK_EQ(rig.root().build_program(program, 0u, nullptr, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().compile_program(program, 0u, nullptr, nullptr, 0u, nullptr, nullptr, ClProgramNotify()),
           CL_OUT_OF_RESOURCES);
  CHECK(rig.root().link_program(run.context(), 0u, nullptr, nullptr, 1u, &program, ClProgramNotify(), &error) ==
        nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(rig.root().create_program_with_il(run.context(), words, sizeof words, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(rig.root().create_program_with_binary(run.context(), 1u, &device, &length, &binary, &status, &error) ==
        nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(rig.root().create_program_with_built_in_kernels(run.context(), 1u, &device, name, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().program_info(program, CL_PROGRAM_SOURCE, sizeof wide, &wide, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().set_specialization_constant(program, 0u, sizeof value, &value), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().create_kernels_in_program(program, 1u, kernels, &made), CL_OUT_OF_RESOURCES);
  CHECK(rig.root().clone_kernel(run.kernel(), &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().kernel_info(run.kernel(), CL_KERNEL_FUNCTION_NAME, sizeof wide, &wide, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().kernel_arg_info(run.kernel(), 0u, CL_KERNEL_ARG_NAME, sizeof wide, &wide, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().kernel_sub_group_info(run.kernel(), nullptr, CL_KERNEL_MAX_NUM_SUB_GROUPS, 0u, nullptr,
                                            sizeof wide, &wide, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().set_kernel_exec_info(run.kernel(), CL_KERNEL_EXEC_INFO_SVM_PTRS, sizeof wide, &wide),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, a_program_or_kernel_that_belongs_to_no_context_runs_no_host_call)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  ClObject *const bare_program = ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::program, 0x6711u);
  ClObject *const bare_kernel = ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::kernel, 0x6811u);
  CHECK(bare_program != nullptr && bare_kernel != nullptr);
  if (bare_program == nullptr || bare_kernel == nullptr) return;
  const cl_program program = reinterpret_cast<cl_program>(bare_program);
  const cl_kernel  kernel = reinterpret_cast<cl_kernel>(bare_kernel);
  const std::uint32_t sent = rig.kmt().commands();
  cl_device_id device = run.device();
  CHECK_EQ(rig.root().build_program(program, 1u, &device, nullptr), CL_INVALID_DEVICE);
  CHECK_EQ(rig.root().build_program(program, 0u, nullptr, nullptr), CL_OUT_OF_RESOURCES);
  cl_device_id shown = reinterpret_cast<cl_device_id>(&shown);
  CHECK_EQ(rig.root().program_info(program, CL_PROGRAM_DEVICES, sizeof shown, &shown, nullptr), CL_SUCCESS);
  CHECK(shown == nullptr);
  cl_int value = 0;
  CHECK_EQ(rig.root().set_kernel_exec_info(kernel, CL_KERNEL_EXEC_INFO_SVM_PTRS, sizeof value, &value),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, a_program_from_binary_or_built_in_kernels_names_what_it_needs_and_what_the_host_made)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  cl_device_id          device = run.device();
  cl_device_id          stranger = reinterpret_cast<cl_device_id>(run.program());
  const unsigned char  *binary = binary_bytes;
  const unsigned char  *absent = nullptr;
  const std::size_t     length = sizeof binary_bytes;
  cl_int                status = CL_SUCCESS;
  cl_int                error = CL_SUCCESS;
  const std::uint32_t   sent = rig.kmt().commands();

  CHECK(rig.root().create_program_with_binary(run.context(), 1u, nullptr, &length, &binary, &status, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(rig.root().create_program_with_binary(run.context(), 1u, &device, &length, nullptr, &status, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(rig.root().create_program_with_binary(run.context(), 1u, &device, &length, &absent, &status, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(status, CL_INVALID_VALUE);
  CHECK(rig.root().create_program_with_binary(run.context(), 1u, &stranger, &length, &binary, &status, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);

  CHECK(rig.root().create_program_with_built_in_kernels(reinterpret_cast<cl_context>(run.program()), 1u, &device, "k",
                                                        &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(rig.root().create_program_with_built_in_kernels(run.context(), 1u, nullptr, "k", &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);

  rig.host().cl_handle(host_other_program, CL_SUCCESS);
  CHECK(rig.root().create_program_with_built_in_kernels(run.context(), 1u, &device, "k", &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  rig.host().cl_handle(0u, CL_SUCCESS);
  CHECK(rig.root().create_program_with_built_in_kernels(run.context(), 1u, &device, "k", &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);

  const std::uint32_t words[2] = {0x07230203u, 0u};
  rig.host().cl_handle(0u, CL_SUCCESS);
  CHECK(rig.root().create_program_with_il(run.context(), words, sizeof words, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, a_compile_or_link_the_api_or_the_host_refuses_tells_nobody_it_ran)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_program program = run.program();
  cl_device_id     stranger = reinterpret_cast<cl_device_id>(run.queue());
  cl_int           error = CL_SUCCESS;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().compile_program(reinterpret_cast<cl_program>(run.queue()), 0u, nullptr, nullptr, 0u, nullptr,
                                      nullptr, ClProgramNotify()),
           CL_INVALID_PROGRAM);
  CHECK_EQ(rig.root().compile_program(program, 1u, &stranger, nullptr, 0u, nullptr, nullptr, ClProgramNotify()),
           CL_INVALID_DEVICE);
  CHECK_EQ(rig.kmt().commands(), sent);

  rig.host().cl_result(CL_INVALID_OPERATION);
  CHECK_EQ(rig.root().compile_program(program, 0u, nullptr, nullptr, 0u, nullptr, nullptr,
                                      ClProgramNotify(&no_notice, nullptr)),
           CL_INVALID_OPERATION);

  CHECK(rig.root().link_program(run.context(), 0u, nullptr, nullptr, 0u, &program, ClProgramNotify(), &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  rig.host().cl_handle(0u, CL_SUCCESS);
  CHECK(rig.root().link_program(run.context(), 0u, nullptr, nullptr, 1u, &program, ClProgramNotify(), &error) ==
        nullptr);
  CHECK_EQ(error, CL_LINK_PROGRAM_FAILURE);

  rig.host().cl_handle(host_other_program, CL_SUCCESS);
  ClIcdRootProbe::fill_the_object_pool(rig, 0u);
  rig.platform().fail_allocation_at(1u);
  CHECK(rig.root().link_program(run.context(), 0u, nullptr, nullptr, 1u, &program, ClProgramNotify(), &error) !=
        nullptr || error != CL_SUCCESS);
  CHECK(error != CL_SUCCESS);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, the_binaries_of_a_program_are_fetched_only_when_the_host_sizes_them_as_one_number)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char  into[16] = {};
  unsigned char *pointers[1] = {into};
  const std::uint32_t half = 6u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&half), sizeof half);
  CHECK_EQ(rig.root().program_info(run.program(), CL_PROGRAM_BINARIES, sizeof pointers, pointers, nullptr),
           CL_OUT_OF_RESOURCES);
  const std::size_t none = 0u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&none), sizeof none);
  CHECK_EQ(rig.root().program_info(run.program(), CL_PROGRAM_BINARIES, sizeof pointers, pointers, nullptr),
           CL_SUCCESS);
  const std::uint32_t pattern = 0u;
  CHECK_EQ(rig.root().set_specialization_constant(run.program(), 0u, 0u, &pattern), CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, a_header_or_a_device_that_is_not_this_contexts_is_refused_by_name)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  ClObject *const bare_program = ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::program, 0x6712u);
  ClObject *const bare_device = ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::device, 0x6312u);
  CHECK(bare_program != nullptr && bare_device != nullptr);
  if (bare_program == nullptr || bare_device == nullptr) return;
  const cl_program header = reinterpret_cast<cl_program>(bare_program);
  cl_device_id     foreign = reinterpret_cast<cl_device_id>(bare_device);
  const char      *name = "h.h";
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().compile_program(run.program(), 0u, nullptr, nullptr, 1u, &header, &name, ClProgramNotify()),
           CL_INVALID_PROGRAM);
  CHECK_EQ(rig.root().build_program(run.program(), 1u, &foreign, nullptr), CL_INVALID_DEVICE);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().link_program(run.context(), 0u, nullptr, nullptr, 1u, &header, ClProgramNotify(), &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROGRAM);
  CHECK(rig.root().link_program(run.context(), 0u, nullptr, nullptr, 1u, &header, ClProgramNotify(&no_notice, nullptr),
                                nullptr) == nullptr);
  CHECK(rig.root().link_program(run.context(), 0u, nullptr, nullptr, 1u, &header, ClProgramNotify(nullptr, &error),
                                &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, a_kernel_query_the_api_would_refuse_asks_the_host_nothing)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_kernel not_a_kernel = reinterpret_cast<cl_kernel>(run.queue());
  std::size_t     wide = 0u;
  cl_int          value = 0;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().kernel_arg_info(not_a_kernel, 0u, CL_KERNEL_ARG_NAME, sizeof wide, &wide, nullptr),
           CL_INVALID_KERNEL);
  CHECK_EQ(rig.root().kernel_sub_group_info(not_a_kernel, nullptr, CL_KERNEL_MAX_NUM_SUB_GROUPS, 0u, nullptr,
                                            sizeof wide, &wide, nullptr),
           CL_INVALID_KERNEL);
  CHECK_EQ(rig.root().set_kernel_exec_info(not_a_kernel, CL_KERNEL_EXEC_INFO_SVM_PTRS, sizeof wide, &wide),
           CL_INVALID_KERNEL);
  CHECK_EQ(rig.root().set_kernel_exec_info(run.kernel(), CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM, sizeof value,
                                           nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().set_kernel_exec_info(run.kernel(), CL_KERNEL_EXEC_INFO_SVM_PTRS, sizeof wide, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().set_kernel_exec_info(run.kernel(), CL_KERNEL_EXEC_INFO_SVM_PTRS, 0u, &wide), CL_INVALID_VALUE);
  const std::size_t huge = (std::size_t{0x40000000u}) * sizeof(void *);
  CHECK_EQ(rig.root().set_kernel_exec_info(run.kernel(), CL_KERNEL_EXEC_INFO_SVM_PTRS, huge, &wide), CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, a_kernel_names_its_device_for_sub_groups_and_svm_pointers_for_the_host)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  std::size_t wide = 0u;
  const std::size_t answered = 3u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&answered), sizeof answered);
  CHECK_EQ(rig.root().kernel_sub_group_info(run.kernel(), run.device(), CL_KERNEL_MAX_NUM_SUB_GROUPS, 0u, nullptr,
                                            sizeof wide, &wide, nullptr),
           CL_SUCCESS);
  CHECK_EQ(wide, 3u);

  rig.host().cl_handle(0x0000'7F55'0000'1000ull, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(run.context(), CL_MEM_READ_WRITE, 256u, 0u);
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;
  void *pointers[1] = {mirror};
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_exec_info(run.kernel(), CL_KERNEL_EXEC_INFO_SVM_PTRS, sizeof pointers, pointers),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, kernels_made_together_with_no_room_for_a_wrapper_hand_the_rest_back_to_the_host)
{ for (std::uint32_t made_by_host = 3u; made_by_host <= 4u; ++made_by_host) {
      ClIcdRig     rig;
      ClRunningRig run(rig);
      CHECK(run.made());
      if (!run.made()) return;
      const std::uint64_t hosts[4] = {host_other_program, host_other_program + 1u, host_other_program + 2u,
                                      host_other_program + 3u};
      ClIcdRootProbe::fill_the_object_pool(rig, 1u);
      rig.host().cl_ids(CL_SUCCESS, hosts, made_by_host > 3u ? 3u : made_by_host, made_by_host);
      rig.host().cl_result(CL_SUCCESS);
      rig.platform().fail_allocation_at(2u);
      cl_kernel kernels[3] = {};
      CHECK_EQ(rig.root().create_kernels_in_program(run.program(), 3u, kernels, nullptr), CL_OUT_OF_HOST_MEMORY);
      CHECK(rig.root().invariants_hold());
  } }

TEST(cl_icd_root_programs, a_host_that_makes_no_clone_names_an_error_and_a_kernel_list_without_memory_is_refused)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  cl_int error = CL_SUCCESS;
  rig.host().cl_handle(0u, CL_SUCCESS);
  CHECK(rig.root().clone_kernel(run.kernel(), &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  cl_kernel kernels[1] = {};
  rig.platform().fail_allocation_at(1u);
  CHECK_EQ(rig.root().create_kernels_in_program(run.program(), 1u, kernels, nullptr), CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, a_build_that_failed_logs_the_hosts_log_line_by_line_up_to_a_limit)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  av::clicd::GuestClContext *const guest = ClIcdRootProbe::context(rig.root(), run.context());
  CHECK(guest != nullptr);
  if (guest == nullptr) return;
  const std::uint32_t sent = rig.kmt().commands();
  ClIcdRootProbe::note_build_log(rig.root(), *guest->wire(), ClRunningRig::host_program, 0u);
  CHECK_EQ(rig.kmt().commands(), sent);

  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  ClIcdRootProbe::note_build_log(rig.root(), *guest->wire(), ClRunningRig::host_program, ClRunningRig::host_device);
  rig.host().cl_info(CL_SUCCESS, "", 0u);
  ClIcdRootProbe::note_build_log(rig.root(), *guest->wire(), ClRunningRig::host_program, ClRunningRig::host_device);

  const char log[] = "first problem\nsecond problem";
  rig.host().cl_info(CL_SUCCESS, log, sizeof log);
  rig.platform().fail_allocation_at(1u);
  ClIcdRootProbe::note_build_log(rig.root(), *guest->wire(), ClRunningRig::host_program, ClRunningRig::host_device);
  CHECK_EQ(rig.log().holding("build: first problem"), 0u);

  rig.host().cl_info(CL_SUCCESS, log, sizeof log);
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  ClIcdRootProbe::note_build_log(rig.root(), *guest->wire(), ClRunningRig::host_program, ClRunningRig::host_device);
  CHECK_EQ(rig.log().holding("build: first problem"), 0u);

  rig.host().cl_info(CL_SUCCESS, log, sizeof log);
  rig.host().cl_info(CL_SUCCESS, log, sizeof log);
  ClIcdRootProbe::note_build_log(rig.root(), *guest->wire(), ClRunningRig::host_program, ClRunningRig::host_device);
  CHECK_EQ(rig.log().holding("build: first problem"), 1u);
  CHECK_EQ(rig.log().holding("build: second problem"), 1u);

  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, a_long_build_log_is_cut_at_a_limit_of_lines)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  av::clicd::GuestClContext *const guest = ClIcdRootProbe::context(rig.root(), run.context());
  CHECK(guest != nullptr);
  if (guest == nullptr) return;
  char        many[400] = {};
  std::size_t at = 0u;
  for (unsigned line = 1u; line <= 30u; ++line) {
      many[at++] = 'l';
      many[at++] = static_cast<char>('0' + line / 10u);
      many[at++] = static_cast<char>('0' + line % 10u);
      many[at++] = '\n';
  }
  rig.host().cl_info(CL_SUCCESS, many, static_cast<std::uint32_t>(at + 1u));
  rig.host().cl_info(CL_SUCCESS, many, static_cast<std::uint32_t>(at + 1u));
  const std::uint32_t before = rig.log().lines();
  ClIcdRootProbe::note_build_log(rig.root(), *guest->wire(), ClRunningRig::host_program, ClRunningRig::host_device);
  CHECK_EQ(rig.log().lines(), before + 25u);
  CHECK_EQ(rig.log().holding("build: l24"), 1u);
  CHECK_EQ(rig.log().holding("build: l25"), 0u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_programs, a_host_that_makes_more_kernels_than_there_is_room_for_hands_back_what_fits)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::uint64_t hosts[3] = {host_other_program, host_other_program + 1u, host_other_program + 2u};
  rig.host().cl_ids(CL_SUCCESS, hosts, 3u, 4u);
  cl_kernel kernels[3] = {};
  cl_uint   made = 0u;
  CHECK_EQ(rig.root().create_kernels_in_program(run.program(), 3u, kernels, &made), CL_SUCCESS);
  CHECK_EQ(made, 4u);
  CHECK(kernels[0] != nullptr && kernels[1] != nullptr && kernels[2] != nullptr);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }
