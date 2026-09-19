#include "harness/guest/scripted_cl.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::uint32_t never = 0u;
constexpr std::uint32_t none = ~std::uint32_t{0};
/* cl.h's, as cl_probe.cpp spells them. */
constexpr std::uint32_t platform_name = 0x0902u;
constexpr std::uint32_t platform_icd_suffix = 0x0920u;
constexpr std::uint32_t device_name = 0x102Bu;
constexpr std::uint32_t device_vendor_id = 0x1001u;
constexpr std::uint32_t device_luid = 0x106Cu;
constexpr std::uint32_t device_svm_capabilities = 0x1053u;
/* CL_DEVICE_SVM_COARSE_GRAIN_BUFFER: what a device that has shared memory
 * offers at least. */
constexpr std::uint64_t coarse_grain_buffer = 1u;
/* CL_SUBMITTED, which a user event is until its status is set, and
 * CL_COMPLETE. */
constexpr std::int32_t submitted = 3;
constexpr std::int32_t complete = 0;
/* CL_INVALID_KERNEL_NAME. */
constexpr std::int32_t unknown_kernel = -46;
/* Handles this hands out: a base per family plus a row, never a real
 * address. */
constexpr std::uintptr_t platform_base = 0x5100u;
constexpr std::uintptr_t device_base = 0x5200u;
constexpr std::uintptr_t object_base = 0x5300u;

}  // namespace

ScriptedCl::ScriptedCl()
    : fail_at_{}, silent_at_{}, from_{}, errors_{}, calls_{}, suffixes_{}, platforms_(1u), overstating_(false), devices_(1u),
      device_name_("scripted rvkgpu"), build_log_(""), asked_devices_of_(""), luid_(default_luid),
      vendor_(default_vendor), svm_capabilities_(coarse_grain_buffer), wrong_(0u), stale_(0u),
      forgets_callbacks_(false), notifies_twice_(false), objects_{}, live_(0u), misuses_(0u),
      shared_{}, shared_live_(false), shared_argument_(nullptr)
{
    suffixes_[0] = "RVK";
}

void ScriptedCl::fail_at(Call call, std::uint32_t nth, std::int32_t error)
{
    const auto index = static_cast<std::uint32_t>(call);
    fail_at_[index] = nth;
    from_[index] = false;
    errors_[index] = error != 0 ? error : refused;
}

void ScriptedCl::fail_from(Call call, std::uint32_t nth, std::int32_t error)
{
    fail_at(call, nth, error);
    from_[static_cast<std::uint32_t>(call)] = true;
}

void ScriptedCl::fail_silently_at(Call call, std::uint32_t nth)
{
    silent_at_[static_cast<std::uint32_t>(call)] = nth;
}

std::uint32_t ScriptedCl::calls(Call call) const
{
    return calls_[static_cast<std::uint32_t>(call)];
}

void ScriptedCl::set_platform_suffix(std::uint32_t index, const char *suffix)
{
    if (index < platforms_) suffixes_[index] = suffix;
}

void ScriptedCl::overstate_platforms() { overstating_ = true; }

void ScriptedCl::add_platform(const char *suffix)
{
    if (platforms_ == max_platforms) return;
    suffixes_[platforms_] = suffix;
    platforms_++;
}

void ScriptedCl::set_device_count(std::uint32_t count)
{
    devices_ = count < max_devices ? count : max_devices;
}

void ScriptedCl::set_device_name(const char *name) { device_name_ = name; }
void ScriptedCl::set_luid(std::uint64_t luid) { luid_ = luid; }
void ScriptedCl::set_vendor(std::uint64_t vendor) { vendor_ = vendor; }
void ScriptedCl::offer_no_svm() { svm_capabilities_ = 0u; }
void ScriptedCl::set_build_log(const char *log) { build_log_ = log; }
void ScriptedCl::miscompute(std::uint32_t wrong) { wrong_ = wrong; }
void ScriptedCl::stale_mappings(std::uint32_t wrong) { stale_ = wrong; }
void ScriptedCl::forget_callbacks() { forgets_callbacks_ = true; }
void ScriptedCl::run_callbacks_twice() { notifies_twice_ = true; }

