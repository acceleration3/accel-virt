#include "executor/shim/egl_extensions.hpp"
#include "harness/framework/test_registry.hpp"

using av::EglExtensions;

TEST(egl_extensions, lists_nothing_before_a_display_is_asked_or_after_it_is_forgotten)
{ EglExtensions extensions;
  CHECK(!extensions.lists("EGL_KHR_image_base"));
  extensions.configure(nullptr);
  CHECK(!extensions.lists("EGL_KHR_image_base"));
  extensions.forget();
  CHECK(!extensions.lists("")); }
