#include "harness/framework/test_registry.hpp"
#include "harness/icd/fake_opengl32.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/system_opengl.hpp"

#include <cstring>

using av::FakeOpengl32;
using av::GlIcdRig;
using Load = FakeOpengl32::Load;

TEST(fake_opengl32, loads_the_icd_through_its_fifteen_exports)
{ GlIcdRig rig;
  CHECK(rig.opengl32().load(2u, 1u) == Load::loaded);
  const av::glicd::DriverExports exports;
  for (std::uint32_t index = 0u; index < av::glicd::DriverExports::count; index++)
      CHECK(exports.address(exports.name(index)) != nullptr);
  CHECK(exports.name(av::glicd::DriverExports::count) == nullptr);
  CHECK(exports.address("DrvPresentBuffers") == nullptr);
  CHECK(exports.address(nullptr) == nullptr);
  CHECK(std::strcmp(exports.name(0u), "DrvValidateVersion") == 0); }

TEST(fake_opengl32, refuses_an_icd_missing_what_opengl32_requires)
{ const char *const required[] = {"DrvDeleteContext", "DrvSetContext", "DrvReleaseContext", "DrvValidateVersion",
                                  "DrvSetCallbackProcs"};
  for (const char *const name : required) {
      GlIcdRig rig;
      rig.opengl32().hide_export(name);
      CHECK(rig.opengl32().load(2u, 0u) == Load::missing_entry);
  }
  GlIcdRig both;
  both.opengl32().hide_export("DrvCreateContext");
  CHECK(both.opengl32().load(2u, 0u) == Load::loaded);
  both.opengl32().hide_export("DrvCreateLayerContext");
  CHECK(both.opengl32().load(2u, 0u) == Load::missing_entry);
  const char *const formats[] = {"DrvDescribePixelFormat", "DrvSetPixelFormat", "DrvSwapBuffers"};
  for (const char *const name : formats) {
      GlIcdRig rig;
      rig.opengl32().hide_export(name);
      CHECK(rig.opengl32().load(2u, 0u) == Load::loaded);
      CHECK(rig.opengl32().load(2u, 1u) == Load::missing_entry);
  } }

TEST(fake_opengl32, refuses_an_icd_that_does_not_validate_the_version)
{ GlIcdRig rig;
  CHECK(rig.opengl32().load(1u, 1u) == Load::refused_version);
  CHECK(rig.opengl32().load(3u, 1u) == Load::refused_version);
  CHECK(rig.opengl32().load(2u, 1u) == Load::loaded); }

TEST(fake_opengl32, takes_a_proc_table_per_thread_and_answers_contexts_by_hglrc)
{ GlIcdRig rig;
  rig.load_with_format();
  av::glicd::SystemOpengl &system = rig.opengl32();
  CHECK(system.create_context(rig.memory_dc()) == nullptr);
  CHECK(rig.opengl32().slot(0u) == nullptr);
  CHECK_EQ(rig.opengl32().table_entries(), 0u);
  void *const context = rig.current_context();
  CHECK(context != nullptr);
  CHECK_EQ(rig.opengl32().table_entries(), FakeOpengl32::full_table);
  CHECK(rig.opengl32().slot(FakeOpengl32::full_table) == nullptr);
  CHECK(rig.opengl32().driver_context(context) != 0u);
  CHECK_EQ(rig.opengl32().driver_context(reinterpret_cast<void *>(0x5u)), 0u);
  rig.threads().switch_to(2u);
  CHECK(rig.opengl32().slot(0u) == nullptr);
  CHECK(rig.opengl32().make_current(rig.dc(), nullptr));
  rig.threads().switch_to(1u);
  CHECK(rig.opengl32().make_current(rig.dc(), nullptr));
  CHECK_EQ(rig.opengl32().table_entries(), 0u);
  CHECK(!rig.opengl32().make_current(rig.dc(), reinterpret_cast<void *>(0x5u)));
  CHECK(!rig.opengl32().delete_context(reinterpret_cast<void *>(0x5u)));
  CHECK(rig.opengl32().delete_context(context));
  CHECK(!rig.opengl32().make_current(rig.dc(), context)); }

TEST(fake_opengl32, names_a_function_only_to_a_thread_with_a_context_current)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.opengl32().get_proc_address("wglCreateContextAttribsARB") == nullptr);
  CHECK(rig.opengl32().driver_proc_address("wglCreateContextAttribsARB") != nullptr);
  void *const context = rig.current_context();
  CHECK(rig.opengl32().get_proc_address("wglCreateContextAttribsARB") ==
        rig.opengl32().driver_proc_address("wglCreateContextAttribsARB"));
  rig.threads().switch_to(2u);
  CHECK(rig.opengl32().get_proc_address("wglCreateContextAttribsARB") == nullptr);
  rig.threads().switch_to(1u);
  CHECK(rig.opengl32().make_current(rig.dc(), nullptr));
  CHECK(rig.opengl32().get_proc_address("wglCreateContextAttribsARB") == nullptr);
  CHECK(rig.opengl32().delete_context(context)); }
