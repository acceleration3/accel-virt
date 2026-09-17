/* The OpenGL ICD's thread state in a test: the test names the calling thread,
 * and each thread keeps its own slot value and last error, so two threads are
 * played in one without a scheduler. */
#ifndef AV_SCRIPTED_THREAD_SLOT_HPP
#define AV_SCRIPTED_THREAD_SLOT_HPP

#include "icd/opengl/thread_slot.hpp"

#include <cstdint>

namespace av {

class ScriptedThreadSlot final : public glicd::ThreadSlot {
public:
    static constexpr std::uint32_t max_threads = 8u;
    static constexpr std::uint32_t first_thread = 1u;

    ScriptedThreadSlot();

    /* Calls from now on come from thread `id` (1 to max_threads). */
    void          switch_to(std::uint32_t id);
    /* What GetLastError would answer thread `id`: 0 until something set it. */
    std::uint32_t last_error(std::uint32_t id) const;
    std::uint32_t last_error() const;
    void          clear_last_error();

    void         *get() override;
    void          set(void *value) override;
    std::uint32_t thread_id() override;
    void          set_last_error(std::uint32_t code) override;

private:
    std::uint32_t current_;
    void         *values_[max_threads];
    std::uint32_t errors_[max_threads];
};

}  // namespace av

#endif  // AV_SCRIPTED_THREAD_SLOT_HPP
