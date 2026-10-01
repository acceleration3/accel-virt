/* av-renderer's entry point: the process root and its control loop
 * (docs/renderer.md). */
#include "core/support/object_storage.hpp"
#include "device/shim/renderer_process.hpp"

namespace {

constinit av::ObjectStorage<av::RendererProcess> renderer_root;

}  // namespace

int main() { return renderer_root.construct() == av::Status::ok ? renderer_root.object().run() : 1; }