const char   *ScriptedCl::asked_devices_of() const { return asked_devices_of_; }
std::uint32_t ScriptedCl::live_objects() const { return live_; }
std::uint32_t ScriptedCl::misuses() const { return misuses_; }

std::int32_t ScriptedCl::failure(Call call)
{
    const auto index = static_cast<std::uint32_t>(call);
    calls_[index]++;
    if (fail_at_[index] == never) return 0;
    if (from_[index] ? calls_[index] < fail_at_[index] : calls_[index] != fail_at_[index]) return 0;
    return errors_[index];
}

bool ScriptedCl::answers_nothing(Call call, std::int32_t &error)
{
    /* failure() counts the call, so the silent check reads the count it
     * left behind rather than counting again. */
    error = failure(call);
    if (error != 0) return true;
    const auto index = static_cast<std::uint32_t>(call);
    return silent_at_[index] != never && calls_[index] == silent_at_[index];
}

void *ScriptedCl::make(Kind kind)
{
    for (std::uint32_t row = 0u; row < max_objects; row++) {
        if (objects_[row].live) continue;
        objects_[row] = Object{};
        objects_[row].kind = kind;
        objects_[row].live = true;
        live_++;
        return reinterpret_cast<void *>(object_base + row);
    }
    return nullptr;
}

ScriptedCl::Object *ScriptedCl::required(void *handle, Kind kind)
{
    const std::uintptr_t row = reinterpret_cast<std::uintptr_t>(handle) - object_base;
    if (row >= max_objects || !objects_[row].live || objects_[row].kind != kind) {
        misuses_++;
        return nullptr;
    }
    return &objects_[row];
}

void ScriptedCl::release(void *handle, Kind kind, Call call)
{
    static_cast<void>(failure(call));
    Object *const object = required(handle, kind);
    if (object == nullptr) return;
    object->live = false;
    object->kind = Kind::none;
    live_--;
}

std::uint32_t ScriptedCl::platform_row(void *platform) const
{
    const std::uintptr_t row = reinterpret_cast<std::uintptr_t>(platform) - platform_base;
    return row < platforms_ ? static_cast<std::uint32_t>(row) : none;
}

bool ScriptedCl::is_device(void *device) const
{
    const std::uintptr_t row = reinterpret_cast<std::uintptr_t>(device) - device_base;
    return row < devices_;
}

std::int32_t ScriptedCl::answer_text(const char *text, char *into, std::size_t room,
                                     std::size_t &bytes) const
{
    if (text == nullptr) return refused;
    const std::size_t length = std::strlen(text);
    bytes = length + 1u;
    if (into == nullptr) return 0;
    if (room < bytes) return refused;
    std::memcpy(into, text, bytes);
    return 0;
}

std::int32_t ScriptedCl::platform_count(std::uint32_t &count)
{
    const std::int32_t failed = failure(Call::platform_count);
    if (failed != 0) return failed;
    count = platforms_;
    return 0;
}

std::int32_t ScriptedCl::platforms(void **into, std::uint32_t room, std::uint32_t &written)
{
    const std::int32_t failed = failure(Call::platforms);
    if (failed != 0) return failed;
    if (into == nullptr) return refused;
    const std::uint32_t fits = platforms_ < room ? platforms_ : room;
    for (std::uint32_t index = 0u; index < fits; index++) {
        into[index] = reinterpret_cast<void *>(platform_base + index);
    }
    /* A loader that overstates writes what it can and says it wrote more. */
    written = overstating_ ? platforms_ : fits;
    return 0;
}

std::int32_t ScriptedCl::platform_text(void *platform, std::uint32_t name, char *into,
                                       std::size_t room, std::size_t &bytes)
{
    const std::int32_t failed = failure(Call::platform_text);
    if (failed != 0) return failed;
    const std::uint32_t row = platform_row(platform);
    if (row == none) {
        misuses_++;
        return refused;
    }
    if (name == platform_icd_suffix) return answer_text(suffixes_[row], into, room, bytes);
    if (name == platform_name) return answer_text("scripted OpenCL", into, room, bytes);
    return refused;
}

