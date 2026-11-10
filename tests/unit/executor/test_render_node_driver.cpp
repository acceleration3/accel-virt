#include "executor/gpu_clock_hold.hpp"
#include "executor/render_node_driver.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using av::GpuClockHold;
using av::RenderNodeDriver;

namespace {

struct DrmClass {
    std::string root;

    DrmClass()
    {
        char pattern[] = "/tmp/av-drm-class-XXXXXX";
        const char *const made = mkdtemp(pattern);
        root = made != nullptr ? made : "";
    }

    ~DrmClass()
    {
        const std::string command = "rm -rf '" + root + "'";
        if (!root.empty()) static_cast<void>(std::system(command.c_str()));
    }

    void node(std::int32_t minor, const char *driver) const
    {
        const std::string directory = root + "/renderD" + std::to_string(minor);
        mkdir(directory.c_str(), 0700);
        mkdir((directory + "/device").c_str(), 0700);
        const std::string target = std::string("../../../bus/pci/drivers/") + driver;
        static_cast<void>(symlink(target.c_str(), (directory + "/device/driver").c_str()));
    }
};

}

TEST(render_node_driver, names_the_kernel_driver_a_render_node_is_bound_to)
{ DrmClass drm;
  drm.node(128, "amdgpu");
  drm.node(129, "nvidia");
  drm.node(130, "i915");
  RenderNodeDriver drivers(drm.root.c_str());
  CHECK(drivers.named(128, "amdgpu"));
  CHECK(!drivers.named(128, "nvidia"));
  CHECK(drivers.named(129, "nvidia"));
  CHECK(!drivers.named(129, "amdgpu"));
  CHECK(drivers.named(130, "i915"));
  CHECK(!drivers.named(130, "amdgpu")); }

TEST(render_node_driver, a_node_that_is_missing_or_unnamed_belongs_to_no_driver)
{ DrmClass drm;
  drm.node(128, "amdgpu");
  RenderNodeDriver drivers(drm.root.c_str());
  CHECK(!drivers.named(131, "amdgpu"));
  CHECK(!drivers.named(-1, "amdgpu"));
  CHECK(!drivers.named(128, nullptr));
  RenderNodeDriver nowhere("/nonexistent-drm-class");
  CHECK(!nowhere.named(128, "amdgpu")); }

TEST(render_node_driver, a_driver_name_that_only_ends_alike_is_another_driver)
{ DrmClass drm;
  drm.node(128, "xamdgpu");
  RenderNodeDriver drivers(drm.root.c_str());
  CHECK(!drivers.named(128, "amdgpu")); }

TEST(gpu_clock_hold_open, a_node_bound_to_another_vendors_driver_holds_nothing)
{ DrmClass drm;
  drm.node(129, "nvidia");
  drm.node(130, "i915");
  GpuClockHold hold(drm.root.c_str());
  CHECK(!hold.open(129));
  CHECK(!hold.open(129));
  CHECK(!hold.open(130));
  hold.busy();
  hold.close(); }
