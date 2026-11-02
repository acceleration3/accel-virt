#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_d3d11_objects.hpp"
#include "icd/opencl/d3d11_resource.hpp"

#include <cstring>

using av::MockPlatform;
using av::Status;
using av::clicd::ClD3d11Objects;
using av::clicd::D3d11Resource;

namespace {

int device = 0, resource = 0, other = 0;
int first = 0, second = 0, stranger = 0;
const D3d11Resource buffer(D3d11Resource::Kind::buffer, 0u, 64u, 0u, 0u, 0u, 1u, 1u, 1u);

cl_mem as_mem(int &object) { return reinterpret_cast<cl_mem>(&object); }

}

TEST(cl_d3d11_objects, it_keeps_one_object_per_subresource_and_forgets_it)
{ MockPlatform   platform;
  ClD3d11Objects objects(platform);
  CHECK(objects.device() == nullptr);
  CHECK(objects.invariants_hold());
  objects.share_with(&device);
  CHECK(objects.device() == &device);
  CHECK_STATUS(objects.keep(&first, &resource, 0u, buffer, CL_MEM_READ_WRITE), Status::ok);
  CHECK(objects.made_over(&resource, 0u));
  CHECK(!objects.made_over(&resource, 1u));
  CHECK(!objects.made_over(&other, 0u));
  CHECK(objects.find(&first) != nullptr);
  CHECK(objects.find(&second) == nullptr);
  CHECK_EQ(objects.live(), 1u);
  void *forgotten = nullptr;
  CHECK(!objects.forget(&second, forgotten));
  CHECK(forgotten == nullptr);
  CHECK(objects.forget(&first, forgotten));
  CHECK(forgotten == &resource);
  CHECK_EQ(objects.live(), 0u);
  CHECK(objects.invariants_hold()); }

TEST(cl_d3d11_objects, a_list_is_taken_and_given_whole_or_not_at_all)
{ MockPlatform   platform;
  ClD3d11Objects objects(platform);
  objects.share_with(&device);
  CHECK_STATUS(objects.keep(&first, &resource, 0u, buffer, CL_MEM_READ_WRITE), Status::ok);
  CHECK_STATUS(objects.keep(&second, &other, 0u, buffer, CL_MEM_READ_WRITE), Status::ok);
  const cl_mem both[2] = {as_mem(first), as_mem(second)};
  const cl_mem with_stranger[2] = {as_mem(first), as_mem(stranger)};
  CHECK_EQ(objects.take(with_stranger, 2u), CL_INVALID_MEM_OBJECT);
  CHECK(!objects.find(&first)->acquired());
  CHECK_EQ(objects.take(both, 0u), CL_INVALID_VALUE);
  CHECK_EQ(objects.take(nullptr, 2u), CL_INVALID_VALUE);
  CHECK_EQ(objects.give(both, 2u), ClD3d11Objects::not_acquired);
  CHECK_EQ(objects.take(both, 2u), CL_SUCCESS);
  CHECK(objects.find(&first)->acquired());
  CHECK(objects.find(&second)->acquired());
  CHECK_EQ(objects.take(both, 1u), ClD3d11Objects::already_acquired);
  CHECK_EQ(objects.give(both, 2u), CL_SUCCESS);
  CHECK(!objects.find(&second)->acquired());
  CHECK_EQ(objects.take(both, 2u), CL_SUCCESS);
  objects.untake(with_stranger, 2u);
  CHECK(!objects.find(&first)->acquired());
  CHECK(objects.find(&second)->acquired());
  CHECK_EQ(objects.give(both, 1u), ClD3d11Objects::not_acquired);
  objects.ungive(with_stranger, 2u);
  CHECK(objects.find(&first)->acquired());
  CHECK(objects.invariants_hold()); }

TEST(cl_d3d11_objects, an_object_without_a_device_is_one_nothing_could_have_made)
{ MockPlatform   platform;
  ClD3d11Objects objects(platform);
  CHECK_STATUS(objects.keep(&first, &resource, 0u, buffer, CL_MEM_READ_WRITE), Status::ok);
  CHECK(!objects.invariants_hold());
  objects.share_with(&device);
  CHECK(objects.invariants_hold());
  CHECK_STATUS(objects.keep(&second, nullptr, 0u, buffer, CL_MEM_READ_WRITE), Status::ok);
  CHECK(!objects.invariants_hold()); }

TEST(cl_d3d11_objects, its_state_names_the_device_and_each_object)
{ MockPlatform   platform;
  ClD3d11Objects objects(platform);
  objects.share_with(&device);
  CHECK_STATUS(objects.keep(&first, &resource, 3u, buffer, CL_MEM_READ_WRITE), Status::ok);
  char            text[512] = {};
  av::StateWriter writer(text, sizeof text);
  objects.report(writer);
  CHECK(std::strstr(text, "d3d11/0") != nullptr);
  CHECK(std::strstr(text, "live=1") != nullptr);
  CHECK(std::strstr(text, "subresource=3") != nullptr);
  CHECK(std::strstr(text, "acquired=false") != nullptr); }