std::int32_t ScriptedCl::devices(void *platform, void **into, std::uint32_t room,
                                 std::uint32_t &written)
{
    const std::int32_t   failed = failure(Call::devices);
    const std::uint32_t  row = platform_row(platform);
    if (row == none) {
        misuses_++;
        return refused;
    }
    asked_devices_of_ = suffixes_[row];
    if (failed != 0) return failed;
    if (into == nullptr) return refused;
    written = devices_ < room ? devices_ : room;
    for (std::uint32_t index = 0u; index < written; index++) {
        into[index] = reinterpret_cast<void *>(device_base + index);
    }
    return 0;
}

std::int32_t ScriptedCl::device_text(void *device, std::uint32_t name, char *into,
                                     std::size_t room, std::size_t &bytes)
{
    const std::int32_t failed = failure(Call::device_text);
    if (failed != 0) return failed;
    if (!is_device(device)) {
        misuses_++;
        return refused;
    }
    if (name != device_name) return refused;
    return answer_text(device_name_, into, room, bytes);
}

std::int32_t ScriptedCl::device_number(void *device, std::uint32_t name, std::uint64_t &value)
{
    const std::int32_t failed = failure(Call::device_number);
    if (failed != 0) return failed;
    if (!is_device(device)) {
        misuses_++;
        return refused;
    }
    if (name == device_vendor_id) value = vendor_;
    else if (name == device_luid) value = luid_;
    else if (name == device_svm_capabilities) value = svm_capabilities_;
    else return refused;
    return 0;
}

void *ScriptedCl::create_context(void *device, std::int32_t &error)
{
    error = failure(Call::create_context);
    if (error != 0) return nullptr;
    if (!is_device(device)) {
        misuses_++;
        error = refused;
        return nullptr;
    }
    void *const made = make(Kind::context);
    if (made == nullptr) error = refused;
    return made;
}

void *ScriptedCl::create_queue(void *context, void *device, std::int32_t &error)
{
    error = failure(Call::create_queue);
    if (error != 0) return nullptr;
    if (required(context, Kind::context) == nullptr || !is_device(device)) {
        error = refused;
        return nullptr;
    }
    void *const made = make(Kind::queue);
    if (made == nullptr) error = refused;
    return made;
}

void *ScriptedCl::create_buffer(void *context, std::uint64_t flags, std::size_t bytes,
                                std::int32_t &error)
{
    if (answers_nothing(Call::create_buffer, error)) return nullptr;
    if (required(context, Kind::context) == nullptr) {
        error = refused;
        return nullptr;
    }
    if (flags == 0u || bytes == 0u || bytes > sizeof(float) * max_elements) {
        misuses_++;
        error = refused;
        return nullptr;
    }
    void *const made = make(Kind::buffer);
    if (made == nullptr) error = refused;
    return made;
}

void *ScriptedCl::create_program(void *context, const char *source, std::int32_t &error)
{
    error = failure(Call::create_program);
    if (error != 0) return nullptr;
    if (required(context, Kind::context) == nullptr) {
        error = refused;
        return nullptr;
    }
    /* Anything that is not a kernel is a caller that lost its source. */
    if (source == nullptr || std::strncmp(source, "__kernel", 8u) != 0) {
        misuses_++;
        error = refused;
        return nullptr;
    }
    void *const made = make(Kind::program);
    if (made == nullptr) error = refused;
    return made;
}

std::int32_t ScriptedCl::build_program(void *program, void *device, const char *)
{
    const std::int32_t failed = failure(Call::build_program);
    Object *const built = required(program, Kind::program);
    if (built == nullptr || !is_device(device)) return refused;
    if (failed != 0) return failed;
    built->built = true;
    return 0;
}

std::int32_t ScriptedCl::program_build_text(void *program, void *device, char *into,
                                            std::size_t room, std::size_t &bytes)
{
    const std::int32_t failed = failure(Call::program_build_text);
    if (failed != 0) return failed;
    if (required(program, Kind::program) == nullptr || !is_device(device)) return refused;
    return answer_text(build_log_, into, room, bytes);
}

