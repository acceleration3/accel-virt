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

    void          switch_to(std::uint32_t id);
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

}

#endif
