/* A host API that runs whatever a test says it runs.
 *
 * The fourth implementation of SessionBackend and the only one that works on a
 * machine with no GPU: it echoes a payload, refuses an id, fails to decode,
 * loses its device, or asks the arena for more than the block holds, on
 * command. */
#ifndef AV_MOCK_SESSION_BACKEND_HPP
#define AV_MOCK_SESSION_BACKEND_HPP

#include "device/executor/session_backend.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockSessionBackend final : public SessionBackend {
public:
    /* Echoes the payload into the reply. */
    static constexpr std::uint32_t echo_id = 0x10001u;
    /* Answers `answer_bytes` of zeros, whatever the payload was. */
    static constexpr std::uint32_t long_answer_id = 0x10002u;
    /* Fails to decode, without touching the arena. */
    static constexpr std::uint32_t malformed_id = 0x10003u;
    /* Asks the arena for more than the block holds, then fails. */
    static constexpr std::uint32_t greedy_id = 0x10004u;
    /* Runs, and leaves the host device lost behind it. */
    static constexpr std::uint32_t lose_id = 0x10005u;

    MockSessionBackend();

    /* How long long_answer_id's answer is. */
    void          set_answer_bytes(std::size_t bytes);
    std::uint32_t ran() const;
    /* Whether adopt takes what it is offered. A backend that refuses is a real
     * case: an API that cannot import this kind is copied through instead. */
    void          set_adopts(bool adopts);
    std::uint32_t adopted() const;
    std::uint32_t abandoned() const;

    bool knows(std::uint32_t command_id) const override;
    [[nodiscard]] bool run(std::uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;

private:
    std::size_t   answer_bytes_;
    std::uint32_t ran_;
    std::uint32_t adopted_;
    std::uint32_t abandoned_;
    bool          lost_;
    bool          adopts_;
};

}  // namespace av

#endif  // AV_MOCK_SESSION_BACKEND_HPP