void *ScriptedCl::create_kernel(void *program, const char *name, std::int32_t &error)
{
    error = failure(Call::create_kernel);
    if (error != 0) return nullptr;
    Object *const built = required(program, Kind::program);
    if (built == nullptr || !built->built) {
        error = refused;
        return nullptr;
    }
    if (name == nullptr || std::strcmp(name, "vector_add") != 0) {
        error = unknown_kernel;
        return nullptr;
    }
    void *const made = make(Kind::kernel);
    if (made == nullptr) error = refused;
    return made;
}

std::int32_t ScriptedCl::set_kernel_buffer(void *kernel, std::uint32_t index, void *buffer)
{
    const std::int32_t failed = failure(Call::set_kernel_buffer);
    Object *const bound = required(kernel, Kind::kernel);
    if (bound == nullptr || required(buffer, Kind::buffer) == nullptr || index >= 3u) {
        return refused;
    }
    if (failed != 0) return failed;
    bound->arguments[index] = buffer;
    return 0;
}

std::int32_t ScriptedCl::write_buffer(void *queue, void *buffer, std::size_t bytes,
                                      const void *from)
{
    const std::int32_t failed = failure(Call::write_buffer);
    Object *const into = required(buffer, Kind::buffer);
    if (required(queue, Kind::queue) == nullptr || into == nullptr) return refused;
    if (from == nullptr || bytes > sizeof into->values) return refused;
    if (failed != 0) return failed;
    std::memcpy(into->values, from, bytes);
    return 0;
}

std::int32_t ScriptedCl::read_buffer(void *queue, void *buffer, std::size_t bytes, void *into)
{
    const std::int32_t failed = failure(Call::read_buffer);
    Object *const from = required(buffer, Kind::buffer);
    if (required(queue, Kind::queue) == nullptr || from == nullptr) return refused;
    if (into == nullptr || bytes > sizeof from->values) return refused;
    if (failed != 0) return failed;
    std::memcpy(into, from->values, bytes);
    return 0;
}

std::int32_t ScriptedCl::run_kernel(void *queue, void *kernel, std::size_t elements)
{
    const std::int32_t failed = failure(Call::run_kernel);
    Object *const running = required(kernel, Kind::kernel);
    if (required(queue, Kind::queue) == nullptr || running == nullptr) return refused;
    if (elements == 0u || elements > max_elements) return refused;
    if (failed != 0) return failed;
    Object *const first = required(running->arguments[0], Kind::buffer);
    Object *const second = required(running->arguments[1], Kind::buffer);
    Object *const sum = required(running->arguments[2], Kind::buffer);
    if (first == nullptr || second == nullptr || sum == nullptr) return refused;
    for (std::size_t index = 0u; index < elements; index++) {
        sum->values[index] = first->values[index] + second->values[index];
        /* A device that ran the wrong program writes something else into the
         * elements a test named. */
        if (index < wrong_) sum->values[index] += 1.0f;
    }
    return 0;
}

std::int32_t ScriptedCl::finish(void *queue)
{
    const std::int32_t failed = failure(Call::finish);
    if (required(queue, Kind::queue) == nullptr) return refused;
    return failed;
}

void *ScriptedCl::map_buffer(void *queue, void *buffer, std::uint64_t flags, std::size_t bytes,
                             std::int32_t &error)
{
    const bool refuses = answers_nothing(Call::map_buffer, error);
    Object *const mapped = required(buffer, Kind::buffer);
    if (required(queue, Kind::queue) == nullptr || mapped == nullptr) {
        error = refused;
        return nullptr;
    }
    if (flags == 0u || bytes == 0u || bytes > sizeof mapped->values) {
        misuses_++;
        error = refused;
        return nullptr;
    }
    if (refuses) return nullptr;
    if (mapped->mapped) {
        misuses_++;
        error = refused;
        return nullptr;
    }
    mapped->mapped = true;
    for (std::uint32_t index = 0u; index < stale_ && index < max_elements; index++) {
        mapped->values[index] += 1.0f;
    }
    return mapped->values;
}

