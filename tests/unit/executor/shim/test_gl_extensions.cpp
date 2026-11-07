#include "executor/shim/gl_extensions.hpp"
#include "harness/framework/test_registry.hpp"

using av::GlExtensions;

TEST(gl_extensions, lists_nothing_until_it_has_a_context_to_ask)
{ GlExtensions extensions;
  CHECK(!extensions.lists("GL_EXT_memory_object")); }
