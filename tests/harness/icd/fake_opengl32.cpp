#include "harness/icd/fake_opengl32.hpp"

#include "harness/icd/scripted_thread_slot.hpp"

#include <cstring>

namespace av {

namespace {

/* opengl32's callbacks are handed no context: the one fake alive answers. */
FakeOpengl32 *alive = nullptr;

std::uintptr_t APIENTRY get_dhglrc(void *context)
{
    if (alive == nullptr) return 0u;
    alive->count_lookup();
    return alive->driver_context(context);
}
std::uintptr_t APIENTRY unused_callback() { return 0u; }

using Validate = std::int32_t (APIENTRY *)(std::uint32_t);
using SetProcs = void (APIENTRY *)(std::int32_t, void *const *);
using Describe = std::int32_t (APIENTRY *)(void *, std::int32_t, std::uint32_t, std::uint8_t *);
using SetFormat = std::int32_t (APIENTRY *)(void *, std::int32_t);
using CreateLayer = std::uintptr_t (APIENTRY *)(void *, std::int32_t);
using Delete = std::int32_t (APIENTRY *)(std::uintptr_t);
using SetContext = void *(APIENTRY *)(void *, std::uintptr_t, void *);
using Release = std::int32_t (APIENTRY *)(std::uintptr_t);
using Swap = std::int32_t (APIENTRY *)(void *);
using Share = std::int32_t (APIENTRY *)(std::uintptr_t, std::uintptr_t);
using GetProc = void *(APIENTRY *)(const char *);

/* The HGLRC for context row n: a value no pointer the test holds is. */
constexpr std::uintptr_t hglrc_base = 0x10000u;

}  // namespace

FakeOpengl32::FakeOpengl32(ScriptedThreadSlot &threads)
    : threads_(threads), exports_(), hidden_{}, hidden_count_(0u), contexts_{}, tables_{}, current_{}, lookups_(0u),
      refuse_create_(false), skip_driver_(false), procs_{}
{
    alive = this;
    for (void *&proc : procs_) proc = reinterpret_cast<void *>(&unused_callback);
    procs_[2] = reinterpret_cast<void *>(&get_dhglrc);
}

FakeOpengl32::~FakeOpengl32() { alive = nullptr; }

void FakeOpengl32::hide_export(const char *name)
{
    if (hidden_count_ < 4u) hidden_[hidden_count_++] = name;
}

FakeOpengl32::Load FakeOpengl32::load(std::uint32_t version, std::uint32_t flags)
{
    const bool creates = resolve("DrvCreateContext") != nullptr || resolve("DrvCreateLayerContext") != nullptr;
    const char *const required[] = {"DrvDeleteContext", "DrvSetContext", "DrvReleaseContext"};
    const char *const formats[] = {"DrvDescribePixelFormat", "DrvSetPixelFormat", "DrvSwapBuffers"};
    bool present = creates && resolve("DrvValidateVersion") != nullptr && resolve("DrvSetCallbackProcs") != nullptr;
    for (const char *const name : required) present = present && resolve(name) != nullptr;
    for (const char *const name : formats) present = present && ((flags & 1u) == 0u || resolve(name) != nullptr);
    if (!present) return Load::missing_entry;
    if (reinterpret_cast<Validate>(resolve("DrvValidateVersion"))(version) == 0) return Load::refused_version;
    reinterpret_cast<SetProcs>(resolve("DrvSetCallbackProcs"))(callbacks, procs_);
    return Load::loaded;
}

std::int32_t FakeOpengl32::describe_pixel_format(void *dc, std::int32_t index, std::uint32_t bytes,
                                                 std::uint8_t *descriptor)
{
    return reinterpret_cast<Describe>(resolve("DrvDescribePixelFormat"))(dc, index, bytes, descriptor);
}

bool FakeOpengl32::set_pixel_format(void *dc, std::int32_t index)
{
    return reinterpret_cast<SetFormat>(resolve("DrvSetPixelFormat"))(dc, index) != 0;
}

void FakeOpengl32::refuse_next_create() { refuse_create_ = true; }
void FakeOpengl32::skip_driver_next_create() { skip_driver_ = true; }

void *FakeOpengl32::create_context(void *dc)
{
    if (refuse_create_) {
        refuse_create_ = false;
        return nullptr;
    }
    if (skip_driver_) {
        skip_driver_ = false;
        return reinterpret_cast<void *>(hglrc_base + max_contexts);
    }
    const std::uintptr_t made = reinterpret_cast<CreateLayer>(resolve("DrvCreateLayerContext"))(dc, 0);
    if (made == 0u) return nullptr;
    for (std::uint32_t row = 0u; row < max_contexts; row++) {
        if (contexts_[row] != 0u) continue;
        contexts_[row] = made;
        return reinterpret_cast<void *>(hglrc_base + row);
    }
    return nullptr;
}

bool FakeOpengl32::make_current(void *dc, void *context)
{
    const std::uint32_t thread = threads_.thread_id() - 1u;
    if (context == nullptr) {
        if (current_[thread] == 0u) return true;
        const bool released = reinterpret_cast<Release>(resolve("DrvReleaseContext"))(current_[thread]) != 0;
        current_[thread] = 0u;
        tables_[thread] = nullptr;
        return released;
    }
    const std::uintptr_t driver = driver_context(context);
    if (driver == 0u) return false;
    const auto *const table = static_cast<const Table *>(
        reinterpret_cast<SetContext>(resolve("DrvSetContext"))(dc, driver, nullptr));
    if (table == nullptr) return false;
    if (table->entries != static_cast<std::int32_t>(full_table) &&
        table->entries != static_cast<std::int32_t>(short_table))
        return false;
    tables_[thread] = table;
    current_[thread] = driver;
    return true;
}

bool FakeOpengl32::delete_context(void *context)
{
    const std::uintptr_t row = reinterpret_cast<std::uintptr_t>(context) - hglrc_base;
    const std::uintptr_t driver = driver_context(context);
    if (driver == 0u) return false;
    const bool deleted = reinterpret_cast<Delete>(resolve("DrvDeleteContext"))(driver) != 0;
    if (deleted) contexts_[row] = 0u;
    return deleted;
}

bool FakeOpengl32::share_lists(void *first, void *second)
{
    return reinterpret_cast<Share>(resolve("DrvShareLists"))(driver_context(first), driver_context(second)) != 0;
}

bool FakeOpengl32::swap_buffers(void *dc) { return reinterpret_cast<Swap>(resolve("DrvSwapBuffers"))(dc) != 0; }

/* opengl32's wglGetProcAddress asks the ICD only for a thread with a context
 * of it current, and answers null otherwise - the live guest's attribs step
 * failed with ERROR_PROC_NOT_FOUND for exactly that. */
void *FakeOpengl32::get_proc_address(const char *name)
{
    if (current_[threads_.thread_id() - 1u] == 0u) return nullptr;
    return driver_proc_address(name);
}

void *FakeOpengl32::driver_proc_address(const char *name)
{
    return reinterpret_cast<GetProc>(resolve("DrvGetProcAddress"))(name);
}

std::uintptr_t FakeOpengl32::driver_context(void *context) const
{
    const std::uintptr_t row = reinterpret_cast<std::uintptr_t>(context) - hglrc_base;
    return row < max_contexts ? contexts_[row] : 0u;
}

void *FakeOpengl32::slot(std::uint32_t index) const
{
    const Table *const table = tables_[threads_.thread_id() - 1u];
    return table != nullptr && index < full_table ? table->slots[index] : nullptr;
}

std::uint32_t FakeOpengl32::table_entries() const
{
    const Table *const table = tables_[threads_.thread_id() - 1u];
    return table != nullptr ? static_cast<std::uint32_t>(table->entries) : 0u;
}

std::uint32_t FakeOpengl32::dhglrc_lookups() const { return lookups_; }
void          FakeOpengl32::count_lookup() { lookups_++; }

void *FakeOpengl32::resolve(const char *name) const
{
    for (std::uint32_t index = 0u; index < hidden_count_; index++)
        if (std::strcmp(hidden_[index], name) == 0) return nullptr;
    return exports_.address(name);
}

}  // namespace av
