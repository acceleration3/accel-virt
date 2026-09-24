/* The OpenGL samples (samples/opengl) through the OpenGL ICD on this
 * machine's GPU, with no guest: FakeOpengl32 loads the ICD as the system
 * opengl32 would, and the ICD root, over DdiKmt, carries every command through
 * the real driver and device core to the executor library - the same path
 * rvkglsamples.exe takes in the VM, less the VM. Its frames and metrics are
 * the guest run's in shape, so tools/opengl_report.py can set them beside the
 * host's (--loopback). It is a tool, not a test: a sample the ICD cannot run
 * is reported, not failed. */
#include "device/abi/rvk_executor.h"
#include "harness/executor/executor_library_file.hpp"
#include "harness/executor/widest_host_gpu.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "harness/icd/fake_opengl32.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread_slot.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/gl_icd_root.hpp"

#include "samples/opengl/framework.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

/* The executor opens every host API when it loads, and those drivers keep
 * what they allocate until exit. */
extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\nleak:libEGL\nleak:libGLX\nleak:libglapi\nleak:libvulkan\nleak:libnvidia\n"
           "leak:iris_dri\nleak:libdbus-1\nleak:libz\nleak:libLLVM\nleak:libgallium\nleak:librvk_executor\n";
}

namespace {

constexpr std::uint32_t window_side = 64u;
constexpr std::uint32_t max_slots = av::FakeOpengl32::full_table;
/* WGL_ARB_create_context and WGL_ARB_create_context_profile. */
constexpr std::int32_t wgl_context_major_version = 0x2091, wgl_context_minor_version = 0x2092,
                       wgl_context_profile_mask = 0x9126, wgl_context_core_profile_bit = 0x0001;

/* The ICD as one guest process holds it, as test_fullstack_opengl_icd builds
 * it. */
class GlProcess {
public:
    explicit GlProcess(av::DdiKmt &kmt)
        : lock_(trace_, 'H'), send_lock_(trace_, 'S'), opengl32_(threads_),
          root_(memory_, kmt, lock_, send_lock_, log_, surface_, threads_, opengl32_, av::glicd::gl_icd_binding)
    {
        surface_.add_dc(&dc_, &window_, window_side, window_side);
    }

    av::FakeOpengl32 &opengl32() { return opengl32_; }
    av::RecordingLog &log() { return log_; }
    void             *dc() { return &dc_; }

private:
    av::MockPlatform          memory_;
    av::LockTrace             trace_;
    av::RecordingThreadLock   lock_;
    av::RecordingThreadLock   send_lock_;
    av::RecordingLog          log_;
    av::ScriptedWindowSurface surface_;
    av::ScriptedThreadSlot    threads_;
    av::FakeOpengl32          opengl32_;
    av::glicd::GlIcdRoot      root_;
    int                       dc_ = 0;
    int                       window_ = 0;
};

/* opengl32's own table: the GL 1.1 names are its exports, found by slot. */
char slot_names[max_slots][48];

void read_slot_names()
{
    std::FILE *file = std::fopen(AV_PROC_TABLE, "r");
    if (file == nullptr) return;
    char line[128];
    while (std::fgets(line, sizeof line, file) != nullptr) {
        unsigned index = 0u;
        char     name[48] = {};
        if (line[0] == '#' || std::sscanf(line, "%u %47s", &index, name) != 2 || index >= max_slots) continue;
        std::memcpy(slot_names[index], name, sizeof name);
    }
    std::fclose(file);
}

/* --kernel-submits: the process never publishes its own ring tail, and every
 * batch goes through the kernel - the path a driver that refuses the escape
 * gives, and the one to compare the shared tail's against. */
bool                       kernel_submits = false;
av::WidestHostGpu         *gpu = nullptr;
av::ExecutorLibraryFile   *library = nullptr;
av::FullStackRig          *rig = nullptr;
GlProcess                 *process = nullptr;

}  // namespace

