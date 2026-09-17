#include "harness/icd/scripted_thread_slot.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

ScriptedThreadSlot::ScriptedThreadSlot() : current_(first_thread), values_{}, errors_{} {}

void ScriptedThreadSlot::switch_to(std::uint32_t id)
{
    if (id < first_thread || id > max_threads) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "thread %u cannot be played", id);
        return;
    }
    current_ = id;
}

std::uint32_t ScriptedThreadSlot::last_error(std::uint32_t id) const
{
    return id >= first_thread && id <= max_threads ? errors_[id - first_thread] : 0u;
}

std::uint32_t ScriptedThreadSlot::last_error() const { return last_error(current_); }
void          ScriptedThreadSlot::clear_last_error() { errors_[current_ - first_thread] = 0u; }
void         *ScriptedThreadSlot::get() { return values_[current_ - first_thread]; }
void          ScriptedThreadSlot::set(void *value) { values_[current_ - first_thread] = value; }
std::uint32_t ScriptedThreadSlot::thread_id() { return current_; }
void          ScriptedThreadSlot::set_last_error(std::uint32_t code) { errors_[current_ - first_thread] = code; }

}  // namespace av
