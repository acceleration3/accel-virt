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
    /* An adopted blob's wire handle is its id plus this. */
    static constexpr std::uint64_t wire_offset = 0x2000u;
    /* An imported object's wire handle, which no adopted blob can be. */
    static constexpr std::uint64_t object_handle = 0x9000u;

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
    /* Holds nothing across a batch and waits on no GPU. */
    void end_batch() override;
    bool poll() override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;
    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;
    [[nodiscard]] bool export_object(std::uint32_t kind, std::uint64_t wire_handle,
                                     std::uint32_t &handle_kind, int &fd) override;
    [[nodiscard]] bool import_object(std::uint32_t kind, std::uint32_t handle_kind, int fd,
                                     std::uint64_t size, std::uint64_t &wire_handle) override;
    void               release_object(std::uint32_t kind, std::uint64_t wire_handle) override;

    /* Whether this backend shares objects at all, so a test can be both an API
     * that does and one that does not. */
    void          set_shares(bool shares);
    std::uint32_t exported_objects() const;
    std::uint32_t imported_objects() const;
    std::uint32_t released_objects() const;

private:
    std::size_t   answer_bytes_;
    std::uint32_t ran_;
    std::uint32_t adopted_;
    std::uint32_t abandoned_;
    bool          lost_;
    bool          adopts_;
    bool          shares_;
    std::uint32_t exported_objects_;
    std::uint32_t imported_objects_;
    std::uint32_t released_objects_;
    int           descriptor_;
};

}  // namespace av

#endif  // AV_MOCK_SESSION_BACKEND_HPP