std::int32_t ScriptedCl::unmap(void *queue, void *buffer, void *pointer)
{
    const std::int32_t failed = failure(Call::unmap);
    Object *const mapped = required(buffer, Kind::buffer);
    if (required(queue, Kind::queue) == nullptr || mapped == nullptr) return refused;
    if (!mapped->mapped || pointer != mapped->values) {
        misuses_++;
        return refused;
    }
    mapped->mapped = false;
    return failed;
}

void *ScriptedCl::svm_alloc(void *context, std::size_t bytes)
{
    const std::int32_t failed = failure(Call::svm_alloc);
    if (required(context, Kind::context) == nullptr) return nullptr;
    if (failed != 0 || svm_capabilities_ == 0u || shared_live_ || bytes == 0u ||
        bytes > sizeof shared_) {
        return nullptr;
    }
    shared_live_ = true;
    live_++;
    return shared_;
}

void ScriptedCl::svm_free(void *context, void *pointer)
{
    if (required(context, Kind::context) == nullptr) return;
    if (pointer != shared_ || !shared_live_) {
        misuses_++;
        return;
    }
    shared_live_ = false;
    shared_argument_ = nullptr;
    live_--;
}

std::int32_t ScriptedCl::set_kernel_svm(void *kernel, std::uint32_t index, void *pointer)
{
    const std::int32_t failed = failure(Call::set_kernel_svm);
    if (required(kernel, Kind::kernel) == nullptr || index >= 3u) return refused;
    if (pointer != shared_ || !shared_live_) {
        misuses_++;
        return refused;
    }
    if (failed != 0) return failed;
    shared_argument_ = pointer;
    return 0;
}

void *ScriptedCl::create_user_event(void *context, std::int32_t &error)
{
    if (answers_nothing(Call::create_user_event, error)) return nullptr;
    if (required(context, Kind::context) == nullptr) {
        error = refused;
        return nullptr;
    }
    void *const made = make(Kind::event);
    if (made == nullptr) {
        error = refused;
        return nullptr;
    }
    required(made, Kind::event)->status = submitted;
    return made;
}

std::int32_t ScriptedCl::watch_event(void *event, std::int32_t status, Notify notify,
                                     void *user_data)
{
    const std::int32_t failed = failure(Call::watch_event);
    Object *const watched = required(event, Kind::event);
    if (watched == nullptr || notify == nullptr) return refused;
    if (watched->watching) {
        misuses_++;
        return refused;
    }
    if (failed != 0) return failed;
    watched->watching = true;
    watched->watched = status;
    watched->notify = notify;
    watched->user_data = user_data;
    return 0;
}

std::int32_t ScriptedCl::set_user_event_status(void *event, std::int32_t status)
{
    const std::int32_t failed = failure(Call::set_user_event_status);
    Object *const set = required(event, Kind::event);
    if (set == nullptr) return refused;
    if (failed != 0) return failed;
    set->status = status;
    if (!set->watching || forgets_callbacks_ || status > set->watched) return 0;
    set->notify(event, status, set->user_data);
    /* An ICD whose event thread ran the same callback twice. */
    if (notifies_twice_) set->notify(event, status, set->user_data);
    return 0;
}

std::int32_t ScriptedCl::wait_for_event(void *event)
{
    const std::int32_t failed = failure(Call::wait_for_event);
    Object *const waited = required(event, Kind::event);
    if (waited == nullptr) return refused;
    if (failed != 0) return failed;
    /* Nothing here blocks: an event that is not complete is one this run would
     * have waited on forever. */
    return waited->status == complete ? 0 : refused;
}

void ScriptedCl::release_event(void *event) { release(event, Kind::event, Call::release_event); }
void ScriptedCl::release_kernel(void *kernel) { release(kernel, Kind::kernel, Call::release_kernel); }
void ScriptedCl::release_program(void *program)
{
    release(program, Kind::program, Call::release_program);
}
void ScriptedCl::release_buffer(void *buffer) { release(buffer, Kind::buffer, Call::release_buffer); }
void ScriptedCl::release_queue(void *queue) { release(queue, Kind::queue, Call::release_queue); }
void ScriptedCl::release_context(void *context)
{
    release(context, Kind::context, Call::release_context);
}

}  // namespace av