namespace samples {

void log(const char *text)
{
    std::fputs(text, stdout);
    std::fflush(stdout);
}

void log_number(const char *label, double value) { std::printf("%s%.3f\n", label, value); }

void *allocate(size_t bytes) { return std::calloc(1, bytes != 0 ? bytes : 1); }
void  release_memory(void *memory) { std::free(memory); }

double seconds()
{
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) * 1e-9;
}

bool write_file(const char *path, const void *data, size_t size)
{
    std::FILE *file = std::fopen(path, "wb");
    if (file == nullptr) return false;
    const bool ok = std::fwrite(data, 1, size, file) == size;
    std::fclose(file);
    return ok;
}

bool read_file(const char *path, Blob &blob)
{
    std::FILE *file = std::fopen(path, "rb");
    if (file == nullptr) return false;
    std::fseek(file, 0, SEEK_END);
    blob.size = static_cast<size_t>(std::ftell(file));
    std::fseek(file, 0, SEEK_SET);
    blob.data = allocate(blob.size);
    const bool ok = std::fread(blob.data, 1, blob.size, file) == blob.size;
    std::fclose(file);
    return ok;
}

/* The GPU is the one the rig's executor runs the most sub-APIs on. */
bool create_context(const char *)
{
    read_slot_names();
    gpu = new av::WidestHostGpu();
    if ((gpu->sub_apis() & RVK_EXECUTOR_SUB_API_OPENGL) == 0u) return false;
    library = new av::ExecutorLibraryFile(AV_EXECUTOR_LIBRARY);
    if (library->ops() == nullptr) return false;
    rig = new av::FullStackRig(*library->ops(), gpu->uuid());
    if (rig->started() != 0) return false;
    rig->kmt().let_process_watch();
    if (kernel_submits) rig->kmt().refuse_tail_publishing();
    process = new GlProcess(rig->kmt());
    av::FakeOpengl32 &gl = process->opengl32();
    if (gl.load(av::glicd::GlIcdRoot::icd_version, 1u) != av::FakeOpengl32::Load::loaded ||
        !gl.set_pixel_format(process->dc(), 1))
        return false;
    void *const legacy = gl.create_context(process->dc());
    if (legacy == nullptr || !gl.make_current(process->dc(), legacy)) return false;
    using Create = void *(APIENTRY *)(void *, void *, const std::int32_t *);
    const auto create = reinterpret_cast<Create>(gl.get_proc_address("wglCreateContextAttribsARB"));
    const std::int32_t attributes[] = {wgl_context_major_version, 4, wgl_context_minor_version, 5,
                                       wgl_context_profile_mask, wgl_context_core_profile_bit, 0};
    void *const core = create != nullptr ? create(process->dc(), nullptr, attributes) : nullptr;
    gl.make_current(nullptr, nullptr);
    gl.delete_context(legacy);
    return core != nullptr && gl.make_current(process->dc(), core);
}

void *gl_proc(const char *name)
{
    for (std::uint32_t index = 0u; index < max_slots; index++)
        if (std::strcmp(slot_names[index], name) == 0) return process->opengl32().slot(index);
    return process->opengl32().get_proc_address(name);
}

}  // namespace samples

int main(int argc, char **argv)
{
    int kept = 0;
    for (int i = 0; i < argc; i++) {
        if (std::strcmp(argv[i], "--kernel-submits") == 0) kernel_submits = true;
        else argv[kept++] = argv[i];
    }
    const int status = samples::run(kept, argv);
    if (process != nullptr)
        for (std::uint32_t index = 0u; index < process->log().lines(); index++)
            std::fprintf(stderr, "icd: %s\n", process->log().line(index));
    /* The rig and the ICD are left to the process's end: taking them down in
     * order is the fullstack tests' business, and a tool that has written
     * its results has nothing left to prove. */
    std::fflush(stdout);
    std::_Exit(status);
}
